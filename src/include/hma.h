/* 
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef __HMA_H
#define __HMA_H

#include "extern.h"

EXTERN int a20;
extern unsigned char *ext_mem_base;
void set_a20(int);
void extmem_copy(char *dst, char *src, unsigned long len);

#endif
