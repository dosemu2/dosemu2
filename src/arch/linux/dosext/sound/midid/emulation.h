/* Set emulation type */

#ifndef _EMULATION_H
#define _EMULATION_H

#include"midid.h"

/* Current instrument mapping needed for emulation */
int *imap;

/* Sets emulation mode to new_mode */
void emulation_set(Emumode new_mode);

#endif
