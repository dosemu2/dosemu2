/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __HMA_H
#define __HMA_H

#include "extern.h"

EXTERN int a20;
void set_a20(int);
void extmem_copy(char *dst, char *src, unsigned long len);

#endif
