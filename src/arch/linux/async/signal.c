/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "config.h"
#include "emu.h"
#include "bios.h"
#include "mouse.h"
#include "video.h"
#include "timers.h"
#include "int.h"
#include "shared.h"
#include "dpmi.h"
#include "pic.h"
#include "ipx.h"
#include "pktdrvr.h"
#include "iodev.h"

#include "keyb_clients.h"
#include "keyb_server.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif

#ifdef X86_EMULATOR
#include "cpu-emu.h"
#endif

/* Variables for keeping track of signals */
#define MAX_SIG_QUEUE_SIZE 50
static u_short SIGNAL_head=0; u_short SIGNAL_tail=0;
struct  SIGNAL_queue {
/*  struct sigcontext_struct context; */
  void (* signal_handler)(void);
};
static struct SIGNAL_queue signal_queue[MAX_SIG_QUEUE_SIZE];


/* for use by cli() and sti() */
static sigset_t oldset;

#ifdef __linux__
/*
 * Thomas Winder <thomas.winder@sea.ericsson.se> wrote:
 * glibc-2 uses a different struct sigaction type than the one used in
 * the kernel. The function dosemu_sigaction (in
 * src/arch/linux/async/signal.c) bypasses the glibc provided syscall
 * by doint the int 0x80 by hand. This call expects struct sigaction
 * types as defined by the kernel. The whole story ends up with an
 * incorrect set sa_restorer field, which yields a wrong stack when
 * handling signals occuring when dpmi is active, which eventually
 * segfaults the program.
 */
int
dosemu_sigaction(int sig, struct sigaction *new, struct sigaction *old)
{
  struct my_sigaction {
    __sighandler_t my_sa_handler;
    unsigned long sa_mask;
    unsigned long sa_flags;
    void (*sa_restorer)(void);
  };

  struct my_sigaction my_sa;

  my_sa.my_sa_handler = new->sa_handler;
  my_sa.sa_mask = *((unsigned long *) &(new->sa_mask));
  my_sa.sa_flags = new->sa_flags;
  my_sa.sa_restorer = new->sa_restorer;

  return(syscall(SYS_sigaction, sig, &my_sa, NULL));
}
#endif /* __linux__ */

/* this cleaning up is necessary to avoid the port server becoming
   a zombie process */
static void cleanup_child(void)
{
  int status;
  restore_eflags_fs_gs();
  if (portserver_pid &&
      waitpid(portserver_pid, &status, WNOHANG) > 0 &&
      WIFSIGNALED(status))
    leavedos(1);
}

static void leavedos_signal(int sig)
{
  restore_eflags_fs_gs();
  leavedos(sig);
}

static void sigwinch(int sig)
{
  restore_eflags_fs_gs();
  gettermcap(sig);
}

/* Silly Interrupt Generator Initialization/Closedown */

#ifdef SIG
SillyG_t       *SillyG = 0;
static SillyG_t SillyG_[16 + 1];
#endif

/*
 * DANG_BEGIN_FUNCTION SIG_init
 * 
 * description: Allow DOSEMU to be made aware when a hard interrupt occurs
 * The IRQ numbers to monitor are taken from config.sillyint, each bit
 * corresponding to one IRQ. The higher 16 bit are defining the use of
 * SIGIO
 * 
 * DANG_END_FUNCTION
 */
void SIG_init(void)
{
#if defined(SIG)
    PRIV_SAVE_AREA
    /* Get in touch with Silly Interrupt Handling */
    if (config.sillyint) {
	char            prio_table[] =
	{8, 9, 10, 11, 12, 14, 15, 3, 4, 5, 6, 7};
	int             i,
	                irq;
	SillyG_t       *sg = SillyG_;
	for (i = 0; i < sizeof(prio_table); i++) {
	    irq = prio_table[i];
	    if (config.sillyint & (1 << irq)) {
		int ret;
		enter_priv_on();
		ret = vm86_plus(VM86_REQUEST_IRQ, (SIGIO << 8) | irq);
		leave_priv_setting();
		if ( ret > 0) {
		    g_printf("Gonna monitor the IRQ %d you requested\n", irq);
		    sg->fd = -1;
		    sg->irq = irq;
		    g_printf("SIG: IRQ%d, enabling PIC-level %ld\n", irq, pic_irq_list[irq]);
		    pic_seti(pic_irq_list[irq], SillyG_do_irq, 0, NULL);
		    pic_unmaski(pic_irq_list[irq]);
		    sg++;
		}
	    }
	}
	sg->fd = 0;
	if (sg != SillyG_)
	    SillyG = SillyG_;
    }
#endif
}

void SIG_close(void)
{
#if defined(SIG)
    if (SillyG) {
	SillyG_t       *sg = SillyG;
	while (sg->fd) {
	    vm86_plus(VM86_FREE_IRQ, sg->irq);
	    sg++;
	}
	g_printf("Closing all IRQ you opened!\n");
    }
#endif
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

  save_eflags_fs_gs();

  /* block no additional signals (i.e. get the current signal mask) */
  sigemptyset(&trashset);
  sigprocmask(SIG_BLOCK, &trashset, &oldset);
  g_printf("Initialized all signals to NOT-BLOCK\n");

#ifdef HAVE_SIGALTSTACK
  {
    stack_t ss;
    ss.ss_sp = cstack;
    ss.ss_size = sizeof(cstack);
    ss.ss_flags = SS_ONSTACK;
    
    if (sigaltstack(&ss, NULL) == 0)
      have_working_sigaltstack = 1;
  }
#endif

  /* init signal handlers - these are the defined signals:
   ---------------------------------------------
   SIGHUP		 1	S	leavedos
   SIGINT		 2	S	leavedos
   SIGQUIT		 3	S	sigquit
   SIGILL		 4	N	dosemu_fault
   SIGTRAP		 5	N	dosemu_fault
   SIGABRT		 6	S	leavedos
   SIGBUS		 7	N	dosemu_fault
   SIGFPE		 8	N	dosemu_fault
   SIGKILL		 9	na
   SIGUSR1		10	?	threads
   SIGSEGV		11	N	dosemu_fault
   SIGUSR2		12	NQ	(SIG_ACQUIRE)
   SIGPIPE		13      S	SIG_IGN
   SIGALRM		14	NQ	(SIG_TIME)sigalrm
   SIGTERM		15	S	leavedos
   SIGSTKFLT		16
   SIGCHLD		17	S       cleanup_child
   SIGCONT		18
   SIGSTOP		19
   SIGTSTP		20
   SIGTTIN		21	unused	was SIG_SER??
   SIGTTOU		22
   SIGURG		23
   SIGXCPU		24
   SIGXFSZ		25
   SIGVTALRM		26
   SIGPROF		27	N
   SIGWINCH		28	NQ	(SIG_RELEASE)
   SIGIO		29	NQ	sigio
   SIGPWR		30
   SIGUNUSED		31	na
  ------------------------------------------------ */
  NEWSETSIG(SIGILL, dosemu_fault);
  NEWSETQSIG(SIGALRM, sigalrm);
  NEWSETSIG(SIGFPE, dosemu_fault);
  NEWSETSIG(SIGTRAP, dosemu_fault);

#ifdef SIGBUS /* for newer kernels */
  NEWSETSIG(SIGBUS, dosemu_fault);
#endif
  SETSIG(SIGINT, leavedos_signal);   /* for "graceful" shutdown for ^C too*/
  SETSIG(SIGHUP, leavedos_signal);	/* for "graceful" shutdown */
  SETSIG(SIGTERM, leavedos_signal);
#if 0 /* Richard Stevens says it can't be caught. It's returning an
       * error anyway
       */
  SETSIG(SIGKILL, leavedos_signal);
#endif
  SETSIG(SIGQUIT, sigquit);
  SETSIG(SIGPIPE, SIG_IGN);

#ifdef X_SUPPORT
  if(config.X) {
    SETSIG(SIGWINCH, SIG_IGN);
  }
  else
#endif
  if(!config.console_video && !config.console_keyb) {
    SETSIG(SIGWINCH, sigwinch); /* Adjust window sizes in DOS */
  }
#ifdef X86_EMULATOR
  SETSIG(SIGPROF, SIG_IGN);
#endif
/*
  SETSIG(SIGUNUSED, timint);
*/
  NEWSETQSIG(SIGIO, sigio);
  NEWSETSIG(SIGSEGV, dosemu_fault);
  SETSIG(SIGCHLD, cleanup_child);

  SIG_init();			/* silly int generator support */
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
#ifdef X86_EMULATOR
    if ((config.cpuemu>1) && (debug_level('e')>3))
      {e_printf("EMU86: SIGNAL at %d\n",SIGNAL_head);}
#endif
    signal_pending = 0;
    signal_queue[SIGNAL_head].signal_handler();
    SIGNAL_head = (SIGNAL_head + 1) % MAX_SIG_QUEUE_SIZE;
/* 
 * If more SIGNALS need to be dealt with, make sure we request interruption
 * by the kernel ASAP.
 */
      if (SIGNAL_head != SIGNAL_tail) {
	signal_pending = 1;
      }
  }

  if (!signal_pending) {
    if (in_dpmi)
      dpmi_eflags &= ~VIP;
    REG(eflags) &= ~VIP;
  } else {
    if (in_dpmi)
      dpmi_eflags |= VIP;
    REG(eflags) |= VIP;
  }
}

/* ==============================================================
 *
 * This is called by default at around 100Hz.
 * (see timer_interrupt_init() in init.c)
 *
 * The actual formulas, starting with the configurable parameter
 * config.freq, are:
 *	config.freq				default=18
 *	config.update = 1E6/config.freq		default=54945
 *	timer tick(us) = config.update/6	default=9157.5us
 *		       = 166667/config.freq
 *	timer tick(Hz) = 6*config.freq		default=100Hz
 *
 * 6 is the magical TIMER_DIVISOR macro used to get 100Hz
 *
 * This call should NOT be used if you need timing accuracy - many
 * signals can get lost e.g. when kernel accesses disk, and the whole
 * idea of timing-by-counting is plain wrong. We'll need the Pentium
 * counter here.
 * ============================================================== */

static void SIGALRM_call(void)
{
  static int first = 0;
  static hitimer_t cnt200 = 0;
  static hitimer_t cnt1000 = 0;
  static volatile int running = 0;
#if VIDEO_CHECK_DIRTY
  static int update_pending = 0;
#endif
  int retval;
  
  if (first==0) {
    cnt200 =
    cnt1000 =
    pic_sys_time;	/* initialize */
    first = 1;
  }

#ifdef X_SUPPORT
  if (config.X) {
     X_handle_events();
     /* although actually the event handler handles the keyboard in X, keyb_client_run
      * still needs to be called in order to handle pasting.
      */
     keyb_client_run();
  }
  else
#endif
  /* for the SLang terminal we'll delay the release of shift, ctrl, ...
     keystrokes a bit */
  if (!config.console_keyb)
    keyb_client_run();

  /* for other front-ends, keyb_client_run() is called from ioctl.c if data is
   * available, so we don't need to do it here.
   */
  
  keyb_server_run();
   

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
  if (!running) {
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
       v_printf("update cursor\n");
       Video->update_cursor();
    }
  }
  else if (running > 0) {
    running--;
  }
  
  mouse_curtick();

#ifdef USE_SBEMU
  /* This is a macro */
  run_sb();
#endif

  /* TRB - perform processing for the IPX Asynchronous Event Service */
#ifdef IPX
  if (config.ipxsup)
    AESTimerTick();
#endif

  timer_tick();

#if 0
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
#endif

  io_select(fds_sigio);	/* we need this in order to catch lost SIGIOs */
  if (not_use_sigio)
    io_select(fds_no_sigio);

  /* Here we 'type in' prestrokes from commandline, as long as there are any
   * Were won't overkill dosemu, hence we type at a speed of 14cps
   */
  if (config.pre_stroke) {
    static int count=-1;
    if (--count < 0) {
      count = type_in_pre_strokes();
      if (count <0) count =7; /* with HZ=100 we have a stroke rate of 14cps */
    }
  }

  /* this should be for per-second activities, it is actually at
   * 200ms more or less (PARTIALS=5) */
  if ((pic_sys_time-cnt200) >= (PIT_TICK_RATE/PARTIALS)) {
    cnt200 = pic_sys_time;
/*    g_printf("**** ALRM: %dms\n",(1000/PARTIALS)); */

#ifdef IPX
    if (config.ipxsup)
      pic_request (PIC_IPX);
#endif
    printer_tick(0);
    if (config.fastfloppy)
      floppy_tick();
  }

/* We update the RTC from here if it has not been defined as a thread */

  /* this is for EXACT per-second activities (can produce bursts) */
  if ((pic_sys_time-cnt1000) >= PIT_TICK_RATE) {
    cnt1000 += PIT_TICK_RATE;
/*    g_printf("**** ALRM: 1sec\n"); */
    rtc_update();
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
inline void SIGNAL_save( void (*signal_call)(void) ) {
  signal_queue[SIGNAL_tail].signal_handler=signal_call;
  SIGNAL_tail = (SIGNAL_tail + 1) % MAX_SIG_QUEUE_SIZE;
  signal_pending = 1;
  if (in_dpmi)
    dpmi_eflags |= VIP;
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
static void SIGIO_call(void){
  /* Call select to see if any I/O is ready on devices */
  io_select(fds_sigio);
}

#ifdef __linux__
void
sigio(int sig, struct sigcontext_struct context)
{
  restore_eflags_fs_gs();
  if (in_dpmi && !in_vm86)
    dpmi_sigio(&context);
  SIGNAL_save(SIGIO_call);
}
#endif


#ifdef __linux__
void
sigalrm(int sig, struct sigcontext_struct context)
{
  restore_eflags_fs_gs();
  if (in_dpmi && !in_vm86)
    dpmi_sigio(&context);
  SIGNAL_save(SIGALRM_call);
}

#ifdef X86_EMULATOR
/* this is the same thing, but with a pointer parameter */
void
e_sigalrm(struct sigcontext_struct *context)
{
  if (in_dpmi && !in_vm86)
    dpmi_sigio(context);
  SIGNAL_save(SIGALRM_call);
}
#endif
#endif


void
sigquit(int sig)
{
  restore_eflags_fs_gs();
  in_vm86 = 0;

  error("sigquit called\n");
  show_ints(0, 0x33);
  show_regs(__FILE__, __LINE__);

  *(unsigned char *)BIOS_KEYBOARD_FLAGS = 0x80;	/* ctrl-break flag */

  do_soft_int(0x1b);
}

#if 0
void
timint(int sig)
{
  restore_eflags_fs_gs();
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


