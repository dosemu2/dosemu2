/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* Set emulation type */

#ifndef _EMULATION_H
#define _EMULATION_H

#include"midid.h"

/* Current instrument mapping needed for emulation */
int *imap;

/* Sets emulation mode to new_mode */
void emulation_set(Emumode new_mode);

#endif
