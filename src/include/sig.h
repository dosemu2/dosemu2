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

/* signals for Linux's process control of consoles */
#define SIG_RELEASE     SIGUSR1
#define SIG_ACQUIRE     SIGUSR2
#define SIG_THREAD_NOTIFY (SIGRTMIN + 0)

typedef struct sigcontext sigcontext_t;

#define _scp_gs     (scp->gs)
#define _scp_fs     (scp->fs)
#ifdef __x86_64__
#define _scp_es     HI_WORD(scp->trapno)
#define _scp_ds     (((union g_reg *)&(scp->trapno))->w[2])
#define _scp_rdi    (scp->rdi)
#define _scp_rsi    (scp->rsi)
#define _scp_rbp    (scp->rbp)
#define _scp_rsp    (scp->rsp)
#define _scp_rbx    (scp->rbx)
#define _scp_rdx    (scp->rdx)
#define _scp_rcx    (scp->rcx)
#define _scp_rax    (scp->rax)
#define _scp_rip    (scp->rip)
#define _scp_ss     (scp->__pad0)
#define _scp_edi    DWORD_(_scp_rdi)
#define _scp_esi    DWORD_(_scp_rsi)
#define _scp_ebp    DWORD_(_scp_rbp)
#define _scp_esp    DWORD_(_scp_rsp)
#define _scp_ebx    DWORD_(_scp_rbx)
#define _scp_edx    DWORD_(_scp_rdx)
#define _scp_ecx    DWORD_(_scp_rcx)
#define _scp_eax    DWORD_(_scp_rax)
#define _scp_eip    DWORD_(_scp_rip)
#define _scp_eax    DWORD_(_scp_rax)
#define _scp_eip    DWORD_(_scp_rip)
#define _scp_err    LO_WORD(scp->err)
#define _scp_lerr   (unsigned long)LO_WORD(scp->err)
#define _scp_trapno LO_WORD(scp->trapno)
#define _scp_ltrapno (unsigned long)LO_WORD(scp->trapno)
#define _scp_cs     LO_WORD(scp->cs)
#define _scp_eflags LO_WORD(scp->eflags)
#define _scp_lflags (unsigned long)LO_WORD(scp->eflags)
#define _scp_cr2    (scp->cr2)
#define _scp_fpstate (scp->fpstate)
#else
#define _scp_es     (scp->es)
#define _scp_ds     (scp->ds)
#define _scp_edi    (scp->edi)
#define _scp_esi    (scp->esi)
#define _scp_ebp    (scp->ebp)
#define _scp_esp    (scp->esp)
#define _scp_ebx    (scp->ebx)
#define _scp_edx    (scp->edx)
#define _scp_ecx    (scp->ecx)
#define _scp_eax    (scp->eax)
#define _scp_eip    (scp->eip)
#define _scp_ss     (scp->ss)
#define _scp_err    (scp->err)
#define _scp_lerr   (scp->err)
#define _scp_trapno (scp->trapno)
#define _scp_ltrapno (scp->trapno)
#define _scp_cs     (scp->cs)
#define _scp_eflags (scp->eflags)
#define _scp_lflags (scp->eflags)
#define _scp_cr2    (scp->cr2)
#define _scp_fpstate (scp->fpstate)
/* some compat */
#define _scp_rip _scp_eip
#define _scp_rsp _scp_esp
#define _scp_rax _scp_eax
#endif

extern void SIGNAL_save( void (*signal_call)(void *), void *arg, size_t size,
	const char *name );
extern void handle_signals(void);
extern void do_periodic_stuff(void);
extern void sig_ctx_prepare(int tid, void *arg, void *arg2);
extern void sig_ctx_restore(int tid, void *arg, void *arg2);

extern int sigchld_register_handler(pid_t pid, void (*handler)(void));
extern int sigchld_enable_cleanup(pid_t pid);
extern int sigchld_enable_handler(pid_t pid, int on);
extern int sigalrm_register_handler(void (*handler)(void));
extern void registersig(int sig, void (*handler)(sigcontext_t *,
	siginfo_t *));
extern void registersig_std(int sig, void (*handler)(void *));
extern void init_handler(sigcontext_t *scp, unsigned long uc_flags);
extern void deinit_handler(sigcontext_t *scp, unsigned long *uc_flags);

extern void dosemu_fault(int, siginfo_t *, void *);
void signal_block_async_nosig(sigset_t *old_mask);

extern pthread_t dosemu_pthread_self;

#ifdef DNATIVE
void signal_switch_to_dosemu(void);
void signal_switch_to_dpmi(void);
void signal_return_to_dosemu(void);
void signal_return_to_dpmi(void);
void signal_unblock_async_sigs(void);
void signal_restore_async_sigs(void);
void signal_set_altstack(int on);
#else
static inline void signal_switch_to_dosemu(void) {}
static inline void signal_switch_to_dpmi(void) {}
static inline void signal_return_to_dosemu(void) {}
static inline void signal_return_to_dpmi(void) {}
static inline void signal_unblock_async_sigs(void) {}
static inline void signal_restore_async_sigs(void) {}
static inline void signal_set_altstack(int on) {}
#endif

#endif
