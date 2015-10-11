#ifndef MSDOS_LDT_H
#define MSDOS_LDT_H

int msdos_ldt_setup(unsigned char *backbuf, unsigned char *alias);
void msdos_ldt_init(int clnt_num);
u_short DPMI_ldt_alias(void);
int msdos_ldt_fault(struct sigcontext *scp);
int msdos_ldt_pagefault(struct sigcontext *scp);
void msdos_ldt_update(int entry, u_char *buf, int len);

#endif
