/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */


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

#ifdef __linux__
#include <signal.h>
#include <sys/vt.h>
#include <sys/vm86.h>
#include <syscall.h>
#endif

#include <sys/socket.h>
#include <sys/ioctl.h>


#include "config.h"
#include "memory.h"

#ifdef USE_MHPDBG
#include "mhpdbg.h"
#endif

#include "emu.h"

#include "bios.h"
#include "termio.h"
#include "video.h"
#include "timers.h"
#include "cmos.h"
#include "mouse.h"
#include "dosio.h"
#include "disks.h"
#include "xms.h"
#include "hgc.h"
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
void (*io_callback_func[MAX_FD])(void);

#if defined(SIG)
int SillyG_pendind_irq_bits=0;

int SillyG_do_irq(void)
{
  int irq=pic_level_list[pic_ilevel], ret;
  ret = do_irq();
  SillyG_pendind_irq_bits &= ~(1 << irq);
  return ret;
}
#endif

static inline int process_interrupt(SillyG_t *sg)
{
  int irq, ret=0;

  if ((irq = sg->irq) != 0) {
    h_printf("INTERRUPT: 0x%02x\n", irq);
    ret=(pic_request(pic_irq_list[irq])==PIC_REQ_OK);
  }
  return ret;
}


#if defined(SIG)
inline void irq_select(void)
{
  if (SillyG) {
    int irq_bits =vm86_plus(VM86_GET_IRQ_BITS,0) & ~SillyG_pendind_irq_bits;
    if (irq_bits) {
      SillyG_t *sg=SillyG;
      while (sg->fd) {
        if (irq_bits & (1 << sg->irq)) {
          if (process_interrupt(sg)) {
            vm86_plus(VM86_GET_AND_RESET_IRQ,sg->irq);
            SillyG_pendind_irq_bits |= 1 << sg->irq;
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
io_select(fd_set fds)
{
  int selrtn, i;
  struct timeval tvptr;

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
      break;

    case -1:			/* error (not EINTR) */
      error("bad io_select: %s\n", strerror(errno));
      break;

    default:			/* has at least 1 descriptor ready */
      for(i = 0; i < numselectfd; i++) {
        if (FD_ISSET(i, &fds) && io_callback_func[i]) {
	  g_printf("GEN: fd %i has data\n", i);
	  io_callback_func[i]();
	}
      }
      break;
    }
}

/* @@@ MOVE_END @@@ 24576 */



/*  */
/* io_select_init,add_to_io_select,remove_from_io_select,do_ioctl,queue_ioctl,do_queued_ioctl @@@  32768 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/emu.c --> src/base/misc/ioctl.c  */
/*
 * DANG_BEGIN_FUNCTION io_select_init
 * 
 * description: 
 * Initialize fd_sets to NULL for both SIGIO and NON-SIGIO.
 * 
 * DANG_END_FUNCTION
 */
void 
io_select_init(void) {
int i;
    FD_ZERO(&fds_sigio);	/* initialize both fd_sets to 0 */
    FD_ZERO(&fds_no_sigio);
    for (i = 0; i < MAX_FD; i++)
      io_callback_func[i] = NULL;
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
add_to_io_select(int new_fd, u_char want_sigio, void (*func)(void))
{
    if ((new_fd+1) > numselectfd) numselectfd = new_fd+1;
    if (numselectfd > MAX_FD) {
	error("Too many IO fds used.\n");
	leavedos(76);
    }
    if (want_sigio) {
	int             flags;
	flags = fcntl(new_fd, F_GETFL);
	fcntl(new_fd, F_SETOWN, getpid());
	fcntl(new_fd, F_SETFL, flags | O_ASYNC);
	FD_SET(new_fd, &fds_sigio);
	g_printf("GEN: fd=%d gets SIGIO\n", new_fd);
    } else {
	FD_SET(new_fd, &fds_no_sigio);
	g_printf("GEN: fd=%d does not get SIGIO\n", new_fd);
	not_use_sigio++;
    }
    io_callback_func[new_fd] = func;
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
void 
remove_from_io_select(int new_fd, u_char used_sigio)
{
    if (new_fd < 0) {
	g_printf("GEN: removing bogus fd %d (ignoring)\n", new_fd);
	return;
    }
    if (used_sigio) {
	int             flags;
	flags = fcntl(new_fd, F_GETFL);
	fcntl(new_fd, F_SETOWN, NULL);
	fcntl(new_fd, F_SETFL, flags & ~O_ASYNC);
	FD_CLR(new_fd, &fds_sigio);
	g_printf("GEN: fd=%d removed from select SIGIO\n", new_fd);
    } else {
	FD_CLR(new_fd, &fds_no_sigio);
	g_printf("GEN: fd=%d removed from select\n", new_fd);
	not_use_sigio++;
    }
    io_callback_func[new_fd] = NULL;
}

int
do_ioctl(int fd, int req, int param3)
{
    int             tmp;

    if (in_sighandler && in_ioctl) {
	k_printf("KBD: do_ioctl(): in ioctl %d 0x%04x 0x%04x.\nqueuing: %d 0x%04x 0x%04x\n",
		 curi.fd, curi.req, curi.param3, fd, req, param3);
	queue_ioctl(fd, req, param3);
	errno = EDEADLOCK;
#ifdef SYNC_ALOT
	fflush(stdout);
	sync();			/* for safety */
#endif
	return -1;
    } else {
	in_ioctl = 1;
	curi.fd = fd;
	curi.req = req;
	curi.param3 = param3;
	if (iq.queued) {
	    k_printf("KBD: detected queued ioctl in do_ioctl(): %d 0x%04x 0x%04x\n",
		     iq.fd, iq.req, iq.param3);
	}
	k_printf("KBD: IOCTL fd=0x%x, req=0x%x, param3=0x%x\n", fd, req, param3);
	tmp = ioctl(fd, req, param3);
	in_ioctl = 0;
	return tmp;
    }
}

int
queue_ioctl(int fd, int req, int param3)
{
    if (iq.queued) {
	error("ioctl already queued: %d 0x%04x 0x%04x\n", iq.fd, iq.req,
	      iq.param3);
	return 1;
    }
    iq.fd = fd;
    iq.req = req;
    iq.param3 = param3;
    iq.queued = 1;

    return 0;			/* success */
}

void
do_queued_ioctl(void)
{
    if (iq.queued) {
	iq.queued = 0;
	do_ioctl(iq.fd, iq.req, iq.param3);
    }
}
/* @@@ MOVE_END @@@ 32768 */



