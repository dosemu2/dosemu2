#ifndef MSDOS_LDT_H
#define MSDOS_LDT_H

unsigned short msdos_ldt_init(void);
void msdos_ldt_done(void);
int msdos_ldt_fault(sigcontext_t *scp, uint16_t sel);
int msdos_ldt_pagefault(sigcontext_t *scp);
void msdos_ldt_update(int entry, u_char *buf, int len);

#endif
