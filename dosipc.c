/* first baby steps for using SYSV IPC in dosemu 0.48p1+
 *
 * Robert Sanders, started 3/1/93
 *
 * $Date: 1994/02/05 21:45:55 $
 * $Source: /home/src/dosemu0.49pl4g/RCS/dosipc.c,v $
 * $Revision: 1.22 $
 * $State: Exp $
 *
 * $Log: dosipc.c,v $
 * Revision 1.22  1994/02/05  21:45:55  root
 * Arranged keyboard to handle setting bios prior to calling int15 4f.
 *
 * Revision 1.21  1994/02/02  21:12:56  root
 * Bringing the pktdrvr up to speed.
 *
 * Revision 1.20  1994/02/01  20:57:31  root
 * With unlimited thanks to gorden@jegnixa.hsc.missouri.edu (Jason Gorden),
 * here's a packet driver to compliment Tim_R_Bird@Novell.COM's IPX work.[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[D[his packet driver  to compliment Tim_R_Bird@Novell.COM's IPX work.
 *
 * Revision 1.19  1994/02/01  19:25:49  root
 * Fix to allow multiple graphics DOS sessions with my Trident card.
 *
 * Revision 1.18  1994/01/31  22:40:18  root
 * Mouse
 *
 * Revision 1.17  1994/01/31  21:09:02  root
 * Mouse working with X.
 *
 * Revision 1.16  1994/01/31  18:44:24  root
 * Work on making mouse work
 *
 * Revision 1.15  1994/01/30  12:30:23  root
 * Attempting to get mouse to turn of when switching to another console.
 *
 * Revision 1.14  1994/01/27  19:43:54  root
 * Preparing for IPX of Tim's.
 *
 * Revision 1.13  1994/01/25  20:02:44  root
 * Exchange stderr <-> stdout.
 *
 * Revision 1.12  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.11  1994/01/19  17:51:14  root
 * More Mickey Mousing around with keyboard to get it emulating proper keyboard
 * behavior. Better, but still not correct!
 *
 * Revision 1.10  1994/01/01  17:06:19  root
 * More debug for keyboard
 *
 * Revision 1.9  1993/12/22  11:45:36  root
 * Keyboard enhancements, going to REAL int9
 *
 * Revision 1.8  1993/12/05  20:59:03  root
 * Keyboard Work.
 *
 * Revision 1.7  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.6  1993/11/29  22:44:11  root
 * More work on keyboards
 * ,
 *
 * Revision 1.5  1993/11/29  00:05:32  root
 * Overhaul Keyboard.
 *
 * Revision 1.4  1993/11/25  22:45:21  root
 * About to destroy keybaord routines.
 *
 * Revision 1.3  1993/11/23  22:24:53  root
 * *** empty log message ***
 *
 * Revision 1.2  1993/11/17  22:29:33  root
 * Keyboard char behind patch
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.4  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.3  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.2  1993/03/04  22:35:12  root
 * put in perfect shared memory, HMA and everything.  added PROPER_STI.
 *
 * Revision 1.1  1993/03/02  03:06:42  root
 * Initial revision
 *
 */
#define DOSIPC_C

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_ether.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "config.h"
#include "memory.h"
#include "emu.h"
#include "termio.h"

/* also includes <sys/ipc.h> and <sys/shm.h> */
#include "dosipc.h"

/* #define SIG 1 */

extern u_char keepkey;
extern int_count[];
extern struct config_info config;
extern int in_readkeyboard, keybint;
extern int ignore_segv;
extern struct CPU cpu;
extern int pd_sock;

param_t *param;

#define PAGE_SIZE	4096

/* when first started, dosemu should do a start_dosipc().
 * before leaving for good, dosemu should do a stop_dosipc().
 *
 */

#ifndef SA_RESTART
#define SA_RESTART 0
#endif

#define SETSIG(sa, sig, fun)	sa.sa_handler = fun; \
				sa.sa_flags = SA_RESTART; \
                                sigemptyset(&sa.sa_mask); \
				sigaddset(&sa.sa_mask, SIGALRM);  \
				sigaction(sig, &sa, NULL);

pid_t parent_pid = -1, ipc_pid = -1;
int ipc_fd[2] =
{-1, -1}, key_fd[2] =
{-1, -1};
int exitipc = 0;
int in_int16 = 0;
int readkey = 0;

/* For the packet driver */
us *cur_pd_size = (us *) 0xCFFFE;
char *cur_pd_buf;
int cpd_sock=0;

/* sent_key is is used to hold answers to a DMSG_READKEY request */
u_short sent_key = 0;

u_long secno = 0;

void child_tick(int sig);
int printer_tick(u_long secno);
void ipc_command(struct ipcpkt *pkt), ipc_command_channel(void);
void child_dokey(int);
void inline process_interrupt(int);
void inline child_packet(int);

/* my test shared memory IDs */
struct {
  int idt,			/* 0x00000-0x0ffff */
   low,				/* 0x10000-0x9ffff */
   high;			/* 0xc0000-0xfffff */

  int param;			/* shared parameter block */
  char *paramaddr;		/* shared param address */

  int HMA,			/* normal mem if idt not attached at 0x100000 */
   hmastate;			/* 1 if HMA mapped in, 0 if idt mapped in */
}

sharedmem;

void 
set_a20(int enableHMA)
{
  if (sharedmem.hmastate == enableHMA)
    error("ERROR: redundant %s of A20!\n", enableHMA ? "enabling" :
	  "disabling");

  /* to turn the A20 on, one must unmap the "wrapped" low page, and
   * map in the real HMA memory. to turn it off, one must unmap the HMA
   * and make FFFF:xxxx addresses wrap to the low page.
   */
  if (enableHMA)
    HMA_MAP(sharedmem.HMA);
  else
    HMA_MAP(sharedmem.idt);

  sharedmem.hmastate = enableHMA;
}

/* this is for the parent process...currently NOT compatible with
 * the -b map_bios flags!
 *
 * this is very kludgy.  don't even ask.  you don't want to know
 */

void 
memory_setup(void)
{
  struct shmid_ds crap;
  unsigned char *ptr;

  /* dirty all pages */
  ignore_segv++;
  for (ptr = 0; ptr < (unsigned char *) (1024 * 1024); ptr += 4096)
    *ptr = *ptr;
  ignore_segv--;

  /* for clearing out a section for attaching shared memory segments,
   * munmap() from the second page to start of video mem, and
   * top of video buffer to top of HMA */
  if (munmap((caddr_t) PAGE_SIZE, (size_t) (0xa0000 - PAGE_SIZE)) == -1)
    error("ERROR: munmap1 failed: %s (%d)\n", strerror(errno), errno);

  if (munmap((caddr_t) 0x100000, (size_t) (0x110000 - 0x100000)) == -1)
    error("ERROR: munmap2 failed: %s (%d)\n", strerror(errno), errno);

  /* create the shared memory regions.
   * IDT  = region from 0-0xffff.  this can also be mapped to the HMA
   * LOW  =   "     "   0x10000-0x9ffff. everything up to video memory.
   * HIGH =   "     "   0xc0000-0xfffff. everything up to HMA.
   */
#define ALLOC_SHMSEG(keyholder, size, perms) do { \
  if ((keyholder = shmget(IPC_PRIVATE, size, perms)) == - 1) \
    error("ERROR: shmget size 0x%08x failed: %s (%d)\n", size, \
   strerror(errno), errno); } while(0)

  ALLOC_SHMSEG(sharedmem.idt, 0x10000, 0755);
  ALLOC_SHMSEG(sharedmem.low, 0xa0000 - 0x10000, 0755);
  ALLOC_SHMSEG(sharedmem.HMA, 0x10000, 0755);

  ALLOC_SHMSEG(sharedmem.param, PARAM_SIZE, 0755);

  I_printf("IPC: shm...idt: %d, low: %d, param %d, HMA %d\n", sharedmem.idt,
	   sharedmem.low, sharedmem.param, sharedmem.HMA);

  /* attach regions: page 0 (idt) at address 0 (must be specified as 1 and
   * SHM_RND, and must specify new SHM_REMAP flag to overwrite existing
   * memory (cannot munmap() it!) and code space.
   */
#define ATTACH_SHMSEG(key, address, flags) ({ char *tmp; \
  if ( (tmp=shmat(key, (char *)(address), flags)) == (char *)-1) \
    error("ERROR: shmat @ 0x%08x failed: %s (%d)\n", address, \
   strerror(errno), errno); tmp; })

  /* initially, no HMA */
  sharedmem.hmastate = 0;

  ATTACH_SHMSEG(sharedmem.idt, 0x000001, SHM_REMAP | SHM_RND);
  ATTACH_SHMSEG(sharedmem.idt, 0x100000, SHM_REMAP);
  ATTACH_SHMSEG(sharedmem.low, 0x010000, SHM_REMAP);

  /* XXX - to be safe, we should specify the address here */
  param = (param_t *) sharedmem.paramaddr = ATTACH_SHMSEG(sharedmem.param, 0, 0);

  I_printf("PARAM: number %d, address %p\n",
	   sharedmem.param, sharedmem.paramaddr);

  /* mark the three attached segments for deletion.  Note that the
   * segment is not deleted until the attach count is 0, and these
   * three will always have an attach count of at least 1 because
   * of the parent process.  the sharedmem.HMA segment will vary from
   * 0 to 1 (starting at 0), so we mark it for deletion in the child
   * process.
   */
  shmctl(sharedmem.idt, IPC_RMID, &crap);
  shmctl(sharedmem.low, IPC_RMID, &crap);
  shmctl(sharedmem.param, IPC_RMID, &crap);

#if 0
  /* zero the DOS address space... is this really necessary? */
  memset(NULL, 0, 640 * 1024);
#endif

  /* for EMS */
  bios_emm_init();
  xms_init();
}

void
memory_shutdown(void)
{
  I_printf("IPC: do-nothing memory_shutdown()\n");
}

void
post_dosipc(void)
{
}

#ifdef SIG
static int SillyG = 0;

#endif

void
start_dosipc(void)
{
  struct shmid_ds crap;
  struct sigaction sig;
  struct itimerval itv;

  /* open stream pipe between processes...Linux pipe(), unlike SYSV,
   * returns a uni-directional pipe, so we use the BSD socketpair()
   * function...note that we want the PARENT_FD to be non-blocking,
   * as the termio.c functions assume so, but we wish the child
   * process' end to be blocking (as we have no need for it to be
   * otherwise).  DON'T CHANGE THIS!
   */
  socketpair(AF_UNIX, SOCK_STREAM, 0, ipc_fd);
  fcntl(PARENT_FD, F_SETFL, O_RDWR /* | O_NONBLOCK */ );
  fcntl(CHILD_FD, F_SETFL, O_RDWR /* | O_NONBLOCK */ );

  I_printf("IPC: starting up...fd's %d and %d\n", ipc_fd[0], ipc_fd[1]);

  parent_pid = getpid();

  if ((ipc_pid = fork()) == -1) {
    error("ERROR: can't fork: %s (%d)\n", strerror(errno), errno);
    leavedos(1);
  }
  if (ipc_pid) 
    return;			/* parent process: return */

#ifdef SIG
  /* Get in touch with my Silly Interupt Driver */
  if ((SillyG = open("/dev/int/12", O_RDWR)) < 1) {
    fprintf(stderr, "Not gonna touch INT you requested!\n");
    SillyG = 0;
  }
  else {
    /* Reset interupt incase it went off already */
    write(SillyG, NULL, (int) NULL);
    fprintf(stderr, "Gonna monitor the INT you requested, Return=0x%02x\n", SillyG);
  }
#endif

  /* NOTE! the child inherits the parent's shared memory segments at
 * the same attach points; this is like an implicit shmat() for every
 * segment after the fork()
 */

  /* this is kind of nasty...remember that DOS runs in the lower 1 MB
   * of address space, and that child&parent processes under Linux
   * share pages until they're modified (copy-on-write).  Well, it
   * doesn't take long for DOS to make a lot of the pages dirty, and
   * we end up with anywhere from 0 to 1Mb of unused, copied pages
   * sitting in the child process' address space.  So we munmap() it.
   * in fact, we munmap() everything from the start of the second page
   * (PAGE_SIZE) to the start of the library (LIBSTART)
   *
   * munmap() cannot take a start address of 0, so we leave the
   * first page intact. details, details...
   */

  /* this is actually incorrect, memory from 0xa0000 to 0xbffff is not
   * shared...so we unmap it. once/if this gets shared, leave it in.
   */

  if (munmap((caddr_t) 0xa0000, (size_t) (0xc0000 - 0xa0000)) == -1)
    error("ERROR: child munmap1 failed: %s (%d)\n", strerror(errno), errno);

  if (munmap((caddr_t) 0x110000, (size_t) (LIBSTART - 0x110000)) == -1)
    error("ERROR: child munmap2 failed: %s (%d)\n", strerror(errno), errno);

  /* attach HMA segment to 0x1000000, and then mark it for deletion
   * this makes sure that the HMA shared segment will be deleted on
   * exit of the processes
   */
  HMA_MAP(sharedmem.HMA);
  shmctl(sharedmem.HMA, IPC_RMID, &crap);

  SETSIG(sig, SIGALRM, child_tick);
  SETSIG(sig, SIGVTALRM, child_tick);
  secno = 0;

  itv.it_interval.tv_sec = 1;
  itv.it_interval.tv_usec = 0;
  itv.it_value.tv_sec = 1;
  itv.it_value.tv_usec = 0;
  setitimer(ITIMER_VIRTUAL, &itv, NULL);

  main_dosipc();
}

void
stop_dosipc(void)
{
  if (ipc_pid > 1) {
    pid_t pid;
    int status, i;

    I_printf("IPC: closing %ld down..\n", (long) ipc_pid);

#ifdef SIG
    if (SillyG) {
      close(SillyG);
      fprintf(stderr, "Closing INT you opened!\n");
    }
#endif

  /* Close the packet driver socket */
  if (pd_sock)
    {
      pd_printf("Closing the network pd_sock: %x\n",pd_sock);
      CloseNetworkLink(pd_sock);
    }

    /* DMSG_ACK any outstanding waits, write "exit" message to pipe,
      * & set exitipc */
    /* ipc_send2child(DMSG_ACK); */
    ipc_send2child(DMSG_EXIT);

    /* Wait up to two seconds for ipc_pid to die */
    for (i = 0; i < 10; ++i) {
      usleep(200 * 1000L);
      pid = waitpid(ipc_pid, &status, WNOHANG);
      if (pid == ipc_pid)
	break;
    }

    /* If it didn't die on its own, kill it */
    if (pid != ipc_pid) {
      kill(ipc_pid, SIGHUP);
      pid = waitpid(ipc_pid, &status, 0);
    }

    /* Report child status */
    if (pid == -1)
      I_printf("wait error: %s (%d)\n", strerror(errno), errno);
    else {
      I_printf("IPC: exited with status 0x%X..", status);
      if (WIFEXITED(status))
	I_printf("exit argument: %d\n", WEXITSTATUS(status));
      else if (WIFSIGNALED(status))
	I_printf("terminal signal: %d\n", WTERMSIG(status));
      else if (WIFSTOPPED(status))
	I_printf("stopped: %d\n", WSTOPSIG(status));
      else
	I_printf("\nERROR: cannot determine how child died!\n");
    }

    wait(&status);
  }

  memory_shutdown();		/* kill all shared memory */
}

void 
dosipc_sighandler(int sig)
{
  I_printf("IPC: dosipc_sighandler caught signal %d\n", sig);
  fflush(stdout);
  exitipc = 1;
  _exit(1);
}

int 
ipc_waitingfromchild(void)
{
  int num_ready;

  RPT_SYSCALL(ioctl(PARENT_FD, FIONREAD, &num_ready));
  return (num_ready);
}

void 
ipc_sendpkt2child(struct ipcpkt *pkt)
{
  RPT_SYSCALL(write(PARENT_FD, pkt, MSGSIZE));
}

void 
ipc_sendpkt2parent(struct ipcpkt *pkt)
{
  RPT_SYSCALL(write(CHILD_FD, pkt, MSGSIZE));
}

void 
ipc_recvpktfromchild(struct ipcpkt *pkt)
{
  u_char *ptr = (u_char *) pkt;
  int chars = 0, new;

  I_printf("IPC: ipc_recvpktfromchild\n");

  do {
    new = RPT_SYSCALL(read(PARENT_FD, ptr + chars, MSGSIZE - chars));
    if (new != -1)
      chars += new;
    else
      error("recv error: %s\n", strerror(errno));
  } while (chars != MSGSIZE);
}

void 
ipc_recvpktfromparent(struct ipcpkt *pkt)
{
  u_char *ptr = (u_char *) pkt;
  int chars = 0, new;

  do {
    new = RPT_SYSCALL(read(CHILD_FD, ptr + chars, MSGSIZE - chars));
    if (new != -1)
      chars += new;
    else
      error("recv error: %s\n", strerror(errno));
  } while (chars != MSGSIZE);
}

/* ipc_send2child() *******************************************
 * parent calls this function to send messages to child
 */
void 
ipc_send2child(int msg)
{
  struct ipcpkt pkt;

  pkt.cmd = msg;
  RPT_SYSCALL(write(PARENT_FD, &pkt, MSGSIZE));
}

void 
ipc_send2parent(int msg)
{
  struct ipcpkt pkt;

  pkt.cmd = msg;
  RPT_SYSCALL(write(CHILD_FD, &pkt, MSGSIZE));
}

/* wait_for_ack() **********************************************
 * child calls this function to wait for an DMSG_ACK message from
 * the parent.
 */
void 
wait_for_ack(void)
{
  char abuf;
  int count;
  struct ipcpkt pkt;

  do {
    ipc_recvpktfromchild(&pkt);

    if (pkt.cmd != DMSG_ACK) {
      error("IPC: non DMSG_ACK message %d in wait_for_ack\n", pkt.cmd);
      ipc_command(&pkt);
    }

  } while ((pkt.cmd != DMSG_ACK) && (!exitipc));

  I_printf("IPC: (CHILD!) I died in wait_for_ack! exitipc = %d\n", exitipc);
  fflush(stdout);
}

void
ipc_wakeparent()
{
  I_printf("IPC: waking parent\n");
  if (parent_pid > 1)
    kill(parent_pid, SIGIPC);
}

#define SCANQ_LEN 100
u_short scan_queue[SCANQ_LEN];
int scan_queue_start = 0;
int scan_queue_end = 0;
u_char scanned = 0;
extern int convKey();
extern int InsKeyboard();
extern u_char move_kbd_key_flags;

/* the reason for the RPT_SYSCALL() around select() is this: system call
 * restarting has traditionally not applied to select().
 */
fd_set fds;
void 
ipc_select(void)
{
  int selrtn;

  readkey = 0;
  do {
    FD_ZERO(&fds);
    FD_SET(kbd_fd, &fds);
    FD_SET(CHILD_FD, &fds);

#ifdef SIG
    if (SillyG)
      FD_SET(SillyG, &fds);
#endif

    serial_fdset(&fds);

    switch (selrtn = RPT_SYSCALL(select(255, &fds, NULL, NULL, NULL))) {
    case 0:			/* none ready (timeout??? shouldn't happen) */
      error("ERROR: 0 select\n");
      break;

    case -1:			/* error (not EINTR) */
      error("ERROR: bad ipc_select: %s\n", strerror(errno));
      break;

    default:			/* has at least 1 descriptor ready */

#ifdef SIG
      if (SillyG)
	if (FD_ISSET(SillyG, &fds)) {
	  process_interrupt(SillyG);
	}
#endif

      if (FD_ISSET(kbd_fd, &fds)) {
	child_dokey(kbd_fd);
      }

      /* XXX */
      fflush(stdout);

      if (FD_ISSET(CHILD_FD, &fds)) {
	ipc_command_channel();
      }

      check_serial_ready(&fds);

      break;
    }

  } while (!exitipc);

}

void
child_sigsegv(int sig)
{
  int i;

  error("ERROR: CHILD SIGSEGV!!!!\n");
  for (i = 10; i > 0; i--) {
    fprintf(stderr, "\007");
    sleep(1);
  }
  fflush(stdout);
}

/*****************************************************************/
void 
main_dosipc(void)
{
  I_printf("IPC: entering main process...\n");

  {
    struct sigaction sigact;
    sigset_t mymask;

    sigfillset(&mymask);
    sigdelset(&mymask, SIGHUP);
    sigdelset(&mymask, SIGSEGV);
    sigprocmask(SIG_SETMASK, &mymask, NULL);

    sigact.sa_handler = dosipc_sighandler;
    sigact.sa_flags = 0;
    sigact.sa_mask = 0;
    sigaction(SIGHUP, &sigact, NULL);

    sigact.sa_handler = child_sigsegv;
    sigact.sa_flags = 0;
    sigact.sa_mask = 0;
    sigaction(SIGSEGV, &sigact, NULL);
  }

  ipc_select();

  I_printf("IPC: (CHILD!) I died in wait_for_ack! exitipc = %d\n", exitipc);
  fflush(stdout);
  _exit(0);
}

/*****************************************************************/

void 
ipc_command(struct ipcpkt *pkt)
{
  struct ipcpkt npkt;
  struct fd_set readset;
  struct timeval readwait;
  void child_puttx(int, u_char);

  switch (pkt->cmd) {
  case DMSG_EXIT:
    exitipc = 1;
    break;
  case DMSG_READKEY:
    k_printf("IPC/KBD: (child) got readkey request\n");
    readkey = 1;
    break;
  case DMSG_NOREADKEY:
    k_printf("IPC/KBD: (child) got NOreadkey request\n");
    readkey = 0;
    break;
  case DMSG_ACK:
    /* the exit routines send an extraneous IPC_ACK... */
    if (!exitipc)
      error("IPC: IPC_ACK message out of place!\n");
    break;
  case DMSG_SHMTEST:
    warn("IPC: testing child access to shared memory:\n");
    show_ints(0, 0x33);
    warn("IPC: HMA area...\n");
    {
      int i, j;
      unsigned char *ptr;

      for (i = 0, ptr = (void *) 0x100000; i < 4; i++, dbug_printf("\n"))
	for (j = 0; j <= 35; j++, ptr++)
	  dbug_printf("%02x", *ptr);

      dbug_printf("IPC: low page...\n");

      for (i = 0, (void *) ptr = 0; i < 4; i++, dbug_printf("\n"))
	for (j = 0; j <= 35; j++, ptr++)
	  dbug_printf("%02x", *ptr);
    }
    dbug_printf("A20 line tests %s, internal flag shows %s\n",
		memcmp((void *) 0, (void *) 0x100000, 0x10000) ? "on" :
		"off", sharedmem.hmastate ? "on" : "off");
    break;
#if 0
  case DMSG_INTO_INT16:
    I_printf("IPC/KBD: parent has entered a keyboard read...\n");
    in_int16 = 1;
    break;
  case DMSG_OUTOF_INT16:
    I_printf("IPC/KBD: parent has left a keyboard read...\n");
    in_int16 = 0;
    break;
#endif
  case DMSG_SER:
    error("ERROR: DMSG_SER???\n");
    break;
  case DMSG_MOPEN:
    child_open_mouse();
    break;
  case DMSG_MCLOSE:
    child_close_mouse();
    break;
  default:
    error("IPC: unknown command byte %d\n", pkt->cmd);
  }
}

void 
ipc_command_channel()
{
  char command;
  int count;
  struct ipcpkt pkt;
  struct timeval tv;
  fd_set fds;

  /* signals dont queue while packets do */
  do {

    ipc_recvpktfromparent(&pkt);

    /* parse the actual command */
    ipc_command(&pkt);
    /* check if any other packet has arrived with the same si */
    FD_ZERO(&fds);
    FD_SET(CHILD_FD, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
  } while (select(255, &fds, NULL, NULL, &tv) == 1);
}

void 
parent_setscan(u_short scan)
{
  struct ipcpkt pkt;

  pkt.cmd = DMSG_SETSCAN;
  pkt.u.key = scan;
  I_printf("child set scan %x\n", scan);
  ipc_wakeparent();
  ipc_sendpkt2parent(&pkt);
}

static int lastchr;
int inschr;

void
set_keyboard_bios(void)
{

    if (config.console_keyb) {
      if (config.keybint) {
        keepkey = 1;
	inschr = convKey(HI(ax));
      }
      else
	inschr = convKey(lastscan);
    }
    else
      inschr = lastchr;
    k_printf("parent nextscan found inschr=0x%02x, lastchr = 0x%02x, lastscan = 0x%04x scaned=%d\n", inschr, lastchr, lastscan, scanned);
    k_printf("MOVING   key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	     *(u_char *) 0x496, *(u_char *) 0x497, *(u_char *) 0x417, *(u_char *) 0x418);
}

void
insert_into_keybuffer(void)
{
    /* int15 fn=4f will reset CF if scan key is not to be used */
    if (!config.keybint || LWORD(eflags) & CF)
      keepkey = 1;
    else
      keepkey = 0;

    if (inschr && keepkey) {
      k_printf("IPC/KBD: (child) putting key in buffer\n");
      if (InsKeyboard(inschr))
	dump_kbuffer();
      else
	error("ERROR: InsKeyboard could not put key into buffer!\n");
    }
}

int 
parent_nextscan()
{

  k_printf("ENTERING key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	   *(u_char *) 0x496, *(u_char *) 0x497, *(u_char *) 0x417, *(u_char *) 0x418);
  k_printf("scanned=%d, start=%d, end=%d\n", scanned, scan_queue_start, scan_queue_end);
  if (!scanned && (scan_queue_start != scan_queue_end)) {
    scanned = 1;
    lastchr = scan_queue[scan_queue_start];
    if (scan_queue_start != scan_queue_end)
      scan_queue_start = (scan_queue_start + 1) % SCANQ_LEN;
    if (!config.console_keyb) {
      lastscan = lastchr >> 8;
    }
    else {
      lastscan = lastchr;
    }
  }
  k_printf("parent nextscan lastscan = 0x%04x scaned=%d\n", lastscan, scanned);
  k_printf("INSIDE   key 96 0x%02x, 97 0x%02x, kbc1 0x%02x, kbc2 0x%02x\n",
	   *(u_char *) 0x496, *(u_char *) 0x497, *(u_char *) 0x417, *(u_char *) 0x418);
  if (move_kbd_key_flags) {

    set_keyboard_bios();
    insert_into_keybuffer();

    move_kbd_key_flags = 0;
  }
  return 0;			/* no error */
}

void 
sigipc(int sig)
{
  struct ipcpkt npkt;
  struct timeval tv;
  fd_set fds;
  int i;

  in_vm86 = 0;

  do {
    I_printf("IPC: (parent) sigipc!!\n");

    while (ipc_waitingfromchild()) {
      struct ipcpkt pkt;

      ipc_recvpktfromchild(&pkt);

      switch (pkt.cmd) {
      case DMSG_CTRLBRK:
	I_printf("IPC: ctrl-break!\n");
	_regs.eflags &= ~0x40;
	do_int(0x1b);
	break;
      case DMSG_INT8:
	I_printf("IPC: timer int req!\n");
	do_hard_int(8);
	break;
      case DMSG_INT9:
	I_printf("IPC: keyb int req!\n");
	do_hard_int(9);
	break;
      case DMSG_PAUSE:
	I_printf("IPC: parent pausing\n");
	/* XXX - bad, no other commands processed */
	do {
	  ipc_recvpktfromchild(&pkt);
	} while (pkt.cmd != DMSG_UNPAUSE);
	break;
      case DMSG_UNPAUSE:
	I_printf("IPC: ERROR: unpause packet!\n");
	break;
      case DMSG_SETSCAN:
	k_printf("parent got set scan %x, startq=%d, endq=%d\n", pkt.u.key, scan_queue_start, scan_queue_end);
	scan_queue[scan_queue_end] = pkt.u.key;
	scan_queue_end = (scan_queue_end + 1) % SCANQ_LEN;
	if (config.keybint) {
	  k_printf("Hard queue\n");
	  queue_hard_int(9, NULL, NULL);
	}
	else {
	  k_printf("NOT Hard queue\n");
	  scanned = 0;
	  move_kbd_key_flags = 1;
	  parent_nextscan();
	}
	break;
      case DMSG_SER:
	s_printf("COMCOMCOMCOMCOM: ser packet in parent ser:%d\n",
		 pkt.u.params.param1);
	break;
      case DMSG_EXIT:
	I_printf("IPC: child requested exit\n");
	leavedos(0);
	break;
      case DMSG_SENDKEY:
	I_printf("IPC: (par) got SENDKEY\n");
	sent_key = pkt.u.key;
	break;
      case DMSG_SENDINT:
	I_printf("IPC: Hard interrupt request!\n");
	do_hard_int(pkt.u.key);
	break;
      default:
	I_printf("IPC: (par) unknown packet type %d\n", pkt.cmd);
      }
    }
    /* check if any other packet has arrived with the same si */
    FD_ZERO(&fds);
    FD_SET(PARENT_FD, &fds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
  } while (select(255, &fds, NULL, NULL, &tv) == 1);
}

void 
child_dokey(int fd)
{
  int pollret;
  u_int chr;

  k_printf("Prior to ReadKeyboard dosipc.c\n");
  ReadKeyboard(&chr, NOWAIT);
  k_printf("IPC/KBD: readret = 0x%04x\n", chr);
  k_printf("After ReadKeyboard dosipc.c\n");

}

void inline 
process_interrupt(int fd)
{
  int pollret, rrtn;
  u_int chr;

  if ((rrtn = read(fd, " ", 1))) {
    struct ipcpkt pkt;

    h_printf("IPC/INTERRUPT: 0x%02x, fd=%x\n", rrtn, fd);
    pkt.cmd = DMSG_SENDINT;
    if (rrtn < 8)
      pkt.u.key = rrtn + 8;
    else
      pkt.u.key = rrtn + 0x68;
    ipc_wakeparent();
    ipc_sendpkt2parent(&pkt);
  }
}

void 
child_tick(int sig)
{
  secno++;
#if CHILD_PRINTER
  printer_tick(secno);
#endif
}

void
dos_pause(void)
{
  ipc_wakeparent();
  ipc_send2parent(DMSG_PAUSE);
}

void
dos_unpause(void)
{
  ipc_send2parent(DMSG_UNPAUSE);
}

#undef DOSIPC_C
