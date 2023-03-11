#ifndef DNATIVE_H
#define DNATIVE_H

#ifdef DNATIVE

int native_dpmi_setup(void);
void native_dpmi_done(void);
int native_dpmi_control(cpuctx_t *scp);
int native_dpmi_exit(cpuctx_t *scp);
void native_dpmi_enter(void);
void native_dpmi_leave(void);
void dpmi_return(sigcontext_t *scp, int retcode);
void dpmi_switch_sa(int sig, siginfo_t * inf, void *uc);

#define DPMI_TMP_SIG SIGUSR1

#else

static inline int native_dpmi_setup(void)
{
    error("Native DPMI not compiled in\n");
    return -1;
}

static inline void native_dpmi_done(void)
{
}

static inline int native_dpmi_control(cpuctx_t *scp)
{
    return 0;
}

static inline int native_dpmi_exit(cpuctx_t *scp)
{
    return 0;
}

static inline void native_dpmi_enter(void)
{
}

static inline void native_dpmi_leave(void)
{
}

static inline void dpmi_return(sigcontext_t *scp, int retcode)
{
}

#endif

#endif
