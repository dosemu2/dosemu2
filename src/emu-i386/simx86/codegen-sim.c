/***************************************************************************
 *
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
 *  (linux/arch/i386/kernel/vm86.c). This code originaly was written by
 *  Linus Torvalds with later enhancements by Lutz Molgedey and Hans Lermen.
 *
 ***************************************************************************/

/*
 * BACK-END for the cpuemu interpreter.
 *
 * This module is a plain stupid interpreter for x86 code in C.
 * There is a bit of 'lazy' flag handling but not much else.
 * This is only a reference framework. No real-time stuff is possible
 * with full interpretation (unless of course you have a 3GHz+ CPU)
 *
 * All instructions operate on a virtual CPU image in memory ("TheCPU"),
 * and are completely self-contained. They read from TheCPU registers,
 * operate, and store back to the same registers. There's an exception -
 * FLAGS, which are not stored back until the end of a code block.
 *
 * There are two main functions here:
 *	AddrGen, which implements the AGU (Address Generation Unit).
 *		It calculates the address coming from ModRM and stores
 *		it into a well-defined register (edi in the x86 case)
 *	Gen, which does the ALU work and all the rest. There is no
 *		branch specific unit, as the branches are (in principle)
 *		all interpreted.
 * Both functions use a variable parameter approach, just to make them
 *	hard to follow ;-)
 *
 */

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "port.h"
#include "emu86.h"
#include "vgaemu.h"
#include "codegen-sim.h"

#undef	DEBUG_MORE

void (*Gen)(int op, int mode, ...);
void (*AddrGen)(int op, int mode, ...);
unsigned char *(*CloseAndExec)(unsigned char *PC, TNode *G, int mode, int ln);
static void Gen_sim(int op, int mode, ...);
static void AddrGen_sim(int op, int mode, ...);
static unsigned char *CloseAndExec_sim(unsigned char *PC, TNode *G, int mode, int ln);

int TrapVgaOn = 0;
int UseLinker = 0;

/////////////////////////////////////////////////////////////////////////////

#define	Offs_From_Arg()		(char)(va_arg(ap,int))

/* WARNING - these are signed char offsets, NOT pointers! */
char OVERR_DS=Ofs_XDS, OVERR_SS=Ofs_XSS;

/* working registers of the host CPU */
wkreg DR1;	// "eax"
wkreg DR2;	// "edx"
wkreg AR1;	// "edi"
wkreg AR2;	// "esi"
wkreg SR1;	// "ebp"
wkreg TR1;	// "ecx"
flgtmp RFL;

/////////////////////////////////////////////////////////////////////////////

/* empirical!! */
//static int goodmemref(long m)
//{
//	if ((m>0) && (m<0x110000)) return 1;
//	if ((m>0x40000000) && (m<mMaxMem)) return 1;
//	return 0;
//}

static inline void MARK(void)	// oops...objdump shows all the code at the
				// END of a switch statement(!)
{
	__asm__ __volatile__ ("cmc; cmc");
}

/////////////////////////////////////////////////////////////////////////////

#define SET_CF(c)	CPUBYTE(Ofs_FLAGS)=((CPUBYTE(Ofs_FLAGS)&0xfe)|(c))

static inline void FlagSync_C (int sub)
{
	int cy;
	if (RFL.mode & MBYTE)
	    cy = RFL.RES.b.bh & 1;
	else if (RFL.mode & DATA16)
	    cy = RFL.RES.b.b2 & 1;
	else {
/* add/sub rule for carry using bit31:
 *	src1 src2  res	b31(a) b31(s)
 *	  0    0    0    0	0
 *	  0    0    1    0	1
 *	  0    1    0    1	1
 *	  0    1    1    0	1
 *	  1    0    0    1	0
 *	  1    0    1    0	0
 *	  1    1    0    1	0
 *	  1    1    1    1	1
 *
 * This add/sub flag evaluation is tricky too. In 16-bit mode, the carry
 * flag is always at its correct position (bit 16), while in 32-bit mode
 * it has to be calculated from src1,src2,res. The original Willows code
 * had the correct definition for the ADD case, but totally failed in the
 * SUB/CMP case. It turned out that there isn't any simple expression
 * covering both cases, BUT that the sub carry is the inverse of what we
 * get for ADD if src2 is inverted (NOT negated!); see for yourself.
 */
	    cy = (((((RFL.S1)^(RFL.S2)) & ~(RFL.RES.d)) | ((RFL.S1)&(RFL.S2))) >> 31) & 1;
	    if (sub) cy = !cy ^ (RFL.S2==0);
	}
	if (debug_level('e')>1) e_printf("Sync CY flag = %d\n", cy);
	SET_CF(cy);
}


static inline int FlagSync_NZ (void)
{
	int zr,pl,nf;
	if (RFL.valid==V_INVALID) return (CPUBYTE(Ofs_FLAGS)&0xc0);
	if (RFL.mode & MBYTE) {
	    zr = (RFL.RES.b.bl==0) << 6;
	    pl = RFL.RES.b.bl & 0x80;
	}
	else if (RFL.mode & DATA16) {
	    zr = (RFL.RES.w.l==0) << 6;
	    pl = (RFL.RES.d>>8) & 0x80;
	}
	else {
	    zr = (RFL.RES.d==0) << 6;
	    pl = (RFL.RES.d>>24) & 0x80;
	}
	nf = zr | pl;
	if (debug_level('e')>2) e_printf("Sync NZ flags = %02x\n", nf);
	return nf;
}

// Clear OF, leaving PF, AF, SF, ZF alone.
static void ClearOF(void)
{
	// Working on lazy flags
	if(RFL.valid!=V_INVALID)
		RFL.mode = (RFL.mode & ~(IGNOVF | SETOVF)) | CLROVF;
	// Working on real flags
	else
		CPUWORD(Ofs_FLAGS) = CPUWORD(Ofs_FLAGS) & ~0x800;
}

// Set OF, leaving PF, AF, SF, ZF alone.
static void SetOF(void)
{
	// Working on lazy flags
	if(RFL.valid!=V_INVALID)
		RFL.mode = (RFL.mode & ~(IGNOVF | CLROVF)) | SETOVF;
	// Working on real flags
	else
		CPUWORD(Ofs_FLAGS) = CPUWORD(Ofs_FLAGS) | 0x800;
}

#define SET_OF(ov) { if(ov) SetOF(); else ClearOF(); }

static inline int FlagSync_O_ (void)
{
	int of,xof,nf;
	register wkreg s2;
	// OF
	/* overflow rule using MSB:
	 *	src1 src2  res	ovf
	 *	  0    0    0    0
	 *	  0    0    1    1	BUT stc;sbb 0,0 -> ..ff, OF=0
	 *	  0    1    0    0
	 *	  0    1    1    0
	 *	  1    0    0    0	BUT stc;sbb 80..,0 -> 7f.., OF=1
	 *	  1    0    1    0
	 *	  1    1    0    1
	 *	  1    1    1    0
	 */
	if (RFL.valid==V_INVALID) return (CPUWORD(Ofs_FLAGS)&0x800);
	if (RFL.mode & CLROVF)
		nf = 0;
	else if (RFL.mode & SETOVF)
		nf = 0x800;
	else {
		s2.d = RFL.S2;
		of = ~(RFL.S1 ^ s2.d) & (RFL.S1 ^ RFL.RES.d);
		xof = 0;
		if (RFL.mode & MBYTE) {		// 0080->0800
		    if (((RFL.valid==V_SUB)&&(s2.b.bl==0x80)) ||
			((RFL.valid==V_SBB)&&(s2.b.bl==0x7f))) xof=0x800;
		    of <<= 4;
		}
		else if (RFL.mode & DATA16) {	// 8000->0800
		    if (((RFL.valid==V_SUB)&&(s2.w.l==0x8000)) ||
			((RFL.valid==V_SBB)&&(s2.w.l==0x7fff))) xof=0x800;
		    of >>= 4;
		}
		else {				// 80000000->0800
		    if (((RFL.valid==V_SUB)&&(s2.d==0x80000000)) ||
			((RFL.valid==V_SBB)&&(s2.d==0x7fffffff))) xof=0x800;
		    of >>= 20;
		}
		nf = (of^xof) & 0x800;
	}
	if (debug_level('e')>1) e_printf("Sync O flag = %04x\n", nf);
	return nf;
}

void FlagSync_O (void)
{
	int nf;
	if (RFL.mode & IGNOVF) return;
	nf = FlagSync_O_();
	CPUWORD(Ofs_FLAGS) = (CPUWORD(Ofs_FLAGS) & 0xf7ff) | nf;
}


static unsigned char parity[256] =
   { 4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
     4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
     4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     0, 4, 4, 0, 4, 0, 0, 4, 4, 0, 0, 4, 0, 4, 4, 0,
     4, 0, 0, 4, 0, 4, 4, 0, 0, 4, 4, 0, 4, 0, 0, 4 };


static inline int FlagSync_AP_ (void)
{
	int af,pf,nf;
	if (RFL.valid==V_INVALID) return (CPUBYTE(Ofs_FLAGS)&0x14);
	// AF
	if ((RFL.valid==V_SUB)||(RFL.valid==V_SBB))
	    af = (((RFL.S1 & 0x0f) - ((-RFL.S2) & 0x0f)) & 0x10);
	else
	    af = (((RFL.S1 & 0x0f) + (RFL.S2 & 0x0f)) & 0x10);
	if ((RFL.valid==V_ADC) && ((RFL.RES.d&0x1f)==0)) af = 0x10;
	// PF
	pf = parity[RFL.RES.b.bl];
	nf = af | pf;
	if (debug_level('e')>2) e_printf("Sync AP flags = %02x\n", nf);
	return nf;
}

void FlagSync_AP (void)
{
	int nf = FlagSync_AP_();
	CPUBYTE(Ofs_FLAGS) = (CPUBYTE(Ofs_FLAGS) & 0xeb) | nf;
}


void FlagSync_All (void)
{
	int nf,mk;
	if (RFL.valid==V_INVALID) return;
	nf = FlagSync_AP_() | FlagSync_NZ();
	if (RFL.mode & IGNOVF)
		mk = 0xff2b;
	else {
		nf |= FlagSync_O_();
		mk = 0xf72b;
	}
	if (debug_level('e')>1) e_printf("Sync ALL flags = %04x\n", nf);
	CPUWORD(Ofs_FLAGS) = (CPUWORD(Ofs_FLAGS) & mk) | nf;
	RFL.valid = V_INVALID;
}


/////////////////////////////////////////////////////////////////////////////


void InitGen(void)
{
#ifdef HOST_ARCH_X86
	if (!config.cpusim) {
		InitGen_x86();
		return;
	}
#endif
	Gen = Gen_sim;
	AddrGen = AddrGen_sim;
	CloseAndExec = CloseAndExec_sim;
	RFL.S1 = RFL.S2 = RFL.RES.d = 0;
	RFL.valid = V_INVALID;
	InitTrees();
}

/*
 * address generator unit
 * careful - do not use eax, and NEVER change any flag!
 */
static void AddrGen_sim(int op, int mode, ...)
{
	va_list	ap;
#ifdef PROFILE
	hitimer_t t0 = GETTSC();
#endif

	va_start(ap, mode);
	switch(op) {
	case LEA_DI_R:
		if (mode&IMMED)	{
			int o = va_arg(ap,int);
			GTRACE3("LEA_DI_R IMM",0xff,0xff,o);
			AR1.d += o;
		}
		else {
			signed char o = Offs_From_Arg();
			GTRACE1("LEA_DI_R",o);
			AR1.d = (long)(CPUOFFS(o));
		}
		break;
	case A_DI_0:			// base(32), imm
	case A_DI_1: {			// base(32), {imm}, reg, {shift}
			long idsp=0;
			char ofs;
			ofs = va_arg(ap,int);
			if (mode & MLEA) {		// discard base	reg
				AR1.d = 0;	// ofs = Ofs_RZERO;
			}
			else AR1.d = CPULONG(ofs);

			if (mode&IMMED)	{
				idsp = va_arg(ap,int);
				if (op==A_DI_0) {
					GTRACE3("A_DI_0",0xff,0xff,idsp);
					if (idsp) { AR1.d += idsp; }
					break;
				}
			}
			if (mode & ADDR16) {
				signed char o = Offs_From_Arg();
				GTRACE3("A_DI_1",o,ofs,idsp);
				TR1.d = CPUWORD(o);
				if ((mode&IMMED) && (idsp!=0)) {
					TR1.w.l += idsp;
				}
			}
			else {
				signed char o = Offs_From_Arg();
				GTRACE3("A_DI_1",o,ofs,idsp);
				TR1.d = CPULONG(o);
				if ((mode&IMMED) && (idsp!=0)) {
					AR1.d += idsp;
				}
			}
			AR1.d += TR1.d;
		}
		break;
	case A_DI_2: {			// base(32), {imm}, reg, reg, {shift}
			long idsp=0;
			char ofs;
			ofs = va_arg(ap,int);
			if (mode & MLEA) {		// discard base	reg
				AR1.d = 0;	// ofs = Ofs_RZERO;
			}
			else AR1.d = CPULONG(ofs);

			if (mode&IMMED)	{
				idsp = va_arg(ap,int);
			}
			if (mode & ADDR16) {
				signed char o1 = Offs_From_Arg();
				signed char o2 = Offs_From_Arg();
				GTRACE4("A_DI_2",o1,ofs,o2,idsp);
				TR1.d = CPUWORD(o1) + CPUWORD(o2);
				if ((mode&IMMED) && (idsp!=0)) {
					TR1.d += idsp;
				}
				AR1.d += TR1.w.l;
			}
			else {
				signed char o1 = Offs_From_Arg();
				signed char o2 = Offs_From_Arg();
				AR1.d += CPULONG(o1);
				TR1.d = CPULONG(o2);
				if (mode & RSHIFT) {
				    unsigned char sh = (unsigned char)(va_arg(ap,int));
				    GTRACE5("A_DI_2",o1,ofs,o2,idsp,sh);
				    if (sh) {
					TR1.d <<= (sh & 0x1f);
				    }
				}
				else {
				    GTRACE4("A_DI_2",o1,ofs,o2,idsp);
				}
				if ((mode&IMMED) && idsp) {
				    AR1.d += idsp;
				}
				AR1.d += TR1.d;
			}
		}
		break;
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
			long idsp;
			if (mode & MLEA) {
				AR1.d = 0;
			}
			else {
				AR1.d = CPULONG(OVERR_DS);
			}
			idsp = va_arg(ap,int);
			if (idsp) {
				AR1.d += idsp;
			}
			if (!(mode & IMMED)) {
				unsigned char sh;
				signed char o = Offs_From_Arg();
				TR1.d = CPULONG(o);
				sh = (unsigned char)(va_arg(ap,int));
				GTRACE4("A_DI_2D",o,0xff,idsp,sh);
				if (sh)	{
				    TR1.d <<= (sh & 0x1f);
				}
				AR1.d += TR1.d;
			}
			else {
				GTRACE3("A_DI_2D",0xff,0xff,idsp);
			}
		}
		break;
	case A_SR_SH4: {	// real mode make base addr from seg
		signed char o = Offs_From_Arg();
		GTRACE1("A_SR_SH4",o);
		SetSegReal(CPUWORD(o), o);
		}
		break;
	}
	va_end(ap);
#ifdef PROFILE
	GenTime += (GETTSC() - t0);
#endif
}

static inline int vga_access(unsigned int m)
{
	return (vga.inst_emu &&
		(unsigned)(m - vga.mem.bank_base) < vga.mem.bank_len);
}

static inline int vga_read_access(unsigned int m)
{
	/* unmapped VGA memory or using a planar mode */
	return (!(TheCPU.mode&RM_REG) && TrapVgaOn &&
		(unsigned)(m - 0xa0000) < 0x20000 &&
		((unsigned)(m - vga.mem.bank_base) >= vga.mem.bank_len ||
		 vga.inst_emu));
}

static inline int vga_write_access(unsigned int m)
{
	/* unmapped VGA memory, VGA BIOS, or a planar mode */
	return (!(TheCPU.mode&RM_REG) && TrapVgaOn &&
		(unsigned)(m - 0xa0000) < 0x20000 + (vgaemu_bios.pages<<12) &&
		((unsigned)(m - vga.mem.bank_base) >= vga.mem.bank_len ||
		 m >= 0xc0000 ||
		 vga.inst_emu));
}

static void Gen_sim(int op, int mode, ...)
{
	int rcod=0;
	va_list	ap;
#ifdef PROFILE
	hitimer_t t0 = GETTSC();
#endif

	va_start(ap, mode);
	switch(op) {
	case L_NOP:
		GTRACE0("L_NOP");
		break;
	// Special case: CR0&0x3f
	case L_CR0:
		GTRACE0("L_CR0");
		DR1.d = CPULONG(Ofs_CR0) & 0x3f;
		break;

	case O_FOP: {
		unsigned char exop = (unsigned char)va_arg(ap,int);
		int reg = va_arg(ap,int);
		GTRACE2("O_FPOP",exop,reg);
		if (Fp87_op(exop, reg)) TheCPU.err = -96;
		}
		break;

	case L_REG: {
		signed char o = Offs_From_Arg();
		if (mode&MBYTE)	{
			GTRACE1("L_REG_BYTE",o);
			DR1.b.bl = CPUBYTE(o);
		}
		else {
			GTRACE1("L_REG_WL",o);
			if ((mode)&DATA16) DR1.w.l = CPUWORD(o);
			else DR1.d = CPULONG(o);
		} }
		break;
	case S_REG: {
		signed char o = Offs_From_Arg();
		if (mode&MBYTE)	{
			GTRACE1("S_REG_BYTE",o);
			CPUBYTE(o) = DR1.b.bl;
		}
		else {
			GTRACE1("S_REG_WL",o);
			if ((mode)&DATA16) CPUWORD(o) = DR1.w.l;
			else CPULONG(o) = DR1.d;
		} }
		break;
	case L_REG2REG: {
		signed char o1 = Offs_From_Arg();
		signed char o2 = Offs_From_Arg();
		GTRACE2("REGtoREG",o1,o2);
		if ((mode) & MBYTE) {
			CPUBYTE(o2) = CPUBYTE(o1);
		} else if ((mode) & DATA16) {
			CPUWORD(o2) = CPUWORD(o1);
		} else {
			CPULONG(o2) = CPULONG(o1);
		} }
		break;
	case S_DI_R: {
		signed char o = Offs_From_Arg();
		GTRACE1("S_DI_R",o);
		if (mode & DATA16)
			CPUWORD(o) = AR1.w.l;
		else
			CPULONG(o) = AR1.d;
		}
		break;
	case S_DI_IMM: {
		int v = va_arg(ap,int);
		if (vga_write_access(AR1.d)) {
			GTRACE0("S_DI_IMM_VGA");
			if (!vga_access(AR1.d)) break;
			e_VgaWrite(AR1.d, v, mode); break;
		}
		if (mode&MBYTE) {
			GTRACE3("S_DI_IMM_B",0xff,0xff,v);
			*AR1.pu = (char)v;
		} else {
			GTRACE3("S_DI_IMM_WL",0xff,0xff,v);
			if ((mode)&DATA16) *AR1.pwu = (short)v;
			else *AR1.pdu = v;
		} }
		break;

	case L_IMM: {
		signed char o = Offs_From_Arg();
		int v = va_arg(ap, int);
		GTRACE3("L_IMM",o,0xff,v);
		if (mode & MBYTE) {
			CPUBYTE(o) = (char)v;
		}
		else if (mode & DATA16) {
			CPUWORD(o) = (short)v;
		}
		else {
			CPULONG(o) = v;
		} }
		break;
	case L_IMM_R1: {
		int v = va_arg(ap, int);
		GTRACE3("L_IMM_R1",0xff,0xff,v);
		if (mode & MBYTE) {
			DR1.b.bl = (char)v;
		}
		else if (mode & DATA16) {
			DR1.w.l = (short)v;
		}
		else {
			DR1.d = v;
		} }
		break;
	case L_MOVZS: {
		signed char o;
		rcod = (va_arg(ap,int)&1)<<3;	// 0=z 8=s
		o = Offs_From_Arg();
		GTRACE3("L_MOVZS",o,0xff,rcod);
		if (vga_read_access(AR1.d)) {
		    if (vga_access(AR1.d))
			DR1.d = e_VgaRead(AR1.d, mode);
		    else
			DR1.d = 0xffffffff;
		    if (rcod) {
			if (mode & MBYTX)
			    DR1.d = DR1.bs.bl;
			else
			    DR1.d = DR1.ws.l;
		    }
		}
		else if (mode & MBYTX) {
		    if (rcod)
			DR1.d = *AR1.ps;
		    else
			DR1.d = *AR1.pu;
		}
		else {
		    if (rcod)
			DR1.d = *AR1.pws;
		    else
			DR1.d = *AR1.pwu;
		}
		if (mode & DATA16) {
			CPUWORD(o) = DR1.w.l;
		}
		else {
			CPULONG(o) = DR1.d;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;

	case L_LXS1: {
		signed char o = Offs_From_Arg();
		GTRACE1("L_LXS1",o);
		if (mode&DATA16) {
		    CPUWORD(o) = DR1.w.l = *AR1.pwu;
		    AR1.d += 2;
		}
		else {
		    CPULONG(o) = DR1.d = *AR1.pdu;
		    AR1.d += 4;
		} }
		break;
	case L_LXS2: {	/* real mode segment base from segment value */
		signed char o = Offs_From_Arg();
		GTRACE1("L_LXS2",o);
		DR1.d = *AR1.pwu;
		SetSegReal(DR1.w.l, o);
		}
		break;
	case L_ZXAX:
		GTRACE0("L_ZXAX");
		DR1.w.h = 0;
		break;

	case L_VGAREAD:
		if (vga_read_access(AR1.d)) {
			GTRACE0("L_VGAREAD");
			if (vga_access(AR1.d))
			    DR1.d = e_VgaRead(AR1.d, mode);
			else
			    DR1.d = 0xffffffff;
			break;
		}
		GTRACE0("L_DI");
		if (mode & MBYTE) {
		    DR1.b.bl = *AR1.pu;
		}
		else if (mode & DATA16) {
		    DR1.w.l = *AR1.pwu;
		}
		else {
		    DR1.d = *AR1.pdu;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		break;
	case L_VGAWRITE:
		if (vga_write_access(AR1.d)) {
			GTRACE0("L_VGAWRITE");
			if (!vga_access(AR1.d)) break;
			e_VgaWrite(AR1.d, DR1.d, mode); break;
		}
		GTRACE0("S_DI");
		if (mode&MBYTE) {
		    *AR1.pu = DR1.b.bl;
		}
		else if (mode & DATA16) {
		    *AR1.pwu = DR1.w.l;
		}
		else {
		    *AR1.pdu = DR1.d;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		break;

	case O_ADD_R: {		// OSZAPC
		register wkreg v;
		v.d = va_arg(ap, int);
		RFL.mode = mode;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_ADD_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADD_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    if (mode & IMMED) RFL.S2 = v.b.bl;
			else RFL.S2 = CPUBYTE(v.bs.bl);
		    DR1.b.bl = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (mode & IMMED) RFL.S2 = v.w.l;
			else RFL.S2 = CPUWORD(v.bs.bl);
		    DR1.w.l = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else {
		    RFL.S1 = DR1.d;
		    if (mode & IMMED) RFL.S2 = v.d;
			else RFL.S2 = CPULONG(v.bs.bl);
		    DR1.d = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		FlagSync_C(0);
		}
		break;
	case O_OR_R: {		// O=0 SZP C=0
		int v = va_arg(ap, int);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_OR_R",0xff,0xff,v);}
		    else {GTRACE3("O_OR_R",v,0xff,v);}
		if (mode & IMMED) RFL.S2 = v;
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    if (!(mode & IMMED)) RFL.S2 = CPUBYTE(v);
		    DR1.b.bl = RFL.RES.d = RFL.S1 | RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (!(mode & IMMED)) RFL.S2 = CPUWORD(v);
		    DR1.w.l = RFL.RES.d = RFL.S1 | RFL.S2;
		}
		else {
		    RFL.S1 = DR1.d;
		    if (!(mode & IMMED)) RFL.S2 = CPULONG(v);
		    DR1.d = RFL.RES.d = RFL.S1 | RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		SET_CF(0);
		}
		break;
	case O_AND_R: {		// O=0 SZP C=0
		int v = va_arg(ap, int);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_AND_R",0xff,0xff,v);}
		    else {GTRACE3("O_AND_R",v,0xff,v);}
		if (mode & IMMED) RFL.S2 = v;
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    if (!(mode & IMMED)) RFL.S2 = CPUBYTE(v);
		    DR1.b.bl = RFL.RES.d = RFL.S1 & RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (!(mode & IMMED)) RFL.S2 = CPUWORD(v);
		    DR1.w.l = RFL.RES.d = RFL.S1 & RFL.S2;
		}
		else {
		    RFL.S1 = DR1.d;
		    if (!(mode & IMMED)) RFL.S2 = CPULONG(v);
		    DR1.d = RFL.RES.d = RFL.S1 & RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		SET_CF(0);
		}
		break;
	case O_XOR_R: {		// O=0 SZP C=0
		int v = va_arg(ap, int);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_XOR_R",0xff,0xff,v);}
		    else {GTRACE3("O_XOR_R",v,0xff,v);}
		if (mode & IMMED) RFL.S2 = v;
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    if (!(mode & IMMED)) RFL.S2 = CPUBYTE(v);
		    DR1.b.bl = RFL.RES.d = RFL.S1 ^ RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (!(mode & IMMED)) RFL.S2 = CPUWORD(v);
		    DR1.w.l = RFL.RES.d = RFL.S1 ^ RFL.S2;
		}
		else {
		    RFL.S1 = DR1.d;
		    if (!(mode & IMMED)) RFL.S2 = CPULONG(v);
		    DR1.d = RFL.RES.d = RFL.S1 ^ RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		SET_CF(0);
		}
		break;
	case O_SUB_R: {		// OSZAPC
		register wkreg v;
		v.d = va_arg(ap, int);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & IMMED) {GTRACE3("O_SUB_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_SUB_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    if (mode & IMMED) RFL.S2 = -v.b.bl;
			else RFL.S2 = -(CPUBYTE(v.bs.bl));
		    DR1.b.bl = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (mode & IMMED) RFL.S2 = -v.w.l;
			else RFL.S2 = -(CPUWORD(v.bs.bl));
		    DR1.w.l = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else {
		    RFL.S1 = DR1.d;
		    if (mode & IMMED) RFL.S2 = -v.d;
			else RFL.S2 = -(CPULONG(v.bs.bl));
		    DR1.d = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		FlagSync_C(1);
		}
		break;
	case O_CMP_R: {		// OSZAPC
		register wkreg v;
		v.d = va_arg(ap, int);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & IMMED) {GTRACE3("O_CMP_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_CMP_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    if (mode & IMMED) RFL.S2 = -v.b.bl;
			else RFL.S2 = -(CPUBYTE(v.bs.bl));
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (mode & IMMED) RFL.S2 = -v.w.l;
			else RFL.S2 = -(CPUWORD(v.bs.bl));
		}
		else {
		    RFL.S1 = DR1.d;
		    if (mode & IMMED) RFL.S2 = -v.d;
			else RFL.S2 = -(CPULONG(v.bs.bl));
		}
		RFL.RES.d = RFL.S1 + RFL.S2;
		FlagSync_C(1);
		}
		break;
	case O_ADC_R: {		// OSZAPC
		register wkreg v;
		int cy;
		v.d = va_arg(ap, int);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		RFL.mode = mode;
		RFL.valid = (cy? V_ADC:V_GEN);
		if (mode & IMMED) {GTRACE3("O_ADC_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADC_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    if (mode & IMMED) RFL.S2 = (v.b.bl + cy);
			else RFL.S2 = (CPUBYTE(v.bs.bl) + cy);
		    DR1.b.bl = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (mode & IMMED) RFL.S2 = (v.w.l + cy);
			else RFL.S2 = (CPUWORD(v.bs.bl) + cy);
		    DR1.w.l = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else {
		    RFL.S1 = DR1.d;
		    if (mode & IMMED) RFL.S2 = (v.d + cy);
			else RFL.S2 = (CPULONG(v.bs.bl) + cy);
		    DR1.d = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		FlagSync_C(0);
		}
		break;
	case O_SBB_R: {		// OSZAPC
		register wkreg v;
		int cy;
		v.d = va_arg(ap, int);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		RFL.mode = mode;
		RFL.valid = V_SBB;
		if (mode & IMMED) {GTRACE3("O_SBB_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_SBB_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.S1 = DR1.b.bl;
		    // movzbl v.b.bl->eax; add cy,eax; neg eax
		    if (mode & IMMED) RFL.S2 = -(v.b.bl + cy);
			else RFL.S2 = -(CPUBYTE(v.bs.bl) + cy);
		    DR1.b.bl = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = DR1.w.l;
		    if (mode & IMMED) RFL.S2 = -(v.w.l + cy);
			else RFL.S2 = -(CPUWORD(v.bs.bl) + cy);
		    DR1.w.l = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else {
		    RFL.S1 = DR1.d;
		    if (mode & IMMED) RFL.S2 = -(v.d + cy);
			else RFL.S2 = -(CPULONG(v.bs.bl) + cy);
		    DR1.d = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		FlagSync_C(1);
		}
		break;
	case O_CLEAR: {		// == XOR r,r
		signed char o = Offs_From_Arg();
		GTRACE1("O_CLEAR",o);
		if (mode & MBYTE) {
		    CPUBYTE(o) = 0;
		}
		else if (mode & DATA16) {
		    CPUWORD(o) = 0;
		}
		else {
		    CPULONG(o) = 0;
		}
		CPUWORD(Ofs_FLAGS) = (CPUWORD(Ofs_FLAGS) & 0x7700) | 0x46;
		RFL.valid = V_INVALID;
		}
		break;
	case O_TEST: {		// == OR r,r
		signed char o = Offs_From_Arg();
		GTRACE1("O_TEST",o);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & MBYTE) {
		    RFL.RES.d = CPUBYTE(o);
		}
		else if (mode & DATA16) {
		    RFL.RES.d = CPUWORD(o);
		}
		else {
		    RFL.RES.d = CPULONG(o);
		}
		SET_CF(0);
		}
		break;
	case O_SBSELF: {
		signed char o = Offs_From_Arg();
		GTRACE1("O_SBBSELF",o);
		// if CY=0 -> reg=0,  flag=xx46, OF=0
		// if CY=1 -> reg=-1, flag=xx97, OF=0
		if (CPUBYTE(Ofs_FLAGS)&1) {
		    RFL.RES.d = 0xffffffff;
		    CPUWORD(Ofs_FLAGS) = (CPUWORD(Ofs_FLAGS) & 0x7700) | 0x97;
		}
		else {
		    RFL.RES.d = 0;
		    CPUWORD(Ofs_FLAGS) = (CPUWORD(Ofs_FLAGS) & 0x7700) | 0x46;
		}
		if (mode & MBYTE) {
		    CPUBYTE(o) = RFL.RES.b.bl;
		}
		else if (mode & DATA16) {
		    CPUWORD(o) = RFL.RES.w.l;
		}
		else {
		    CPULONG(o) = RFL.RES.d;
		}
		RFL.valid = V_INVALID;
		}
		break;
	case O_INC_R: {		// OSZAP
		signed char o = Offs_From_Arg();
		GTRACE1("O_INC_R",o);
		RFL.mode = mode;
		RFL.valid = V_GEN;
		RFL.S2 = 1;
		if (mode & MBYTE) {
		    RFL.S1 = CPUBYTE(o);
		    CPUBYTE(o) = RFL.RES.d = RFL.S1 + 1;
		}
		else if (mode & DATA16) {
		    RFL.S1 = CPUWORD(o);
		    CPUWORD(o) = RFL.RES.d = RFL.S1 + 1;
		}
		else {
		    RFL.S1 = CPULONG(o);
		    CPULONG(o) = RFL.RES.d = RFL.S1 + 1;
		}
		}
		break;
	case O_DEC_R: {		// OSZAP
		signed char o = Offs_From_Arg();
		GTRACE1("O_DEC_R",o);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		RFL.S2 = -1;
		if (mode & MBYTE) {
		    RFL.S1 = CPUBYTE(o);
		    CPUBYTE(o) = RFL.RES.d = RFL.S1 - 1;
		}
		else if (mode & DATA16) {
		    RFL.S1 = CPUWORD(o);
		    CPUWORD(o) = RFL.RES.d = RFL.S1 - 1;
		}
		else {
		    RFL.S1 = CPULONG(o);
		    CPULONG(o) = RFL.RES.d = RFL.S1 - 1;
		}
		}
		break;
	case O_SBB_FR: {	// OSZAPC
		register wkreg v;
		signed char o;
		int cy;
		v.d = va_arg(ap, int);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		RFL.mode = mode;
		RFL.valid = V_SBB;
		if (mode & IMMED) {GTRACE3("O_SBB_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_SBB_FR",v.bs.bl,0xff,v.d);}
		o = (mode & IMMED) ? Ofs_EAX : v.bs.bl;
		if (mode & MBYTE) {
		    RFL.S1 = CPUBYTE(o);
		    // movzbl v.bs.bl->eax; add cy,eax; neg eax
		    if (mode & IMMED) RFL.S2 = -(v.b.bl + cy);
			else RFL.S2 = -(DR1.b.bl + cy);
		    CPUBYTE(o) = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = CPUWORD(o);
		    if (mode & IMMED) RFL.S2 = -(v.w.l + cy);
			else RFL.S2 = -(DR1.w.l + cy);
		    CPUWORD(o) = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else {
		    RFL.S1 = CPULONG(o);
		    if (mode & IMMED) RFL.S2 = -(v.d + cy);
			else RFL.S2 = -(DR1.d + cy);
		    CPULONG(o) = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		FlagSync_C(1);
		}
		break;
	case O_SUB_FR: {	// OSZAPC
		register wkreg v;
		signed char o;
		v.d = va_arg(ap, int);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & IMMED) {GTRACE3("O_SUB_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_SUB_FR",v.bs.bl,0xff,v.d);}
		o = (mode & IMMED) ? Ofs_EAX : v.b.bl;
		if (mode & MBYTE) {
		    RFL.S1 = CPUBYTE(o);
		    if (mode & IMMED) RFL.S2 = -v.b.bl;
			else RFL.S2 = -DR1.b.bl;
		    CPUBYTE(o) = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else if (mode & DATA16) {
		    RFL.S1 = CPUWORD(o);
		    if (mode & IMMED) RFL.S2 = -v.w.l;
			else RFL.S2 = -DR1.w.l;
		    CPUWORD(o) = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		else {
		    RFL.S1 = CPULONG(o);
		    if (mode & IMMED) RFL.S2 = -v.d;
			else RFL.S2 = -DR1.d;
		    CPULONG(o) = RFL.RES.d = RFL.S1 + RFL.S2;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		FlagSync_C(1);
		}
		break;
	case O_CMP_FR: {	// OSZAPC
		register wkreg v;
		signed char o;
		v.d = va_arg(ap, int);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & IMMED) {GTRACE3("O_CMP_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_CMP_FR",v.bs.bl,0xff,v.d);}
		o = (mode & IMMED) ? Ofs_EAX : v.b.bl;
		if (mode & MBYTE) {
		    RFL.S1 = CPUBYTE(o);
		    if (mode & IMMED) RFL.S2 = -v.b.bl;
			else RFL.S2 = -DR1.b.bl;
		}
		else if (mode & DATA16) {
		    RFL.S1 = CPUWORD(o);
		    if (mode & IMMED) RFL.S2 = -v.w.l;
			else RFL.S2 = -DR1.w.l;
		}
		else {
		    RFL.S1 = CPULONG(o);
		    if (mode & IMMED) RFL.S2 = -v.d;
			else RFL.S2 = -DR1.d;
		}
		RFL.RES.d = RFL.S1 + RFL.S2;
		FlagSync_C(1);
		}
		break;
	case O_NOT:		// no flags
		GTRACE0("O_NOT");
		if (mode & MBYTE) {
			DR1.b.bl = ~DR1.b.bl;
		}
		else if (mode & DATA16) {
			DR1.w.l = ~DR1.w.l;
		}
		else {
			DR1.d = ~DR1.d;
		}
		break;
	case O_NEG:		// OSZAPC
		GTRACE0("O_NEG");
		RFL.mode = mode;
		RFL.valid = V_SUB;
		RFL.S1 = 0;
		if (mode & MBYTE) {
			DR1.b.bl = RFL.RES.d = -(RFL.S2=DR1.b.bl);
		}
		else if (mode & DATA16) {
			DR1.w.l = RFL.RES.d = -(RFL.S2=DR1.w.l);
		}
		else {
			DR1.d = RFL.RES.d = -(RFL.S2=DR1.d);
		}
		SET_CF(RFL.S2!=0);
		break;
	case O_INC:		// OSZAP
		GTRACE0("O_INC");
		RFL.mode = mode;
		RFL.valid = V_GEN;
		RFL.S2 = 1;
		if (mode & MBYTE) {
			RFL.S1 = DR1.bs.bl;
			DR1.b.bl = RFL.RES.d = RFL.S1 + 1;
		}
		else if (mode & DATA16) {
			RFL.S1 = DR1.ws.l;
			DR1.w.l = RFL.RES.d = RFL.S1 + 1;
		}
		else {
			RFL.S1 = DR1.d;
			DR1.d = RFL.RES.d = RFL.S1 + 1;
		}
		break;
	case O_DEC:		// OSZAP
		GTRACE0("O_DEC");
		RFL.mode = mode;
		RFL.valid = V_SUB;
		RFL.S2 = -1;
		if (mode & MBYTE) {
			RFL.S1 = DR1.bs.bl;
			DR1.b.bl = RFL.RES.d = RFL.S1 - 1;
		}
		else if (mode & DATA16) {
			RFL.S1 = DR1.ws.l;
			DR1.w.l = RFL.RES.d = RFL.S1 - 1;
		}
		else {
			RFL.S1 = DR1.d;
			DR1.d = RFL.RES.d = RFL.S1 - 1;
		}
		break;
	case O_XCHG: {
		signed char o = Offs_From_Arg();
		GTRACE1("O_XCHG",o);
		if (mode & MBYTE) {
			DR2.b.bl = DR1.b.bl;
			DR1.b.bl = CPUBYTE(o);
			CPUBYTE(o) = DR2.b.bl;
		}
		else if (mode & DATA16) {
			DR2.w.l = DR1.w.l;
			DR1.w.l = CPUWORD(o);
			CPUWORD(o) = DR2.w.l;
		}
		else {
			DR2.d = DR1.d;
			DR1.d = CPULONG(o);
			CPULONG(o) = DR2.d;
		} }
		break;
	case O_XCHG_R: {
		signed char o1 = Offs_From_Arg();
		signed char o2 = Offs_From_Arg();
		GTRACE2("O_XCHG_R",o1,o2);
		if (mode & MBYTE) {
			DR1.b.bl = CPUBYTE(o1);
			CPUBYTE(o1) = CPUBYTE(o2);
			CPUBYTE(o2) = DR1.b.bl;
		}
		else if (mode & DATA16) {
			DR1.w.l = CPUWORD(o1);
			CPUWORD(o1) = CPUWORD(o2);
			CPUWORD(o2) = DR1.w.l;
		}
		else {
			DR1.d = CPULONG(o1);
			CPULONG(o1) = CPULONG(o2);
			CPULONG(o2) = DR1.d;
		} }
		break;

	case O_MUL: {		// OC
		int of;
		GTRACE0("O_MUL");
		RFL.mode = mode;
		RFL.valid = V_GEN;
		if (mode & MBYTE) {
		    DR1.w.l =
			(unsigned int)CPUBYTE(Ofs_AL) * (unsigned int)DR1.b.bl;
		    CPUWORD(Ofs_AX) = DR1.w.l;
		    of = (DR1.b.bh != 0);
		}
		else if (mode&DATA16) {
		    DR1.d =
			(unsigned int)CPUWORD(Ofs_AX) * (unsigned int)DR1.w.l;
		    CPUWORD(Ofs_AX) = DR1.w.l;
		    CPUWORD(Ofs_DX) = DR1.w.h;
		    of = (DR1.w.h != 0);
		}
		else {
		    u_int64_u v;
		    v.td = (u_int64_t)CPULONG(Ofs_EAX) * (u_int64_t)DR1.d;
		    CPULONG(Ofs_EAX) = v.t.tl;
		    CPULONG(Ofs_EDX) = v.t.th;
		    of = (v.t.th != 0);
		}
		if (of) {
		    SET_CF(1);
		    RFL.mode |= SETOVF;
		}
		else {
		    SET_CF(0);
		    RFL.mode |= CLROVF;
		} }
		break;
	case O_IMUL: {		// OC
		int of;
		RFL.mode = mode;
		RFL.valid = V_GEN;
		if (mode & MBYTE) {
		    if ((mode&(IMMED|DATA16))==(IMMED|DATA16)) {
			int b = va_arg(ap, int);
			signed char o = Offs_From_Arg();
			GTRACE3("O_IMUL",o,0xff,b);
			DR1.ds = (int)DR1.ws.l * b;
			CPUWORD(o) = DR1.w.l;
			DR1.ds >>= 15;
			of = ((DR1.ds!=0) && (DR1.ds!=-1));
		    }
		    else if ((mode&(IMMED|DATA16))==IMMED) {
			int b = va_arg(ap, int);
			signed char o = Offs_From_Arg();
			int64_t v;
			GTRACE3("O_IMUL",o,0xff,b);
			v = (int64_t)DR1.ds * b;
			CPULONG(o) = v & 0xffffffff;
			v >>= 31;
			of = ((v != 0) && (v != -1));
		    }
		    else {
		    	GTRACE0("O_IMUL");
			DR1.ds =
			    (int)(signed char)CPUBYTE(Ofs_AL) * (int)DR1.bs.bl;
			CPUWORD(Ofs_AX) = DR1.w.l;
			DR1.ds >>= 7;	// AX sign extend of AL?
			of = ((DR1.ds!=0) && (DR1.ds!=-1));
		    }
		}
		else if (mode&DATA16) {
		    if (mode&IMMED) {
			int b = va_arg(ap, int);
			signed char o = Offs_From_Arg();
			GTRACE3("O_IMUL",o,0xff,b);
		    	DR1.ds = (int)DR1.ws.l * b;
		    	CPUWORD(o) = DR1.w.l;
		    }
		    else if (mode&MEMADR) {
			signed char o = Offs_From_Arg();
			GTRACE1("O_IMUL",o);
			DR1.ds = (int)(signed short)CPUWORD(o) * (int)DR1.ws.l;
			CPUWORD(o) = DR1.w.l;
		    }
		    else {
		    	GTRACE0("O_IMUL");
			DR1.ds =
			    (int)(signed short)CPUWORD(Ofs_AX) * (int)DR1.ws.l;
			CPUWORD(Ofs_AX) = DR1.w.l;
			CPUWORD(Ofs_DX) = DR1.w.h;
		    }
		    DR1.ds >>= 15;	// DX:AX sign extend of AX?
		    of = ((DR1.ds!=0) && (DR1.ds!=-1));
		}
		else {
		    int64_t v;
		    if (mode&IMMED) {
			int b = va_arg(ap, int);
			signed char o = Offs_From_Arg();
			GTRACE3("O_IMUL",o,0xff,b);
			v = (int64_t)DR1.ds * b;
			CPULONG(o) = v & 0xffffffff;
		    }
		    else if (mode&MEMADR) {
			signed char o = Offs_From_Arg();
			int vd = CPULONG(o);
			GTRACE1("O_IMUL",o);
			v = (int64_t)DR1.ds * (int64_t)vd;
			CPULONG(o) = v & 0xffffffff;
		    }
		    else {
			int vd = CPULONG(Ofs_EAX);
		    	GTRACE0("O_IMUL");
			v = (int64_t)vd * (int64_t)DR1.ds;
			CPULONG(Ofs_EAX) = v & 0xffffffff;
			CPULONG(Ofs_EDX) = (v >> 32) & 0xffffffff;
		    }
		    v >>= 31;		// EDX:EAX sign extend of EAX?
		    of = ((v != 0) && (v != -1));
		}
		if (of) {
		    SET_CF(1);
		    RFL.mode |= SETOVF;
		}
		else {
		    SET_CF(0);
		    RFL.mode |= CLROVF;
		} }
		break;

	case O_DIV:		// no flags
		GTRACE0("O_DIV");
		if (mode & MBYTE) {
			RFL.RES.w.l = CPUWORD(Ofs_AX);
			RFL.RES.w.h = 0;
			/* exception trap: save current PC */
			CPULONG(Ofs_CR2) = va_arg(ap,int);
			RFL.S1 = DR1.b.bl;
			if (RFL.S1==0)
			    TheCPU.err = EXCP00_DIVZ;
			else {
			    CPUBYTE(Ofs_AL) = RFL.RES.d / RFL.S1;
			    CPUBYTE(Ofs_AH) = RFL.RES.d % RFL.S1;
			}
		}
		else {
			if (mode&DATA16) {
				RFL.RES.w.l = CPUWORD(Ofs_AX);
				RFL.RES.w.h = CPUWORD(Ofs_DX);
				/* exception trap: save current PC */
				CPULONG(Ofs_CR2) = va_arg(ap,int);
				RFL.S1 = DR1.w.l;
				if (RFL.S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    CPUWORD(Ofs_AX) = RFL.RES.d / RFL.S1;
				    CPUWORD(Ofs_DX) = RFL.RES.d % RFL.S1;
				}
			}
			else {
				u_int64_u v;
				unsigned long rem;
				v.t.tl = CPULONG(Ofs_EAX);
				v.t.th = CPULONG(Ofs_EDX);
				/* exception trap: save current PC */
				CPULONG(Ofs_CR2) = va_arg(ap,int);
				RFL.S1 = DR1.d;
				if (RFL.S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    rem = v.td % (unsigned)RFL.S1;
				    v.td /= (unsigned)RFL.S1;
				    // if (v.t.th) excp(div_by_zero);
				    CPULONG(Ofs_EAX) = RFL.RES.d = v.t.tl;
				    CPULONG(Ofs_EDX) = rem;
		    		}
			}
		}
		break;
	case O_IDIV:		// no flags
		GTRACE0("O_IDIV");
		if (mode & MBYTE) {
			RFL.RES.ds = (signed short)CPUWORD(Ofs_AX);
			/* exception trap: save current PC */
			CPULONG(Ofs_CR2) = va_arg(ap,int);
			RFL.S1 = DR1.bs.bl;
			if (RFL.S1==0)
			    TheCPU.err = EXCP00_DIVZ;
	    		else {
			    CPUBYTE(Ofs_AL) = RFL.RES.ds / RFL.S1;
			    CPUBYTE(Ofs_AH) = RFL.RES.ds % RFL.S1;
			}
		}
		else {
			if (mode&DATA16) {
				RFL.RES.w.l = CPUWORD(Ofs_AX);
				RFL.RES.w.h = CPUWORD(Ofs_DX);
				/* exception trap: save current PC */
				CPULONG(Ofs_CR2) = va_arg(ap,int);
				RFL.S1 = DR1.ws.l;
				if (RFL.S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    CPUWORD(Ofs_AX) = RFL.RES.ds / RFL.S1;
				    CPUWORD(Ofs_DX) = RFL.RES.ds % RFL.S1;
		    		}
			}
			else {
				int64_t v;
				long rem;
				v = CPULONG(Ofs_EAX) |
				  ((int64_t)CPULONG(Ofs_EDX) << 32);
				/* exception trap: save current PC */
				CPULONG(Ofs_CR2) = va_arg(ap,int);
				RFL.S1 = DR1.d;
				if (RFL.S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    rem = v % RFL.S1;
				    v /= RFL.S1;
				    // if ((((unsigned)(v>>32)+1)&(-2))!=0) excp(div_by_zero);
				    CPULONG(Ofs_EAX) = RFL.RES.d = v & 0xffffffff;
				    CPULONG(Ofs_EDX) = rem;
		    		}
			}
		}
		break;
	case O_CBWD:
		GTRACE0("O_CBWD");
		DR1.d = CPULONG(Ofs_EAX);
		if (mode & MBYTE) {		/* 0x98: CBW,CWDE */
			if (mode & DATA16) {	// AL->AX
				// cbw
				CPUBYTE(Ofs_AH) = (DR1.b.bl&0x80? 0xff:0);
			}
			else {			// AX->EAX
				// cwde
				CPUWORD(Ofs_AXH) = (DR1.b.bh&0x80? 0xffff:0);
			}
		}
		else if	(mode &	DATA16)	{	/* 0x99: AX->DX:AX */
			// cwd
			CPUWORD(Ofs_DX) = (DR1.b.bh&0x80? 0xffff:0);
		}
		else {				/* 0x99: EAX->EDX:EAX */
			// cdq
			CPULONG(Ofs_EDX) = (DR1.b.b3&0x80? 0xffffffff:0);
		}
		break;
	case O_XLAT:
		GTRACE0("XLAT");
		AR1.d = CPULONG(OVERR_DS);
		if (mode & DATA16) {
			TR1.d = CPUWORD(Ofs_BX);
		}
		else {
			TR1.d = CPULONG(Ofs_EBX);
		}
		AR1.d += TR1.d;
		TR1.d = CPUBYTE(Ofs_AL);
		AR1.d += TR1.d;
		CPUBYTE(Ofs_AL) = DR1.b.bl = *AR1.pu;
		break;

	case O_ROL: {		// O(if sh==1),C(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, rbef, raft, cy, ov;
		GTRACE1("O_ROL",o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if(!sh)
			break;	// Carry unmodified, Overflow undefined.

		if (mode & MBYTE) {
			sh &= 7;
			rbef = *AR1.pu;
			raft = (rbef<<sh) | (rbef>>(8-sh));
			*AR1.pu = raft;
			cy = raft & 1;
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh &= 15;
			rbef = *AR1.pwu;
			raft = (rbef<<sh) | (rbef>>(16-sh));
			*AR1.pwu = raft;
			cy = raft & 1;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			// sh != 0, else we "break"ed above.
			// sh < 32, so 32-sh is in range 1..31, which is OK.
			rbef = *AR1.pdu;
			raft = (rbef<<sh) | (rbef>>(32-sh));
			*AR1.pdu = raft;
			cy = raft & 1;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		e_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1)
			RFL.mode |= IGNOVF;
		else
			SET_OF(ov);
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_RCL: {		// O(if sh==1),C(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, rbef, raft, cy, ov;
		GTRACE1("O_RCL",o);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if(!sh)
			break;
			
		if (mode & MBYTE) {
			sh %= 9;
			rbef = *AR1.pu;
			raft = (rbef<<sh) | (rbef>>(9-sh)) | (cy<<(sh-1));
			*AR1.pu = raft;
			if (sh)
				cy = (rbef>>(8-sh)) & 1;
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh %= 17;
			rbef = *AR1.pwu;
			raft = (rbef<<sh) | (rbef>>(17-sh)) | (cy<<(sh-1));
			*AR1.pwu = raft;
			if (sh) 
				cy = (rbef>>(16-sh)) & 1;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			rbef = *AR1.pdu;
			raft = (rbef<<sh) | (cy<<(sh-1));
			if (sh>1) raft |= (rbef>>(33-sh));
			*AR1.pdu = raft;
			if(sh)
				cy = (rbef>>(32-sh)) & 1;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		e_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1)
			RFL.mode |= IGNOVF;
		else
			SET_OF(ov);
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_SHL: {		// O(if sh==1),SZPC(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, rbef, raft=0, cy=0;
		GTRACE3("O_SHL",0xff,0xff,o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;	// happens on 286 and above, not on 8086
		if(!sh)
			// All flags unchanged (at least on PIII)
			break;
			
		RFL.mode = mode;
		RFL.valid = V_GEN;
		if (mode & MBYTE) 
			rbef = *AR1.pu;
		else if (mode & DATA16)
			rbef = *AR1.pwu;
		else
			rbef = *AR1.pdu;
		
		// To simulate overflow flag. Keep in mind it is only defined
		// for sh==1. In that case, SHL r/m,1 behaves like ADD r/m,r/m.
		// So flag state gets set up like that
		RFL.S1 = RFL.S2 = rbef;
		raft = (rbef << sh);
		RFL.RES.d = raft;
			
		if (mode & MBYTE) {
			cy = (raft & 0x100) != 0;
			*AR1.pu = raft;
		}
		else if (mode & DATA16) {
			cy = (raft & 0x10000) != 0;
			*AR1.pwu = raft;
		}
		else {
			cy = (rbef>>(32-sh)) & 1;
			*AR1.pdu = raft;
		}
		RFL.RES.d = raft;
		e_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1) RFL.mode |= IGNOVF;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_ROR: {		// O(if sh==1),C(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, rbef, raft, cy, ov;
		GTRACE1("O_ROL",o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if(!sh)
			break;	// Carry unmodified, Overflow undefined.

		if (mode & MBYTE) {
			sh &= 7;
			RFL.S1 = RFL.S2 = rbef = *AR1.pu;
			raft = (rbef>>sh) | (rbef<<(8-sh));
			*AR1.pu = RFL.RES.d = raft;
			cy = (raft & 0x80) != 0;
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh &= 15;
			rbef = *AR1.pwu;
			raft = (rbef>>sh) | (rbef<<(16-sh));
			*AR1.pwu = raft;
			cy = (raft & 0x8000) != 0;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			// sh != 0, else we "break"ed above.
			// sh < 32, so 32-sh is in range 1..31, which is OK.
			rbef = *AR1.pdu;
			raft = (rbef>>sh) | (rbef<<(32-sh));
			*AR1.pdu = raft;
			cy = (raft & 0x80000000U) != 0;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		e_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1)
			RFL.mode |= IGNOVF;
		else
			SET_OF(ov);
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_RCR: {		// O(if sh==1),C(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, rbef, raft, cy, ov;
		GTRACE3("O_RCR",0xff,0xff,o);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if(!sh)
			break;	// Carry unmodified, Overflow undefined.

		if (mode & MBYTE) {
			sh %= 9;
			RFL.S1 = RFL.S2 = rbef = *AR1.pu;
			raft = (rbef>>sh) | (rbef<<(9-sh)) | (cy<<(8-sh));
			*AR1.pu = RFL.RES.d = raft;
			if(sh)
				cy = (rbef>>(sh-1)) & 1;
			// else keep carry.
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh %= 17;
			RFL.S1 = RFL.S2 = rbef = *AR1.pwu;
			raft = (rbef>>sh) | (rbef<<(17-sh)) | (cy<<(16-sh));
			*AR1.pwu = RFL.RES.d = raft;
			if(sh)
				cy = (rbef>>(sh-1)) & 1;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			RFL.S1 = RFL.S2 = rbef = *AR1.pdu;
			raft = (rbef>>sh) | (cy<<(32-sh));
			if (sh>1) raft |= (rbef<<(33-sh));
			*AR1.pdu = RFL.RES.d = raft;
			cy = (rbef>>(sh-1)) & 1;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		e_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1)
			RFL.mode |= IGNOVF;
		else
			SET_OF(ov);
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_SHR: {		// O(if sh==1),SZPC(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, rbef, raft, cy=0;
		GTRACE3("O_SHR",0xff,0xff,o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if (!sh)
		{
			ClearOF();
			break;	// shift count 0, flags unchanged, except OVFL
		}
		  
		RFL.mode = mode;
		RFL.valid = V_GEN;
		if (mode & MBYTE) 
			rbef = *AR1.pu;
		else if (mode & DATA16)
			rbef = *AR1.pwu;
		else
			rbef = *AR1.pdu;

		RFL.S1 = RFL.S2 = rbef;
		cy = (rbef >> (sh-1)) & 1;
		raft = rbef >> sh;
		RFL.RES.d = raft;
		
		if (mode & MBYTE)
			*AR1.pu = raft;
		else if (mode & DATA16) 
			*AR1.pwu = raft;
		else
			*AR1.pdu = raft;

		e_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1) RFL.mode |= CLROVF;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_SAR: {		// O(if sh==1),SZPC(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, cy=0;
		signed int rbef, raft=0;
		GTRACE3("O_SHR",0xff,0xff,o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if (!sh)
		{
			ClearOF();
			break;	// shift count 0, flags unchanged, except OVFL
		}
		  
		RFL.mode = mode;
		RFL.valid = V_GEN;
		if (mode & MBYTE) 
			rbef = *AR1.ps;
		else if (mode & DATA16)
			rbef = *AR1.pws;
		else
			rbef = *AR1.pds;

		RFL.S1 = RFL.S2 = rbef;
		cy = (rbef >> (sh-1)) & 1;
		raft = rbef >> sh;
		RFL.RES.d = raft;
		
		if (mode & MBYTE)
			*AR1.ps = raft;
		else if (mode & DATA16) 
			*AR1.pws = raft;
		else
			*AR1.pds = raft;

		e_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1) RFL.mode |= CLROVF;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;

	case O_OPAX: {	/* used by DAA..AAD */
		int n =	va_arg(ap,int);
		// get n bytes from parameter stack
		unsigned char subop = Offs_From_Arg();
		GTRACE3("O_OPAX",0xff,0xff,n);
		RFL.mode = mode;
		/* sync AF *before* changing RFL.valid */
		if (subop != AAM && subop != AAD)
			FlagSync_AP();
		RFL.valid = V_GEN;
		DR1.d = CPULONG(Ofs_EAX);
		switch (subop) {
			case DAA: {
				char cy = 0;
				unsigned char altmp = DR1.b.bl;
				if (((DR1.b.bl & 0x0f) > 9 ) || (IS_AF_SET)) {
					DR1.b.bl += 6;
					cy = (CPUBYTE(Ofs_FLAGS)&1) || (altmp > 0xf9);
					RFL.S1 |= 8; RFL.S2 |= 8; // SET_AF (8+8 gives AF)
				} else {
					RFL.S1 &= 0xf7; RFL.S2 &= 0xf7; // CLR_AF
				}
				if ((altmp > 0x99) || (IS_CF_SET)) {
					DR1.b.bl += 0x60;
					cy = 1;
				}
				SET_CF(cy);
				RFL.RES.d = DR1.bs.bl; /* for flags */
				}
				break;
			case DAS: {
				char cy = 0;
				unsigned char altmp = DR1.b.bl;
				if (((altmp & 0x0f) > 9) || (IS_AF_SET)) {
					DR1.b.bl -= 6;
					cy = (CPUBYTE(Ofs_FLAGS)&1) || (altmp < 6);
					RFL.S1 |= 8; RFL.S2 |= 8; // SET_AF
				} else {
					RFL.S1 &= 0xf7; RFL.S2 &= 0xf7; // CLR_AF
				}
				if ((altmp > 0x99) || (IS_CF_SET)) {
					DR1.b.bl -= 0x60;
					cy = 1;
				}
				SET_CF(cy);
				RFL.RES.d = DR1.bs.bl; /* for flags */
				}
				break;
			case AAA: {
				char icarry = (DR1.b.bl > 0xf9);
				if (((DR1.b.bl & 0x0f) > 9 ) || (IS_AF_SET)) {
					DR1.b.bl = (DR1.b.bl + 6) & 0x0f;
					DR1.b.bh = (DR1.b.bh + 1 + icarry);
					RFL.S1 |= 8; RFL.S2 |= 8; // SET_AF
					SET_CF(1);
				} else {
					RFL.S1 &= 0xf7; RFL.S2 &= 0xf7; // CLR_AF
					SET_CF(0);
					DR1.b.bl &= 0x0f;
				} }
				break;
			case AAS: {
				char icarry = (DR1.b.bl < 6);
				if (((DR1.b.bl & 0x0f) > 9 ) || (IS_AF_SET)) {
					DR1.b.bl = (DR1.b.bl - 6) & 0x0f;
					DR1.b.bh = (DR1.b.bh - 1 - icarry);
					RFL.S1 |= 8; RFL.S2 |= 8; // SET_AF
					SET_CF(1);
				} else {
					RFL.S1 &= 0xf7; RFL.S2 &= 0xf7; // CLR_AF
					SET_CF(0);
					DR1.b.bl &= 0x0f;
				} }
				break;
			case AAM: {
				char tmp = DR1.b.bl;
				int base = Offs_From_Arg();
				DR1.b.bh = tmp / base;
				RFL.RES.d = DR1.bs.bl = tmp % base;
				}
				break;
			case AAD: {
				int base = Offs_From_Arg();
				DR1.w.l = ((DR1.b.bh * base) + DR1.b.bl) & 0xff;
				RFL.RES.d = DR1.bs.bl; /* for flags */
				}
				break;
		}
		CPULONG(Ofs_EAX) = DR1.d;
		}
		break;

	case O_PUSH: {
		signed char o = Offs_From_Arg();
		if (mode&MEMADR) {GTRACE0("O_PUSH");}
		    else  {GTRACE1("O_PUSH",o);}
		if (mode & DATA16) {
			DR1.w.l = (mode&MEMADR? *AR1.pwu : CPUWORD(o));
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 2;
			SR1.d &= CPULONG(Ofs_STACKM);
			*((short *)(uintptr_t)(AR2.d + SR1.d)) = DR1.w.l;
			CPULONG(Ofs_ESP) = SR1.d;
		}
		else {
			long stackm = CPULONG(Ofs_STACKM);
			long tesp;
			DR1.d = (mode&MEMADR? *AR1.pdu : CPULONG(o));
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = (tesp = CPULONG(Ofs_ESP)) - 4;
			SR1.d &= stackm;
			*((int *)(uintptr_t)(AR2.d + SR1.d)) = DR1.d;
#if 0	/* keep high 16-bits of ESP in small-stack mode */
			SR1.d |= (tesp & ~stackm);
#endif
			CPULONG(Ofs_ESP) = SR1.d;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

/* PUSH derived (sub-)sequences: */
	case O_PUSH1:
		GTRACE0("O_PUSH1");
		AR2.d = CPULONG(Ofs_XSS);
		SR1.d = CPULONG(Ofs_ESP);
		break;

	case O_PUSH2: {
		signed char o = Offs_From_Arg();
		GTRACE1("O_PUSH2",o);
		if (mode & DATA16) {
			DR1.w.l = (mode&MEMADR? *AR1.pwu : CPUWORD(o));
			SR1.d -= 2;
			SR1.d &= CPULONG(Ofs_STACKM);
			*((short *)(uintptr_t)(AR2.d + SR1.d)) = DR1.w.l;
		}
		else {
			DR1.d = (mode&MEMADR? *AR1.pdu : CPULONG(o));
			SR1.d -= 4;
			SR1.d &= CPULONG(Ofs_STACKM);
			*((int *)(uintptr_t)(AR2.d + SR1.d)) = DR1.d;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

	case O_PUSH3:
		GTRACE0("O_PUSH3");
		CPULONG(Ofs_ESP) = SR1.d;
		break;

	case O_PUSH2F: {
		int ftmp;
		GTRACE0("O_PUSHF");
		FlagSync_All();
		ftmp = CPULONG(Ofs_EFLAGS);
/*?*/		if (in_dpmi) ftmp = (ftmp & ~0x200) | (get_vFLAGS(TheCPU.eflags) & 0x200);
		AR2.d = CPULONG(Ofs_XSS);
		SR1.d = CPULONG(Ofs_ESP);
		if (mode & DATA16) {
			SR1.d = (SR1.d - 2) & CPULONG(Ofs_STACKM);
			*((short *)(uintptr_t)(AR2.d + SR1.d)) = ftmp & 0x7eff;
		}
		else {
			SR1.d = (SR1.d - 4) & CPULONG(Ofs_STACKM);
			*((int *)(uintptr_t)(AR2.d + SR1.d)) = ftmp & 0x3c7eff;
		}
		CPULONG(Ofs_ESP) = SR1.d;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",ftmp&0x3c7eff);
		} break;

	case O_PUSHI: {
		int v = va_arg(ap, int);
		GTRACE3("O_PUSHI",0xff,0xff,v);
		if (mode & DATA16) {
			DR1.w.l = (short)v;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 2;
			SR1.d &= CPULONG(Ofs_STACKM);
			*((short *)(uintptr_t)(AR2.d + SR1.d)) = DR1.w.l;
			CPULONG(Ofs_ESP) = SR1.d;
		}
		else {
			DR1.d = v;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 4;
			SR1.d &= CPULONG(Ofs_STACKM);
			*((int *)(uintptr_t)(AR2.d + SR1.d)) = DR1.d;
			CPULONG(Ofs_ESP) = SR1.d;
		}
		} break;

	case O_PUSHA: {
		/* push order: eax ecx edx ebx esp ebp esi edi */
		GTRACE0("O_PUSHA");
		if (mode & DATA16) {
			short *pw;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 16;
			SR1.d &= CPULONG(Ofs_STACKM);
			pw = (short *)(uintptr_t)(AR2.d + SR1.d);
			*pw++ = CPUWORD(Ofs_DI);
			*pw++ = CPUWORD(Ofs_SI);
			*pw++ = CPUWORD(Ofs_BP);
			*pw++ = CPUWORD(Ofs_SP);
			*pw++ = CPUWORD(Ofs_BX);
			*pw++ = CPUWORD(Ofs_DX);
			*pw++ = CPUWORD(Ofs_CX);
			*pw++ = CPUWORD(Ofs_AX);
			CPULONG(Ofs_ESP) = SR1.d;
		}
		else {
			int *pd;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 32;
			SR1.d &= CPULONG(Ofs_STACKM);
			pd = (int *)(uintptr_t)(AR2.d + SR1.d);
			memcpy(pd, CPUOFFS(Ofs_EDI), 32);
			CPULONG(Ofs_ESP) = SR1.d;
		}
		} break;

	case O_POP: {
		signed char o = Offs_From_Arg();
		long stackm = CPULONG(Ofs_STACKM);
		if (mode&MEMADR) {GTRACE0("O_POP");}
		    else  {GTRACE1("O_POP",o);}
		if (mode & DATA16) {
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP);
			SR1.d &= stackm;
			DR1.w.l = *((short *)(uintptr_t)(AR2.d + SR1.d));
			if (!(mode & MEMADR)) CPUWORD(o) = DR1.w.l;
			SR1.d += 2;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
			CPULONG(Ofs_ESP) = SR1.d;
		}
		else {
			long tesp;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = tesp = CPULONG(Ofs_ESP);
			SR1.d &= stackm;
			DR1.d = *((int *)(uintptr_t)(AR2.d + SR1.d));
			if (!(mode & MEMADR)) CPULONG(o) = DR1.d;
			SR1.d += 4;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			SR1.d |= (tesp & ~stackm);
#endif
			CPULONG(Ofs_ESP) = SR1.d;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

/* POP derived (sub-)sequences: */
	case O_POP1:
		GTRACE0("O_POP1");
		AR2.d = CPULONG(Ofs_XSS);
		SR1.d = CPULONG(Ofs_ESP);
		break;

	case O_POP2: {
		signed char o = Offs_From_Arg();
		long stackm = CPULONG(Ofs_STACKM);
		GTRACE1("O_POP2",o);
		if (mode & DATA16) {
			SR1.d &= stackm;
			DR1.w.l = *((short *)(uintptr_t)(AR2.d + SR1.d));
			if (!(mode & MEMADR)) CPUWORD(o) = DR1.w.l;
			SR1.d += 2;
		}
		else {
			SR1.d &= stackm;
			DR1.d = *((int *)(uintptr_t)(AR2.d + SR1.d));
			if (!(mode & MEMADR)) CPULONG(o) = DR1.d;
			SR1.d += 4;
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			SR1.d |= (CPULONG(Ofs_ESP) & ~stackm);
#endif
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

	case O_POP3:
		GTRACE0("O_POP3");
		CPULONG(Ofs_ESP) = SR1.d;
		break;

	case O_POPA: {
		long stackm = CPULONG(Ofs_STACKM);
		/* pop order: edi esi ebp (esp) ebx edx ecx eax */
		GTRACE0("O_POPA");
		if (mode & DATA16) {
			short *pw;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP);
			SR1.d &= stackm;
			pw = (short *)(uintptr_t)(AR2.d + SR1.d);
			CPUWORD(Ofs_DI) = *pw++;
			CPUWORD(Ofs_SI) = *pw++;
			CPUWORD(Ofs_BP) = *pw++;
			pw++;
			CPUWORD(Ofs_BX) = *pw++;
			CPUWORD(Ofs_DX) = *pw++;
			CPUWORD(Ofs_CX) = *pw++;
			CPUWORD(Ofs_AX) = *pw++;
			SR1.d += 16;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
			CPULONG(Ofs_ESP) = SR1.d;
		}
		else {
			char *pd;
			long tesp;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = tesp = CPULONG(Ofs_ESP);
			SR1.d &= stackm;
			pd = (char *)(uintptr_t)(AR2.d + SR1.d);
			memcpy(CPUOFFS(Ofs_EDI), pd, 32);
			SR1.d += 32;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			SR1.d |= (tesp & ~stackm);
#endif
			CPULONG(Ofs_ESP) = SR1.d;
		} }
		break;

	case O_LEAVE: {
		long stackm = CPULONG(Ofs_STACKM);
		GTRACE0("O_LEAVE");
		if (mode & DATA16) {
			SR1.d = CPUWORD(Ofs_BP);
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d &= stackm;
			DR1.w.l = *((short *)(uintptr_t)(AR2.d + SR1.d));
			CPUWORD(Ofs_BP) = DR1.w.l;
			SR1.d += 2;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
			CPULONG(Ofs_ESP) = SR1.d;
		}
		else {
			SR1.d = CPULONG(Ofs_EBP);
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d &= stackm;
			DR1.d = *((int *)(uintptr_t)(AR2.d + SR1.d));
			CPULONG(Ofs_EBP) = DR1.d;
			SR1.d += 4;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			SR1.d |= (CPULONG(Ofs_ESP) & ~stackm);
#endif
			CPULONG(Ofs_ESP) = SR1.d;
		} }
		break;

	case O_MOVS_SetA:
		GTRACE0("O_MOVS_SetA");
		if (mode&ADDR16) {
		    if (mode&MOVSSRC) {
	    		AR2.d = CPULONG(OVERR_DS);
			DR2.d = CPUWORD(Ofs_SI); /* for overflow calc */
			AR2.d += DR2.d;
		    }
		    if (mode&MOVSDST) {
	    		AR1.d = CPULONG(Ofs_XES);
			SR1.d = CPUWORD(Ofs_DI); /* for overflow calc */
			AR1.d += SR1.d;
			
		    }
		    TR1.d = (mode&(MREP|MREPNE)? CPUWORD(Ofs_CX) : 1);
		}
		else {
		    if (mode&MOVSSRC) {
	    		AR2.d = CPULONG(OVERR_DS) + CPULONG(Ofs_ESI);
		    }
		    if (mode&MOVSDST) {
	    		AR1.d = CPULONG(Ofs_XES) + CPULONG(Ofs_EDI);
		    }
		    TR1.d = (mode&(MREP|MREPNE)? CPULONG(Ofs_ECX) : 1);
		}
		break;

	case O_MOVS_MovD: {
		int df = CPUWORD(Ofs_FLAGS) & EFLAGS_DF;
		register unsigned int i, v;
		i = TR1.d;
		GTRACE4("O_MOVS_MovD",0xff,0xff,df,i);
		v = vga_read_access(AR2.d) | (vga_write_access(AR1.d) << 1);
		if (v) {
		    int op;
		    struct sigcontext_struct s, *scp = &s;
		    _err = v;
		    _rdi = AR1.d;
		    _rsi = AR2.d;
		    _rcx = i;
		    df = 1 - 2*(df ? 1 : 0);
		    op = 2 | (v == 3 ? 4 : 0);
		    if (mode & MBYTE) {
			op |= 1;
		    } else {
			df *= 2;
			if ((mode & DATA32)) {
			    df *= 2;
			}
		    }
		    e_VgaMovs(scp, op, (mode & DATA16) ? 1 : 0, df);
		    AR1.d = _edi;
		    AR2.d = _esi;
		}
		else if (df) {
		    if (mode&MBYTE) {
			while (i--) *AR1.pu-- = *AR2.pu--;
		    }
		    else if (mode&DATA16) {
	    		while (i--) *AR1.pwu-- = *AR2.pwu--;
		    }
		    else {
			while (i--) *AR1.pdu-- = *AR2.pdu--;
		    }
		}
	    	else {
		    if (mode&MBYTE) {
			while (i--) *AR1.pu++ = *AR2.pu++;
		    }
		    else if (mode&DATA16) {
	    		while (i--) *AR1.pwu++ = *AR2.pwu++;
		    }
		    else {
			while (i--) *AR1.pdu++ = *AR2.pdu++;
		    }
		}
		if (mode&(MREP|MREPNE))	TR1.d = 0;
		// ! Warning DI,SI wrap	in 16-bit mode
		}
		break;
	case O_MOVS_LodD: {
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		register unsigned int i;
		i = TR1.d;
		GTRACE4("O_MOVS_LodD",0xff,0xff,df,i);
		if (mode&(MREP|MREPNE))	{
		    dbug_printf("odd: REP LODS %d\n",i);
		}
		if (vga_read_access(AR2.d)) {
		    while (i--) {
			if (vga_access(AR2.d))
			    DR1.d = e_VgaRead(AR2.d, mode);
			else
			    DR1.d = 0xffffffff;
			AR2.pu += df;
			if (!(mode&MBYTE)) {
			    AR2.pu += df;
			    if (!(mode&DATA16)) AR2.pu += 2*df;
			}
		    }
		}
		else if (mode&MBYTE) {
		    while (i--) { DR1.b.bl = *AR2.pu; AR2.pu += df; }
		}
		else if (mode&DATA16) {
	    	    while (i--) { DR1.w.l = *AR2.pwu; AR2.pwu += df; }
		}
		else {
		    while (i--) { DR1.d = *AR2.pdu; AR2.pdu += df; }
		}
		if (mode&(MREP|MREPNE))	TR1.d = 0;
		// ! Warning DI,SI wrap	in 16-bit mode
		}
		break;
	case O_MOVS_StoD: {
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		register unsigned int i;
		i = TR1.d;
		GTRACE4("O_MOVS_StoD",0xff,0xff,df,i);
		if (vga_write_access(AR1.d)) {
		    while (i--) {
		        if (vga_access(AR1.d))
			    e_VgaWrite(AR1.d, DR1.d, mode);
			AR1.pu += df;
			if (!(mode&MBYTE)) {
			    AR1.pu += df;
			    if (!(mode&DATA16)) AR1.pu += 2*df;
			}
		    }
		}
		else if(mode & ADDR16 && df == 1 &&
		        OPSIZE(mode)*i + SR1.d > 0x10000)
		{
			unsigned int possible, remaining;
			/* 16 bit address overflow detected */
			if(AR1.d & (OPSIZE(mode)-1))
			{
				/* misaligned overflow generates trap. */
				TheCPU.err=EXCP0D_GPF;
				break;
			}
			possible = (0x10000-SR1.d)/OPSIZE(mode);
			remaining = i - possible;
			TR1.d = possible;
			Gen_sim(O_MOVS_StoD,mode);
			AR1.d -= 0x10000;
			TR1.d = remaining;
			Gen_sim(O_MOVS_StoD,mode);
		}
		else if(mode & ADDR16 && df == -1 && i &&
		        OPSIZE(mode)*(i-1) > SR1.d)
		{
			unsigned int possible, remaining;
			/* 16 bit address overflow detected */
			if(AR1.d & (OPSIZE(mode)-1))
			{
				/* misaligned overflow generates trap. */
				TheCPU.err=EXCP0D_GPF;
				break;
			}
			possible = SR1.d/OPSIZE(mode) + 1;
			remaining = i - possible;
			TR1.d = possible;
			Gen_sim(O_MOVS_StoD,mode);
			AR1.d += 0x10000;
			TR1.d = remaining;
			Gen_sim(O_MOVS_StoD,mode);
		}
		else if (mode&MBYTE) {
		    while (i--) { *AR1.pu = DR1.b.bl; AR1.pu += df; }
		}
		else if (mode&DATA16) {
	    	    while (i--) { *AR1.pwu = DR1.w.l; AR1.pwu += df; }
		}
		else {
		    while (i--) { *AR1.pdu = DR1.d; AR1.pdu += df; }
		}
		if (mode&(MREP|MREPNE))	TR1.d = 0;
		}
		break;
	case O_MOVS_ScaD: {	// OSZAPC
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		register unsigned int i;
		char k, z;
		i = TR1.d;
		GTRACE4("O_MOVS_ScaD",0xff,0xff,df,i);
		if (i == 0) break; /* eCX = 0, no-op, no flags updated */
		RFL.mode = mode;
		RFL.valid = V_SUB;
		z = k = (mode&MREP? 1:0);
		while (i && (z==k)) {
		    if (mode&MBYTE) {
			RFL.RES.d = (RFL.S1=DR1.b.bl) + (RFL.S2=-(*AR1.pu));
			AR1.pu += df;
			z = (RFL.RES.b.bl==0);
		    }
		    else if (mode&DATA16) {
			RFL.RES.d = (RFL.S1=DR1.w.l) + (RFL.S2=-(*AR1.pwu));
			AR1.pwu += df;
			z = (RFL.RES.w.l==0);
		    }
		    else {
			RFL.RES.d = (RFL.S1=DR1.d) + (RFL.S2=-(*AR1.pdu));
			AR1.pdu += df;
			z = (RFL.RES.d==0);
		    }
		    i--;
		}
		if (mode&(MREP|MREPNE))	TR1.d = i;
		FlagSync_C(1);
		// ! Warning DI,SI wrap	in 16-bit mode
		}
		break;
	case O_MOVS_CmpD: {	// OSZAPC
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		register unsigned int i;
		char k, z;
		i = TR1.d;
		GTRACE4("O_MOVS_CmpD",0xff,0xff,df,i);
		if (i == 0) break; /* eCX = 0, no-op, no flags updated */
		RFL.mode = mode;
		RFL.valid = V_SUB;
		z = k = (mode&MREP? 1:0);
		while (i && (z==k)) {
		    if (mode&MBYTE) {
			RFL.RES.d = (RFL.S1=*AR2.pu) + (RFL.S2=-(*AR1.pu));
			AR1.pu += df; AR2.pu += df;
			z = (RFL.RES.b.bl==0);
		    }
		    else if (mode&DATA16) {
			RFL.RES.d = (RFL.S1=*AR2.pwu) + (RFL.S2=-(*AR1.pwu));
			AR1.pwu += df; AR2.pwu += df;
			z = (RFL.RES.w.l==0);
		    }
		    else {
			RFL.RES.d = (RFL.S1=*AR2.pdu) + (RFL.S2=-(*AR1.pdu));
			AR1.pdu += df; AR2.pdu += df;
			z = (RFL.RES.d==0);
		    }
		    i--;
		}
		if (mode&(MREP|MREPNE))	TR1.d = i;
		FlagSync_C(1);
		// ! Warning DI,SI wrap	in 16-bit mode
		}
		break;

	case O_MOVS_SavA:
		GTRACE0("O_MOVS_SavA");
		if (mode&ADDR16) {
		    if (mode&(MREP|MREPNE)) {
	    		CPUWORD(Ofs_CX) = TR1.w.l;
		    }
		    if (mode&MOVSSRC) {
	    		AR2.d -= CPULONG(OVERR_DS);
			CPUWORD(Ofs_SI) = AR2.w.l;
		    }
		    if (mode&MOVSDST) {
	    		AR1.d -= CPULONG(Ofs_XES);
			CPUWORD(Ofs_DI) = AR1.w.l;
		    }
		}
		else {
		    if (mode&(MREP|MREPNE)) {
	    		CPULONG(Ofs_ECX) = TR1.d;
		    }
		    if (mode&MOVSSRC) {
	    		AR2.d -= CPULONG(OVERR_DS);
			CPULONG(Ofs_ESI) = AR2.d;
		    }
		    if (mode&MOVSDST) {
	    		AR1.d -= CPULONG(Ofs_XES);
			CPULONG(Ofs_EDI) = AR1.d;
		    }
		}
		break;
	case O_SLAHF:
		rcod = va_arg(ap,int)&1;	// 0=LAHF 1=SAHF
		if (rcod==0) {		/* LAHF */
			GTRACE0("O_LAHF");
			FlagSync_All();
			CPUBYTE(Ofs_AH) = CPUBYTE(Ofs_FLAGS);
		}
		else {			/* SAHF */
			GTRACE0("O_SAHF");
			CPUBYTE(Ofs_FLAGS) = (CPUBYTE(Ofs_AH)&0xd5)|0x02;
			RFL.valid = V_INVALID;
		}
		break;
	case O_SETFL: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		switch(o1) {	// these are direct on x86
		case CMC:
			GTRACE0("O_CMC");
			CPUBYTE(Ofs_FLAGS) ^= 1;
			break;
		case CLC:
			GTRACE0("O_CLC");
			SET_CF(0);
			break;
		case STC:
			GTRACE0("O_STC");
			SET_CF(1);
			break;
		case CLD:
			GTRACE0("O_CLD");
			CPUWORD(Ofs_FLAGS) &= 0xfbff;
			break;
		case STD:
			GTRACE0("O_STD");
			CPUWORD(Ofs_FLAGS) |= 0x400;
			break;
		} }
		break;
	case O_BSWAP: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		register long v;
		GTRACE1("O_BSWAP",o1);
		v = CPULONG(o1);
		v = ((v & 0xff00ff00) >> 8) | ((v & 0x00ff00ff) << 8);
		v = ((v & 0xffff0000) >> 16) | ((v & 0x0000ffff) << 16);
		CPULONG(o1) = v;
		}
		break;
	case O_SETCC: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		GTRACE3("O_SETCC",0xff,0xff,o1);
		FlagSync_All();
		switch(o1) {
			case 0x00: *AR1.pu = IS_OF_SET; break;
			case 0x01: *AR1.pu = !IS_OF_SET; break;
			case 0x02: *AR1.pu = IS_CF_SET; break;
			case 0x03: *AR1.pu = !IS_CF_SET; break;
			case 0x04: *AR1.pu = IS_ZF_SET; break;
			case 0x05: *AR1.pu = !IS_ZF_SET; break;
			case 0x06: *AR1.pu = IS_CF_SET || IS_ZF_SET; break;
			case 0x07: *AR1.pu = !IS_CF_SET && !IS_ZF_SET; break;
			case 0x08: *AR1.pu = IS_SF_SET; break;
			case 0x09: *AR1.pu = !IS_SF_SET; break;
			case 0x0a:
				e_printf("!!! SETp\n");
				*AR1.pu = IS_PF_SET; break;
			case 0x0b:
				e_printf("!!! SETnp\n");
				*AR1.pu = !IS_PF_SET; break;
			case 0x0c: *AR1.pu = IS_SF_SET ^ IS_OF_SET; break;
			case 0x0d: *AR1.pu = !(IS_SF_SET ^ IS_OF_SET); break;
			case 0x0e: *AR1.pu = (IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET; break;
			case 0x0f: *AR1.pu = !(IS_SF_SET ^ IS_OF_SET) && !IS_ZF_SET; break;
		}
		}
		break;
	case O_BITOP: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		signed char o2 = Offs_From_Arg();
		register int flg;
		GTRACE3("O_BITOP",o2,0xff,o1);
		if (o1 == 0x1c || o1 == 0x1d) { /* bsf/bsr */
			DR1.d = (mode & DATA16) ? *AR1.pwu : *AR1.pdu;
			DR1.d = o1 == 0x1c ? find_bit(DR1.d) : find_bit_r(DR1.d);
			if (DR1.d == -1) {
				flg = 0x40;
			} else {
				flg = 0;
				if (mode & DATA16)
					CPUWORD(o2) = DR1.d;
				else
					CPULONG(o2) = DR1.d;
			}
			// set ZF
			FlagSync_All();
			CPUBYTE(Ofs_FLAGS)=(CPUBYTE(Ofs_FLAGS)&0xbf)|flg;
			break;
		}
		if(o1 >= 0x20)
			DR1.d = o2 & 0x1f;
		else if (mode & DATA16)
			DR1.d = CPUWORD(o2);
		else
			DR1.d = CPULONG(o2);
		switch (o1) {
		case 0x03: case 0x20: flg = test_bit(DR1.d, AR1.pdu); break;
		case 0x0b: case 0x28: flg = set_bit(DR1.d, AR1.pdu); break;
		case 0x13: case 0x30: flg = clear_bit(DR1.d, AR1.pdu); break;
		case 0x1b: case 0x38: flg = change_bit(DR1.d, AR1.pdu); break;
		default: flg = 2;
		}
		if (flg != 2)
			SET_CF(flg&1);
		} break;
	case O_SHFD: {
		unsigned char l_r = (unsigned char)va_arg(ap,int)&8;
		signed char o = Offs_From_Arg();
		unsigned char shc;
		int cy;
		if (mode & IMMED) {
			shc = (unsigned char)va_arg(ap,int)&0x1f;
			GTRACE4("O_SHFD",o,0xff,l_r,shc);
		}
		else {
			shc = CPUBYTE(Ofs_CL)&0x1f;
			GTRACE3("O_SHFD",o,0xff,l_r);
		}
		if (shc==0) break;
		if (mode & DATA16) {
			if (l_r==0) {	// left:  <<reg|mem<<
				DR1.w.l = CPUWORD(o);
				DR1.w.h = *AR1.pwu;
				if (shc==1) RFL.S1=RFL.S2=DR1.w.h;
				cy = (DR1.d >> (32-shc)) & 1;
				DR1.d <<= shc;
				RFL.RES.d = *AR1.pwu = DR1.w.h;
			}
			else {		// right: >>mem|reg>>
				DR1.w.h = CPUWORD(o);
				DR1.w.l = *AR1.pwu;
				if (shc==1) RFL.S1=RFL.S2=DR1.w.l;
				cy = (DR1.d >> (shc-1)) & 1;
				DR1.d >>= shc;
				RFL.RES.d = *AR1.pwu = DR1.w.l;
			}
		}
		else {
			u_int64_u v;
			if (l_r==0) {	// left:  <<reg|mem<<
				v.t.tl = CPULONG(o);
				v.t.th = *AR1.pdu;
				if (shc==1) RFL.S1=RFL.S2=v.t.th;
				cy = (v.td >> (32-shc)) & 1;
				v.td <<= shc;
				RFL.RES.d = *AR1.pdu = v.t.th;
			}
			else {		// right: >>mem|reg>>
				v.t.th = CPULONG(o);
				v.t.tl = *AR1.pdu;
				if (shc==1) RFL.S1=RFL.S2=v.t.tl;
				cy = (v.td >> (shc-1)) & 1;
				v.td >>= shc;
				RFL.RES.d = *AR1.pdu = v.t.tl;
			}
		}
		} break;

	case O_RDTSC: {		// dont trust this one
		hitimer_u t0, t1;
		GTRACE0("O_RDTSC");
		t0.td = GETTSC();
		t1.td = t0.td - TheCPU.EMUtime;
		TheCPU.EMUtime = t1.td;
		CPULONG(Ofs_EAX) = t0.t.tl;
		CPULONG(Ofs_EDX) = t0.t.th;
		}
		break;

	case O_INPDX:
		GTRACE0("O_INPDX");
		DR2.d = CPULONG(Ofs_EDX);
		if (mode&MBYTE) {
			DR1.b.bl = port_real_inb(DR2.w.l);
			CPUBYTE(Ofs_AL) = DR1.b.bl;
		}
		else if (mode & DATA16) {
			DR1.w.l = port_real_inw(DR2.w.l);
			CPUWORD(Ofs_AX) = DR1.w.l;
		}
		else {
			DR1.d = port_real_ind(DR2.w.l);
			CPULONG(Ofs_EAX) = DR1.d;
		}
		break;
	case O_OUTPDX:
		GTRACE0("O_OUTPDX");
		DR2.d = CPULONG(Ofs_EDX);
		if (mode&MBYTE) {
			DR1.b.bl = CPUBYTE(Ofs_AL);
			port_real_outb(DR2.w.l,DR1.b.bl);
		}
		else if (mode & DATA16) {
			DR1.w.l = CPUWORD(Ofs_AX);
			port_real_outw(DR2.w.l,DR1.w.l);
		}
		else {
			DR1.d = CPULONG(Ofs_EAX);
			port_real_outd(DR2.w.l,DR1.d);
		}
		break;

	case JMP_LINK:		// cond, dspt, retaddr, link
	case JF_LINK:
	case JB_LINK:		// cond, PC, dspt, dspnt, link
	case JLOOP_LINK:	// cond, PC, dspt, dspnt, link
	default:
		leavedos(0x494c4c);
		break;

	}

	va_end(ap);
#ifdef DEBUG_MORE
	if (debug_level('e')>3) {
#else
	if (debug_level('e')>6) {
#endif
	    dbug_printf("(R) DR1=%08x DR2=%08x AR1=%08x AR2=%08x\n",
		DR1.d,DR2.d,AR1.d,AR2.d);
	    dbug_printf("(R) SR1=%08x TR1=%08x\n",
		SR1.d,TR1.d);
	    dbug_printf("(R) RFL m=[%s] v=%d S1=%08x S2=%08x RES=%08x\n",
		showmode(RFL.mode),RFL.valid,RFL.S1,RFL.S2,RFL.RES.d);
	    if (debug_level('e')==9) dbug_printf("\n%s",e_print_regs());
	}

	/* was there at least one FP op in the sequence? */
	if (TheCPU.mode & M_FPOP) {
		int exs = TheCPU.fpus & 0x7f;
		if (debug_level('e')>3) {
		    e_printf("  %s\n", e_trace_fp());
		}
		if (exs) {
			e_printf("FPU: error status %02x\n",exs);
			if ((exs & ~TheCPU.fpuc) & 0x3f) {
				e_printf("FPU exception\n");
				TheCPU.err = EXCP10_COPR;
			}
		}
	}
	
#ifdef PROFILE
	GenTime += (GETTSC() - t0);
#endif
}


/////////////////////////////////////////////////////////////////////////////


static unsigned char *CloseAndExec_sim(unsigned char *PC, TNode *G, int mode, int ln)
{
	if (debug_level('e')>1) {
	    if (TheCPU.sigalrm_pending>0) e_printf("** SIGALRM is pending\n");
	    if (debug_level('e')>2) {
		e_printf("== (%04d) == Closing sequence at %p\n",ln,PC);
	    }
	}
	CurrIMeta = -1;

	if (RFL.valid!=V_INVALID)
	    CPUBYTE(Ofs_FLAGS) = (CPUBYTE(Ofs_FLAGS) & 0x3f) | FlagSync_NZ();
#if defined(SINGLESTEP)||defined(SINGLEBLOCK)
	if (debug_level('e')>1) e_printf("\n%s",e_print_regs());
#endif
#ifdef DEBUG_MORE
	if (debug_level('e')>1) {
#else
	if (debug_level('e')>3) {
#endif
	    dbug_printf("(R) DR1=%08x DR2=%08x AR1=%08x AR2=%08x\n",
		DR1.d,DR2.d,AR1.d,AR2.d);
	    dbug_printf("(R) SR1=%08x TR1=%08x\n",
		SR1.d,TR1.d);
	    dbug_printf("(R) RFL m=[%s] v=%d S1=%08x S2=%08x RES=%08x\n\n",
		showmode(RFL.mode),RFL.valid,RFL.S1,RFL.S2,RFL.RES.d);
	}
	if (signal_pending) {
		CEmuStat|=CeS_SIGPEND;
	}
	TheCPU.sigalrm_pending = 0;
	TheCPU.EMUtime = GETTSC();
	return PC;
}


/////////////////////////////////////////////////////////////////////////////

