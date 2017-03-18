#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#ifndef EDEADLOCK
  #define EDEADLOCK EDEADLK
#endif
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <limits.h>
#include <assert.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include "config.h"
#include "memory.h"

#include "mhpdbg.h"
#include "emu.h"
#ifdef __linux__
#include "sys_vm86.h"
#endif
#include "bios.h"
#include "video.h"
#include "timers.h"
#include "cmos.h"
#include "mouse.h"
#include "xms.h"
#include "ipx.h"		/* TRB - add support for ipx */
#include "serial.h"
#include "int.h"
#include "bitops.h"
#include "pic.h"
#include "dpmi.h"

#include "keyb_clients.h"

#ifdef USE_MHPDBG
  #include "mhpdbg.h"
#endif

#include "userhook.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE       4096
#endif

#define MAX_FD 1024
static struct callback_s io_callback_func[MAX_FD];
static fd_set fds_sigio;

#if defined(SIG)
static inline int process_interrupt(SillyG_t *sg)
{
  int irq, ret=0;

  if ((irq = sg->irq) != 0) {
    h_printf("INTERRUPT: 0x%02x\n", irq);
    ret=(pic_request(pic_irq_list[irq])!=PIC_REQ_LOST);
  }
  return ret;
}

static inline void irq_select(void)
{
  if (SillyG) {
    int irq_bits = vm86_plus(VM86_GET_IRQ_BITS, 0);
    if (irq_bits) {
      SillyG_t *sg=SillyG;
      while (sg->fd) {
        if (irq_bits & (1 << sg->irq)) {
          if (process_interrupt(sg)) {
            vm86_plus(VM86_GET_AND_RESET_IRQ,sg->irq);
            h_printf("SIG: We have an interrupt\n");
          }
        }
        sg++;
      }
    }
  }
}
#endif

/*  */
/* io_select @@@  24576 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/base/misc/dosio.c --> src/base/misc/ioctl.c  */

static int numselectfd= 0;

void
io_select(void)
{
  int selrtn, i;
  struct timeval tvptr;
  fd_set fds = fds_sigio;

  tvptr.tv_sec=0L;
  tvptr.tv_usec=0L;

#if defined(SIG)
  irq_select();
#endif

  while ( ((selrtn = select(numselectfd, &fds, NULL, NULL, &tvptr)) == -1)
        && (errno == EINTR)) {
    tvptr.tv_sec=0L;
    tvptr.tv_usec=0L;
    g_printf("WARNING: interrupted io_select: %s\n", strerror(errno));
  }

  switch (selrtn) {
    case 0:			/* none ready, nothing to do :-) */
      return;

    case -1:			/* error (not EINTR) */
      error("bad io_select: %s\n", strerror(errno));
      break;

    default:			/* has at least 1 descriptor ready */
      for(i = 0; i < numselectfd; i++) {
        if (FD_ISSET(i, &fds) && io_callback_func[i].func) {
	  g_printf("GEN: fd %i has data for %s\n", i,
		io_callback_func[i].name);
	  io_callback_func[i].func(io_callback_func[i].arg);
	}
      }
      reset_idle(0);
      break;
    }
}

/*
 * DANG_BEGIN_FUNCTION add_to_io_select
 *
 * arguments:
 * fd - File handle to add to select statment
 * want_sigio - want SIGIO (1) if it's available, or not (0).
 *
 * description:
 * Add file handle to one of 2 select FDS_SET's depending on
 * whether the kernel can handle SIGIO.
 *
 * DANG_END_FUNCTION
 */
void
add_to_io_select_new(int new_fd, void (*func)(void *), void *arg,
	const char *name)
{
    int flags;
    if ((new_fd+1) > numselectfd) numselectfd = new_fd+1;
    if (numselectfd > MAX_FD) {
	error("Too many IO fds used.\n");
	leavedos(76);
    }
    flags = fcntl(new_fd, F_GETFL);
    fcntl(new_fd, F_SETOWN, getpid());
    fcntl(new_fd, F_SETFL, flags | O_ASYNC);
    FD_SET(new_fd, &fds_sigio);
    g_printf("GEN: fd=%d gets SIGIO for %s\n", new_fd, name);
    io_callback_func[new_fd].func = func;
    io_callback_func[new_fd].arg = arg;
    io_callback_func[new_fd].name = name;
}

/*
 * DANG_BEGIN_FUNCTION remove_from_io_select
 *
 * arguments:
 * fd - File handle to remove from select statment.
 * used_sigio - used SIGIO (1) if it's available, or not (0).
 *
 * description:
 * Remove a file handle from one of 2 select FDS_SET's depending
 * on whether the kernel can handle SIGIO.
 *
 * DANG_END_FUNCTION
 */
void remove_from_io_select(int new_fd)
{
    int flags;
    if (new_fd < 0) {
	g_printf("GEN: removing bogus fd %d (ignoring)\n", new_fd);
	return;
    }
    flags = fcntl(new_fd, F_GETFL);
    fcntl(new_fd, F_SETOWN, NULL);
    fcntl(new_fd, F_SETFL, flags & ~O_ASYNC);
    FD_CLR(new_fd, &fds_sigio);
    g_printf("GEN: fd=%d removed from select SIGIO\n", new_fd);
    io_callback_func[new_fd].func = NULL;
}

void ioselect_done(void)
{
    int i;
    for (i = 0; i < MAX_FD; i++) {
	if (io_callback_func[i].func) {
	    remove_from_io_select(i);
	    close(i);
	}
    }
}
