/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2000 Alberto Vignani, FIAT Research Center
 *				a.vignani@crf.it
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Additional copyright notes:
 *
 * 1. The kernel-level vm86 handling was taken out of the Linux kernel
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 * linux/kernel/ldt.c
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds
 *
 ***************************************************************************/

#include "config.h"

#ifdef X86_EMULATOR

/* ======================================================================= */

#include <stdlib.h>
#include <setjmp.h>
#include <errno.h>
#include "emu.h"
#include "timers.h"
#include "pic.h"
#include "emu86.h"
#include "cpu-emu.h"
#include "codegen.h"
#include "protmode.h"

Descriptor *GDT = NULL;
Descriptor *LDT = NULL;
Gatedesc   *IDT = NULL;

static unsigned short sysxfer[] = {
	DT_NO_XFER, DT_XFER_TSS16, DT_XFER_LDT, DT_XFER_TSS16,
	DT_XFER_CG16, DT_XFER_TSKG, DT_XFER_IG16, DT_XFER_TRP16,
	DT_NO_XFER, DT_XFER_TSS32, DT_NO_XFER, DT_XFER_TSS32,
	DT_XFER_CG32, DT_NO_XFER, DT_XFER_IG32, DT_XFER_TRP32
};

int SetSegreg(int mode, SDTR *sd, unsigned char *big, unsigned long csel)
{
	unsigned short wFlags, sofs, sys;
	unsigned long sel;
	unsigned char lbig;
	Descriptor *dt;

	// csel Was:	MK_CS=0 MK_DS=10000 MK_ES=20000
	//		MK_SS=30000 MK_FS=40000 MK_GS=50000
	sel = csel & 0xffff;
	if (REALADDR()) {
	    sd->BoundL = (sel<<4); if (big) *big=0;
	    /*
	     * Intel docs say: BOUND gives an error if index<boundl or
	     * index>(boundh+2 or 4). For the moment BOUND is only
	     * generated in 32-bit mode -> err if index>=((limit+1)+4).
	     */
	    sd->BoundH = sd->BoundL + 0xfffb;
	    if (d.emu>4) {
		    e_printf("SetSeg REAL %08lx -> %08lx:%08lx\n",csel,
		    	sd->BoundL,sd->BoundH);
	    }
	    return 0;	/* always valid */
	}
	TheCPU.scp_err = sel & 0xfffc;
	sofs = (csel>>16)&0xff;
	if (sel < 4) {
	    if ((sofs==Ofs_CS)||(sofs==Ofs_SS)) return EXCP0D_GPF;
	    return 0;	/* DS..GS can be 0 */
	}

	/* DT checks */
	if (sel & 4) {
	    dt = LDT;
	    if ((dt == NULL) || (dt[sel>>3].S==0) ||
		((sel & 0xfff8) > TheCPU.LDTR.Limit)) {
		e_printf("Invalid LDT selector %#lx\n", sel);
		return EXCP0D_GPF;
	    }
	}
	else {
	    dt = GDT;
	    if ((dt == NULL) ||	((sel & 0xfff8) > TheCPU.GDTR.Limit)) {
		dbug_printf("Invalid GDT selector %#lx\n", sel);
		return EXCP0D_GPF;
	    }
	}
	/* should set CPL here if seg==CS ??? */
	wFlags = GetSelectorFlags(sel);
	sys = (wFlags & DF_USER);
	if (!(wFlags & DF_PRESENT)) {
	    e_printf("DT: selector %lx not present\n",sel);
	    if (sofs==Ofs_SS) return EXCP0C_STACK;
		else return EXCP0B_NOSEG;
	}
	if (!sys) {	/* must be GDT now */
	    unsigned short sx = sysxfer[wFlags&15];
	    if (d.emu>3)
	        e_printf("GDT system segment %#lx type %d\n",sel,sx);
	    if (sx==DT_NO_XFER) return EXCP0D_GPF;
	    sd->BoundH = 0;	  /* try to trap if not checked */
	    return 0;	  /* will check sys segment again later */
	}
	lbig = (wFlags & DF_32)? 0xff : 0;
	/* check data/code */
	if (sofs==Ofs_CS) {
	    /* data can't be executed... really? */
	    if (!(wFlags & DF_CODE)) {
		dbug_printf("Attempt to execute into data segment %lx\n",sel);
		//return EXCP0D_GPF;
	    }
	    mode = (mode & ~ADDR16) | (lbig? 0:ADDR16);
	}
	else {
	    /* we CAN move a code sel into [DEFG]S provided that it
	     * can be read - but how can we trap writes? */
	    if (wFlags & DF_CODE)
		if (!(wFlags & DF_CREADABLE)) return EXCP0D_GPF;
	}
	if (lbig && (mode&ADDR16)) {
	    if (d.emu>3)
	        e_printf("Large segment %#lx in 16-bit mode\n",sel);
	}
	else if (!lbig && !(mode&ADDR16)) {
	    if (d.emu>3)
	        e_printf("Small segment %#lx in 32-bit mode\n",sel);
	}
	SetFlagAccessed(sel);
	sd->BoundL = (long)GetPhysicalAddress(sel);
	sd->BoundH = sd->BoundL + (long)GetSelectorByteLimit(sel);
#ifdef USE_BOUND
	/* aargh aargh... BOUND uses SIGNED limits!! */
	if ((int)sd->BoundH < 0) {
	    if (d.emu>3)
		e_printf("SEG: Bound H %08lx is negative!\n",sd->BoundH);
	    sd->BoundH = 0x7fffffff;
	}
#endif
	/*
	 * Intel docs say: BOUND gives an error if index<boundl or
	 * index>(boundh+2 or 4). For the moment BOUND is only
	 * generated in 32-bit mode -> err if index>=((limit+1)+4).
	 */
	sd->BoundH -= 4;
	if (big) *big = lbig;
	if (d.emu>4) {
		e_printf("PMSEL %#lx bounds=%08lx:%08lx flg=%04x big=%d\n",
			sel, sd->BoundL, sd->BoundH, wFlags, lbig&1);
	}
	return 0;
}

/* not (yet) used */
void ValidateAddr(unsigned char *addr, unsigned short sel)
{
	unsigned char *base;
	unsigned short wFlags;

	if (!PROTMODE()) return;	/* always ok */
	wFlags = GetSelectorFlags(sel);
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
	leavedos(0);
}


/* flags with 0x409e mask:
 *	0090	16 RO data
 *	0092	16 RW data
 *	0094	16 RO data
 *	0096	16 RW data
 *	0098	16 FO NC code
 *	009a	16 RF NC code
 *	009c	16 FO C  code
 *	009e	16 RF C  code
 *	4090	32 RO data
 *	4092	32 RW data
 *	4094	32 RO data
 *	4096	32 RW data
 *	4098	32 FO NC code
 *	409a	32 RF NC code
 *	409c	32 FO C  code
 *	409e	32 RF C  code
 * flags with 0x009f mask:
 *	x081	16 TSS
 *	x082	LDT
 *	x083	16 TSS
 *	x084	16 call gate
 *	x085	task gate
 *	x086	16 int gate
 *	x087	16 trap gate
 *	x089	32 TSS
 *	x08b	32 TSS
 *	x08c	32 call gate
 *	x08e	32 int gate
 *	x08f	32 trap gate
 */
unsigned short GetSelectorXfer(unsigned short w)
{
  unsigned short flags;
  if (!PROTMODE()) return DT_RM_XFER;
  flags=DTgetFlags(w);
  if (!(flags&DF_PRESENT)) return DT_NO_XFER;
  if (flags&DF_USER) {
	if (flags&DF_CODE) return (flags&DF_32? DT_XFER_CODE32:DT_XFER_CODE16);  
	    else return (flags&DF_32? DT_XFER_DATA32:DT_XFER_DATA16);  
  }
  return sysxfer[flags&15];
}


/* ======================================================================= */


int hsw_verr(unsigned short sel)
{
	unsigned short wFlags;
	if (V86MODE()) return -1;	/* maybe error */
	/* test for present && CPL>=DPL && readable */
	wFlags = GetSelectorFlags(sel);
	if (wFlags & DF_PRESENT) {
	    if (!(wFlags & DF_CODE) || (wFlags & DF_CREADABLE))
		return 1;
	}
	return 0;	/* not ok, ZF->0 */
}

int hsw_verw(unsigned short sel)
{
	unsigned short wFlags;
	if (V86MODE()) return -1;	/* maybe error */
	/* test for present && CPL>=DPL && writeable */
	wFlags = GetSelectorFlags(sel);
	if (wFlags & DF_PRESENT) {
	    if (!(wFlags & DF_CODE) || (wFlags & DF_DWRITEABLE))
		return 1;
	}
	return 0;	/* not ok, ZF->0 */
}


/* ======================================================================= */


static int emu_read_ldt(char *ptr, unsigned long bytecount)
{
	__u32 *lp = (__u32 *)LDT;
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



static int emu_update_LDT (struct modify_ldt_ldt_s *ldt_info, int oldmode)
{
	static char *xftab[] = { "D16","D32","C16","C32" };
	Descriptor *lp;
	int bSelType;

	/* Install the new entry ...  */
	lp = &LDT[ldt_info->entry_number];

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

	D_printf("EMU86: write LDT entry %#x type %s: %08x %08x %04x\n",
		ldt_info->entry_number, xftab[bSelType], DT_BASE(lp),
		DT_LIMIT(lp), DT_FLAGS(lp));
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

int e_larlsl(int mode, unsigned short sel)
{
	Descriptor *dt;
	DTR *dtr;
	int pl;

	dt  = (sel & 4? LDT : GDT);
	dtr = (sel & 4? &(TheCPU.LDTR) : &(TheCPU.GDTR));
	/* check DT and limits */
	if (dt==NULL || ((sel&0xfff8) > dtr->Limit)) return 0;
	/* check system segments */
	if (dt==GDT && (GDT[sel>>3].S==0)) {
	    int styp=GDT[sel>>3].type;
	    if (styp==0 || styp==6 || styp==7 || styp==8 ||
		styp==10 || styp==13 || styp==14 || styp==15) return 0;
	}
	/* check present bit and RPL */
	pl = CPL; if ((sel&3)>pl) pl=(sel&3);
	if (dt[sel>>3].present==0 || (pl > dt->DPL)) return 0;
	/* is all ok now? */
	return 1;
}

/* ======================================================================= */

/* called from dpmi.c */
unsigned short emu_do_LAR (unsigned short selector)
{
    unsigned short flg=0;
    if (LDT) {
      flg = DT_FLAGS(&LDT[selector>>3]) & 0xff;
      if (flg) LDT[selector>>3].type |= 1;
    }
    return flg;
}

/* called from dpmi.c */
void emu_mhp_SetTypebyte (unsigned short selector, int typebyte)
{
    if (LDT) {
#if 0	/* clean version */
      Descriptor *lp = &LDT[selector>>3];
      lp->type = typebyte & 15;
      lp->S = typebyte>>4;
      lp->DPL = (typebyte>>5)&3;
      lp->present = typebyte>>7;
#else	/* quick & dirty & little-endian too */
      char *lp = (char *)(&LDT[selector>>3]);
      lp[5] = (char)typebyte;
#endif
    }
}

/* ======================================================================= */

#endif	/* X86_EMULATOR */
