#ifndef SEGREG_PRIV_H
#define SEGREG_PRIV_H

enum MfRet { MFR_HANDLED, MFR_NOT_HANDLED, MFR_ERROR };

void msdos_fault_handler(sigcontext_t *scp, void *arg);
void msdos_pagefault_handler(sigcontext_t *scp, void *arg);

#endif
