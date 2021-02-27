#ifndef MSDOS_LDT_H
#define MSDOS_LDT_H

unsigned short msdos_ldt_init(void);
void msdos_ldt_done(void);
int msdos_ldt_fault(sigcontext_t *scp, uint16_t sel);
int msdos_ldt_access(unsigned char *cr2);
void msdos_ldt_write(sigcontext_t *scp, uint32_t op, int len,
    unsigned char *cr2);
int msdos_ldt_pagefault(sigcontext_t *scp);
int msdos_ldt_is32(unsigned short selector);

#endif
