/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

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

#endif /* DOSIPC_H */
