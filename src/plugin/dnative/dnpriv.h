#ifndef DNPRIV_H
#define DNPRIV_H

#define DPMI_TMP_SIG SIGUSR1

void dpmi_return(sigcontext_t *scp, int retcode);
void dpmi_switch_sa(int sig, siginfo_t * inf, void *uc);
int dpmi_fault(sigcontext_t *scp);

void deinit_handler(sigcontext_t *scp, unsigned long *uc_flags);

void signal_switch_to_dosemu(void);
void signal_switch_to_dpmi(void);
void signal_return_to_dosemu(void);
void signal_return_to_dpmi(void);
void signal_set_altstack(int on);
void unsetsig(int sig);
void signative_start(void);
void signative_stop(void);
void signative_init(void);

#endif
