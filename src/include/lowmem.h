/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef __LOWMEM_H
#define __LOWMEM_H

int lowmem_heap_init(void);
void *lowmem_heap_alloc(int size);
void lowmem_heap_free(char *p);

#endif
