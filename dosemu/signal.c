#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <syscall.h>

#include "emu.h"
#include "mouse.h"
#include "video.h"
#include "timers.h"
#include "int.h"
#include "shared.h"
#include "../dpmi/dpmi.h"
#include "pic.h"
#include "ipx.h"

/* Variables for keeping track of signals */
#define MAX_SIG_QUEUE_SIZE 50
static u_short SIGNAL_head=0; u_short SIGNAL_tail=0;
struct  SIGNAL_queue {
  struct sigcontext_struct context;
  void (* signal_handler)(void);
};
static struct SIGNAL_queue signal_queue[MAX_SIG_QUEUE_SIZE];

/* for use by cli() and sti() */
static sigset_t oldset;

/* Similar to the sigaction function in libc, except it leaves alone the
   restorer field
   stolen from the wine-project */
int
dosemu_sigaction(int sig, struct sigaction *new, struct sigaction *old)
{
  __asm__("int $0x80":"=a"(sig)
	  :"0"(SYS_sigaction), "b"(sig), "c"(new), "d"(old));
  if (sig >= 0)
    return 0;
  errno = -sig;
  return -1;
}

/* DANG_BEGIN_FUNCTION signal_init
 *
 * description:
 *  Initialize the signals to have NONE being blocked.
 * Currently this is NOT of much use to DOSEMU.
 *
 * DANG_END_FUNCTION
 *
 */
void
signal_init(void)
{
  struct sigaction sa;
  sigset_t trashset;
  u_short counter;

  /* block no additional signals (i.e. get the current signal mask) */
  sigemptyset(&trashset);
  sigprocmask(SIG_BLOCK, &trashset, &oldset);
  g_printf("Initialized all signals to NOT-BLOCK\n");

  /* init signal handlers */

  NEWSETSIG(SIGILL, dosemu_fault);
  NEWSETQSIG(SIG_TIME, sigalrm);
  NEWSETSIG(SIGFPE, dosemu_fault);
  NEWSETSIG(SIGTRAP, dosemu_fault);

#ifdef SIGBUS /* for newer kernels */
  NEWSETSIG(SIGBUS, dosemu_fault);
#endif
  SETSIG(SIGINT, leavedos);   /* for "graceful" shutdown for ^C too*/
  SETSIG(SIGHUP, leavedos);	/* for "graceful" shutdown */
  SETSIG(SIGTERM, leavedos);
#if 0 /* Richard Stevens says it can't be caught. It's returning an
       * error anyway
       */
  SETSIG(SIGKILL, leavedos);
#endif
  SETSIG(SIGQUIT, sigquit);
  SETSIG(SIGWINCH, gettermcap); /* Adjust window sizes in DOS */
/*
  SETSIG(SIGUNUSED, timint);
*/
  NEWSETQSIG(SIGIO, sigio);
  NEWSETSIG(SIGSEGV, dosemu_fault);
}

/* 
 * DANG_BEGIN_FUNCTION cli
 *
 * description:
 *  Stop additional signals from interrupting DOSEMU.
 *
 * DANG_END_FUNCTION
 */
void
cli(void)
{
  sigset_t blockset;

  return; /* Should be OK now */
  sigfillset(&blockset);
  DOS_SYSCALL(sigprocmask(SIG_SETMASK, &blockset, &oldset));
}

/* 
 * DANG_BEGIN_FUNCTION sti
 *
 * description:
 *  Allow all signals to interrupt DOSEMU.
 *
 * DANG_END_FUNCTION
 */
void
sti(void)
{
  sigset_t blockset;

  return; /* Should be OK now */
  DOS_SYSCALL(sigprocmask(SIG_SETMASK, &oldset, &blockset));
}

/*
 * DANG_BEGIN_FUNCTION handle_signals
 *
 * description:
 *  Due to signals happening at any time, the actual work to be done 
 * because a signal occurs is done here in a serial fashion.
 *
 * The concept, should this eventualy work, is that a signal should only
 * flag that it has occurred and let DOSEMU deal with it in an orderly 
 * fashion as it executes the rest of it's code.
 *
 * DANG_END_FUNCTION
 *
 */
void handle_signals(void) {
  if ( SIGNAL_head != SIGNAL_tail ) {
    signal_queue[SIGNAL_head].signal_handler();
    SIGNAL_head = (SIGNAL_head + 1) % MAX_SIG_QUEUE_SIZE;
/* 
 * If more SIGNALS need to be dealt with, make sure we request interruption
 * by the kernel ASAP.
 */
      if (SIGNAL_head != SIGNAL_tail) {
	if (in_dpmi)
	  dpmi_eflags |= VIP;
        REG(eflags) |= VIP;
      }
  }
}

void SIGALRM_call(void){

  static volatile int running = 0;
  static volatile inalrm = 0;
  static int partials = 0;
#if VIDEO_CHECK_DIRTY
  static int update_pending = 0;
#endif
  int retval;
  
#ifdef X_SUPPORT
  if (config.X) 
     X_handle_events();
#endif

#if 1
#ifdef USING_NET
  /* check for available packets on the packet driver interface */
  /* (timeout=0, so it immediately returns when none are available) */
  pkt_check_receive(0);
#endif
#endif

  /* If it is running in termcap mode, then update the screen.
   * First it sets a running flag, so as to avoid re-entrancy of 
   * update_screen while it is in use.  After update_screen is done,
   * it returns a nonzero value if there was any updates to the screen.
   * If there were any updates to the screen, then set a countdown value
   * in order to give DOSEMU more CPU time, between screen updates.
   * This increases the DOSEMU-to-termcap update efficiency greatly.
   * The countdown counter is currently at a value of 2.
   */
   
   /* This now (again) tests screen_bitmap, i.e. checks if the screen 
    * was written to at all. This doesn't seem to achieve much for now,
    * but it will be helpful when implementing X graphics.
    * It's a bit tricky, however, because previous calls of update_screen
    * might not have updated the entire screen. Therefore update_pending
    * is set to 1 if only part of the screen was updated (update_screen
    * returns 2), meaning that update_screen will in any case be called
    * next time.
    * (*** this only applies if VIDEO_CHECK_DIRTY is set, which is 
    *      currently not the default! ***)
    *
    * return vales for update_screen are now:    
    *       0 nothing changed
    *       1 changed, entire screen updated
    *       2 changed, only partially updated
    *
    * note that update_screen also updates the cursor.
    */
#ifdef X_SUPPORT
  if (config.X && config.X_blinkrate) {
     X_blink_cursor();
  }
#endif
  if (!running && !video_update_lock) {
    if (Video->update_screen 
#if VIDEO_CHECK_DIRTY
       && (update_pending || vm86s.screen_bitmap&screen_mask)
#endif
       ) 
    {
       running = -1;
       retval = Video->update_screen();
#if 0
       v_printf("update_screen returned %d\n",retval);
#endif
#ifdef X_SUPPORT
       running = retval ? (config.X?config.X_updatefreq:config.term_updatefreq) 
                        : 0;
#else
       running = retval ? config.term_updatefreq : 0;
#endif
#if VIDEO_CHECK_DIRTY
       update_pending=(retval==2);
       vm86s.screen_bitmap=0;
#endif
    }
    else if (Video->update_cursor) {
       Video->update_cursor();
    }
  }
  else if (running > 0) {
    running--;
  }
  
  if (mice->intdrv)
    mouse_curtick();

  /* TRB - perform processing for the IPX Asynchronous Event Service */
#ifdef IPX
  if (config.ipxsup)
    AESTimerTick();
#endif

  timer_tick();

/*
 * DANG_BEGIN_REMARK
 *  Check for keyboard coming from client
 *  For now, first byte is interrupt requests from Client 
 * DANG_END_REMARK
 */
 if (*(u_char *)(shared_qf_memory + CLIENT_REQUEST_FLAG_AREA) & 0x40) {
   k_printf("KBD: Client sent key\n");
   pic_request (PIC_IRQ1);
   *(u_char *)(shared_qf_memory + CLIENT_REQUEST_FLAG_AREA) &=  ~0x40;
 }

  if (not_use_sigio)
    io_select(fds_no_sigio);

  /* this is for per-second activities */
  partials++;
  if (partials == FREQ) {
    partials = 0;
    printer_tick((u_long) 0);
    if (config.fastfloppy)
      floppy_tick();
  }

}

/* DANG_BEGIN_FUNCTION SIGNAL_save
 *
 * arguments:
 * context     - signal context to save.
 * signal_call - signal handling routine to be called.
 *
 * description:
 *  Save into an array structure queue the signal context of the current
 * signal as well as the function to call for dealing with this signal.
 * This is a queue because any signal may occur multiple times before
 * DOSEMU deals with it down the road.
 *
 * DANG_END_FUNCTION
 *
 */
inline void SIGNAL_save( void (*signal_call)() ) {
  signal_queue[SIGNAL_tail].signal_handler=signal_call;
  SIGNAL_tail = (SIGNAL_tail + 1) % MAX_SIG_QUEUE_SIZE;
  if (in_dpmi)
    dpmi_eflags |= VIP;
  REG(eflags) |= VIP;
}

/*
 * DANG_BEGIN_FUNCTION SIGIO_call
 *
 * description:
 *  Whenever I/O occurs on devices allowing SIGIO to occur, DOSEMU
 * will be flagged to run this call which inturn checks which 
 * fd(s) was set and execute the proper routine to get the I/O
 * from that device.
 *
 * DANG_END_FUNCTION
 *
 */
void SIGIO_call(void){
  /* Call select to see if any I/O is ready on devices */
  io_select(fds_sigio);
}

void
sigio(int sig, struct sigcontext_struct context)
{
  if (in_dpmi && !in_vm86)
    dpmi_sigio(&context);
  SIGNAL_save(SIGIO_call);
}

void
sigalrm(int sig, struct sigcontext_struct context)
{
  if (in_dpmi && !in_vm86)
    dpmi_sigio(&context);
  SIGNAL_save(SIGALRM_call);
}

void
sigquit(int sig)
{
  in_vm86 = 0;
  in_sighandler = 1;

  error("ERROR: sigquit called\n");
  show_ints(0, 0x33);
  show_regs(__FILE__, __LINE__);

  ignore_segv++;
  *(unsigned char *) 0x471 = 0x80;	/* ctrl-break flag */
  ignore_segv--;

  do_soft_int(0x1b);
  in_sighandler = 0;
}

#if 0
void
timint(int sig)
{
  in_vm86 = 0;
  in_sighandler = 1;

  warn("timint called: %04x:%04x -> %05x\n", ISEG(8), IOFF(8), IVEC(8));
  warn("(vec 0x1c)     %04x:%04x -> %05x\n", ISEG(0x1c), IOFF(0x1c),
       IVEC(0x1c));
  show_regs(__FILE__, __LINE__);

  pic_request(PIC_IRQ0);

  in_sighandler = 0;
}
#endif


