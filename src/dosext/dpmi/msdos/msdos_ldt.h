#ifndef MSDOS_LDT_H
#define MSDOS_LDT_H

int msdos_ldt_setup(unsigned char *backbuf, unsigned char *alias);
u_short msdos_ldt_init(int clnt_num);
enum MfRet msdos_ldt_fault(struct sigcontext *scp, uint16_t sel);
int msdos_ldt_pagefault(struct sigcontext *scp);
void msdos_ldt_update(int entry, u_char *buf, int len);

#endif
