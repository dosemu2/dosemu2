/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* Set emulation type */

#ifndef _EMULATION_H
#define _EMULATION_H

#include "device.h"

/* Current instrument mapping needed for emulation */
extern int *imap;

extern int imap_default[128], imap_mt2gm[128];

/* Sets emulation mode to new_mode */
void emulation_set(Device *dev, Emumode new_mode);

#endif
