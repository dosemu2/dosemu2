#ifndef MSDOS_LDT_H
#define MSDOS_LDT_H

int msdos_ldt_setup(unsigned char *backbuf, unsigned char *alias);
u_short msdos_ldt_init(int clnt_num);
void msdos_ldt_done(int clnt_num);
int msdos_ldt_pagefault(struct sigcontext *scp);
void msdos_ldt_update(int entry, u_char *buf, int len);

#endif
