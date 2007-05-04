/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* file lowmem.c
 *
 * Management for the static 32K heap in a low memory.
 * Used by various dosemu internal subsystems.
 */

#include <signal.h>
#include "emu.h"
#include "memory.h"
#include "smalloc.h"
#include "utilities.h"
#include "lowmem.h"

static smpool mp;

int lowmem_heap_init()
{
    sminit(&mp, (void *)SEGOFF2LINEAR(DOSEMU_LMHEAP_SEG, DOSEMU_LMHEAP_OFF),
	DOSEMU_LMHEAP_SIZE);
    smregister_error_notifier(dosemu_error);
    return 1;
}

void * lowmem_heap_alloc(int size)
{
	char *ptr = smalloc(&mp, size);
	if (!ptr) {
		error("lowmem_heap: OOM, size=%i\n", size);
		leavedos(86);
	}
	return ptr;
}

void lowmem_heap_free(char *p)
{
	return smfree(&mp, p);
}
