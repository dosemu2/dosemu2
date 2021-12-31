#ifndef DNATIVE_H
#define DNATIVE_H

void native_dpmi_setup(void);
void native_dpmi_done(void);
int native_dpmi_control(sigcontext_t *scp);
int native_dpmi_exit(sigcontext_t *scp);
void native_dpmi_enter(void);
void native_dpmi_leave(void);

#endif
