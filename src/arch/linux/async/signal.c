#include "config.h"

#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <assert.h>
#include <linux/version.h>

#include "emu.h"
#include "bios.h"
#include "mouse.h"
#include "video.h"
#include "vgaemu.h"
#include "vgatext.h"
#include "render.h"
#include "timers.h"
#include "int.h"
#include "lowmem.h"
#include "coopth.h"
#include "dpmi.h"
#include "pic.h"
#include "ipx.h"
#include "pktdrvr.h"
#include "iodev.h"
#include "serial.h"
#include "debug.h"
#include "mhpdbg.h"
#include "utilities.h"
#include "userhook.h"
#include "ringbuf.h"
#include "dosemu_config.h"
#include "keyb_clients.h"
#include "keyb_server.h"
#include "sound.h"
#include "cpu-emu.h"
#include "sig.h"
/* work-around sigaltstack badness - disable when kernel is fixed */
#define SIGALTSTACK_WA 1
#if SIGALTSTACK_WA
#include "mcontext.h"
#include "mapping.h"
#endif

/* Variables for keeping track of signals */
#define MAX_SIG_QUEUE_SIZE 50
#define MAX_SIG_DATA_SIZE 128
static u_short SIGNAL_head=0; u_short SIGNAL_tail=0;
struct  SIGNAL_queue {
  void (*signal_handler)(void *);
  char arg[MAX_SIG_DATA_SIZE];
  size_t arg_size;
  const char *name;
};
static struct SIGNAL_queue signal_queue[MAX_SIG_QUEUE_SIZE];

#define MAX_SIGCHLD_HANDLERS 10
struct sigchld_hndl {
  pid_t pid;
  void (*handler)(void);
  int enabled;
};
static struct sigchld_hndl chld_hndl[MAX_SIGCHLD_HANDLERS];
static int chd_hndl_num;

static sigset_t q_mask;
static void *cstack;
#if SIGALTSTACK_WA
static void *backup_stack;
#endif

static int sh_tid;
static int in_handle_signals;
static void handle_signals_force_enter(int tid);
static void handle_signals_force_leave(int tid);
static void async_awake(void *arg);
static int event_fd;
static struct rng_s cbks;
#define MAX_CBKS 1000
static pthread_mutex_t cbk_mtx = PTHREAD_MUTEX_INITIALIZER;

struct eflags_fs_gs eflags_fs_gs;

static void (*sighandlers[NSIG])(struct sigcontext *);
static void (*qsighandlers[NSIG])(int sig, siginfo_t *si, void *uc);

static void sigquit(struct sigcontext *);
static void sigalrm(struct sigcontext *);
static void sigio(struct sigcontext *);
static void sigasync(int sig, siginfo_t *si, void *uc);

static void newsetqsig(int sig, void (*fun)(int sig, siginfo_t *si, void *uc))
{
	if (qsighandlers[sig])
		return;
	/* first need to collect the mask, then register all handlers
	 * because the same mask is used for every handler */
	sigaddset(&q_mask, sig);
	qsighandlers[sig] = fun;
}

static void qsig_init(void)
{
	struct sigaction sa;
	int i;

	sa.sa_flags = SA_RESTART | SA_ONSTACK | SA_SIGINFO;
	/* initially block all async signals. The handler will unblock some
	 * when it is safe (after segment registers are restored) */
	sa.sa_mask = q_mask;
	for (i = 0; i < NSIG; i++) {
		if (qsighandlers[i]) {
			sa.sa_sigaction = qsighandlers[i];
			sigaction(i, &sa, NULL);
		}
	}
}

void registersig(int sig, void (*fun)(struct sigcontext *))
{
	newsetqsig(sig, sigasync);
	sighandlers[sig] = fun;
}

static void newsetsig(int sig, void (*fun)(int sig, siginfo_t *si, void *uc))
{
	struct sigaction sa;

	sa.sa_flags = SA_RESTART | SA_ONSTACK | SA_SIGINFO;
	if (kernel_version_code >= KERNEL_VERSION(2, 6, 14))
		sa.sa_flags |= SA_NODEFER;
	sa.sa_mask = q_mask;
	sa.sa_sigaction = fun;
	sigaction(sig, &sa, NULL);
}

/* init_handler puts the handler in a sane state that glibc
   expects. That means restoring fs and gs for vm86 (necessary for
   2.4 kernels) and fs, gs and eflags for DPMI. */
__attribute__((no_instrument_function))
static void __init_handler(struct sigcontext *scp, int async)
{
#ifdef __x86_64__
  unsigned short __ss;
#endif
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
  _ds = getsegment(ds);
  _es = getsegment(es);
  /* some kernels save and switch ss, some do not... The simplest
   * thing is to assume that if the ss is from GDT, then it is already
   * saved. */
  __ss = getsegment(ss);
  if (DPMIValidSelector(__ss))
    _ss = __ss;
  _fs = getsegment(fs);
  _gs = getsegment(gs);
  if (_cs == 0) {
      if (config.dpmi && config.cpuemu < 4) {
	fprintf(stderr, "Cannot run DPMI code natively ");
	if (kernel_version_code < KERNEL_VERSION(2, 6, 15))
	  fprintf(stderr, "because your Linux kernel is older than version 2.6.15.\n");
	else
	  fprintf(stderr, "for unknown reasons.\nPlease contact linux-msdos@vger.kernel.org.\n");
	fprintf(stderr, "Set $_cpu_emu=\"full\" or \"fullsim\" to avoid this message.\n");
      }
      config.cpu_vm = CPUVM_EMU;
      config.cpuemu = 4;
      _cs = getsegment(cs);
  }
#endif

  if (in_vm86) {
#ifdef __i386__
#ifdef X86_EMULATOR
    if (config.cpu_vm != CPUVM_EMU)
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

  /* for async signals need to restore fs/gs even if dosemu code
   * was interrupted because it can be interrupted in a switching
   * routine when fs or gs are already switched */
  if (!DPMIValidSelector(_cs) && !async)
    return;

  /* restore %fs and %gs for compatibility with NPTL. */
  if (getsegment(fs) != eflags_fs_gs.fs)
    loadregister(fs, eflags_fs_gs.fs);
  if (getsegment(gs) != eflags_fs_gs.gs)
    loadregister(gs, eflags_fs_gs.gs);
#ifdef __x86_64__
  loadregister(ds, eflags_fs_gs.ds);
  loadregister(es, eflags_fs_gs.es);
  /* kernel has the following rule: non-zero selector means 32bit base
   * in GDT. Zero selector means 64bit base, set via msr.
   * So if we set selector to 0, need to use also prctl(ARCH_SET_xS). */
  if (!eflags_fs_gs.fs)
    dosemu_arch_prctl(ARCH_SET_FS, eflags_fs_gs.fsbase);
  if (!eflags_fs_gs.gs)
    dosemu_arch_prctl(ARCH_SET_GS, eflags_fs_gs.gsbase);
#endif
}

__attribute__((no_instrument_function))
void init_handler(struct sigcontext *scp, int async)
{
  /* Async signals are initially blocked.
   * We need to restore registers before unblocking async signals.
   * Otherwise the nested signal handler will restore the registers
   * and return; the current signal handler will then save the wrong
   * registers.
   * Note: in 64bit mode some segment registers are neither saved nor
   * restored by the signal dispatching code in kernel, so we have
   * to restore them by hands.
   * Note: most async signals are left blocked, we unblock only few.
   * Sync signals like SIGSEGV are never blocked.
   */
  sigset_t mask;
  __init_handler(scp, async);
  sigemptyset(&mask);
  sigaddset(&mask, SIGINT);
  sigaddset(&mask, SIGHUP);
  sigaddset(&mask, SIGTERM);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

#ifdef __x86_64__
__attribute__((no_instrument_function))
void deinit_handler(struct sigcontext *scp)
{
  /* no need to restore anything when returning to dosemu, but
   * can't check _cs because dpmi_iret_setup() could clobber it.
   * So just restore segregs unconditionally to stay safe.
   * It would also be a bit disturbing to return to dosemu with
   * DOS ds/es/ss, which is what the signal handler work with
   * in 64bit mode.
   * Note: ss in 64bit mode is reset by a syscall (sigreturn()),
   * so we don't need to restore it manually when returning to
   * dosemu at least (when returning to DPMI, dpmi_iret_setup()
   * takes care of ss).
   */

  if (_fs != getsegment(fs))
    loadregister(fs, _fs);
  if (_gs != getsegment(gs))
    loadregister(gs, _gs);

  loadregister(ds, _ds);
  loadregister(es, _es);
}
#endif

static int ld_sig;
static void leavedos_call(void *arg)
{
  int *sig = arg;
  leavedos(*sig);
}

int sigchld_register_handler(pid_t pid, void (*handler)(void))
{
  assert(chd_hndl_num < MAX_SIGCHLD_HANDLERS);
  chld_hndl[chd_hndl_num].handler = handler;
  chld_hndl[chd_hndl_num].pid = pid;
  chld_hndl[chd_hndl_num].enabled = 1;
  chd_hndl_num++;
  return 0;
}

int sigchld_enable_handler(pid_t pid, int on)
{
  int i;
  for (i = 0; i < chd_hndl_num; i++) {
    if (chld_hndl[i].pid == pid)
      break;
  }
  if (i >= chd_hndl_num)
    return -1;
  chld_hndl[i].enabled = on;
  return 0;
}

static void cleanup_child(void *arg)
{
  int i, status;
  pid_t pid2, pid = *(pid_t *)arg;

  for (i = 0; i < chd_hndl_num; i++) {
    if (chld_hndl[i].pid == pid)
      break;
  }
  if (i >= chd_hndl_num)
    return;
  if (!chld_hndl[i].enabled)
    return;
  pid2 = waitpid(pid, &status, WNOHANG);
  if (pid2 != pid)
    return;
  if (chld_hndl[i].handler)
    chld_hndl[i].handler();
}

/* this cleaning up is necessary to avoid the port server becoming
   a zombie process */
__attribute__((no_instrument_function))
static void sig_child(int sig, siginfo_t *si, void *uc)
{
  struct sigcontext *scp =
	(struct sigcontext *)&((ucontext_t *)uc)->uc_mcontext;
  init_handler(scp, 1);
  SIGNAL_save(cleanup_child, &si->si_pid, sizeof(si->si_pid), __func__);
  dpmi_iret_setup(scp);
  deinit_handler(scp);
}

void leavedos_from_sig(int sig)
{
  /* anything more sophisticated? */
  leavedos_main(sig);
}

static void leavedos_sig(int sig)
{
  dbug_printf("Terminating on signal %i\n", sig);
  if (ld_sig) {
    error("leavedos re-entered, exiting\n");
    _exit(3);
  }
  ld_sig = sig;
  SIGNAL_save(leavedos_call, &sig, sizeof(sig), __func__);
  /* abort current sighandlers */
  if (in_handle_signals) {
    g_printf("Interrupting active signal handlers\n");
    in_handle_signals = 0;
  }
}

__attribute__((noinline))
static void _leavedos_signal(int sig, struct sigcontext *scp)
{
  if (ld_sig) {
    error("gracefull exit failed, aborting (sig=%i)\n", sig);
    _exit(sig);
  }
  leavedos_sig(sig);
  if (!in_vm86)
    dpmi_sigio(scp);
  dpmi_iret_setup(scp);
}

__attribute__((no_instrument_function))
static void leavedos_signal(int sig, siginfo_t *si, void *uc)
{
  struct sigcontext *scp =
	(struct sigcontext *)&((ucontext_t *)uc)->uc_mcontext;
  init_handler(scp, 1);
  _leavedos_signal(sig, scp);
  deinit_handler(scp);
}

__attribute__((no_instrument_function))
static void abort_signal(int sig, siginfo_t *si, void *uc)
{
  struct sigcontext *scp =
	(struct sigcontext *)&((ucontext_t *)uc)->uc_mcontext;
  init_handler(scp, 0);
  gdb_debug();
  _exit(sig);
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

void sig_ctx_prepare(int tid)
{
  rm_stack_enter();
  clear_IF();
}

void sig_ctx_restore(int tid)
{
  rm_stack_leave();
}

static void signal_thr_post(int tid)
{
  in_handle_signals--;
}

static void signal_thr(void *arg)
{
  struct SIGNAL_queue *sig = &signal_queue[SIGNAL_head];
  struct SIGNAL_queue sig_c;	// local copy for signal-safety
  sig_c.signal_handler = signal_queue[SIGNAL_head].signal_handler;
  sig_c.arg_size = sig->arg_size;
  if (sig->arg_size)
    memcpy(sig_c.arg, sig->arg, sig->arg_size);
  sig_c.name = sig->name;
  SIGNAL_head = (SIGNAL_head + 1) % MAX_SIG_QUEUE_SIZE;
  if (debug_level('g') > 5)
    g_printf("Processing signal %s\n", sig_c.name);
  sig_c.signal_handler(sig_c.arg);
}

static void sigstack_init(void)
{
#ifndef MAP_STACK
#define MAP_STACK 0
#endif
#if SIGALTSTACK_WA
  cstack = alloc_mapping(MAPPING_SHARED, SIGSTACK_SIZE, -1);
  if (cstack == MAP_FAILED) {
    error("Unable to allocate stack\n");
    config.exitearly = 1;
    return;
  }
  backup_stack = alias_mapping(MAPPING_OTHER, -1, SIGSTACK_SIZE,
	PROT_READ | PROT_WRITE, cstack);
  if (backup_stack == MAP_FAILED) {
    error("Unable to allocate stack\n");
    config.exitearly = 1;
    return;
  }
#else
  cstack = mmap(NULL, SIGSTACK_SIZE, PROT_READ | PROT_WRITE,
	MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  if (cstack == MAP_FAILED) {
    error("Unable to allocate stack\n");
    config.exitearly = 1;
    return;
  }
#endif
}

/* DANG_BEGIN_FUNCTION signal_pre_init
 *
 * description:
 *  Initialize the signals to have NONE being blocked.
 * Currently this is NOT of much use to DOSEMU.
 *
 * DANG_END_FUNCTION
 *
 */
void
signal_pre_init(void)
{
  /* initialize user data & code selector values (used by DPMI code) */
  /* And save %fs, %gs for NPTL */
  eflags_fs_gs.fs = getsegment(fs);
  eflags_fs_gs.gs = getsegment(gs);
  eflags_fs_gs.eflags = getflags();
  dbug_printf("initial register values: fs: 0x%04x  gs: 0x%04x eflags: 0x%04lx\n",
    eflags_fs_gs.fs, eflags_fs_gs.gs, eflags_fs_gs.eflags);
#ifdef __x86_64__
  eflags_fs_gs.ds = getsegment(ds);
  eflags_fs_gs.es = getsegment(es);
  eflags_fs_gs.ss = getsegment(ss);
  /* get long fs and gs bases. If they are in the first 32 bits
     normal 386-style fs/gs switching can happen so we can ignore
     fsbase/gsbase */
  dosemu_arch_prctl(ARCH_GET_FS, &eflags_fs_gs.fsbase);
  if ((unsigned long)eflags_fs_gs.fsbase <= 0xffffffff)
    eflags_fs_gs.fsbase = 0;
  dosemu_arch_prctl(ARCH_GET_GS, &eflags_fs_gs.gsbase);
  if ((unsigned long)eflags_fs_gs.gsbase <= 0xffffffff)
    eflags_fs_gs.gsbase = 0;
  dbug_printf("initial segment bases: fs: %p  gs: %p\n",
    eflags_fs_gs.fsbase, eflags_fs_gs.gsbase);
#endif

  /* first set up the blocking mask: registersig() and newsetqsig()
   * adds to it */
  sigemptyset(&q_mask);
  registersig(SIGALRM, sigalrm);
  registersig(SIGQUIT, sigquit);
  registersig(SIGIO, sigio);
  newsetqsig(SIGINT, leavedos_signal);   /* for "graceful" shutdown for ^C too*/
  newsetqsig(SIGHUP, leavedos_signal);	/* for "graceful" shutdown */
  newsetqsig(SIGTERM, leavedos_signal);
  newsetqsig(SIGCHLD, sig_child);
  /* below ones are initialized by other subsystems */
  registersig(SIGPROF, NULL);
  registersig(SIG_ACQUIRE, NULL);
  registersig(SIG_RELEASE, NULL);
  /* mask is set up, now start using it */
  qsig_init();
  newsetsig(SIGILL, dosemu_fault);
  newsetsig(SIGFPE, dosemu_fault);
  newsetsig(SIGTRAP, dosemu_fault);
  newsetsig(SIGBUS, dosemu_fault);
  newsetsig(SIGABRT, abort_signal);
  newsetsig(SIGSEGV, dosemu_fault);

  /* block async signals so that threads inherit the blockage */
  sigprocmask(SIG_BLOCK, &q_mask, NULL);

  signal(SIGPIPE, SIG_IGN);
}

void
signal_init(void)
{
  sigstack_init();
  dosemu_tid = gettid();
  sh_tid = coopth_create("signal handling");
  /* normally we don't need ctx handlers because the thread is detached.
   * But some crazy code (vbe.c) can call coopth_attach() on it, so we
   * set up the handlers just in case. */
  coopth_set_ctx_handlers(sh_tid, sig_ctx_prepare, sig_ctx_restore);
  coopth_set_sleep_handlers(sh_tid, handle_signals_force_enter,
	handle_signals_force_leave);
  coopth_set_permanent_post_handler(sh_tid, signal_thr_post);
  coopth_set_detached(sh_tid);

  event_fd = eventfd(0, EFD_CLOEXEC);
  add_to_io_select(event_fd, async_awake, NULL);
  rng_init(&cbks, MAX_CBKS, sizeof(struct callback_s));

  /* unblock async signals in main thread */
  pthread_sigmask(SIG_UNBLOCK, &q_mask, NULL);
}

void signal_done(void)
{
    registersig(SIGALRM, NULL);
    registersig(SIGIO, NULL);
    SIGNAL_head = SIGNAL_tail;
}

static void handle_signals_force_enter(int tid)
{
  if (!in_handle_signals) {
    dosemu_error("in_handle_signals=0\n");
    return;
  }
  in_handle_signals--;
}

static void handle_signals_force_leave(int tid)
{
  in_handle_signals++;
}

int signal_pending(void)
{
  return (SIGNAL_head != SIGNAL_tail);
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
  if (in_handle_signals)
    return;

  if (signal_pending()) {
    in_handle_signals++;
    coopth_start(sh_tid, signal_thr, NULL);
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

static void SIGALRM_call(void *arg)
{
  static int first = 0;
  static hitimer_t cnt200 = 0;
  static hitimer_t cnt1000 = 0;

  if (first==0) {
    cnt200 =
    cnt1000 =
    pic_sys_time;	/* initialize */
    first = 1;
  }

  /* update mouse cursor before updating the screen */
  mouse_curtick();

  if (Video->update_screen)
    Video->update_screen();
  if (Video->handle_events)
    Video->handle_events();
  update_screen();

  /* for the SLang terminal we'll delay the release of shift, ctrl, ...
     keystrokes a bit */
  /* although actually the event handler handles the keyboard in X, keyb_client_run
   * still needs to be called in order to handle pasting.
   */
  if (!config.console_keyb)
    keyb_client_run();

  run_sound();

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

  io_select();	/* we need this in order to catch lost SIGIOs */
  /* catch user hooks here */
  if (uhook_fdin != -1) uhook_poll();

  /* here we include the hooks to possible plug-ins */
  #define VM86_RETURN_VALUE retval
  #include "plugin_poll.h"
  #undef VM86_RETURN_VALUE

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
void SIGNAL_save(void (*signal_call)(void *), void *arg, size_t len,
	const char *name)
{
  signal_queue[SIGNAL_tail].signal_handler = signal_call;
  signal_queue[SIGNAL_tail].arg_size = len;
  assert(len <= MAX_SIG_DATA_SIZE);
  if (len)
    memcpy(signal_queue[SIGNAL_tail].arg, arg, len);
  signal_queue[SIGNAL_tail].name = name;
  SIGNAL_tail = (SIGNAL_tail + 1) % MAX_SIG_QUEUE_SIZE;
  if (in_dpmi_pm())
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
static void SIGIO_call(void *arg){
  /* Call select to see if any I/O is ready on devices */
  io_select();
}

#ifdef __linux__
static void sigio(struct sigcontext *scp)
{
  /* prints non reentrant! dont do! */
#if 0
  g_printf("got SIGIO\n");
#endif
  e_gen_sigalrm(scp);
  SIGNAL_save(SIGIO_call, NULL, 0, __func__);
  if (!in_vm86)
    dpmi_sigio(scp);
}

static void sigalrm(struct sigcontext *scp)
{
  if(e_gen_sigalrm(scp)) {
    SIGNAL_save(SIGALRM_call, NULL, 0, __func__);
    if (!in_vm86)
      dpmi_sigio(scp);
  }
}

__attribute__((noinline))
static void sigasync0(int sig, struct sigcontext *scp)
{
  if (gettid() != dosemu_tid)
    dosemu_error("Signal %i from thread\n", sig);
  if (sighandlers[sig])
	  sighandlers[sig](scp);
  dpmi_iret_setup(scp);
}

__attribute__((no_instrument_function))
static void sigasync(int sig, siginfo_t *si, void *uc)
{
  struct sigcontext *scp = (struct sigcontext *)
	   &((ucontext_t *)uc)->uc_mcontext;
  init_handler(scp, 1);
  sigasync0(sig, scp);
  deinit_handler(scp);
}
#endif


static void sigquit(struct sigcontext *scp)
{
  in_vm86 = 0;

  error("sigquit called\n");
  show_ints(0, 0x33);
  show_regs(__FILE__, __LINE__);

  WRITE_BYTE(BIOS_KEYBOARD_FLAGS, 0x80);	/* ctrl-break flag */

  do_soft_int(0x1b);
}

void do_periodic_stuff(void)
{
    check_leavedos();
    handle_signals();
    coopth_run();

#ifdef USE_MHPDBG
    if (mhpdbg.active)
	mhp_debug(DBG_POLL, 0, 0);
#endif

    if (Video->change_config)
	update_xtitle();
}

void add_thread_callback(void (*cb)(void *), void *arg, const char *name)
{
  if (cb) {
    struct callback_s cbk;
    int i;
    cbk.func = cb;
    cbk.arg = arg;
    cbk.name = name;
    pthread_mutex_lock(&cbk_mtx);
    i = rng_put(&cbks, &cbk);
    g_printf("callback %s added, %i queued\n", name, rng_count(&cbks));
    pthread_mutex_unlock(&cbk_mtx);
    if (!i)
      error("callback queue overflow, %s\n", name);
  }
  eventfd_write(event_fd, 1);
  /* unfortunately eventfd does not support SIGIO :( So we kill ourself. */
  kill(0, SIGIO);
}

static void async_awake(void *arg)
{
  struct callback_s cbk;
  int i;
  eventfd_t val;
  eventfd_read(event_fd, &val);
  g_printf("processing %"PRId64" callbacks\n", val);
  do {
    pthread_mutex_lock(&cbk_mtx);
    i = rng_get(&cbks, &cbk);
    pthread_mutex_unlock(&cbk_mtx);
    if (i)
      cbk.func(cbk.arg);
  } while (i);
}

static int saved_fc;

void signal_switch_to_dosemu(void)
{
  saved_fc = fault_cnt;
  fault_cnt = 0;
}

void signal_switch_to_dpmi(void)
{
  fault_cnt = saved_fc;
}

void signal_return_to_dosemu(void)
{
  int err;
  stack_t ss = {};

#if SIGALTSTACK_WA
  m_ucontext_t hack;
  register unsigned long sp asm("sp");
  unsigned char *top = cstack + SIGSTACK_SIZE;
  unsigned char *btop = backup_stack + SIGSTACK_SIZE;
  unsigned long delta = (unsigned long)(top - sp);
  if (getmcontext(&hack) == 0)
    asm volatile(
#ifdef __x86_64__
    "mov %0, %%rsp\n"
#else
    "mov %0, %%esp\n"
#endif
     :: "r"(btop - delta) : "sp");
  else
    return;
#endif
  ss.ss_flags = SS_DISABLE;
  err = sigaltstack(&ss, NULL);
  if (err)
    perror("sigaltstack");
#if SIGALTSTACK_WA
  setmcontext(&hack);
#endif
}

void signal_return_to_dpmi(void)
{
}

void signal_set_altstack(stack_t *stk)
{
  stk->ss_sp = cstack;
  stk->ss_size = SIGSTACK_SIZE;
  stk->ss_flags = SS_ONSTACK;
}
