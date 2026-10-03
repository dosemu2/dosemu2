/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 *
 *
 *  SIMX86 a Intel 80x86 cpu emulator
 *  Copyright (C) 1997,2001 Alberto Vignani, FIAT Research Center
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
 *  (linux/arch/i386/kernel/vm86.c). This code originally was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 * linux/kernel/ldt.c
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds
 *
 ***************************************************************************/

/* ======================================================================= */
/*
 * Some protected-mode stuff, not much changed from the 1999 version.
 * Interfaces with DPMI and its LDT format.
 * Will be heavily reworked later.
 */

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
#include "msdoshlp.h"

Descriptor *GDT = NULL;
Descriptor *LDT = NULL;
Gatedesc   *IDT = NULL;

static unsigned short sysxfer[] = {
	DT_NO_XFER, DT_XFER_TSS16, DT_XFER_LDT, DT_XFER_TSS16,
	DT_XFER_CG16, DT_XFER_TSKG, DT_XFER_IG16, DT_XFER_TRP16,
	DT_NO_XFER, DT_XFER_TSS32, DT_NO_XFER, DT_XFER_TSS32,
	DT_XFER_CG32, DT_NO_XFER, DT_XFER_IG32, DT_XFER_TRP32
};

static char ofsnam[] =	"ES: CS: SS: DS: FS: GS:";

static unsigned char ofsseg[] = {
	Ofs_XES, Ofs_XCS, Ofs_XSS, Ofs_XDS, Ofs_XFS, Ofs_XGS };

#define MKOFSNAM(o,b)   (memcpy((b), (ofsnam+(((o)-Ofs_ES)<<1)), 3), ((b)[3]=0), (b))

unsigned char e_ofsseg(int ofs)
{
	return ofsseg[(ofs-Ofs_ES)>>1];
}

int SetSegReal(unsigned short sel, int ofs)
{
	static char buf[4];
	SDTR *sd;

	sd = (SDTR *)CPUOFFS(e_ofsseg(ofs));

	CPUWORD(ofs) = sel;
	sd->BoundL = sel<<4;
	sd->BoundH = sd->BoundL + 0xffff;

	if (debug_level('e')>1)
	    dbug_printf("SetSeg REAL %s%04x\n",MKOFSNAM(ofs,buf),sel);
	return 0;
}

static int _SetSegProt_check(int ofs, unsigned long sel)
{
	static char buf[4];
	unsigned short wFlags, sys;
	Descriptor *dt;
	SDTR *sd;

	sd = (SDTR *)CPUOFFS(e_ofsseg(ofs));

	if (CPUWORD(ofs) == sel && (sd->BoundH - sd->BoundL) != SDTR_INVALID_LIMIT) {
	    if (debug_level('e') >= 9)
		e_printf("SetSeg PROT %s%04lx cached\n",MKOFSNAM(ofs,buf),sel);
	    return -1;
	}

	if (sel < 4) {
	    if ((ofs==Ofs_CS)||(ofs==Ofs_SS)) return EXCP0D_GPF;
	    return 0;	/* DS..GS can be 0 for some while */
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
	    return EXCP0D_GPF;
#if 0
	    dt = GDT;	/* GDT is not yet there */
	    if ((dt == NULL) ||	((sel & 0xfff8) > TheCPU.GDTR.Limit))
	    {
		if (dt) e_printf("Invalid GDT selector %#lx\n", sel);
		return EXCP0D_GPF;
	    }
#endif
	}
	wFlags = GetSelectorFlags(sel);
	sys = (wFlags & DF_USER);
	if (!(wFlags & DF_PRESENT)) {
	    e_printf("DT: selector %lx not present\n",sel);
	    if (ofs==Ofs_SS) return EXCP0C_STACK;
		else return EXCP0B_NOSEG;
	}
	if (!sys) {	/* must be GDT now */
	    unsigned short sx = sysxfer[wFlags&15];
	    if (debug_level('e')>3)
	        e_printf("GDT system segment %#lx type %d\n",sel,sx);
	    if (sx==DT_NO_XFER) return EXCP0D_GPF;
	    return 0;	  /* will check sys segment again later */
	}
	/* check data/code */
	if (ofs==Ofs_CS) {
	    /* data can't be executed... really? */
	    if (!(wFlags & DF_CODE)) {
		dbug_printf("Attempt to execute into data segment %lx\n",sel);
		return EXCP0D_GPF;
	    }
	}
	else {
	    /* we CAN move a code sel into [DEFG]S provided that it
	     * can be read - but how can we trap writes? */
	    /* Error summary (Intel):
	     *	SS	zero				GP
	     *		RPL != CPL			GP
	     *		DPL != CPL			GP
	     *		data not writable		GP
	     *		not present			SS
	     * [DEFG]S	not data or readable code	GP
	     *		data or nonconf code AND
	     *		  RPL>DPL AND CPL>DPL		GP
	     *		not present			NP
	     */
	    if ((wFlags & DF_CODE)&&(!(wFlags & DF_CREADABLE)))
		    return EXCP0D_GPF;
	}
	return 0;
}

/* A far call whose target selector is a call gate does not go where the
 * instruction points: the gate names the code selector and the entry point,
 * and the offset in the instruction is ignored. A 286|DOS-Extender client
 * reaches its library that way. Only the case that needs no stack switch is
 * handled, which is the only one a dpmi client can build for itself: the
 * gate, the caller and the target all at the same privilege level.
 * Returns -1 when the selector is not a gate, 0 when it is and *selp and
 * *eipp now name the entry point, and an exception number otherwise. */
int call_gate(unsigned int *selp, unsigned int *eipp, int *modep)
{
	unsigned int sel = *selp;
	unsigned short tflags;
	Descriptor *dt;
	Gatedesc *gp;
	unsigned int tsel;

	if (!PROTMODE() || sel < 4)
		return -1;
	dt = (sel & 4) ? LDT : GDT;
	if (dt == NULL)
		return -1;
	if ((sel & 0xfff8) > ((sel & 4) ? TheCPU.LDTR.Limit : TheCPU.GDTR.Limit))
		return -1;
	gp = (Gatedesc *)&dt[sel >> 3];
	if (gp->S || (gp->type & 7) != 4)
		return -1;
	TheCPU.scp_err = sel & 0xfffc;
	if (!gp->present) {
		e_printf("call gate %#x not present\n", sel);
		return EXCP0B_NOSEG;
	}
	/* the gate has to be visible from here */
	if ((int)gp->DPL < CPL || (int)gp->DPL < (int)(sel & 3)) {
		e_printf("call gate %#x dpl %d not visible at cpl %d\n",
			 sel, gp->DPL, CPL);
		return EXCP0D_GPF;
	}
	tsel = gp->seg;
	if (tsel < 4) {
		e_printf("call gate %#x has a null target\n", sel);
		TheCPU.scp_err = 0;
		return EXCP0D_GPF;
	}
	tflags = GetSelectorFlags(tsel);
	if (!(tflags & DF_USER) || !(tflags & DF_CODE)) {
		e_printf("call gate %#x does not point at code\n", sel);
		TheCPU.scp_err = tsel & 0xfffc;
		return EXCP0D_GPF;
	}
	/* a gate to a more privileged segment would need a stack switch,
	 * which nothing here can build, so refuse it rather than run the
	 * target on the caller's stack */
	if (!(tflags & DF_CONFORMING) &&
	    (int)((tflags & DF_DPL) >> 5) != CPL) {
		e_printf("call gate %#x to dpl %d from cpl %d not supported\n",
			 sel, (tflags & DF_DPL) >> 5, CPL);
		TheCPU.scp_err = tsel & 0xfffc;
		return EXCP0D_GPF;
	}
	*selp = tsel | CPL;
	if (gp->type & 8) {
		*eipp = (gp->offs_hi << 16) | gp->offs_lo;
		*modep &= ~DATA16;
	} else {
		*eipp = gp->offs_lo;
		*modep |= DATA16;
	}
	if (debug_level('e') > 2)
		dbug_printf("call gate %#x -> %04x:%08x\n", sel, *selp, *eipp);
	return 0;
}

int SetSegProt_check(int ofs, unsigned long sel)
{
	int e = _SetSegProt_check(ofs, sel);
	if (e > 0) {
		TheCPU.err = e;
		TheCPU.scp_err = sel & 0xfffc;
	}
	return e;
}

void SetSegProt_set(int ofs, unsigned long sel)
{
	static char buf[4];
	unsigned short wFlags = GetSelectorFlags(sel);
	unsigned char lbig = (wFlags & DF_32)? 0xff : 0;
	SDTR *sd = (SDTR *)CPUOFFS(e_ofsseg(ofs));
	int a16;

	/* should set CPL here if seg==CS ??? */
	if (ofs==Ofs_CS)
	    a16 = (lbig? 0:ADDR16);
	else
	    a16 = TheCPU.mode & ADDR16;
	if (lbig && a16) {
	    if (debug_level('e')>3)
	        e_printf("Large segment %#lx in 16-bit mode\n",sel);
	}
	else if (!lbig && !a16) {
	    if (debug_level('e')>3)
	        e_printf("Small segment %#lx in 32-bit mode\n",sel);
	}
	if (sel < 4) {
	    sd->BoundL = NULLSEG_BASE;
	    sd->BoundH = sd->BoundL;
	}
	else if (!(wFlags & DF_USER)) { /* must be GDT now */
	    sd->BoundL = 0;
	    sd->BoundH = 0;	  /* try to trap if not checked */
	}
	else {
	    SetFlagAccessed(sel);
	    sd->BoundL = GetPhysicalAddress(sel);
	    sd->BoundH = sd->BoundL + GetSelectorByteLimit(sel);
	}
	if (debug_level('e')>7) {
		e_printf("SetSeg PROT %s%04lx\n",MKOFSNAM(ofs,buf),sel);
		e_printf("PMSEL %#04lx bounds=%08x:%08x flg=%04x big=%d\n",
			sel, sd->BoundL, sd->BoundH, wFlags, lbig&1);
	}
	CPUWORD(ofs) = sel;
	if (ofs==Ofs_SS) {
		TheCPU.StackMask = (lbig? 0xffffffff : 0x0000ffff);
		if (debug_level('e')>1) e_printf("MAKESEG SS: big=%d basemode=%04x\n",lbig&1,TheCPU.mode);
	} else if (ofs==Ofs_CS) {
		TheCPU.mode &= ~(ADDR16|DATA16|MBIGCS);
		if (lbig) TheCPU.mode |= MBIGCS;
		else TheCPU.mode |= (ADDR16|DATA16);
		if (debug_level('e')>1) e_printf("MAKESEG CS: big=%d basemode=%04x\n",lbig&1,TheCPU.mode);
	}
}

void SetSegProt(int ofs, unsigned long sel)
{
	int e = SetSegProt_check(ofs, sel);
	/* e == -1 means cached, no need to set */
	if (e == 0)
		SetSegProt_set(ofs, sel);
}

#if 0
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
	leavedos_main(0);
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
#endif


/* ======================================================================= */


int hsw_verr(unsigned short sel)
{
	unsigned short wFlags;
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
	/* test for present && CPL>=DPL && writeable */
	wFlags = GetSelectorFlags(sel);
	if (wFlags & DF_PRESENT) {
	    if (!(wFlags & DF_CODE) || (wFlags & DF_DWRITEABLE))
		return 1;
	}
	return 0;	/* not ok, ZF->0 */
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
	if (dt==NULL || dt == GDT || ((sel&0xfff8) > dtr->Limit)) {
		return 0;
	}
	/* check system segments if in GDT */
	if (dt==GDT && (GDT[sel>>3].S==0)) {
	    /* "is a valid descriptor type" */
	    int styp=GDT[sel>>3].type;
	    if (styp==0 || styp==6 || styp==7 || styp==8 ||
		styp==10 || styp==13 || styp==14 || styp==15) return 0;
	}
	/* "if the selector is visible at the current privilege level
	 *  (modified by the selector's RPL)" */
	pl = CPL; if ((sel&3)>pl) pl=(sel&3);
	if (pl > dt[sel>>3].DPL) {
		return 0;
	}
	/* Intel manual says nothing about present bit */
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

int emu_ldt_write(dosaddr_t addr, uint32_t op, int len)
{
	static cpuctx_t sc = {0};
	cpuctx_t *scp = &sc;

	_cr2 = addr;
	_ds = TheCPU.ds;
	_es = TheCPU.es;
	_fs = TheCPU.fs;
	_gs = TheCPU.gs;
	msdos_ldt_write(scp, op, len, _cr2);
	if (_ds == 0) { TheCPU.ds = 0; SetSegProt(Ofs_DS,0); }
	if (_es == 0) { TheCPU.es = 0; SetSegProt(Ofs_ES,0); }
	if (_fs == 0) { TheCPU.fs = 0; SetSegProt(Ofs_FS,0); }
	if (_gs == 0) { TheCPU.gs = 0; SetSegProt(Ofs_GS,0); }
	return 1;
}
