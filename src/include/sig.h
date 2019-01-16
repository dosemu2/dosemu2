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

#if defined(__linux__)
/* replace sigaltstack() to avoid musl bugs */
static inline int dosemu_sigaltstack(const stack_t *ss, stack_t *oss)
{
  return syscall(SYS_sigaltstack, ss, oss);
}
#else
static inline int dosemu_sigaltstack(const stack_t *ss, stack_t *oss)
{
  return sigaltstack(ss, oss);
}
#endif

extern void add_thread_callback(void (*cb)(void *), void *arg, const char *name);
#ifdef __linux__
extern void SIG_init(void);
extern void SIG_close(void);
#endif

/* signals for Linux's process control of consoles */
#define SIG_RELEASE     SIGUSR1
#define SIG_ACQUIRE     SIGUSR2

typedef mcontext_t sigcontext_t;

extern void SIGNAL_save( void (*signal_call)(void *), void *arg, size_t size,
	const char *name );
extern void handle_signals(void);
extern void do_periodic_stuff(void);
extern void sig_ctx_prepare(int tid);
extern void sig_ctx_restore(int tid);

extern int sigchld_register_handler(pid_t pid, void (*handler)(void));
extern int sigchld_enable_cleanup(pid_t pid);
extern int sigchld_enable_handler(pid_t pid, int on);
extern int sigalrm_register_handler(void (*handler)(void));
extern void registersig(int sig, void (*handler)(sigcontext_t *,
	siginfo_t *));
extern void init_handler(sigcontext_t *scp, int async);
extern void deinit_handler(sigcontext_t *scp, unsigned long *uc_flags);

extern void dosemu_fault(int, siginfo_t *, void *);
extern void signal_switch_to_dosemu(void);
extern void signal_switch_to_dpmi(void);
extern void signal_return_to_dosemu(void);
extern void signal_return_to_dpmi(void);
extern void signal_unblock_async_sigs(void);
extern void signal_restore_async_sigs(void);
extern void signal_set_altstack(int on);

extern pthread_t dosemu_pthread_self;

#endif
