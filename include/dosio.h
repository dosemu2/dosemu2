#ifndef DOSIPC_H
#define DOSIPC_H

#if 0
/* this doesn't need to be here */
#include <sys/ipc.h>
#include <sys/shm.h>

#endif
#include "extern.h"

EXTERN u_char keys_ready INIT(0);

void memory_setup(void), set_a20(int);

/* do, while necessary because if { ... }; precludes an else, while
 * do { ... } while(); does not. ugly */
#ifdef NEW_PIC
extern void do_irq1(void);
#endif

#endif /* DOSIPC_H */
