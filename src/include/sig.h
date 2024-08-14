#ifndef SIG_H
#define SIG_H

#include <unistd.h>
#include <signal.h>
#if defined(__linux__)
#include <sys/syscall.h>
#endif

/* reserve 1024 uncommitted pages for stack */
#define SIGSTACK_SIZE (1024 * getpagesize())

#if defined(__x86_64__) && defined(__linux__)
#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004
/* code arch_prctl() in asm as TLS addr may be invalid upon call */
static inline int dosemu_arch_prctl(int code, void *addr)
{
  int _result;

  asm volatile ("syscall\n"
		   : "=a" (_result)
		   : "0" ((unsigned long int) __NR_arch_prctl),
		     "D" ((unsigned long int) code),
		     "S" (addr)
		   : "memory", "cc", "r11", "cx");
  return _result;
}
#else
#define ARCH_SET_GS 0
#define ARCH_SET_FS 0
#define ARCH_GET_FS 0
#define ARCH_GET_GS 0
static inline int dosemu_arch_prctl(int code, void *addr)
{
  return 0;
}
#endif

extern void add_thread_callback(void (*cb)(void *), void *arg, const char *name);
extern void SIG_init(void);
extern void SIG_close(void);

/* signals for Linux's process control of consoles
   Note: on macOS, SIGRTMIN doesn't exist, but neither do Linux consoles, so
   we can just use SIGUSR1 then for SIG_THREAD_NOTIFY
 */
#ifdef SIGRTMIN
#define SIG_RELEASE     SIGUSR1
#define SIG_ACQUIRE     SIGUSR2
#define SIG_THREAD_NOTIFY (SIGRTMIN + 0)
#else
#define SIG_THREAD_NOTIFY SIGUSR1
#endif

typedef mcontext_t sigcontext_t;

#if defined(__FreeBSD__)
#ifdef __x86_64__
#define _scp_eax DWORD_(scp->mc_rax)
#define _scp_ebx DWORD_(scp->mc_rbx)
#define _scp_ecx DWORD_(scp->mc_rcx)
#define _scp_edx DWORD_(scp->mc_rdx)
#define _scp_ebp DWORD_(scp->mc_rbp)
#define _scp_esp DWORD_(scp->mc_rsp)
#define _scp_esi DWORD_(scp->mc_rsi)
#define _scp_edi DWORD_(scp->mc_rdi)
#define _scp_eip DWORD_(scp->mc_rip)
#define _scp_eflags (*(unsigned *)&scp->mc_rflags)
#define _scp_eflags_ (*(const unsigned *)&scp->mc_rflags)
#define _scp_cr2 scp->mc_addr
#define _scp_rip scp->mc_rip
#define _scp_rsp scp->mc_rsp
#define PRI_RG PRIx64
#else
#define _scp_eax scp->mc_eax
#define _scp_ebx scp->mc_ebx
#define _scp_ecx scp->mc_ecx
#define _scp_edx scp->mc_edx
#define _scp_ebp scp->mc_ebp
#define _scp_esp scp->mc_esp
#define _scp_esi scp->mc_esi
#define _scp_edi scp->mc_edi
#define _scp_eip scp->mc_eip
#define _scp_eflags scp->mc_eflags
#define _scp_eflags_ scp->mc_eflags
#define _scp_cr2 scp->mc_addr
#define PRI_RG PRIx32
#endif
#define _scp_cs (*(unsigned *)&scp->mc_cs)
#define _scp_ds (*(unsigned *)&scp->mc_ds)
#define _scp_es (*(unsigned *)&scp->mc_es)
#define _scp_ds_ (*(const unsigned *)&scp->mc_ds)
#define _scp_es_ (*(const unsigned *)&scp->mc_es)
#define _scp_fs (*(unsigned *)&scp->mc_fs)
#define _scp_gs (*(unsigned *)&scp->mc_gs)
#define _scp_ss (*(unsigned *)&scp->mc_ss)
#define _scp_trapno scp->mc_trapno
#define _scp_err (*(unsigned *)&scp->mc_err)
#define _scp_fpstate scp->mc_fpstate
#elif defined(__APPLE__)
#define _scp_eax    DWORD_((*scp)->__ss.__rax)
#define _scp_edx    DWORD_((*scp)->__ss.__rdx)
#define _scp_cs     ((*scp)->__ss.__cs)
#define _scp_eflags ((*scp)->__ss.__rflags)
#define _scp_rip    ((*scp)->__ss.__rip)
#define _scp_rsp    ((*scp)->__ss.__rsp)
#define _scp_cr2    ((*scp)->__es.__faultvaddr)
#define _scp_trapno ((*scp)->__es.__trapno)
#define _scp_err    ((*scp)->__es.__err)
#define PRI_RG PRIx64
#elif defined(__linux__)
#define _scp_fpstate (scp->fpregs)
#ifdef __x86_64__
#define _scp_es     HI_WORD(scp->gregs[REG_TRAPNO])
#define _scp_ds     (((union g_reg *)&(scp->gregs[REG_TRAPNO]))->w[2])
#define _scp_rip    (scp->gregs[REG_RIP])
#define _scp_rsp    (scp->gregs[REG_RSP])
#define _scp_edi    DWORD_(scp->gregs[REG_RDI])
#define _scp_esi    DWORD_(scp->gregs[REG_RSI])
#define _scp_ebp    DWORD_(scp->gregs[REG_RBP])
#define _scp_ebx    DWORD_(scp->gregs[REG_RBX])
#define _scp_edx    DWORD_(scp->gregs[REG_RDX])
#define _scp_ecx    DWORD_(scp->gregs[REG_RCX])
#define _scp_eax    DWORD_(scp->gregs[REG_RAX])
#define _scp_eip    DWORD_(_scp_rip)
#define _scp_esp    DWORD_(_scp_rsp)
#define _scp_err    LO_WORD(scp->gregs[REG_ERR])
#define _scp_trapno LO_WORD(scp->gregs[REG_TRAPNO])
#define _scp_cs     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[0])
#define _scp_gs     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[1])
#define _scp_fs     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[2])
#define _scp_ss     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[3])
#define _scp_eflags LO_WORD(scp->gregs[REG_EFL])
#define _scp_cr2    (scp->gregs[REG_CR2])
#define PRI_RG  "llx"
#else
#define _scp_es     (scp->gregs[REG_ES])
#define _scp_ds     (scp->gregs[REG_DS])
#define _scp_edi    (scp->gregs[REG_EDI])
#define _scp_esi    (scp->gregs[REG_ESI])
#define _scp_ebp    (scp->gregs[REG_EBP])
#define _scp_esp    (scp->gregs[REG_ESP])
#define _scp_ebx    (scp->gregs[REG_EBX])
#define _scp_edx    (scp->gregs[REG_EDX])
#define _scp_ecx    (scp->gregs[REG_ECX])
#define _scp_eax    (scp->gregs[REG_EAX])
#define _scp_eip    (scp->gregs[REG_EIP])
#define _scp_cs     (scp->gregs[REG_CS])
#define _scp_gs     (scp->gregs[REG_GS])
#define _scp_fs     (scp->gregs[REG_FS])
#define _scp_ss     (scp->gregs[REG_SS])
#define _scp_err    (scp->gregs[REG_ERR])
#define _scp_trapno (scp->gregs[REG_TRAPNO])
#define _scp_eflags (scp->gregs[REG_EFL])
#define _scp_cr2    (((union dword *)&scp->cr2)->d)
#define PRI_RG PRIx32
/* some compat */
#define _scp_rip _scp_eip
#define _scp_rsp _scp_esp
#endif
#endif  // __linux__

extern void SIGNAL_save( void (*signal_call)(void *), void *arg, size_t size,
	const char *name );
extern void handle_signals(void);
extern void do_periodic_stuff(void);
extern void sig_ctx_prepare(int tid, void *arg, void *arg2);
extern void sig_ctx_restore(int tid, void *arg, void *arg2);

extern int sigchld_register_handler(pid_t pid, void (*handler)(void*),
	void *arg);
extern int sigchld_enable_cleanup(pid_t pid);
extern int sigchld_enable_handler(pid_t pid, int on);
extern int sigalrm_register_handler(void (*handler)(void));
extern void registersig(int sig, void (*handler)(siginfo_t *));
extern void registersig_std(int sig, void (*handler)(void *));
extern void deinit_handler(sigcontext_t *scp, unsigned long *uc_flags);

void signal_block_async_nosig(sigset_t *old_mask);
void signal_unblock_fatal_sigs(void);
void signal_unblock_async_sigs(void);
void signal_restore_async_sigs(void);
void leavedos_sig(int sig);

extern pthread_t dosemu_pthread_self;
extern pid_t dosemu_pid;
extern sigset_t q_mask;
extern sigset_t nonfatal_q_mask;
extern sigset_t all_sigmask;
extern int sig_threads_wa;

#ifdef DNATIVE
void signal_switch_to_dosemu(void);
void signal_switch_to_dpmi(void);
void signal_return_to_dosemu(void);
void signal_return_to_dpmi(void);
void signal_set_altstack(int on);
void signative_init(void);
void signative_pre_init(void);
void signative_sigbreak(void *uc);
void signative_start(void);
void signative_stop(void);
void unsetsig(int sig);
#else
static inline void signal_switch_to_dosemu(void) {}
static inline void signal_switch_to_dpmi(void) {}
static inline void signal_return_to_dosemu(void) {}
static inline void signal_return_to_dpmi(void) {}
static inline void signal_set_altstack(int on) {}
static inline void signative_init(void) {}
static inline void signative_pre_init(void) {}
static inline void signative_sigbreak(void *uc) {}
static inline void signative_start(void) {}
static inline void signative_stop(void) {}
static inline void unsetsig(int sig) {}
#endif

/* On glibc SIGRTMAX is not a constant but NSIG covers rt signals.
 * On bsd its all the other way around. */
#if defined(__GLIBC__) || !defined(SIGRTMAX)
#define SIGMAX NSIG
#elif defined(__FreeBSD__)
#define SIGMAX SIGRTMAX
#else
#define SIGMAX 64
#endif

#endif
