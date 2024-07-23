#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <pthread.h>
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
#include "memory.h"

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
#include "emudpmi.h"
#include "ioselect.h"

#ifdef USE_MHPDBG
  #include "mhpdbg.h"
#endif


#ifndef PAGE_SIZE
#define PAGE_SIZE       4096
#endif

struct io_callback_s {
  void (*func)(int, void *);
  void *arg;
  const char *name;
  int fd;
  unsigned flags;
};
#define MAX_FD FD_SETSIZE
static struct io_callback_s io_callback_func[MAX_FD];
static struct io_callback_s io_callback_stash[MAX_FD];
static fd_set fds_sigio;
static fd_set fds_masked;

#if defined(SIG)
static inline int process_interrupt(SillyG_t *sg)
{
  int irq, ret=0;

  if ((irq = sg->irq) != 0) {
    h_printf("INTERRUPT: 0x%02x\n", irq);
    pic_request(irq);
  }
  return ret;
}

void irq_select(void)
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
#else
void irq_select(void)
{
}
#endif

/*  */
/* io_select @@@  24576 MOVED_CODE_BEGIN @@@ 01/23/96, ./src/base/misc/dosio.c --> src/base/misc/ioctl.c  */

static int max_fd;
static int syncpipe[2];
static pthread_t io_thr;
static pthread_mutex_t fun_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t blk_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t fds_mtx = PTHREAD_MUTEX_INITIALIZER;

static void ioselect_demux(void *arg)
{
    struct io_callback_s *p = arg;
    struct io_callback_s f;
    int isset;

    f = *p;
    free(p);
    pthread_mutex_lock(&fds_mtx);
    isset = FD_ISSET(f.fd, &fds_sigio);
    pthread_mutex_unlock(&fds_mtx);
    if (!isset) {
        /* already removed, complete event and exit */
        ioselect_complete(f.fd);
        return;
    }
    /* check if not removed from other thread */
    if (f.func) {
        g_printf("GEN: fd %i has data for %s\n", f.fd, f.name);
        f.func(f.fd, f.arg);
        reset_idle(0);
    }
}

static void io_select(void)
{
  int selrtn, i;
  fd_set fds;
  int nfds;

  pthread_mutex_lock(&fds_mtx);
  fds = fds_sigio;
  nfds = max_fd + 1;
  pthread_mutex_unlock(&fds_mtx);

  pthread_mutex_lock(&blk_mtx);
  for (i = 0; i < nfds; i++) {
    if (FD_ISSET(i, &fds_masked))
      FD_CLR(i, &fds);
  }
  pthread_mutex_unlock(&blk_mtx);

  pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  selrtn = RPT_SYSCALL(select(nfds, &fds, NULL, NULL, NULL));
  pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

  switch (selrtn) {
    case 0:			/* none ready, nothing to do :-) */
      return;

    case -1:			/* error (not EINTR) */
      error("bad io_select: %s\n", strerror(errno));
      break;

    default:			/* has at least 1 descriptor ready */
      pthread_mutex_lock(&blk_mtx);
      for (i = 0; i < nfds; i++) {
        if (FD_ISSET(i, &fds_masked))
          continue;
        if (FD_ISSET(i, &fds)) {
          if (io_callback_func[i].flags & IOFLG_IMMED) {
            if (io_callback_func[i].flags & IOFLG_MASKED)
              FD_SET(i, &fds_masked);
            io_callback_func[i].func(i, io_callback_func[i].arg);
          } else {
            struct io_callback_s *f = malloc(sizeof(*f));
            *f = io_callback_func[i];
            FD_SET(i, &fds_masked);
            add_thread_callback(ioselect_demux, f, "ioselect");
          }
        }
      }
      pthread_mutex_unlock(&blk_mtx);
      break;
  }
}

/*
 * DANG_BEGIN_FUNCTION add_to_io_select
 *
 * arguments:
 * fd - File handle to add to select statement
 * want_sigio - want SIGIO (1) if it's available, or not (0).
 *
 * description:
 * Add file handle to one of 2 select FDS_SET's depending on
 * whether the kernel can handle SIGIO.
 *
 * DANG_END_FUNCTION
 */
void
add_to_io_select_new(int new_fd, void (*func)(int, void *), void *arg,
	unsigned flags, const char *name)
{
    struct io_callback_s *f = &io_callback_func[new_fd];

    if (new_fd >= MAX_FD) {
	error("Too many IO fds used.\n");
	leavedos(76);
    }

    io_callback_stash[new_fd] = *f;

    g_printf("GEN: fd=%d gets SIGIO for %s\n", new_fd, name);
    pthread_mutex_lock(&fun_mtx);
    f->func = func;
    f->arg = arg;
    f->name = name;
    f->fd = new_fd;
    f->flags = flags;
    pthread_mutex_unlock(&fun_mtx);

    pthread_mutex_lock(&fds_mtx);
    if (new_fd > max_fd)
        max_fd = new_fd;
    FD_SET(new_fd, &fds_sigio);
    pthread_mutex_unlock(&fds_mtx);

    if (!io_callback_stash[new_fd].func)
	write(syncpipe[1], "+", 1);
}

/*
 * DANG_BEGIN_FUNCTION remove_from_io_select
 *
 * arguments:
 * fd - File handle to remove from select statement.
 * used_sigio - used SIGIO (1) if it's available, or not (0).
 *
 * description:
 * Remove a file handle from one of 2 select FDS_SET's depending
 * on whether the kernel can handle SIGIO.
 *
 * DANG_END_FUNCTION
 */
void remove_from_io_select(int fd)
{
    if (fd < 0 || !io_callback_func[fd].func) {
	g_printf("GEN: removing bogus fd %d (ignoring)\n", fd);
	return;
    }

    pthread_mutex_lock(&fun_mtx);
    io_callback_func[fd] = io_callback_stash[fd];
    pthread_mutex_unlock(&fun_mtx);
    io_callback_stash[fd].func = NULL;

    if (!io_callback_func[fd].func) {
	pthread_mutex_lock(&fds_mtx);
	FD_CLR(fd, &fds_sigio);
	pthread_mutex_unlock(&fds_mtx);
	write(syncpipe[1], "-", 1);
	g_printf("GEN: fd=%d removed from select SIGIO\n", fd);
    }
}

static void do_unmask(int fd)
{
    pthread_mutex_lock(&blk_mtx);
    FD_CLR(fd, &fds_masked);
    pthread_mutex_unlock(&blk_mtx);
    write(syncpipe[1], "=", 1);
}

void ioselect_complete(int fd)
{
    do_unmask(fd);
}

void ioselect_block(int fd)
{
    assert(io_callback_func[fd].flags & IOFLG_IMMED);
    pthread_mutex_lock(&blk_mtx);
    FD_SET(fd, &fds_masked);
    pthread_mutex_unlock(&blk_mtx);
}

void ioselect_unblock(int fd)
{
    assert(io_callback_func[fd].flags & IOFLG_IMMED);
    do_unmask(fd);
}

static void *ioselect_thread(void *arg)
{
    while (1) {
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
	io_select();
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_testcancel();
    }
    return NULL;
}

static void do_syncpipe(int fd, void *arg)
{
    char buf[4096];
    read(fd, buf, sizeof(buf));
}

void ioselect_init(void)
{
    struct io_callback_s *f;
    struct sched_param parm = { .sched_priority = 1 };

    FD_ZERO(&fds_sigio);
    FD_ZERO(&fds_masked);
    pipe(syncpipe);
    assert(syncpipe[0] < MAX_FD);
    f = &io_callback_func[syncpipe[0]];
    f->func = do_syncpipe;
    f->arg = NULL;
    f->name = "syncpipe";
    f->fd = syncpipe[0];
    f->flags = IOFLG_IMMED;
    max_fd = syncpipe[0];
    FD_SET(syncpipe[0], &fds_sigio);
    pthread_create(&io_thr, NULL, ioselect_thread, NULL);
    pthread_setschedparam(io_thr, SCHED_FIFO, &parm);
#if defined(HAVE_PTHREAD_SETNAME_NP) && defined(__GLIBC__)
    pthread_setname_np(io_thr, "dosemu: io");
#endif
}

void ioselect_done(void)
{
    pthread_cancel(io_thr);
    pthread_join(io_thr, NULL);
    close(syncpipe[1]);
}
