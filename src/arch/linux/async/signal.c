/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>

#include "emu.h"
#include "vm86plus.h"
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
#include "dosemu_config.h"

#include "keyb_clients.h"
#include "keyb_server.h"

#ifdef USE_SBEMU
#include "sound.h"
#endif

#include "cpu-emu.h"

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

static struct {
  unsigned long eflags;
  unsigned short fs, gs;
#ifdef __x86_64__
  unsigned char *fsbase, *gsbase;
#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004
#endif
} eflags_fs_gs;

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

static void (*sighandlers[NSIG])(struct sigcontext *);

static void sigquit(struct sigcontext *);
static void sigalrm(struct sigcontext *);
static void sigio(struct sigcontext *);

#ifdef __x86_64__
static void sigasync(int sig, siginfo_t *si, void *uc);
#else

/* my glibc doesn't define this guy */
#ifndef SA_RESTORER
#define SA_RESTORER 0x04000000
#endif

static void sigasync(int sig, struct sigcontext_struct context);

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

void restore (void) asm ("__restore");

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

/* glibc non-pthread sigaction installs this restore
   function, which GDB recognizes as a signal trampoline.
   So it's a good idea for us to do the same
*/
#define XSTR(syscall) STR(syscall)
#define STR(syscall) #syscall
asm (
   ".byte 0\n"
   ".align 8\n"
   "__restore:\n"
   "  popl %eax\n"
   "  movl $"XSTR(SYS_sigreturn)", %eax\n"
   "  int  $0x80");

#endif

static void
dosemu_sigaction_wrapper(int sig, void *fun, int flags)
{
  struct sigaction sa;
  sigset_t mask;

  sa.sa_flags = flags;
  sigemptyset(&mask);
  addset_signals_that_queue(&mask);
  sa.sa_mask = mask;

#ifdef __x86_64__
  if (sa.sa_flags & SA_ONSTACK) {
    sa.sa_flags |= SA_SIGINFO;
    sa.sa_sigaction = fun;
  } else
    sa.sa_handler = fun;
  sigaction(sig, &sa, NULL);
#else
  sa.sa_handler = fun;
  if ((sa.sa_flags & SA_ONSTACK) && !have_working_sigaltstack)
  {
    sa.sa_flags &= ~SA_ONSTACK;
    /* Point to the top of the stack, minus 4
       just in case, and make it aligned  */
    sa.sa_restorer =
      (void (*)(void)) (((unsigned int)(cstack) + sizeof(*cstack) - 4) & ~3);
  } else {
  /* Linuxthread's signal wrappers are incompatible with dosemu:
     1. some old versions don't copy back the sigcontext_struct
        on sigreturn
     2. it may assume %gs points to something valid which it
        does not if we return from DPMI or kernel 2.4.x vm86().
     and it doesn't seem that the actions done by the wrapper
        would affect dosemu: if only seems to affect sigwait()
	and sem_post(), and we (most probably) don't use these */
  /* So use the kernel sigaction */
    sa.sa_flags |= SA_RESTORER;
    sa.sa_restorer = restore;
  }
  dosemu_sigaction(sig, &sa, NULL);
#endif
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
       sigaddset(x, SIGPROF);
       sigaddset(x, SIGWINCH);
       sigaddset(x, SIG_RELEASE);
       sigaddset(x, SIG_ACQUIRE);
}

static void newsetqsig(int sig, void *fun)
{
	dosemu_sigaction_wrapper(sig, fun, SA_RESTART|SA_ONSTACK);
}

void registersig(int sig, void (*fun)(struct sigcontext *))
{
	sighandlers[sig] = fun;
}

static void setsig(int sig, void *fun)
{
	dosemu_sigaction_wrapper(sig, fun, SA_RESTART);
}

static void newsetsig(int sig, void *fun)
{
	int flags = SA_RESTART|SA_ONSTACK;
	if (kernel_version_code >= 0x20600+14)
		flags |= SA_NODEFER;
	dosemu_sigaction_wrapper(sig, fun, flags);
}

#ifdef __x86_64__
static int dosemu_arch_prctl(int code, void *addr)
{
  return syscall(SYS_arch_prctl, code, addr);
}

/* Check if fs or gs point to the base without always needing a syscall
   (12 vs. 800 CPU cycles last time I measured).
   The DPMI client code may have changed fs/gs and then restored it to
   0, and that way the long base is gone (its base is still equal to
   the fs/gs base used by the DPMI client; 64bit code doesn't trap
   NULL selector references).
   There is a very small chance that the mov from %fs:0 page faults.
   In that case we fix it up in dosemu_fault0->check_fix_fs_gs_base.
 */

#define getfs0(byte) asm volatile ("movb %%fs:0, %0" : "=r"(byte))
#define getgs0(byte) asm volatile ("movb %%gs:0, %0" : "=r"(byte))

#define fix_fs_gs_base(seg,SEG)						\
  static void fix_##seg##base(void)					\
  {									\
    unsigned char segbyte, basebyte;					\
									\
    /* always fix fsbase/gsbase the DPMI client changed fs or gs */	\
    if (getsegment(seg) == eflags_fs_gs.seg) {				\
      volatile unsigned char *base = eflags_fs_gs.seg##base;		\
									\
      /* if the two locations have different bytes they must be different */ \
      get##seg##0(segbyte);						\
      basebyte = *base;							\
      if (segbyte == basebyte) {					\
									\
	/* else we must modify one to make sure it's ok */		\
	*base = basebyte + 1;						\
	get##seg##0(segbyte);						\
	*base = basebyte;						\
	if (segbyte != basebyte)					\
	  return;							\
      }									\
    }									\
    dosemu_arch_prctl(ARCH_SET_##SEG, eflags_fs_gs.fsbase);		\
    D_printf("DPMI: Set " #seg "base in signal handler\n");		\
  }

fix_fs_gs_base(fs,FS);
fix_fs_gs_base(gs,GS);

/* this function is called from dosemu_fault0 to check if
   fsbase/gsbase need to be fixed up, if the above asm codes
   cause a page fault.
 */
int check_fix_fs_gs_base(unsigned char prefix)
{
  unsigned char *addr, *base;
  int getcode, setcode;

  if (prefix == 0x65) { /* gs: */
    getcode = ARCH_GET_GS;
    setcode = ARCH_SET_GS;
    base = eflags_fs_gs.gsbase;
  } else {
    getcode = ARCH_GET_FS;
    setcode = ARCH_SET_FS;
    base = eflags_fs_gs.fsbase;
  }

  if (dosemu_arch_prctl(getcode, &addr) != 0)
    return 0;

  /* already fine, not fixing it up, but then the dosemu fault is fatal */
  if (addr == base)
    return 0;

  dosemu_arch_prctl(setcode, base);
  D_printf("DPMI: Fixed up %csbase in fault handler\n", prefix + 2);
  return 1;
}

#endif

/* init_handler puts the handler in a sane state that glibc
   expects. That means restoring fs and gs for vm86 (necessary for
   2.4 kernels) and fs, gs and eflags for DPMI. */
void init_handler(struct sigcontext_struct *scp)
{
  /*
   * FIRST thing to do in signal handlers - to avoid being trapped into int0x11
   * forever, we must restore the eflags.
   */
  loadflags(eflags_fs_gs.eflags);

#ifdef __x86_64__
  /* ds,es, and ss are ignored in 64-bit mode and not present or
     saved in the sigcontext, so we need to do it ourselves
     (using the 3 high words of the trapno field).
     fs and gs are set to 0 in the sigcontext, so we also need
     to save those ourselves */
  if (scp) {
    _ds = getsegment(ds);
    _es = getsegment(es);
    _ss = getsegment(ss);
    _fs = getsegment(fs);
    _gs = getsegment(gs);
  }
#endif

  if (in_vm86) {
#ifdef __i386__
#ifdef X86_EMULATOR
    if (!config.cpuemu)
#endif
      {
	if (getsegment(fs) != eflags_fs_gs.fs)
	  loadregister(fs, eflags_fs_gs.fs);
	if (getsegment(gs) != eflags_fs_gs.gs)
	  loadregister(gs, eflags_fs_gs.gs);
      }
#endif
    return;
  }

  if (scp && _cs == getsegment(cs)) return;

  /* else interrupting DPMI code with an LDT %cs */

  /* restore %fs and %gs for compatibility with NPTL. */
#ifdef __x86_64__
  if (eflags_fs_gs.fsbase)
    fix_fsbase();
  else
#endif
  if (getsegment(fs) != eflags_fs_gs.fs)
    loadregister(fs, eflags_fs_gs.fs);

#ifdef __x86_64__
  if (eflags_fs_gs.gsbase)
    fix_gsbase();
  else
#endif
  if (getsegment(gs) != eflags_fs_gs.gs)
    loadregister(gs, eflags_fs_gs.gs);
}

/* this cleaning up is necessary to avoid the port server becoming
   a zombie process */
static void cleanup_child(struct sigcontext_struct *scp)
{
  int status;
  init_handler(scp);
  if (portserver_pid &&
      waitpid(portserver_pid, &status, WNOHANG) > 0 &&
      WIFSIGNALED(status)) {
    error("port server terminated, exiting\n");
    leavedos(1);
  }
}

static void leavedos_signal(int sig)
{
  init_handler(NULL);
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

  /* initialize user data & code selector values (used by DPMI code) */
  /* And save %fs, %gs for NPTL */
  eflags_fs_gs.fs = getsegment(fs);
  eflags_fs_gs.gs = getsegment(gs);
  eflags_fs_gs.eflags = getflags();
#ifdef __x86_64__
  /* get long fs and gs bases. If they are in the first 32 bits
     normal 386-style fs/gs switching can happen so we can ignore
     fsbase/gsbase */
  dosemu_arch_prctl(ARCH_GET_FS, &eflags_fs_gs.fsbase);
  if ((unsigned long)eflags_fs_gs.fsbase <= 0xffffffff)
    eflags_fs_gs.fsbase = 0;
  dosemu_arch_prctl(ARCH_GET_GS, &eflags_fs_gs.gsbase);
  if ((unsigned long)eflags_fs_gs.gsbase <= 0xffffffff)
    eflags_fs_gs.gsbase = 0;
#endif

  /* init signal handlers - these are the defined signals:
   ---------------------------------------------
   SIGHUP		 1	S	leavedos
   SIGINT		 2	S	leavedos
   SIGQUIT		 3	N	sigquit
   SIGILL		 4	N	dosemu_fault
   SIGTRAP		 5	N	dosemu_fault
   SIGABRT		 6	S	leavedos
   SIGBUS		 7	N	dosemu_fault
   SIGFPE		 8	N	dosemu_fault
   SIGKILL		 9	na
   SIGUSR1		10	NQ	(SIG_RELEASE)sigasync
   SIGSEGV		11	N	dosemu_fault
   SIGUSR2		12	NQ	(SIG_ACQUIRE)sigasync
   SIGPIPE		13      S	SIG_IGN
   SIGALRM		14	NQ	(SIG_TIME)sigasync
   SIGTERM		15	S	leavedos
   SIGSTKFLT		16
   SIGCHLD		17	N       cleanup_child
   SIGCONT		18
   SIGSTOP		19
   SIGTSTP		20
   SIGTTIN		21	unused	was SIG_SER??
   SIGTTOU		22
   SIGURG		23
   SIGXCPU		24
   SIGXFSZ		25
   SIGVTALRM		26
   SIGPROF		27	NQ	(cpuemu,sigprof)sigasync
   SIGWINCH		28	NQ	(sigwinch)sigasync
   SIGIO		29	NQ	(sigio)sigasync
   SIGPWR		30
   SIGUNUSED		31	na
  ------------------------------------------------ */
  newsetsig(SIGILL, dosemu_fault);
  newsetqsig(SIGALRM, sigasync);
  registersig(SIGALRM, sigalrm);
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
  newsetsig(SIGQUIT, sigasync);
  registersig(SIGQUIT, sigquit);
  setsig(SIGPIPE, SIG_IGN);

#ifdef X86_EMULATOR
  setsig(SIGPROF, SIG_IGN);
#endif
/*
  setsig(SIGUNUSED, timint);
*/
  newsetqsig(SIGIO, sigasync);
  registersig(SIGIO, sigio);
  newsetqsig(SIGUSR1, sigasync);
  newsetqsig(SIGUSR2, sigasync);
  newsetqsig(SIGPROF, sigasync);
  newsetqsig(SIGWINCH, sigasync);
  newsetsig(SIGSEGV, dosemu_fault);
  newsetsig(SIGCHLD, sigasync);
  registersig(SIGCHLD, cleanup_child);

  /* unblock SIGIO, SIGALRM, SIG_ACQUIRE, SIG_RELEASE */
  sigemptyset(&set);
  addset_signals_that_queue(&set);
  sigprocmask(SIG_UNBLOCK, &set, NULL);
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

  if (signal_pending) {
    dpmi_return_request();
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

  /* update mouse cursor before updating the screen */
  mouse_curtick();

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
       running = retval ? (config.X?config.X_updatefreq:config.term_updatefreq) 
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
  
  if (Video->handle_events)
     Video->handle_events();

  /* for the SLang terminal we'll delay the release of shift, ctrl, ...
     keystrokes a bit */
  /* although actually the event handler handles the keyboard in X, keyb_client_run
   * still needs to be called in order to handle pasting.
   */
  if (!config.console_keyb)
    keyb_client_run();

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

  if (config.rdtsc)
    update_cputime_TSCBase();
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

  alarm_idle();

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
    dpmi_return_request();
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
static void sigio(struct sigcontext_struct *scp)
{
  SIGNAL_save(SIGIO_call);
  if (in_dpmi && !in_vm86)
    dpmi_sigio(scp);
}
 
static void sigalrm(struct sigcontext_struct *scp)
{
  if(e_gen_sigalrm(scp)) {
    SIGNAL_save(SIGALRM_call);
    if (in_dpmi && !in_vm86)
      dpmi_sigio(scp);
  }
}

static void sigasync0(int sig, struct sigcontext_struct *scp)
{
  init_handler(scp);
  if (sighandlers[sig])
	  sighandlers[sig](scp);
  dpmi_iret_setup(scp);
}

#ifdef __x86_64__
static void sigasync(int sig, siginfo_t *si, void *uc)
{
  sigasync0(sig, (struct sigcontext_struct *)
	   &((ucontext_t *)uc)->uc_mcontext);
}
#else
static void sigasync(int sig, struct sigcontext_struct context)
{
  sigasync0(sig, &context);
}
#endif
#endif


static void sigquit(struct sigcontext_struct *scp)
{
  in_vm86 = 0;

  error("sigquit called\n");
  show_ints(0, 0x33);
  show_regs(__FILE__, __LINE__);

  *(unsigned char *)BIOS_KEYBOARD_FLAGS = 0x80;	/* ctrl-break flag */

  do_soft_int(0x1b);
}
