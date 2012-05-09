/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef __LOWMEM_H
#define __LOWMEM_H

int lowmem_heap_init(void);
void *lowmem_heap_alloc(int size);
void lowmem_heap_free(void *p);
extern unsigned char *dosemu_lmheap_base;
#define DOSEMU_LMHEAP_OFFS_OF(s) \
  (((unsigned char *)(s) - dosemu_lmheap_base) + DOSEMU_LMHEAP_OFF)

#endif
