#ifndef DNATIVE_H
#define DNATIVE_H

#ifdef DNATIVE

int native_dpmi_setup(void);
void native_dpmi_done(void);
int native_dpmi_control(sigcontext_t *scp);
int native_dpmi_exit(sigcontext_t *scp);
void native_dpmi_enter(void);
void native_dpmi_leave(void);
void dpmi_return(sigcontext_t *scp, int retcode);
void dpmi_iret_setup(sigcontext_t *scp);
void dpmi_iret_unwind(sigcontext_t *scp);

#else

static inline int native_dpmi_setup(void)
{
    return -1;
}

static inline void native_dpmi_done(void)
{
}

static inline int native_dpmi_control(sigcontext_t *scp)
{
    return 0;
}

static inline int native_dpmi_exit(sigcontext_t *scp)
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

static inline void dpmi_iret_setup(sigcontext_t *scp)
{
}

static inline void dpmi_iret_unwind(sigcontext_t *scp)
{
}

#endif

#endif
