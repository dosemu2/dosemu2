/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
    CPU-EMU a Intel 80x86 cpu emulator
    Copyright (C) 1997,1998 Alberto Vignani, FIAT Research Center

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


Additional copyright notes:

 1. The emulator uses (modified) parts of the Twin library from
    Willows Software, Inc.
    ( For more information on Twin see http://www.willows.com )

 2. The kernel-level vm86 handling was taken out of the Linux kernel
    (linux/arch/i386/kernel/vm86.c). This code originaly was written by
    Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.

 * linux/kernel/ldt.c
 *
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds

*/

#include "config.h"

#ifdef X86_EMULATOR

/* ======================================================================= */

#include <stdlib.h>
#include <setjmp.h>
#include "emu.h"
#include "timers.h"
#include "pic.h"
#define DOSEMU_TYPESONLY	/* needed by the following: */
#include "hsw_interp.h"
#include "cpu-emu.h"
#include "emu-ldt.h"

DSCR LDT[LDT_ENTRIES];
extern int vm86f;


BOOL
LoadSegment(UINT uiSel)
{
#if 0
    UINT uiSegNum;
    MODULEINFO *modinfo;

#ifdef X386
    if (uiSel == native_cs || uiSel == native_ds)
	return FALSE;
#endif

    if (GetPhysicalAddress(uiSel) != (LPBYTE)-1)
#endif
	return FALSE;	/* segment is already loaded or invalid */
}

int SetSegreg(unsigned char **lp, unsigned char *big, unsigned long csel)
{
	WORD wFlags, sel;
	sel = csel & 0xffff; csel &= 0xf0000;
	if (vm86f) {
	    *lp = (unsigned char *)(sel<<4); *big=0;
	}
	else if ((sel >> 3) == 0) {
	    if ((csel==MK_CS)||(csel==MK_SS)) return EXCP0D_GPF;
	}
	else if ((sel & 4) == 0) {
	    e_printf("LDT: invalid selector %#x\n", sel);
	    return EXCP0D_GPF;
	}
	else {
	    wFlags = GetSelectorFlags(sel);
	    if (!(wFlags & DF_PRESENT)) {
		e_printf("LDT: failed to load selector %x\n",sel);
		if (csel==MK_SS) return EXCP0C_STACK;
		  else return EXCP0B_NOSEG;
	    }
	    *big = ((wFlags & DF_32) != 0);
	    if (*big && !code32) {
	      if (d.emu>3)
	        e_printf("Large segment %#x in 16-bit mode\n",sel);
	    }
	    else if (!(*big) && code32) {
	      if (d.emu>3)
	        e_printf("Small segment %#x in 32-bit mode\n",sel);
	    }
	    SetFlagAccessed(sel);
	    *lp = GetPhysicalAddress(sel);
	}
	return 0;
}


void ValidateAddr(unsigned char *addr, unsigned short sel)
{
	unsigned char *base;
	WORD wFlags;
	if (vm86f) return;	/* always ok */
	wFlags = GetSelectorFlags(sel);
	if (sel&4) {	/* LDT */
	  if (wFlags & DF_PRESENT) {
	    base = GetSelectorAddress(sel);
	    if (addr >= base) {
		base += GetSelectorByteLimit(sel);
		if (addr <= base) return;
		dbug_printf("addr %#x > limit for sel %#x\n",(int)addr,sel);
	    }
	    else
	        dbug_printf("addr %#x < base for sel %#x\n",(int)addr,sel);
	  }
	  else
	    dbug_printf("selector %#x not present\n",sel);
	}
	FatalAppExit(0,"PROT");
}


static int emu_read_ldt(char *ptr, unsigned long bytecount)
{
	DSCR *lp = LDT;
	__u32 entry_1, entry_2;
	int i, size=0;

	for (i = 0; (i < LDT_ENTRIES) && (size < bytecount); i++) {
/*					       ^ hidden bug */
		entry_1 = (((__u32)lp->lpSelBase & 0x0000ffff) << 16) |
			  (lp->dwSelLimit & 0x0ffff);
		entry_2 = ((__u32)lp->lpSelBase & 0xff000000) |
			  (((__u32)lp->lpSelBase & 0x00ff0000) >> 16) |
			  (lp->dwSelLimit & 0xf0000) |
			  ((lp->w86Flags & 0xd0ff) << 8);
		if (entry_1) {
			D_printf("EMU86: read LDT entry %04x: %08x%08x\n",i,
				entry_2, entry_1);
		}
		if (ptr) {
		  ((__u32 *)ptr)[0] = entry_1;
		  ((__u32 *)ptr)[1] = entry_2;
		  ptr += LDT_ENTRY_SIZE;
		}
		size += LDT_ENTRY_SIZE;
		lp++;
	}
	e_printf("EMU86: %d LDT entries read\n", i);
	return size;
}

static int emu_update_Twin_LDT (struct modify_ldt_ldt_s *ldt_info, int oldmode)
{
	DSCR *lp;

	/* Install the new entry ...  */
	lp = &LDT[ldt_info->entry_number];

	lp->lpSelBase  = (LPBYTE)ldt_info->base_addr;
	lp->dwSelLimit = ldt_info->limit;
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
	lp->w86Flags   = ((ldt_info->read_exec_only ^ 1) << 1) |
			  (ldt_info->contents << 2) |
			  ((ldt_info->seg_not_present ^ 1) << 7) |
			  (ldt_info->seg_32bit << 14) |
			  (ldt_info->limit_in_pages << 15) |
			  0x70;		/* always DPL=3, not SYS */
	if (!oldmode)
		lp->w86Flags |= (ldt_info->useable << 12);

	switch (ldt_info->contents&3) {
	  case 0: case 1:
		lp->bSelType = ldt_info->seg_32bit? TRANSFER_DATA32 : TRANSFER_DATA;
		break;
	  case 2: case 3:
		lp->bSelType = ldt_info->seg_32bit? TRANSFER_CODE32 : TRANSFER_CODE16;
		break;
	}
	lp->hGlobal = 0;
	lp->bModIndex = 0;

	D_printf("EMU86: write LDT entry %#x type %d: %08lx %08lx %04x\n",
		ldt_info->entry_number, lp->bSelType, (long)lp->lpSelBase,
		lp->dwSelLimit, lp->w86Flags);
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
	if (ldt_info.entry_number >= LDT_ENTRIES) {
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
	error = emu_update_Twin_LDT(&ldt_info, oldmode);
out:
	return error;
}

int emu_modify_ldt(int func, void *ptr, unsigned long bytecount)
{
	int ret = -ENOSYS;

#if 0
	D_printf("EMU86: modify_ldt %02x %ld [%08lx %08lx %08lx %08lx]\n",
		func, bytecount, ((long *)ptr)[0], ((long *)ptr)[1], 
		((long *)ptr)[2], ((long *)ptr)[3] );
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

/* ======================================================================= */

#endif	/* X86_EMULATOR */
