#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <assert.h>
#ifdef __linux__
#include <linux/version.h>
#endif

#include "emu.h"
#ifdef __linux__
#include "sys_vm86.h"
#endif
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
#include "ringbuf.h"
#include "dosemu_config.h"
#include "sound.h"
#include "cpu-emu.h"
#include "sig.h"

#define SIGALTSTACK_WA_DEFAULT 1
#if SIGALTSTACK_WA_DEFAULT
  #ifdef DISABLE_SYSTEM_WA
    #ifdef SS_AUTODISARM
      #define SIGALTSTACK_WA 0
    #else
      #ifdef WARN_UNDISABLED_WA
        #warning Not disabling SIGALTSTACK_WA, update your kernel
      #endif
      #define SIGALTSTACK_WA 1
    #endif
  #else
    /* work-around sigaltstack badness - disable when kernel is fixed */
    #define SIGALTSTACK_WA 1
  #endif
  #if defined(WARN_OUTDATED_WA) && defined(SS_AUTODISARM)
    #warning SIGALTSTACK_WA is outdated
  #endif
#else
  #define SIGALTSTACK_WA 0
#endif
#if SIGALTSTACK_WA
#include "mcontext.h"
#include "mapping.h"
#endif
/* SS_AUTODISARM is a dosemu-specific sigaltstack extension supported
 * by some kernels */
#ifndef SS_AUTODISARM
#define SS_AUTODISARM  (1U << 31)    /* disable sas during sighandling */
#endif

#ifdef __x86_64__
  #define SIGRETURN_WA_DEFAULT 1
#else
  #define SIGRETURN_WA_DEFAULT 0
#endif
#if SIGRETURN_WA_DEFAULT
  #ifdef DISABLE_SYSTEM_WA
    #ifdef UC_SIGCONTEXT_SS
      #define SIGRETURN_WA 0
    #else
      #ifdef WARN_UNDISABLED_WA
        #warning Not disabling SIGRETURN_WA, update your kernel
      #endif
      #define SIGRETURN_WA 1
    #endif
  #else
    /* work-around sigreturn badness - disable when kernel is fixed */
    #define SIGRETURN_WA 1
  #endif
  #if defined(WARN_OUTDATED_WA) && defined(UC_SIGCONTEXT_SS)
    #warning SIGRETURN_WA is outdated
  #endif
#else
  #define SIGRETURN_WA 0
#endif

#ifdef __x86_64__
#ifndef UC_SIGCONTEXT_SS
/*
 * UC_SIGCONTEXT_SS will be set when delivering 64-bit or x32 signals on
 * kernels that save SS in the sigcontext.  Kernels that set UC_SIGCONTEXT_SS
 * allow signal handlers to set UC_STRICT_RESTORE_SS;
 * if UC_STRICT_RESTORE_SS is set, then sigreturn will restore SS.
 *
 * For compatibility with old programs, the kernel will *not* set
 * UC_STRICT_RESTORE_SS when delivering signals from 32bit code.
 */
#define UC_SIGCONTEXT_SS       0x2
#define UC_STRICT_RESTORE_SS   0x4
#endif
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

#define MAX_SIGALRM_HANDLERS 50
struct sigalrm_hndl {
  void (*handler)(void);
};
static struct sigalrm_hndl alrm_hndl[MAX_SIGALRM_HANDLERS];
static int alrm_hndl_num;

static sigset_t q_mask;
static sigset_t nonfatal_q_mask;
static sigset_t fatal_q_mask;
static void *cstack;
#if SIGALTSTACK_WA
static void *backup_stack;
static int need_sas_wa;
#endif
#if SIGRETURN_WA
static int need_sr_wa;
#endif
static int block_all_sigs;

static int sh_tid;
static int in_handle_signals;
static void handle_signals_force_enter(int tid, int sl_state);
static void handle_signals_force_leave(int tid);
static void async_call(void *arg);
static void process_callbacks(void);
static struct rng_s cbks;
#define MAX_CBKS 1000
static pthread_mutex_t cbk_mtx = PTHREAD_MUTEX_INITIALIZER;

struct eflags_fs_gs eflags_fs_gs;

static void (*sighandlers[NSIG])(sigcontext_t *, siginfo_t *);
static void (*qsighandlers[NSIG])(int sig, siginfo_t *si, void *uc);
static void (*asighandlers[NSIG])(void *arg);
static struct sigaction sacts[NSIG];

static void SIGALRM_call(void *arg);
static void SIGIO_call(void *arg);
static void sigasync(int sig, siginfo_t *si, void *uc);
static void sigasync_std(int sig, siginfo_t *si, void *uc);
static void leavedos_sig(int sig);

static void _newsetqsig(int sig, void (*fun)(int sig, siginfo_t *si, void *uc))
{
	if (qsighandlers[sig])
		return;
	/* collect this mask so that all async (fatal and non-fatal)
	 * signals can be blocked by threads */
	sigaddset(&q_mask, sig);
	qsighandlers[sig] = fun;
}

static void newsetqsig(int sig, void (*fun)(int sig, siginfo_t *si, void *uc))
{
#if SIGRETURN_WA
	sigaddset(&fatal_q_mask, sig);
#endif
	_newsetqsig(sig, fun);
}

static void qsig_init(void)
{
	struct sigaction sa;
	int i;

	sa.sa_flags = SA_RESTART | SA_ONSTACK | SA_SIGINFO;
	if (block_all_sigs)
	{
		/* initially block all async signals. */
		sa.sa_mask = q_mask;
	}
	else
	{
		/* block all non-fatal async signals */
		sa.sa_mask = nonfatal_q_mask;
	}
	for (i = 0; i < NSIG; i++) {
		if (qsighandlers[i]) {
			sa.sa_sigaction = qsighandlers[i];
			sigaction(i, &sa, NULL);
		}
	}
}

/* registers non-emergency async signals */
void registersig(int sig, void (*fun)(sigcontext_t *, siginfo_t *))
{
	/* first need to collect the mask, then register all handlers
	 * because the same mask of non-emergency async signals
	 * is used for every handler */
	sigaddset(&nonfatal_q_mask, sig);
	_newsetqsig(sig, sigasync);
	sighandlers[sig] = fun;
}

void registersig_std(int sig, void (*fun)(void *))
{
	sigaddset(&nonfatal_q_mask, sig);
	_newsetqsig(sig, sigasync_std);
	asighandlers[sig] = fun;
}

static void newsetsig(int sig, void (*fun)(int sig, siginfo_t *si, void *uc))
{
	struct sigaction sa;

	sa.sa_flags = SA_RESTART | SA_ONSTACK | SA_SIGINFO;
#ifdef __linux__
	if (kernel_version_code >= KERNEL_VERSION(2, 6, 14))
#endif
		sa.sa_flags |= SA_NODEFER;
	if (block_all_sigs)
	{
		/* initially block all async signals. */
		sa.sa_mask = q_mask;
	}
	else
	{
		/* block all non-fatal async signals */
		sa.sa_mask = nonfatal_q_mask;
	}
	sa.sa_sigaction = fun;
	sigaction(sig, &sa, NULL);
}

SIG_PROTO_PFX
static void fixup_handler(int sig, siginfo_t *si, void *uc)
{
	struct sigaction *sa;
	ucontext_t *uct = uc;
	sigcontext_t *scp = &uct->uc_mcontext;
	init_handler(scp, 1);
	sa = &sacts[sig];
	if (sa->sa_flags & SA_SIGINFO) {
		sa->sa_sigaction(sig, si, uc);
	} else {
		typedef void (*hdlr_t)(int, siginfo_t *, ucontext_t *);
		hdlr_t hdlr = (hdlr_t)sa->sa_handler;
		hdlr(sig, si, uc);
	}
	deinit_handler(scp, &uct->uc_flags);
}

static void fixupsig(int sig)
{
	struct sigaction sa;
	sigaction(sig, NULL, &sa);
	if (sa.sa_handler == SIG_DFL || sa.sa_handler == SIG_IGN)
		return;
	sacts[sig] = sa;
	sa.sa_flags |= SA_ONSTACK | SA_SIGINFO;
	sa.sa_sigaction = fixup_handler;
	sigaction(sig, &sa, NULL);
}

/* init_handler puts the handler in a sane state that glibc
   expects. That means restoring fs and gs for vm86 (necessary for
   2.4 kernels) and fs, gs and eflags for DPMI. */
SIG_PROTO_PFX
static void __init_handler(sigcontext_t *scp, unsigned long uc_flags)
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
  _ds = getsegment(ds);
  _es = getsegment(es);
  if (!(uc_flags & UC_SIGCONTEXT_SS))
    _ss = getsegment(ss);
  _fs = getsegment(fs);
  _gs = getsegment(gs);
  if (config.cpu_vm_dpmi == CPUVM_NATIVE && _cs == 0) {
      if (config.dpmi
#ifdef X86_EMULATOR
	    && config.cpuemu < 4
#endif
	 ) {
	fprintf(stderr, "Cannot run DPMI code natively ");
#ifdef __linux__
	if (kernel_version_code < KERNEL_VERSION(2, 6, 15))
	  fprintf(stderr, "because your Linux kernel is older than version 2.6.15.\n");
	else
	  fprintf(stderr, "for unknown reasons.\nPlease contact linux-msdos@vger.kernel.org.\n");
#endif
#ifdef X86_EMULATOR
	fprintf(stderr, "Set $_cpu_emu=\"full\" or \"fullsim\" to avoid this message.\n");
#endif
      }
#ifdef X86_EMULATOR
      config.cpu_vm = CPUVM_EMU;
      config.cpuemu = 4;
      _cs = getsegment(cs);
#else
      leavedos_sig(45);
#endif
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

#if SIGRETURN_WA
  if (need_sr_wa && !DPMIValidSelector(_cs))
    dpmi_iret_unwind(scp);
#endif

#if 0
  /* for async signals need to restore fs/gs even if dosemu code
   * was interrupted, because it can be interrupted in a switching
   * routine when fs or gs are already switched but cs is not */
  if (!DPMIValidSelector(_cs) && !async)
    return;
#else
  /* as DIRECT_DPMI_SWITCH support is now removed, the above comment
   * applies only to DPMI_iret, which is now unwound.
   * We don't need to restore segregs for async signals any more. */
  if (!DPMIValidSelector(_cs))
    return;
#endif

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
   * So if we set selector to 0, need to use also prctl(ARCH_SET_xS).
   * Also, if the bases are not used they are 0 so no need to restore,
   * which saves a syscall */
  if (!eflags_fs_gs.fs && eflags_fs_gs.fsbase)
    dosemu_arch_prctl(ARCH_SET_FS, eflags_fs_gs.fsbase);
  if (!eflags_fs_gs.gs && eflags_fs_gs.gsbase)
    dosemu_arch_prctl(ARCH_SET_GS, eflags_fs_gs.gsbase);
#endif
}

SIG_PROTO_PFX
void init_handler(sigcontext_t *scp, unsigned long uc_flags)
{
  /* Async signals are initially blocked.
   * If we don't block them, nested sighandler will clobber SS
   * before we manage to save it.
   * Even if the nested sighandler tries hard, it can't properly
   * restore SS, at least until the proper sigreturn() support is in.
   * For kernels that have the proper SS support, only nonfatal
   * async signals are initially blocked. They need to be blocked
   * because of sas wa and because they should not interrupt
   * deinit_handler() after it changed %fs. In this case, however,
   * we can block them later at the right places, but this will
   * cost a syscall per every signal.
   * Note: in 64bit mode some segment registers are neither saved nor
   * restored by the signal dispatching code in kernel, so we have
   * to restore them by hands.
   * Note: most async signals are left blocked, we unblock only few.
   * Sync signals like SIGSEGV are never blocked.
   */
  __init_handler(scp, uc_flags);
  if (!block_all_sigs)
    return;
#if SIGALTSTACK_WA
  /* for SAS WA we unblock the fatal signals even later if we came
   * from DPMI, as then we'll be switching stacks which is racy when
   * async signals enabled. */
  if (need_sas_wa && DPMIValidSelector(_cs))
    return;
#endif
  /* either came from dosemu/vm86 or having SS_AUTODISARM -
   * then we can unblock any signals we want. For now leave nonfatal
   * signals blocked as they are rarely needed inside sighandlers
   * (needed only for instremu, see
   * https://github.com/stsp/dosemu2/issues/477
   * ) */
  sigprocmask(SIG_UNBLOCK, &fatal_q_mask, NULL);
}

SIG_PROTO_PFX
void deinit_handler(sigcontext_t *scp, unsigned long *uc_flags)
{
#ifdef X86_EMULATOR
  /* in fullsim mode nothing to do */
  if (CONFIG_CPUSIM && config.cpuemu >= 4)
    return;
#endif

  if (!DPMIValidSelector(_cs))
    return;

#ifdef __x86_64__
  if (*uc_flags & UC_SIGCONTEXT_SS) {
    /*
     * On Linux 4.4 (possibly) and up, the kernel can fully restore
     * SS and ESP, so we don't need any special tricks.  To avoid confusion,
     * force strict restore.  (Some 4.1 versions support this as well but
     * without the uc_flags bits.  It's not trying to detect those kernels.)
     */
    *uc_flags |= UC_STRICT_RESTORE_SS;
  } else {
#if SIGRETURN_WA
    if (!need_sr_wa) {
      need_sr_wa = 1;
      warn("Enabling sigreturn() work-around\n");
    }
    dpmi_iret_setup(scp);
#else
    error("Your kernel does not support UC_STRICT_RESTORE_SS and the "
	  "work-around in dosemu is not enabled.\n");
    leavedos_sig(11);
#endif
  }

  if (_fs != getsegment(fs))
    loadregister(fs, _fs);
  if (_gs != getsegment(gs))
    loadregister(gs, _gs);

  loadregister(ds, _ds);
  loadregister(es, _es);
#endif
}

static void leavedos_call(void *arg)
{
  int *sig = arg;
  _leavedos_sig(*sig);
}

int sigchld_register_handler(pid_t pid, void (*handler)(void))
{
  int i;
  for (i = 0; i < chd_hndl_num; i++) {
    if (!chld_hndl[i].pid)
      break;
    /* make sure not yet registered */
    assert(chld_hndl[i].pid != pid);
  }
  if (i == chd_hndl_num) {
    chd_hndl_num++;
    assert(chd_hndl_num <= MAX_SIGCHLD_HANDLERS);
  }
  chld_hndl[i].handler = handler;
  chld_hndl[i].pid = pid;
  chld_hndl[i].enabled = 1;
  return 0;
}

int sigchld_enable_cleanup(pid_t pid)
{
  return sigchld_register_handler(pid, NULL);
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
  /* SIGCHLD handler is one-shot, disarm */
  chld_hndl[i].pid = 0;
  if (chld_hndl[i].handler)
    chld_hndl[i].handler();
}

/* this cleaning up is necessary to avoid the port server becoming
   a zombie process */
static void sig_child(sigcontext_t *scp, siginfo_t *si)
{
  SIGNAL_save(cleanup_child, &si->si_pid, sizeof(si->si_pid), __func__);
}

int sigalrm_register_handler(void (*handler)(void))
{
  assert(alrm_hndl_num < MAX_SIGALRM_HANDLERS);
  alrm_hndl[alrm_hndl_num].handler = handler;
  alrm_hndl_num++;
  return 0;
}

void leavedos_from_sig(int sig)
{
  /* anything more sophisticated? */
  __leavedos_main(0, sig);
}

static void leavedos_sig(int sig)
{
  dbug_printf("Terminating on signal %i\n", sig);
  SIGNAL_save(leavedos_call, &sig, sizeof(sig), __func__);
  /* abort current sighandlers */
  if (in_handle_signals) {
    g_printf("Interrupting active signal handlers\n");
    in_handle_signals = 0;
  }
}

/* noinline is needed to prevent gcc from caching tls vars before
 * calling to init_handler() */
__attribute__((noinline))
static void _leavedos_signal(int sig, sigcontext_t *scp)
{
  leavedos_sig(sig);
  if (!in_vm86)
    dpmi_sigio(scp);
}

SIG_PROTO_PFX
static void leavedos_signal(int sig, siginfo_t *si, void *uc)
{
  ucontext_t *uct = uc;
  sigcontext_t *scp = &uct->uc_mcontext;
  init_handler(scp, uct->uc_flags);
  signal(sig, SIG_DFL);
  _leavedos_signal(sig, scp);
  deinit_handler(scp, &uct->uc_flags);
}

#if 0
/* calling leavedos directly from sighandler may be unsafe - disable */
SIG_PROTO_PFX
static void leavedos_emerg(int sig, siginfo_t *si, void *uc)
{
  ucontext_t *uct = uc;
  sigcontext_t *scp = &uct->uc_mcontext;
  init_handler(scp, uct->uc_flags);
  leavedos_from_sig(sig);
  deinit_handler(scp, &uct->uc_flags);
}
#endif

SIG_PROTO_PFX
static void abort_signal(int sig, siginfo_t *si, void *uc)
{
  ucontext_t *uct = uc;
  sigcontext_t *scp = &uct->uc_mcontext;
  init_handler(scp, uct->uc_flags);
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

  /* sigaltstack_wa is optional. See if we need it. */
  /* .ss_flags is signed int and SS_AUTODISARM is a sign bit :( */
  stack_t dummy = { .ss_flags = (int)(SS_DISABLE | SS_AUTODISARM) };
  int err = dosemu_sigaltstack(&dummy, NULL);
  int errno_save = errno;
#if SIGALTSTACK_WA
  if ((err && errno_save == EINVAL)
#ifdef __i386__
#ifdef __linux__
      /* kernels before 4.11 had the needed functionality only for 64bits */
      || kernel_version_code < KERNEL_VERSION(4, 11, 0)
#endif
#endif
     )
  {
    need_sas_wa = 1;
    warn("Enabling sigaltstack() work-around\n");
    /* for SAS WA block all signals. If we dont, there is a
     * race that the signal can come after we switched to backup stack
     * but before we disabled sigaltstack. We unblock the fatal signals
     * later, only right before switching back to dosemu. */
    block_all_sigs = 1;
  } else if (err) {
    goto unk_err;
  }

  if (need_sas_wa) {
    cstack = alloc_mapping(MAPPING_SHARED, SIGSTACK_SIZE);
    if (cstack == MAP_FAILED) {
      error("Unable to allocate stack\n");
      config.exitearly = 1;
      return;
    }
    backup_stack = alias_mapping_high(MAPPING_OTHER, SIGSTACK_SIZE,
	PROT_READ | PROT_WRITE, cstack);
    if (backup_stack == MAP_FAILED) {
      error("Unable to allocate stack\n");
      config.exitearly = 1;
      return;
    }
  } else {
    cstack = mmap(NULL, SIGSTACK_SIZE, PROT_READ | PROT_WRITE,
	MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (cstack == MAP_FAILED) {
      error("Unable to allocate stack\n");
      config.exitearly = 1;
      return;
    }
  }
#else
  if ((err && errno_save == EINVAL)
#ifdef __i386__
#ifdef __linux__
      || kernel_version_code < KERNEL_VERSION(4, 11, 0)
#endif
#endif
     )
  {
    error("Your kernel does not support SS_AUTODISARM and the "
	  "work-around in dosemu is not enabled.\n");
    config.exitearly = 1;
    return;
  } else if (err) {
    goto unk_err;
  }
  cstack = mmap(NULL, SIGSTACK_SIZE, PROT_READ | PROT_WRITE,
	MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
  if (cstack == MAP_FAILED) {
    error("Unable to allocate stack\n");
    config.exitearly = 1;
    return;
  }
#endif

  return;

unk_err:
  if (err) {
    error("sigaltstack() returned %i, %s\n", errno_save,
        strerror(errno_save));
    config.exitearly = 1;
  }
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
  if (((unsigned long)eflags_fs_gs.fsbase <= 0xffffffff) && eflags_fs_gs.fs)
    eflags_fs_gs.fsbase = 0;
  dosemu_arch_prctl(ARCH_GET_GS, &eflags_fs_gs.gsbase);
  if (((unsigned long)eflags_fs_gs.gsbase <= 0xffffffff) && eflags_fs_gs.gs)
    eflags_fs_gs.gsbase = 0;
  dbug_printf("initial segment bases: fs: %p  gs: %p\n",
    eflags_fs_gs.fsbase, eflags_fs_gs.gsbase);
#endif

  /* first set up the blocking mask: registersig() and newsetqsig()
   * adds to it */
  sigemptyset(&q_mask);
  sigemptyset(&nonfatal_q_mask);
  registersig_std(SIGALRM, SIGALRM_call);
  registersig_std(SIGIO, SIGIO_call);
  registersig_std(SIG_THREAD_NOTIFY, async_call);
  registersig(SIGCHLD, sig_child);
  newsetqsig(SIGQUIT, leavedos_signal);
  newsetqsig(SIGINT, leavedos_signal);   /* for "graceful" shutdown for ^C too*/
//  newsetqsig(SIGHUP, leavedos_emerg);
//  newsetqsig(SIGTERM, leavedos_emerg);
  /* below ones are initialized by other subsystems */
#ifdef X86_EMULATOR
  registersig(SIGVTALRM, NULL);
#endif
  registersig(SIG_ACQUIRE, NULL);
  registersig(SIG_RELEASE, NULL);
  registersig_std(SIGWINCH, NULL);
  fixupsig(SIGPROF);
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
  prctl(PR_SET_PDEATHSIG, SIGQUIT);
  dosemu_pthread_self = pthread_self();
  rng_init(&cbks, MAX_CBKS, sizeof(struct callback_s));
}

void
signal_init(void)
{
  /* signal_init is called after dpmi_setup so this check is safe */
  if (config.cpu_vm_dpmi == CPUVM_NATIVE) {
    sigstack_init();
#if SIGRETURN_WA
    /* 4.6+ are able to correctly restore SS */
#ifdef __linux__
    if (kernel_version_code < KERNEL_VERSION(4, 6, 0)) {
      need_sr_wa = 1;
      warn("Enabling sigreturn() work-around for old kernel\n");
      /* block all sigs for SR WA. If we dont, the signal can come before
       * SS is saved, but we can't restore SS on signal exit. */
      block_all_sigs = 1;
    }
#endif
#endif
  }

  sh_tid = coopth_create("signal handling");
  /* normally we don't need ctx handlers because the thread is detached.
   * But some crazy code (vbe.c) can call coopth_attach() on it, so we
   * set up the handlers just in case. */
  coopth_set_ctx_handlers(sh_tid, sig_ctx_prepare, sig_ctx_restore);
  coopth_set_sleep_handlers(sh_tid, handle_signals_force_enter,
	handle_signals_force_leave);
  coopth_set_permanent_post_handler(sh_tid, signal_thr_post);
  coopth_set_detached(sh_tid);

  /* unblock async signals in main thread */
  pthread_sigmask(SIG_UNBLOCK, &q_mask, NULL);
}

void signal_done(void)
{
    struct itimerval itv;
    int i;

    itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
    itv.it_value = itv.it_interval;
    if (setitimer(ITIMER_REAL, &itv, NULL) == -1)
	g_printf("can't turn off timer at shutdown: %s\n", strerror(errno));
    if (setitimer(ITIMER_VIRTUAL, &itv, NULL) == -1)
	g_printf("can't turn off vtimer at shutdown: %s\n", strerror(errno));
    registersig(SIGALRM, NULL);
    registersig(SIGIO, NULL);
    registersig(SIGCHLD, NULL);
    registersig(SIG_THREAD_NOTIFY, NULL);
    for (i = 0; i < NSIG; i++) {
	if (sigismember(&q_mask, i))
	    signal(i, SIG_DFL);
    }
    SIGNAL_head = SIGNAL_tail;
}

static void handle_signals_force_enter(int tid, int sl_state)
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
void handle_signals(void)
{
  while (signal_pending() && !in_handle_signals) {
    in_handle_signals++;
    coopth_start(sh_tid, signal_thr, NULL);
    coopth_run_tid(sh_tid);
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
  int i;

  if (first==0) {
    cnt200 =
    cnt1000 =
    pic_sys_time;	/* initialize */
    first = 1;
  }

  process_callbacks();

  if (video_initialized && !config.vga)
    update_screen();

  for (i = 0; i < alrm_hndl_num; i++)
    alrm_hndl[i].handler();

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
__attribute__((noinline))
static void sigasync0(int sig, sigcontext_t *scp, siginfo_t *si)
{
  pthread_t tid = pthread_self();
  if (!pthread_equal(tid, dosemu_pthread_self)) {
#if defined(HAVE_PTHREAD_GETNAME_NP) && defined(__GLIBC__)
    char name[128];
    pthread_getname_np(tid, name, sizeof(name));
    dosemu_error("Async signal %i from thread %s\n", sig, name);
#else
    dosemu_error("Async signal %i from thread %i\n", sig, tid);
#endif
  }
  if (sighandlers[sig])
	  sighandlers[sig](scp, si);
}

__attribute__((noinline))
static void sigasync0_std(int sig, sigcontext_t *scp, siginfo_t *si)
{
  pthread_t tid = pthread_self();
  if (!pthread_equal(tid, dosemu_pthread_self)) {
#if defined(HAVE_PTHREAD_GETNAME_NP) && defined(__GLIBC__)
    char name[128];
    pthread_getname_np(tid, name, sizeof(name));
    dosemu_error("Async signal %i from thread %s\n", sig, name);
#else
    dosemu_error("Async signal %i from thread %i\n", sig, tid);
#endif
  }

  if (!asighandlers[sig]) {
    error("handler for sig %i not registered\n", sig);
    return;
  }
  e_gen_sigalrm(scp);
  SIGNAL_save(asighandlers[sig], NULL, 0, __func__);
  if (!in_vm86)
    dpmi_sigio(scp);
}

SIG_PROTO_PFX
static void sigasync(int sig, siginfo_t *si, void *uc)
{
  ucontext_t *uct = uc;
  sigcontext_t *scp = &uct->uc_mcontext;
  init_handler(scp, uct->uc_flags);
  sigasync0(sig, scp, si);
  deinit_handler(scp, &uct->uc_flags);
}

SIG_PROTO_PFX
static void sigasync_std(int sig, siginfo_t *si, void *uc)
{
  ucontext_t *uct = uc;
  sigcontext_t *scp = &uct->uc_mcontext;
  init_handler(scp, uct->uc_flags);
  sigasync0_std(sig, scp, si);
  deinit_handler(scp, &uct->uc_flags);
}
#endif


void do_periodic_stuff(void)
{
    check_leavedos();
    handle_signals();
#ifdef USE_MHPDBG
    /* mhp_debug() must be called exactly after handle_signals()
     * and before coopth_run(). handle_signals() feeds input to
     * debugger, and coopth_run() runs DPMI (oops!). We need to
     * run debugger before DPMI to not lose single-steps. */
    if (mhpdbg.active)
	mhp_debug(DBG_POLL, 0, 0);
#endif
    coopth_run();

    if (video_initialized && Video && Video->change_config)
	update_xtitle();
}

void add_thread_callback(void (*cb)(void *), void *arg, const char *name)
{
  union sigval value;
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
  value.sival_int = 1;
  pthread_sigqueue(dosemu_pthread_self, SIG_THREAD_NOTIFY, value);
}

static void process_callbacks(void)
{
  struct callback_s cbk;
  int i;
  g_printf("processing async callbacks\n");
  do {
    pthread_mutex_lock(&cbk_mtx);
    i = rng_get(&cbks, &cbk);
    pthread_mutex_unlock(&cbk_mtx);
    if (i)
      cbk.func(cbk.arg);
  } while (i);
}

static void async_call(void *arg)
{
  process_callbacks();
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

#if SIGALTSTACK_WA
static void signal_sas_wa(void)
{
  int err;
  stack_t ss = {};
  m_ucontext_t hack;
  unsigned char *sp;
  unsigned char *top = cstack + SIGSTACK_SIZE;
  unsigned char *btop = backup_stack + SIGSTACK_SIZE;
  ptrdiff_t delta;

  if (getmcontext(&hack) == 0) {
    sp = alloca(sizeof(void *));
    delta = top - sp;
    /* switch stack to its mirror (same phys addr) to cheat sigaltstack() */
    asm volatile(
#ifdef __x86_64__
    "mov %0, %%rsp\n"
#else
    "mov %0, %%esp\n"
#endif
     :: "r"(btop - delta));
  } else {
    sigprocmask(SIG_UNBLOCK, &fatal_q_mask, NULL);
    return;
  }

  ss.ss_flags = SS_DISABLE;
  /* sas will re-enable itself when returning from sighandler */
  err = dosemu_sigaltstack(&ss, NULL);
  if (err)
    perror("sigaltstack");

  setmcontext(&hack);
}
#endif

void signal_return_to_dosemu(void)
{
#if SIGALTSTACK_WA
  if (need_sas_wa)
    signal_sas_wa();
#endif
}

void signal_return_to_dpmi(void)
{
}

void signal_set_altstack(int on)
{
  stack_t stk = { 0 };
  int err;

  if (!on) {
    stk.ss_flags = SS_DISABLE;
    err = sigaltstack(&stk, NULL);
  } else {
    stk.ss_sp = cstack;
    stk.ss_size = SIGSTACK_SIZE;
#if SIGALTSTACK_WA
    stk.ss_flags = SS_ONSTACK | (need_sas_wa ? 0 : SS_AUTODISARM);
#else
    stk.ss_flags = SS_ONSTACK | SS_AUTODISARM;
#endif
    err = sigaltstack(&stk, NULL);
    if (err && errno == EINVAL) {
      /* work-around for musl that doesn't accept extension flags */
      err = dosemu_sigaltstack(&stk, NULL);
    }
  }
  if (err) {
    error("sigaltstack(0x%x) returned %i, %s\n",
        stk.ss_flags, err, strerror(errno));
    leavedos(err);
  }
}

void signal_unblock_async_sigs(void)
{
  /* unblock only nonfatal, fatals should already be unblocked */
  sigprocmask(SIG_UNBLOCK, &nonfatal_q_mask, NULL);
}

void signal_restore_async_sigs(void)
{
  /* block sigs even for !sas_wa because if deinit_handler is
   * interrupted after changing %fs, we are in troubles */
  sigprocmask(SIG_BLOCK, &nonfatal_q_mask, NULL);
}
