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
#include "vgaemu.h"
#include "vgatext.h"
#include "timers.h"
#include "int.h"
#include "dpmi.h"
#include "pic.h"
#include "ipx.h"
#include "pktdrvr.h"
#include "iodev.h"
#include "serial.h"
#include "debug.h"

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
/* set if sigaltstack(2) is available */
static int have_working_sigaltstack;

 /* DANG_BEGIN_REMARK
  * We assume system call restarting... under linux 0.99pl8 and earlier,
  * this was the default.  SA_RESTART was defined in 0.99pl8 to explicitly
  * request restarting (and thus does nothing).  However, if this ever
  * changes, I want to be safe
  * DANG_END_REMARK
  */
#ifndef SA_RESTART
#define SA_RESTART 0
#error SA_RESTART Not defined
#endif

#ifndef SA_ONSTACK
#define SA_ONSTACK 0x08000000
#undef HAVE_SIGALTSTACK
#endif

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

struct kernel_sigaction {
  __sighandler_t kernel_sa_handler;
  unsigned long sa_mask;
  unsigned long sa_flags;
  void (*sa_restorer)(void);
};

static int
dosemu_sigaction(int sig, struct sigaction *new, struct sigaction *old)
{
  struct kernel_sigaction my_sa;

  my_sa.kernel_sa_handler = new->sa_handler;
  my_sa.sa_mask = *((unsigned long *) &(new->sa_mask));
  my_sa.sa_flags = new->sa_flags;
  my_sa.sa_restorer = new->sa_restorer;

  return(syscall(SYS_sigaction, sig, &my_sa, NULL));
}

static void
dosemu_sigaction_wrapper(int sig, void *fun, int flags)
{
  struct sigaction sa;
  struct kernel_sigaction kernel_sa;

  sa.sa_handler = (__sighandler_t)fun;
  sa.sa_flags = flags;
  sigemptyset(&sa.sa_mask);
  addset_signals_that_queue(&sa.sa_mask);

  if ((sa.sa_flags & SA_ONSTACK) && !have_working_sigaltstack)
  {
    sa.sa_flags &= ~SA_ONSTACK;
    /* Point to the top of the stack, minus 4
       just in case, and make it aligned  */
    sa.sa_restorer =
      (void (*)(void)) (((unsigned int)(cstack) + sizeof(*cstack) - 4) & ~3);
    dosemu_sigaction(sig, &sa, NULL);
    return;
  }

  sigaction(sig, &sa, NULL);

  syscall(SYS_sigaction, sig, NULL, &kernel_sa);
  /* no wrapper: no problem */
  if (kernel_sa.kernel_sa_handler == sa.sa_handler)
    return;

  /* if glibc installs a wrapper it's incompatible with dosemu:
     1. some old versions don't copy back the sigcontext_struct
        on sigreturn
     2. it may assume %gs points to something valid which it
        does not if we return from DPMI or kernel 2.4.x vm86().
     and it doesn't seem that the actions done by the wrapper
        would affect dosemu: if only seems to affect sigwait()
	and sem_post(), and we (most probably) don't use these */
  kernel_sa.kernel_sa_handler = sa.sa_handler;
  syscall(SYS_sigaction, sig, &kernel_sa, NULL);
}

/* DANG_BEGIN_FUNCTION NEWSETQSIG
 *
 * arguments:
 * sig - the signal to have a handler installed to.
 * fun - the signal handler function to install
 *
 * description:
 *  All signals that wish to be handled properly in context with the
 * execution of vm86() mode, and signals that wish to use non-reentrant
 * functions should add themselves to the ADDSET_SIGNALS_THAT_QUEUE define
 * and use SETQSIG(). To that end they will also need to be set up in an
 * order such as SIGIO.
 *
 * DANG_END_FUNCTION
 *
 */
void addset_signals_that_queue(sigset_t *x)
{
       sigaddset(x, SIGIO);
       sigaddset(x, SIGALRM);
       sigaddset(x, SIG_RELEASE);
       sigaddset(x, SIG_ACQUIRE);
}

void newsetqsig(int sig, void *fun)
{
	dosemu_sigaction_wrapper(sig, fun, SA_RESTART|SA_ONSTACK);
}

void setsig(int sig, void *fun)
{
	dosemu_sigaction_wrapper(sig, fun, SA_RESTART);
}

void newsetsig(int sig, void *fun)
{
	dosemu_sigaction_wrapper(sig, fun, SA_RESTART|SA_NODEFER|SA_ONSTACK);
}

/* this cleaning up is necessary to avoid the port server becoming
   a zombie process */
static void cleanup_child(void)
{
  int status;
  restore_eflags_fs_gs();
  if (portserver_pid &&
      waitpid(portserver_pid, &status, WNOHANG) > 0 &&
      WIFSIGNALED(status)) {
    error("port server terminated, exiting\n");
    leavedos(1);
  }
}

static void leavedos_signal(int sig)
{
  restore_eflags_fs_gs();
  leavedos(sig);
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
  sigset_t set;

#ifdef HAVE_SIGALTSTACK
  {
    stack_t ss;
    ss.ss_sp = cstack;
    ss.ss_size = sizeof(*cstack);
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
   SIGUSR1		10	NQ	(SIG_RELEASE)
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
   SIGWINCH		28	S	sigwinch
   SIGIO		29	NQ	sigio
   SIGPWR		30
   SIGUNUSED		31	na
  ------------------------------------------------ */
  newsetsig(SIGILL, dosemu_fault);
  newsetqsig(SIGALRM, sigalrm);
  newsetsig(SIGFPE, dosemu_fault);
  newsetsig(SIGTRAP, dosemu_fault);

#ifdef SIGBUS /* for newer kernels */
  newsetsig(SIGBUS, dosemu_fault);
#endif
  setsig(SIGINT, leavedos_signal);   /* for "graceful" shutdown for ^C too*/
  setsig(SIGHUP, leavedos_signal);	/* for "graceful" shutdown */
  setsig(SIGTERM, leavedos_signal);
#if 0 /* Richard Stevens says it can't be caught. It's returning an
       * error anyway
       */
  setsig(SIGKILL, leavedos_signal);
#endif
  setsig(SIGQUIT, sigquit);
  setsig(SIGPIPE, SIG_IGN);

#ifdef X86_EMULATOR
  setsig(SIGPROF, SIG_IGN);
#endif
/*
  setsig(SIGUNUSED, timint);
*/
  newsetqsig(SIGIO, sigio);
  newsetsig(SIGSEGV, dosemu_fault);
  setsig(SIGCHLD, cleanup_child);

  /* unblock SIGIO, SIGALRM, SIG_ACQUIRE, SIG_RELEASE */
  sigemptyset(&set);
  addset_signals_that_queue(&set);
  sigprocmask(SIG_UNBLOCK, &set, NULL);

  SIG_init();			/* silly int generator support */
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
static void handle_signals_force(int force_reentry) {
  static int in_handle_signals = 0;
  void (*signal_handler)(void);

  if (in_handle_signals++ && !force_reentry)
    error("BUG: handle_signals() re-entered!\n");

  if ( SIGNAL_head != SIGNAL_tail ) {
#ifdef X86_EMULATOR
    if ((config.cpuemu>1) && (debug_level('e')>3))
      {e_printf("EMU86: SIGNAL at %d\n",SIGNAL_head);}
#endif
    signal_pending = 0;
    signal_handler = signal_queue[SIGNAL_head].signal_handler;
    SIGNAL_head = (SIGNAL_head + 1) % MAX_SIG_QUEUE_SIZE;
    signal_handler();
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

  in_handle_signals--;
}

void handle_signals(void) {
  handle_signals_force(0);
}

void handle_signals_force_reentry(void) {
  handle_signals_force(1);
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

  if (Video->handle_events)
     Video->handle_events();

  /* for the SLang terminal we'll delay the release of shift, ctrl, ...
     keystrokes a bit */
  /* although actually the event handler handles the keyboard in X, keyb_client_run
   * still needs to be called in order to handle pasting.
   */
  if (!config.console_keyb)
    keyb_client_run();

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
  if (Video->update_screen && config.X_blinkrate) {
     blink_cursor();
  }
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
       running = retval ? (Video==&Video_term?config.term_updatefreq:config.X_updatefreq) 
                        : 0;
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

  serial_run();

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


