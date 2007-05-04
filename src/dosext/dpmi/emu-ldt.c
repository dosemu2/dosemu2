/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "emu.h"

#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "cpu.h"
#include "extern.h"
#include "types.h"
#include "dpmi.h"
#include "emu-ldt.h"
#include "dosemu_debug.h"

static int emu_read_ldt(char *ptr, unsigned long bytecount)
{
	uint32_t *lp = (uint32_t *)ldt_buffer;
	int i, size=0;

	for (i = 0; (i < LGDT_ENTRIES) && (size < bytecount); i++) {
		if (*lp) {
			D_printf("EMU86: read LDT entry %04x: %08x%08x\n",i,
				lp[1], lp[0]);
		}
		if (ptr) {
		  memcpy(ptr, lp, LGDT_ENTRY_SIZE);
		  ptr += LGDT_ENTRY_SIZE;
		}
		size += LGDT_ENTRY_SIZE;
		lp+=2;
	}
	e_printf("EMU86: %d LDT entries read\n", i);
	return size;
}

#define LDT_empty(info) (\
	(info)->base_addr	== 0	&& \
	(info)->limit		== 0	&& \
	(info)->contents	== 0	&& \
	(info)->read_exec_only	== 1	&& \
	(info)->seg_32bit	== 0	&& \
	(info)->limit_in_pages	== 0	&& \
	(info)->seg_not_present == 1	&& \
	(info)->useable		== 0	)

static int emu_update_LDT (struct modify_ldt_ldt_s *ldt_info, int oldmode)
{
	static char *xftab[] = { "D16","D32","C16","C32" };
	Descriptor *lp;
	int bSelType;

	/* Install the new entry ...  */
	lp = &((Descriptor *)ldt_buffer)[ldt_info->entry_number];

	/* Allow LDTs to be cleared by the user. */
	if (ldt_info->base_addr == 0 && ldt_info->limit == 0) {
		if (oldmode || LDT_empty(ldt_info)) {
			memset(lp, 0, sizeof(*lp));
			D_printf("EMU86: LDT entry %#x cleared\n",
				 ldt_info->entry_number);
			return 0;
		}
	}

	MKBASE(lp,(long)ldt_info->base_addr);
	MKLIMIT(lp,(long)ldt_info->limit);
	/*
	 * LDT bits 40-43 (5th byte):
	 *  3  2  1  0
	 * C/D E  W  A
	 *  0  0  0		R/O data
	 *  0  0  1		R/W data, expand-up stack
	 *  0  1  0		R/O data
	 *  0  1  1		R/W data, expand-down stack
	 * C/D C  R  A
	 *  1  0  0		F/O code, non-conforming
	 *  1  0  1		F/R code, non-conforming
	 *  1  1  0		F/O code, conforming
	 *  1  1  1		F/R code, conforming
	 *
	 */
	lp->type = ((ldt_info->read_exec_only ^ 1) << 1) |
		   (ldt_info->contents << 2);
	lp->S = 1;	/* not SYS */
	lp->DPL = 3;
	lp->present = (ldt_info->seg_not_present ^ 1);
	lp->AVL = (oldmode? 0:ldt_info->useable);
	lp->DB = ldt_info->seg_32bit;
	lp->gran = ldt_info->limit_in_pages;

	bSelType = ((lp->type>>2)&2) | lp->DB;

	D_printf("EMU86: LDT entry %#x type %s: b=%08x l=%x%s fl=%04x\n",
		ldt_info->entry_number, xftab[bSelType], DT_BASE(lp),
		DT_LIMIT(lp), (lp->gran? "fff":""), DT_FLAGS(lp));
	return 0;
}


static int emu_write_ldt(void *ptr, unsigned long bytecount, int oldmode)
{
	int error;
	struct modify_ldt_ldt_s ldt_info;

	error = -EINVAL;
	if (bytecount != sizeof(ldt_info)) {
		dbug_printf("EMU86: write_ldt: bytecount=%ld\n",bytecount);
		goto out;
	}
	memcpy(&ldt_info, ptr, sizeof(ldt_info));

	error = -EINVAL;
	if (ldt_info.entry_number >= LGDT_ENTRIES) {
		dbug_printf("EMU86: write_ldt: entry=%d\n",ldt_info.entry_number);
		goto out;
	}
	if (ldt_info.contents == 3) {
		if (oldmode) {
			dbug_printf("EMU86: write_ldt: oldmode\n");
			goto out;
		}
		if (ldt_info.seg_not_present == 0) {
			dbug_printf("EMU86: write_ldt: seg_not_present\n");
			goto out;
		}
	}
	error = emu_update_LDT(&ldt_info, oldmode);
out:
	return error;
}

int emu_modify_ldt(int func, void *ptr, unsigned long bytecount)
{
	int ret = -ENOSYS;

#if 1
	{ int *lptr = (int *)ptr;
	  D_printf("EMU86: modify_ldt %02x %ld [%08x %08x %08x %08x]\n",
		func, bytecount, lptr[0], lptr[1], lptr[2], lptr[3] ); }
#endif
	switch (func) {
	case 0:
		ret = emu_read_ldt((char *)ptr, bytecount);
		break;
	case 1:
		ret = emu_write_ldt(ptr, bytecount, 1);
		break;
	case 0x11:
		ret = emu_write_ldt(ptr, bytecount, 0);
		break;
	}
	return ret;
}
