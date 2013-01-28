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
#include <assert.h>
#include "emu.h"
#include "memory.h"
#include "smalloc.h"
#include "utilities.h"
#include "lowmem.h"

static smpool mp;
unsigned char *dosemu_lmheap_base;

int lowmem_heap_init()
{
    dosemu_lmheap_base = MK_FP32(DOSEMU_LMHEAP_SEG, DOSEMU_LMHEAP_OFF);
    sminit(&mp, dosemu_lmheap_base, DOSEMU_LMHEAP_SIZE);
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

void lowmem_heap_free(void *p)
{
	return smfree(&mp, p);
}


#define RM_STACK_SIZE 0x200
static void *rm_stack;
static int in_rm_stack;

int get_rm_stack(Bit16u *ss_p, Bit16u *sp_p)
{
	if (!(in_rm_stack++)) {
		rm_stack = lowmem_heap_alloc(RM_STACK_SIZE);
		assert(rm_stack);
		*ss_p = DOSEMU_LMHEAP_SEG;
		*sp_p = DOSEMU_LMHEAP_OFFS_OF(rm_stack) + RM_STACK_SIZE;
		return 1;
	}
	return 0;
}

void put_rm_stack(void)
{
	assert(in_rm_stack > 0);
	if (!(--in_rm_stack))
		lowmem_heap_free(rm_stack);
}
