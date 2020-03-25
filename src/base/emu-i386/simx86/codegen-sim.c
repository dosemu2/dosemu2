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
#include "video.h"
#include "codegen.h"
#include "codegen-sim.h"

#undef	DEBUG_MORE

void (*Gen)(int op, int mode, ...);
void (*AddrGen)(int op, int mode, ...);
unsigned int (*CloseAndExec)(unsigned int PC, int mode, int ln);
int (*InvalidateNodePage)(int addr, int len, unsigned char *eip, int *codehit);
static unsigned int CloseAndExec_sim(unsigned int PC, int mode, int ln);

int UseLinker = 0;

static unsigned int P0 = (unsigned)-1;

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
#if 0
static inline void MARK(void)	// oops...objdump shows all the code at the
				// END of a switch statement(!)
{
	__asm__ __volatile__ ("cmc; cmc");
}
#endif
/////////////////////////////////////////////////////////////////////////////

static inline int is_zf_set(void)
{
	if (RFL.valid==V_INVALID)
	    return (CPUBYTE(Ofs_FLAGS)&0x40) >> 6;
	if (RFL.mode & MBYTE)
	    return RFL.RES.b.bl==0;
	if (RFL.mode & DATA16)
	    return RFL.RES.w.l==0;
	return RFL.RES.d==0;
}

static inline int is_sf_set(void)
{
	if (RFL.valid==V_INVALID)
	    return (CPUBYTE(Ofs_FLAGS)&0x80) >> 7;
	if (RFL.mode & MBYTE)
	    return (RFL.RES.b.bl & 0x80) >> 7;
	if (RFL.mode & DATA16)
	    return (RFL.RES.w.l & 0x8000) >> 15;
	return (RFL.RES.d & 0x80000000) >> 31;
}

static inline int is_of_set(void)
{
	if (RFL.valid==V_INVALID)
	    return (CPUWORD(Ofs_FLAGS)&0x800) >> 11;
	if (RFL.mode & CLROVF)
	    return 0;
	if (RFL.mode & SETOVF)
	    return 1;
	return ((RFL.cout >> 31) ^ (RFL.cout >> 30)) & 1;
}

#define SET_CF(c)	CPUBYTE(Ofs_FLAGS)=((CPUBYTE(Ofs_FLAGS)&0xfe)|(c))

/* add/sub rule for carry using MSB:
 * the carry-out expressions from Bochs 2.6 are used here.
 * RFL.cout is a cheap-to-compute 32-bit word that encodes the following flags:
 * CF is bit 31
 * OF is bit 31 xor bit 30
 * AF is bit 3
 *
 *	src1 src2  res	cy(add) cy(sub)
 *	  0    0    0    0	0
 *	  0    0    1    0	1
 *	  0    1    0    1	1
 *	  0    1    1    0	1
 *	  1    0    0    1	0
 *	  1    0    1    0	0
 *	  1    1    0    1	0
 *	  1    1    1    1	1
*/
static inline void FlagHandleAdd(unsigned src1, unsigned src2, unsigned res,
				 int wordsize)
{
	unsigned int cout = (src1 & src2) | ((src1 | src2) & ~res);
	if (wordsize == 32) RFL.cout = cout;
	if (wordsize == 16) RFL.cout = ((cout >> 14) << 30) | (cout & 8);
	if (wordsize == 8)  RFL.cout = ((cout >> 6) << 30) | (cout & 8);
	int cy = cout >> (wordsize - 1);
	SET_CF(cy & 1);
}

static inline void FlagHandleSub(unsigned src1, unsigned src2, unsigned res,
				 int wordsize)
{
	unsigned int cout = (~src1 & src2) | ((~src1 ^ src2) & res);
	if (wordsize == 32) RFL.cout = cout;
	if (wordsize == 16) RFL.cout = ((cout >> 14) << 30) | (cout & 8);
	if (wordsize == 8)  RFL.cout = ((cout >> 6) << 30) | (cout & 8);
	int cy = cout >> (wordsize - 1);
	SET_CF(cy & 1);
}

static inline void FlagHandleIncDec(unsigned low, unsigned high, int wordsize)
{
	unsigned int cout = low & ~high;
	if (wordsize == 32) RFL.cout = cout;
	if (wordsize == 16) RFL.cout = ((cout >> 14) << 30) | (cout & 8);
	if (wordsize == 8)  RFL.cout = ((cout >> 6) << 30) | (cout & 8);
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
	int nf;
	// OF
	/* overflow rule using MSB:
	 *	src1 src2 res OF ad/sub
	 *	  0    0    0    0  0
	 *	  0    0    1    1  0
	 *	  0    1    0    0  0
	 *	  0    1    1    0  1
	 *	  1    0    0    0  1
	 *	  1    0    1    0  0
	 *	  1    1    0    1  0
	 *	  1    1    1    0  0
	 */
	if (RFL.valid==V_INVALID) return (CPUWORD(Ofs_FLAGS)&0x800);
	if (RFL.mode & CLROVF)
		nf = 0;
	else if (RFL.mode & SETOVF)
		nf = 0x800;
	else
		// 80000000->0800 ^ 40000000->0800
		nf = ((RFL.cout >> 20) ^ (RFL.cout >> 19)) & 0x800;
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
	// AF bit 4, S1, S2, RES:
	// 0 0 0 -> NA  0 1 0 -> AC  1 0 0 -> AC  1 1 0 -> NA
	// 0 0 1 -> AC  0 1 1 -> NA  1 0 1 -> NA  1 1 1 -> AC
	if ((RFL.valid==V_SUB)||(RFL.valid==V_SBB)||
	    (RFL.valid==V_ADC)||(RFL.valid==V_ADD))
	    af = (RFL.cout & 0x8) << 1;
	else
	    af = CPUBYTE(Ofs_FLAGS)&0x10; // Intel says undefined.
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

static int InvalidateNodePage_sim(int addr, int len, unsigned char *eip,
	int *codehit)
{
	return 0;
}

void InitGen_sim(void)
{
	Gen = Gen_sim;
	AddrGen = AddrGen_sim;
	CloseAndExec = CloseAndExec_sim;
	InvalidateNodePage = InvalidateNodePage_sim;
	RFL.cout = RFL.RES.d = 0;
	RFL.valid = V_INVALID;
}

/*
 * address generator unit
 * careful - do not use eax, and NEVER change any flag!
 */
void AddrGen_sim(int op, int mode, ...)
{
	va_list	ap;
#ifdef PROFILE
	hitimer_t t0 = 0;
	if (debug_level('e')) t0 = GETTSC();
#endif

	va_start(ap, mode);
	switch(op) {
	case A_DI_0:			// base(32), imm
	case A_DI_1: {			// base(32), {imm}, reg, {shift}
			long idsp=0;
			char ofs;
			ofs = va_arg(ap,int);
			if (mode & MLEA) {		// discard base	reg
				AR1.d = 0;	// ofs = Ofs_RZERO;
			}
			else AR1.d = CPULONG(ofs);

			idsp = va_arg(ap,int);
			if (op==A_DI_0) {
				GTRACE3("A_DI_0",0xff,0xff,idsp);
				TR1.d = idsp;
			}
			else if (mode & ADDR16) {
				signed char o = Offs_From_Arg();
				GTRACE3("A_DI_1",o,ofs,idsp);
				TR1.d = CPUWORD(o);
				TR1.w.l += idsp;
			}
			else {
				signed char o = Offs_From_Arg();
				GTRACE3("A_DI_1",o,ofs,idsp);
				TR1.d = CPULONG(o) + idsp;
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

			idsp = va_arg(ap,int);
			if (mode & ADDR16) {
				signed char o1 = Offs_From_Arg();
				signed char o2 = Offs_From_Arg();
				GTRACE4("A_DI_2",o1,ofs,o2,idsp);
				TR1.d = CPUWORD(o1) + CPUWORD(o2) + idsp;
				AR1.d += TR1.w.l;
			}
			else {
				signed char o1 = Offs_From_Arg();
				signed char o2 = Offs_From_Arg();
				unsigned char sh;
				sh = (unsigned char)(va_arg(ap,int));
				GTRACE5("A_DI_2",o1,ofs,o2,idsp,sh);
				TR1.d = CPULONG(o1) +
				  (CPULONG(o2) << (sh & 0x1f)) + idsp;
				AR1.d += TR1.d;
			}
		}
		break;
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
			long idsp;
			unsigned char sh;
			signed char o;
			if (mode & MLEA) {
				AR1.d = 0;
			}
			else {
				AR1.d = CPULONG(OVERR_DS);
			}
			idsp = va_arg(ap,int);
			o = Offs_From_Arg();
			sh = (unsigned char)(va_arg(ap,int));
			GTRACE4("A_DI_2D",o,0xff,idsp,sh);
			TR1.d = (CPULONG(o) << (sh & 0x1f)) + idsp;
			AR1.d += TR1.d;
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
	if (debug_level('e')) GenTime += (GETTSC() - t0);
#endif
}

void Gen_sim(int op, int mode, ...)
{
	va_list ap;
	uint32_t S1, S2;
#ifdef PROFILE
	hitimer_t t0 = 0;
	if (debug_level('e')) t0 = GETTSC();
#endif

	P0 = (unsigned)-1;
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
		if (mode&(MBYTE|MBYTX))	{
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
		if (vga_write_access(DOSADDR_REL(AR1.pu))) {
			GTRACE0("S_DI_IMM_VGA");
			if (!vga_bank_access(DOSADDR_REL(AR1.pu))) break;
			e_VgaWrite(AR1.pu, v, mode); break;
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
		int rcod = va_arg(ap,int)&1;	// 0=z 1=s
		o = Offs_From_Arg();
		GTRACE3("L_MOVZS",o,0xff,rcod);
		if (mode & MBYTX) {
		    if (rcod)
			DR1.d = DR1.bs.bl;
		    else
			DR1.d = DR1.b.bl;
		}
		else {
		    if (rcod)
			DR1.d = DR1.ws.l;
		    else
			DR1.d = DR1.w.l;
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

	case L_DI_R1:
		if (vga_read_access(DOSADDR_REL(AR1.pu))) {
			GTRACE0("L_DI_R1");
			DR1.d = e_VgaRead(AR1.pu, mode);
			break;
		}
		GTRACE0("L_DI");
		if (mode & (MBYTE|MBYTX)) {
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
	case S_DI:
		if (vga_write_access(DOSADDR_REL(AR1.pu))) {
			GTRACE0("L_S_DI");
			if (!vga_bank_access(DOSADDR_REL(AR1.pu))) break;
			e_VgaWrite(AR1.pu, DR1.d, mode); break;
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
		RFL.valid = V_ADD;
		if (mode & IMMED) {GTRACE3("O_ADD_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADD_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.b.bl = RFL.RES.d = S1 + S2;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.w.l = RFL.RES.d = S1 + S2;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = RFL.RES.d = S1 + S2;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_OR_R: {		// O=0 SZP C=0
		int v = va_arg(ap, int);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_OR_R",0xff,0xff,v);}
		    else {GTRACE3("O_OR_R",v,0xff,v);}
		if (mode & MBYTE) {
		    if (!(mode & IMMED)) v = CPUBYTE(v);
		    RFL.RES.d = DR1.b.bl |= v;
		}
		else if (mode & DATA16) {
		    if (!(mode & IMMED)) v = CPUWORD(v);
		    RFL.RES.d = DR1.w.l |= v;
		}
		else {
		    if (!(mode & IMMED)) v = CPULONG(v);
		    RFL.RES.d = DR1.d |= v;
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
		if (mode & MBYTE) {
		    if (!(mode & IMMED)) v = CPUBYTE(v);
		    RFL.RES.d = DR1.b.bl &= v;
		}
		else if (mode & DATA16) {
		    if (!(mode & IMMED)) v = CPUWORD(v);
		    RFL.RES.d = DR1.w.l &= v;
		}
		else {
		    if (!(mode & IMMED)) v = CPULONG(v);
		    RFL.RES.d = DR1.d &= v;
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
		if (mode & MBYTE) {
		    if (!(mode & IMMED)) v = CPUBYTE(v);
		    RFL.RES.d = DR1.b.bl ^= v;
		}
		else if (mode & DATA16) {
		    if (!(mode & IMMED)) v = CPUWORD(v);
		    RFL.RES.d = DR1.w.l ^= v;
		}
		else {
		    if (!(mode & IMMED)) v = CPULONG(v);
		    RFL.RES.d = DR1.d ^= v;
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
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.b.bl = RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.w.l = RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
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
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 32);
		}
		}
		break;
	case O_ADC_R: {		// OSZAPC
		register wkreg v;
		int cy;
		v.d = va_arg(ap, int);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		RFL.mode = mode;
		RFL.valid = (cy? V_ADC:V_ADD);
		if (mode & IMMED) {GTRACE3("O_ADC_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADC_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.b.bl = RFL.RES.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.w.l = RFL.RES.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = RFL.RES.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
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
		    S1 = DR1.b.bl;
		    // movzbl v.b.bl->eax; add cy,eax; neg eax
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.b.bl = RFL.RES.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.w.l = RFL.RES.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = RFL.RES.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, RFL.RES.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
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
		RFL.valid = V_ADD;
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    CPUBYTE(o) = RFL.RES.d = S1 + 1;
		    FlagHandleIncDec(S1, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    CPUWORD(o) = RFL.RES.d = S1 + 1;
		    FlagHandleIncDec(S1, RFL.RES.d, 16);
		}
		else {
		    S1 = CPULONG(o);
		    CPULONG(o) = RFL.RES.d = S1 + 1;
		    FlagHandleIncDec(S1, RFL.RES.d, 32);
		}
		}
		break;
	case O_DEC_R: {		// OSZAP
		signed char o = Offs_From_Arg();
		GTRACE1("O_DEC_R",o);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    CPUBYTE(o) = RFL.RES.d = S1 - 1;
		    FlagHandleIncDec(RFL.RES.d, S1, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    CPUWORD(o) = RFL.RES.d = S1 - 1;
		    FlagHandleIncDec(RFL.RES.d, S1, 16);
		}
		else {
		    S1 = CPULONG(o);
		    CPULONG(o) = RFL.RES.d = S1 - 1;
		    FlagHandleIncDec(RFL.RES.d, S1, 32);
		}
		}
		break;
	case O_ADD_FR: {	// OSZAPC
		register wkreg v;
		signed char o = Offs_From_Arg();
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		RFL.mode = mode;
		RFL.valid = V_ADD;
		if (mode & IMMED) {GTRACE3("O_ADD_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADD_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = RFL.RES.d = S1 + S2;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = RFL.RES.d = S1 + S2;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = RFL.RES.d = S1 + S2;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_OR_FR: {		// O=0 SZP C=0
		register wkreg v;
		signed char o = Offs_From_Arg();
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_OR_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_OR_FR",v.d,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.RES.d = CPUBYTE(o) |= (mode & IMMED ? v.b.bl : DR1.b.bl);
		}
		else if (mode & DATA16) {
		    RFL.RES.d = CPUWORD(o) |= (mode & IMMED ? v.w.l : DR1.w.l);
		}
		else {
		    RFL.RES.d = CPULONG(o) |= (mode & IMMED ? v.d : DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		SET_CF(0);
		}
		break;
	case O_ADC_FR: {	// OSZAPC
		register wkreg v;
		signed char o = Offs_From_Arg();
		int cy;
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		RFL.mode = mode;
		RFL.valid = (cy? V_ADC:V_ADD);
		if (mode & IMMED) {GTRACE3("O_ADC_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADC_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = RFL.RES.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = RFL.RES.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = RFL.RES.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, RFL.RES.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_SBB_FR: {	// OSZAPC
		register wkreg v;
		signed char o = Offs_From_Arg();
		int cy;
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		cy = CPUBYTE(Ofs_FLAGS) & 1;
		RFL.mode = mode;
		RFL.valid = V_SBB;
		if (mode & IMMED) {GTRACE3("O_SBB_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_SBB_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    // movzbl v.bs.bl->eax; add cy,eax; neg eax
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = RFL.RES.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = RFL.RES.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = RFL.RES.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, RFL.RES.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_AND_FR: {		// O=0 SZP C=0
		register wkreg v;
		signed char o = Offs_From_Arg();
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_AND_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_AND_FR",v.d,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.RES.d = CPUBYTE(o) &= (mode & IMMED ? v.b.bl : DR1.b.bl);
		}
		else if (mode & DATA16) {
		    RFL.RES.d = CPUWORD(o) &= (mode & IMMED ? v.w.l : DR1.w.l);
		}
		else {
		    RFL.RES.d = CPULONG(o) &= (mode & IMMED ? v.d : DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		SET_CF(0);
		}
		break;
	case O_SUB_FR: {	// OSZAPC
		register wkreg v;
		signed char o = Offs_From_Arg();
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & IMMED) {GTRACE3("O_SUB_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_SUB_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d,32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_XOR_FR: {		// O=0 SZP C=0
		register wkreg v;
		signed char o = Offs_From_Arg();
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		RFL.mode = mode | CLROVF;
		RFL.valid = V_GEN;
		if (mode & IMMED) {GTRACE3("O_XOR_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_XOR_FR",v.d,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.RES.d = CPUBYTE(o) ^= (mode & IMMED ? v.b.bl : DR1.b.bl);
		}
		else if (mode & DATA16) {
		    RFL.RES.d = CPUWORD(o) ^= (mode & IMMED ? v.w.l : DR1.w.l);
		}
		else {
		    RFL.RES.d = CPULONG(o) ^= (mode & IMMED ? v.d : DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		SET_CF(0);
		}
		break;
	case O_CMP_FR: {	// OSZAPC
		register wkreg v;
		signed char o = Offs_From_Arg();
		v.d = 0;
		if (mode & IMMED) v.d = va_arg(ap,int);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & IMMED) {GTRACE3("O_CMP_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_CMP_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 32);
		}
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
		if (mode & MBYTE) {
			DR1.b.bl = RFL.RES.d = -(S2 = DR1.b.bl);
			FlagHandleSub(0, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
			DR1.w.l = RFL.RES.d = -(S2 = DR1.w.l);
			FlagHandleSub(0, S2, RFL.RES.d, 16);
		}
		else {
			DR1.d = RFL.RES.d = -(S2 = DR1.d);
			FlagHandleSub(0, S2, RFL.RES.d, 32);
		}
		break;
	case O_INC:		// OSZAP
		GTRACE0("O_INC");
		RFL.mode = mode;
		RFL.valid = V_ADD;
		if (mode & MBYTE) {
			S1 = DR1.bs.bl;
			DR1.b.bl = RFL.RES.d = S1 + 1;
			FlagHandleIncDec(S1, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
			S1 = DR1.ws.l;
			DR1.w.l = RFL.RES.d = S1 + 1;
			FlagHandleIncDec(S1, RFL.RES.d, 16);
		}
		else {
			S1 = DR1.d;
			DR1.d = RFL.RES.d = S1 + 1;
			FlagHandleIncDec(S1, RFL.RES.d, 32);
		}
		break;
	case O_DEC:		// OSZAP
		GTRACE0("O_DEC");
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if (mode & MBYTE) {
			S1 = DR1.bs.bl;
			DR1.b.bl = RFL.RES.d = S1 - 1;
			FlagHandleIncDec(RFL.RES.d, S1, 8);
		}
		else if (mode & DATA16) {
			S1 = DR1.ws.l;
			DR1.w.l = RFL.RES.d = S1 - 1;
			FlagHandleIncDec(RFL.RES.d, S1, 16);
		}
		else {
			S1 = DR1.d;
			DR1.d = RFL.RES.d = S1 - 1;
			FlagHandleIncDec(RFL.RES.d, S1, 32);
		}
		break;
	case O_CMPXCHG: {		// OSZAPC
		signed char o1 = va_arg(ap, int);
		signed char o2 = va_arg(ap, int);
		RFL.mode = mode;
		RFL.valid = V_SUB;
		GTRACE2("O_CMPXCHG",o1,o2);
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    S2 = (mode & RM_REG) ? CPUBYTE(o2) : *AR1.pu;
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    S2 = (mode & RM_REG) ? CPUWORD(o2) : *AR1.pwu;
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 16);
		}
		else {
		    S1 = DR1.d;
		    S2 = (mode & RM_REG) ? CPULONG(o2) : *AR1.pdu;
		    RFL.RES.d = S1 - S2;
		    FlagHandleSub(S1, S2, RFL.RES.d, 32);
		}
		if (RFL.RES.d == 0) {
			if (mode & RM_REG) {
				if (mode & MBYTE)
					CPUBYTE(o2) = CPUBYTE(o1);
				else if (mode & DATA16)
					CPUWORD(o2) = CPUWORD(o1);
				else
					CPULONG(o2) = CPULONG(o1);
			}
			else {
				if (mode & MBYTE)
					*AR1.pu = CPUBYTE(o1);
				else if (mode & DATA16)
					*AR1.pwu = CPUWORD(o1);
				else
					*AR1.pdu = CPULONG(o1);
			}
		} else {
			if (mode & MBYTE)
				DR1.b.bl = S2;
			else if (mode & DATA16)
				DR1.w.l = S2;
			else
				DR1.d = S2;
		}
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
			S1 = DR1.b.bl;
			if (S1==0)
			    TheCPU.err = EXCP00_DIVZ;
			else {
			    unsigned v = RFL.RES.d / S1;
			    if (v > 0xff)
				TheCPU.err = EXCP00_DIVZ;
			    else {
				CPUBYTE(Ofs_AL) = v;
				CPUBYTE(Ofs_AH) = RFL.RES.d % S1;
			    }
			}
		}
		else {
			if (mode&DATA16) {
				RFL.RES.w.l = CPUWORD(Ofs_AX);
				RFL.RES.w.h = CPUWORD(Ofs_DX);
				S1 = DR1.w.l;
				if (S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    unsigned v = RFL.RES.d / S1;
				    if (v > 0xffff)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPUWORD(Ofs_AX) = v;
					CPUWORD(Ofs_DX) = RFL.RES.d % S1;
				    }
				}
			}
			else {
				u_int64_u v;
				unsigned long rem;
				v.t.tl = CPULONG(Ofs_EAX);
				v.t.th = CPULONG(Ofs_EDX);
				S1 = DR1.d;
				if (S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    rem = v.td % S1;
				    v.td /= S1;
				    if (v.t.th)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPULONG(Ofs_EAX) = RFL.RES.d = v.t.tl;
					CPULONG(Ofs_EDX) = rem;
				    }
		    		}
			}
		}
		break;
	case O_IDIV:		// no flags
		GTRACE0("O_IDIV");
		if (mode & MBYTE) {
			int32_t S = DR1.bs.bl;
			RFL.RES.ds = (signed short)CPUWORD(Ofs_AX);
			if (S==0)
			    TheCPU.err = EXCP00_DIVZ;
	    		else {
			    int v = RFL.RES.ds / S;
			    if (v > 127 || v < -128)
				TheCPU.err = EXCP00_DIVZ;
			    else {
				CPUBYTE(Ofs_AL) = v;
				CPUBYTE(Ofs_AH) = RFL.RES.ds % S;
			    }
			}
		}
		else {
			if (mode&DATA16) {
				int32_t S = DR1.ws.l;
				RFL.RES.w.l = CPUWORD(Ofs_AX);
				RFL.RES.w.h = CPUWORD(Ofs_DX);
				S = DR1.ws.l;
				if (S==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    int v = RFL.RES.ds / S;
				    if (v > 32767 || v < -32768)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPUWORD(Ofs_AX) = v;
					CPUWORD(Ofs_DX) = RFL.RES.ds % S;
				    }
		    		}
			}
			else {
				int64_t v;
				long rem;
				int32_t S = DR1.d;
				v = CPULONG(Ofs_EAX) |
				  ((uint64_t)CPULONG(Ofs_EDX) << 32);
				if (S==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    rem = v % S;
				    v /= S;
				    if (v > 0x7fffffffLL || v < -0x80000000LL)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPULONG(Ofs_EAX) = RFL.RES.d = v & 0xffffffff;
					CPULONG(Ofs_EDX) = rem;
				    }
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
		TR1.d = CPULONG(Ofs_EBX) + CPUBYTE(Ofs_AL);
		if (mode & ADDR16) {
			TR1.d &= 0xFFFF;
		}
		AR1.d += TR1.d;
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
			rbef = DR1.b.bl;
			raft = (rbef<<sh) | (rbef>>(8-sh));
			DR1.b.bl = raft;
			cy = raft & 1;
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh &= 15;
			rbef = DR1.w.l;
			raft = (rbef<<sh) | (rbef>>(16-sh));
			DR1.w.l = raft;
			cy = raft & 1;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			// sh != 0, else we "break"ed above.
			// sh < 32, so 32-sh is in range 1..31, which is OK.
			rbef = DR1.d;
			raft = (rbef<<sh) | (rbef>>(32-sh));
			DR1.d = raft;
			cy = raft & 1;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
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

		if (mode & MBYTE) {
			sh %= 9;
			if (!sh) break;
			rbef = DR1.b.bl;
			raft = (rbef<<sh) | (rbef>>(9-sh)) | (cy<<(sh-1));
			DR1.b.bl = raft;
			cy = (rbef>>(8-sh)) & 1;
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh %= 17;
			if (!sh) break;
			rbef = DR1.w.l;
			raft = (rbef<<sh) | (rbef>>(17-sh)) | (cy<<(sh-1));
			DR1.w.l = raft;
			cy = (rbef>>(16-sh)) & 1;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			if (!sh) break;
			rbef = DR1.d;
			raft = (rbef<<sh) | (cy<<(sh-1));
			if (sh>1) raft |= (rbef>>(33-sh));
			DR1.d = raft;
			if(sh)
				cy = (rbef>>(32-sh)) & 1;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
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
			rbef = DR1.b.bl;
		else if (mode & DATA16)
			rbef = DR1.w.l;
		else
			rbef = DR1.d;

		// To simulate overflow flag. Keep in mind it is only defined
		// for sh==1. In that case, SHL r/m,1 behaves like ADD r/m,r/m.
		// So flag state gets set up like that
		raft = (rbef << sh);
		RFL.RES.d = raft;

		if (mode & MBYTE) {
			cy = (raft & 0x100) != 0;
			DR1.b.bl = raft;
			RFL.cout = rbef << 24;
		}
		else if (mode & DATA16) {
			cy = (raft & 0x10000) != 0;
			DR1.w.l = raft;
			RFL.cout = rbef << 16;
		}
		else {
			cy = (rbef>>(32-sh)) & 1;
			DR1.d = raft;
			RFL.cout = rbef & 0xc0000000;
		}
		RFL.RES.d = raft;
		if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		if (sh>1) RFL.mode |= IGNOVF;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_ROR: {		// O(if sh==1),C(if sh>0)
		signed char o = Offs_From_Arg();
		unsigned int sh, rbef, raft, cy, ov;
		GTRACE1("O_ROR",o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if(!sh)
			break;	// Carry unmodified, Overflow undefined.

		if (mode & MBYTE) {
			sh &= 7;
			rbef = DR1.b.bl;
			raft = (rbef>>sh) | (rbef<<(8-sh));
			DR1.b.bl = raft;
			cy = (raft & 0x80) != 0;
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh &= 15;
			rbef = DR1.w.l;
			raft = (rbef>>sh) | (rbef<<(16-sh));
			DR1.w.l = raft;
			cy = (raft & 0x8000) != 0;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			// sh != 0, else we "break"ed above.
			// sh < 32, so 32-sh is in range 1..31, which is OK.
			rbef = DR1.d;
			raft = (rbef>>sh) | (rbef<<(32-sh));
			DR1.d = raft;
			cy = (raft & 0x80000000U) != 0;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
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
			rbef = DR1.b.bl;
			raft = (rbef>>sh) | (rbef<<(9-sh)) | (cy<<(8-sh));
			DR1.b.bl = raft;
			if(sh)
				cy = (rbef>>(sh-1)) & 1;
			// else keep carry.
			ov = (rbef & 0x80) != (raft & 0x80);
		}
		else if (mode & DATA16) {
			sh %= 17;
			rbef = DR1.w.l;
			raft = (rbef>>sh) | (rbef<<(17-sh)) | (cy<<(16-sh));
			DR1.w.l = raft;
			if(sh)
				cy = (rbef>>(sh-1)) & 1;
			ov = (rbef & 0x8000) != (raft & 0x8000);
		}
		else {
			rbef = DR1.d;
			raft = (rbef>>sh) | (cy<<(32-sh));
			if (sh>1) raft |= (rbef<<(33-sh));
			DR1.d = raft;
			cy = (rbef>>(sh-1)) & 1;
			ov = (rbef & 0x80000000U) != (raft & 0x80000000U);
		}
		if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
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
			rbef = DR1.b.bl;
		else if (mode & DATA16)
			rbef = DR1.w.l;
		else
			rbef = DR1.d;

		cy = (rbef >> (sh-1)) & 1;
		raft = rbef >> sh;
		RFL.RES.d = raft;

		if (mode & MBYTE) {
			DR1.b.bl = raft;
			RFL.cout = raft << 24;
		}
		else if (mode & DATA16) {
			DR1.w.l = raft;
			RFL.cout = raft << 16;
		}
		else {
			DR1.d = raft;
			RFL.cout = raft & 0xc0000000;
		}
		if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
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
			rbef = DR1.bs.bl;
		else if (mode & DATA16)
			rbef = DR1.ws.l;
		else
			rbef = DR1.ds;

		cy = (rbef >> (sh-1)) & 1;
		raft = rbef >> sh;
		RFL.RES.d = raft;

		if (mode & MBYTE)
			DR1.bs.bl = raft;
		else if (mode & DATA16)
			DR1.ws.l = raft;
		else
			DR1.ds = raft;

		if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
		SET_CF(cy);
		RFL.cout = 0;  /* clears overflow & auxiliary carry flag */
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
		RFL.valid = V_ADD;
		RFL.cout = 0; /* clears overflow & auxiliary carry flag */
		DR1.d = CPULONG(Ofs_EAX);
		switch (subop) {
			case DAA: {
				char cyaf = 0;
				unsigned char altmp = DR1.b.bl;
				if (((DR1.b.bl & 0x0f) > 9 ) || (IS_AF_SET)) {
					DR1.b.bl += 6;
					cyaf = ((CPUBYTE(Ofs_FLAGS)&1) ||
					  (altmp > 0xf9)) | 8;
				}
				if ((altmp > 0x99) || (IS_CF_SET)) {
					DR1.b.bl += 0x60;
					cyaf |= 1;
				}
				SET_CF(cyaf & 1);
				RFL.RES.d = DR1.bs.bl; /* for flags */
				RFL.cout = (RFL.cout & ~8) | (cyaf & 8);
				}
				break;
			case DAS: {
				char cyaf = 0;
				unsigned char altmp = DR1.b.bl;
				if (((altmp & 0x0f) > 9) || (IS_AF_SET)) {
					DR1.b.bl -= 6;
					cyaf = ((CPUBYTE(Ofs_FLAGS)&1) ||
						(altmp < 6)) | 8;
				}
				if ((altmp > 0x99) || (IS_CF_SET)) {
					DR1.b.bl -= 0x60;
					cyaf |= 1;
				}
				SET_CF(cyaf & 1);
				RFL.RES.d = DR1.bs.bl; /* for flags */
				RFL.cout = (RFL.cout & ~8) | (cyaf & 8);
				}
				break;
			case AAA: {
				char cyaf;
				char icarry = (DR1.b.bl > 0xf9);
				if (((DR1.b.bl & 0x0f) > 9 ) || (IS_AF_SET)) {
					DR1.b.bl = (DR1.b.bl + 6) & 0x0f;
					DR1.b.bh = (DR1.b.bh + 1 + icarry);
					cyaf = 9;
				} else {
					cyaf = 0;
					DR1.b.bl &= 0x0f;
				}
				SET_CF(cyaf & 1);
				RFL.cout = (RFL.cout & ~8) | (cyaf & 8);
				}
				break;
			case AAS: {
				char cyaf;
				char icarry = (DR1.b.bl < 6);
				if (((DR1.b.bl & 0x0f) > 9 ) || (IS_AF_SET)) {
					DR1.b.bl = (DR1.b.bl - 6) & 0x0f;
					DR1.b.bh = (DR1.b.bh - 1 - icarry);
					cyaf = 9;
				} else {
					cyaf = 0;
					DR1.b.bl &= 0x0f;
				}
				SET_CF(cyaf & 1);
				RFL.cout = (RFL.cout & ~8) | (cyaf & 8);
				}
				break;
			case AAM: {
				char tmp = DR1.b.bl;
				int base = Offs_From_Arg();
				DR1.b.bh = tmp / base;
				RFL.RES.d = DR1.bs.bl = tmp % base;
				/* clear AF (undefined: found experimentally) */
				S2 = RFL.RES.d;
				}
				break;
			case AAD: {
				int base = Offs_From_Arg();
				DR1.w.l = ((DR1.b.bh * base) + DR1.b.bl) & 0xff;
				RFL.RES.d = DR1.bs.bl; /* for flags */
				/* clear AF (undefined: found experimentally) */
				S2 = RFL.RES.d;
				}
				break;
		}
		CPULONG(Ofs_EAX) = DR1.d;
		}
		break;

	case O_PUSH: {
		GTRACE0("O_PUSH");
		if (mode & DATA16) {
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 2;
			SR1.d &= CPULONG(Ofs_STACKM);
			*((short *)(uintptr_t)(AR2.d + SR1.d)) = DR1.w.l;
			CPULONG(Ofs_ESP) = SR1.d;
		}
		else {
			long stackm = CPULONG(Ofs_STACKM);
			long tesp;
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
			DR1.w.l = CPUWORD(o);
			SR1.d -= 2;
			SR1.d &= CPULONG(Ofs_STACKM);
			*((short *)(uintptr_t)(AR2.d + SR1.d)) = DR1.w.l;
		}
		else {
			DR1.d = CPULONG(o);
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
/*?*///		if (in_dpmi) ftmp = (ftmp & ~0x200) | (get_FLAGS(TheCPU.eflags) & 0x200);
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
		GTRACE1("O_POP",o);
		if (mode & DATA16) {
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP);
			SR1.d &= stackm;
			DR1.w.l = *((short *)(uintptr_t)(AR2.d + SR1.d));
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
			if (!(mode & MPOPRM))
				CPUWORD(o) = DR1.w.l;
			SR1.d += 2;
		}
		else {
			SR1.d &= stackm;
			DR1.d = *((int *)(uintptr_t)(AR2.d + SR1.d));
			if (!(mode & MPOPRM))
				CPULONG(o) = DR1.d;
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
		if (!(mode&(MREP|MREPNE|MOVSDST))) {
		    AR1.d = AR2.d; /* single lodsb uses L_DI_R1 */
		}
		break;

	case O_MOVS_MovD: {
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		register unsigned int i, v;
		i = TR1.d;
		GTRACE4("O_MOVS_MovD",0xff,0xff,df,i);
		if(i == 0)
		    break;
		v = vga_access(DOSADDR_REL(AR2.pu), DOSADDR_REL(AR1.pu));
		if (v) {
		    if (!(mode & MBYTE)) {
			df *= 2;
			if (!(mode & DATA16)) {
			    df *= 2;
			}
		    }
		    e_VgaMovs(&AR1.pu, &AR2.pu, i, df, v);
		    TR1.d = 0;
		    break;
		}
		if(mode & ADDR16) {
		    unsigned int minofs, bytesbefore;
		    /* overflow check (DR2 is SI, SR1 is DI) */
		    if (df == -1) {
			minofs = min(SR1.d,DR2.d);
			bytesbefore = (i-1)*OPSIZE(mode);
		    } else {
			minofs = 0x10000 - max(SR1.d,DR2.d);
			bytesbefore = i*OPSIZE(mode);
		    }
		    if(bytesbefore > minofs) {
		        unsigned int possible;
			/* caught 16 bit address overflow: do it piecewise */

			/* misaligned overflow generates trap. */
			if(minofs & (OPSIZE(mode)-1)) {
			    TheCPU.err=EXCP0D_GPF;
			    break;
			}

			/* do maximal possible amount */
			possible = minofs / (OPSIZE(mode));
			if (df < 0)
			    possible++;
			TR1.d = possible;
			Gen_sim(O_MOVS_MovD,mode);
			/* emulate overflow */
			if(SR1.d == minofs)
			    AR1.d -= df*0x10000;
			if(DR2.d == minofs)
			    AR2.d -= df*0x10000;

			/* do the rest */
			TR1.d = i - possible;
			Gen_sim(O_MOVS_MovD,mode);
			break;
		    }
		}
		if (df<0) {
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
		TR1.d = 0;
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
		if (vga_read_access(DOSADDR_REL(AR2.pu))) {
		    while (i--) {
			DR1.d = e_VgaRead(AR2.pu, mode);
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
		if (vga_write_access(DOSADDR_REL(AR1.pu))) {
		    while (i--) {
		        if (vga_bank_access(DOSADDR_REL(AR1.pu)))
			    e_VgaWrite(AR1.pu, DR1.d, mode);
			AR1.pu += df;
			if (!(mode&MBYTE)) {
			    AR1.pu += df;
			    if (!(mode&DATA16)) AR1.pu += 2*df;
			}
		    }
		    TR1.d = 0;
		    break;
		}
		if((mode & ADDR16) && i) {
		    unsigned int minofs, bytesbefore;
		    /* overflow check (SR1 is DI) */
		    if (df == -1) {
			minofs = SR1.d;
			bytesbefore = (i-1)*OPSIZE(mode);
		    } else {
			minofs = 0x10000 - SR1.d;
			bytesbefore = i*OPSIZE(mode);
		    }
		    if(bytesbefore > minofs) {
		        unsigned int possible;
			/* caught 16 bit address overflow: do it piecewise */
			if(AR1.d & (OPSIZE(mode)-1))
			{
				/* misaligned overflow generates trap. */
				TheCPU.err=EXCP0D_GPF;
				break;
			}
			possible = minofs / (OPSIZE(mode));
			if (df < 0)
			    possible++;
			TR1.d = possible;
			Gen_sim(O_MOVS_StoD,mode);
			AR1.d -= 0x10000*df;
			TR1.d = i - possible;
			Gen_sim(O_MOVS_StoD,mode);
			break;
		    }
		}
		if (mode&MBYTE) {
		    while (i--) { *AR1.pu = DR1.b.bl; AR1.pu += df; }
		}
		else if (mode&DATA16) {
	    	    while (i--) { *AR1.pwu = DR1.w.l; AR1.pwu += df; }
		}
		else {
		    while (i--) { *AR1.pdu = DR1.d; AR1.pdu += df; }
		}
		TR1.d = 0;
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
		if (vga_read_access(DOSADDR_REL(AR1.pu))) while (i && (z==k)) {
		    DR2.d = e_VgaRead(AR1.pu, mode);
		    if (mode&MBYTE) {
			RFL.RES.d = (S1=DR1.b.bl) - (S2=DR2.b.bl);
			FlagHandleSub(S1, S2, RFL.RES.d, 8);
			AR1.pu += df;
			z = (RFL.RES.b.bl==0);
		    }
		    else if (mode&DATA16) {
			RFL.RES.d = (S1=DR1.w.l) - (S2=DR2.w.l);
			FlagHandleSub(S1, S2, RFL.RES.d, 16);
			AR1.pwu += df;
			z = (RFL.RES.w.l==0);
		    }
		    else {
			RFL.RES.d = (S1=DR1.d) - (S2=DR2.d);
			FlagHandleSub(S1, S2, RFL.RES.d, 32);
			AR1.pdu += df;
			z = (RFL.RES.d==0);
		    }
		    i--;
		}
		else while (i && (z==k)) {
		    if (mode&MBYTE) {
			RFL.RES.d = (S1=DR1.b.bl) - (S2=*AR1.pu);
			FlagHandleSub(S1, S2, RFL.RES.d, 8);
			AR1.pu += df;
			z = (RFL.RES.b.bl==0);
		    }
		    else if (mode&DATA16) {
			RFL.RES.d = (S1=DR1.w.l) - (S2=*AR1.pwu);
			FlagHandleSub(S1, S2, RFL.RES.d, 16);
			AR1.pwu += df;
			z = (RFL.RES.w.l==0);
		    }
		    else {
			RFL.RES.d = (S1=DR1.d) - (S2=*AR1.pdu);
			FlagHandleSub(S1, S2, RFL.RES.d, 32);
			AR1.pdu += df;
			z = (RFL.RES.d==0);
		    }
		    i--;
		}
		TR1.d = i;
		// ! Warning DI,SI wrap	in 16-bit mode
		}
		break;
	case O_MOVS_CmpD: {	// OSZAPC
		int df;
		register unsigned int i;
		char k, z;
		i = TR1.d;
		GTRACE4("O_MOVS_CmpD",0xff,0xff,df,i);
		if (i == 0) break; /* eCX = 0, no-op, no flags updated */
		RFL.mode = mode;
		RFL.valid = V_SUB;
		if(!(mode & (MREP|MREPNE))) {
			// assumes DR1=*AR2
			if (vga_read_access(DOSADDR_REL(AR1.pu)))
				DR2.d = e_VgaRead(AR1.pu, mode);
			else if (mode&MBYTE)
				DR2.b.bl = *AR1.pu;
			else if (mode&DATA16)
				DR2.w.l = *AR1.pwu;
			else
				DR2.d = *AR1.pdu;
			if (mode&MBYTE)
				RFL.RES.d = (S1=DR1.b.bl) - (S2=DR2.b.bl);
			else if (mode&DATA16)
				RFL.RES.d = (S1=DR1.w.l) - (S2=DR2.w.l);
			else
				RFL.RES.d = (S1=DR1.d) - (S2=DR2.d);
			FlagHandleSub(S1, S2, RFL.RES.d, OPSIZE(mode)*8);
			break;
		}
		df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		z = k = (mode&MREP? 1:0);
		if (vga_read_access(DOSADDR_REL(AR1.pu)) ||
				vga_read_access(DOSADDR_REL(AR2.pu)))
		while (i && (z==k)) {
		    if (vga_read_access(DOSADDR_REL(AR1.pu)))
			DR1.d = e_VgaRead(AR1.pu, mode);
		    else if (mode&MBYTE)
			DR1.b.bl = *AR1.pu;
		    else if (mode&DATA16)
			DR1.w.l = *AR1.pwu;
		    else
			DR1.d = *AR1.pdu;
		    if (vga_read_access(DOSADDR_REL(AR2.pu)))
			DR2.d = e_VgaRead(AR2.pu, mode);
		    else if (mode&MBYTE)
			DR2.b.bl = *AR2.pu;
		    else if (mode&DATA16)
			DR2.w.l = *AR2.pwu;
		    else
			DR2.d = *AR2.pdu;
		    if (mode&MBYTE) {
			RFL.RES.d = (S1=DR2.b.bl) - (S2=DR1.b.bl);
			FlagHandleSub(S1, S2, RFL.RES.d, 8);
			AR1.pu += df; AR2.pu += df;
			z = (RFL.RES.b.bl==0);
		    }
		    else if (mode&DATA16) {
			RFL.RES.d = (S1=DR2.w.l) - (S2=DR1.w.l);
			FlagHandleSub(S1, S2, RFL.RES.d, 16);
			AR1.pwu += df; AR2.pwu += df;
			z = (RFL.RES.w.l==0);
		    }
		    else {
			RFL.RES.d = (S1=DR2.d) - (S2=DR1.d);
			FlagHandleSub(S1, S2, RFL.RES.d, 32);
			AR1.pdu += df; AR2.pdu += df;
			z = (RFL.RES.d==0);
		    }
		    i--;
		}
		else while (i && (z==k)) {
		    if (mode&MBYTE) {
			RFL.RES.d = (S1=*AR2.pu) - (S2=*AR1.pu);
			FlagHandleSub(S1, S2, RFL.RES.d, 8);
			AR1.pu += df; AR2.pu += df;
			z = (RFL.RES.b.bl==0);
		    }
		    else if (mode&DATA16) {
			RFL.RES.d = (S1=*AR2.pwu) - (S2=*AR1.pwu);
			FlagHandleSub(S1, S2, RFL.RES.d, 16);
			AR1.pwu += df; AR2.pwu += df;
			z = (RFL.RES.w.l==0);
		    }
		    else {
			RFL.RES.d = (S1=*AR2.pdu) - (S2=*AR1.pdu);
			FlagHandleSub(S1, S2, RFL.RES.d, 32);
			AR1.pdu += df; AR2.pdu += df;
			z = (RFL.RES.d==0);
		    }
		    i--;
		}
		TR1.d = i;
		// ! Warning DI,SI wrap	in 16-bit mode
		}
		break;

	case O_MOVS_SavA:
		GTRACE0("O_MOVS_SavA");
		if (!(mode&(MREP|MREPNE))) {
		    // %%edx set to DF's increment
		    DR2.d = (char)CPUBYTE(Ofs_DF_INCREMENTS+OPSIZEBIT(mode));
		    if(mode & MOVSSRC) {
			if (mode & ADDR16)
			    CPUWORD(Ofs_SI) += DR2.w.l;
			else
			    CPULONG(Ofs_SI) += DR2.d;
		    }
		    if(mode & MOVSDST) {
			if (mode & ADDR16)
			    CPUWORD(Ofs_DI) += DR2.w.l;
			else
			    CPULONG(Ofs_DI) += DR2.d;
		    }
		}
		else if (mode&ADDR16) {
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
	case O_SLAHF: {
		int rcod = va_arg(ap,int)&1;	// 0=LAHF 1=SAHF
		if (rcod==0) {		/* LAHF */
			GTRACE0("O_LAHF");
			FlagSync_All();
			CPUBYTE(Ofs_AH) = CPUBYTE(Ofs_FLAGS);
		}
		else {			/* SAHF */
			GTRACE0("O_SAHF");
			CPUBYTE(Ofs_FLAGS) = (CPUBYTE(Ofs_AH)&0xd5)|0x02;
			RFL.valid = V_INVALID;
		} }
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
			TheCPU.df_increments = 0x040201;
			break;
		case STD:
			GTRACE0("O_STD");
			CPUWORD(Ofs_FLAGS) |= 0x400;
			TheCPU.df_increments = 0xfcfeff;
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
			case 0x00: DR1.b.bl = IS_OF_SET; break;
			case 0x01: DR1.b.bl = !IS_OF_SET; break;
			case 0x02: DR1.b.bl = IS_CF_SET; break;
			case 0x03: DR1.b.bl = !IS_CF_SET; break;
			case 0x04: DR1.b.bl = IS_ZF_SET; break;
			case 0x05: DR1.b.bl = !IS_ZF_SET; break;
			case 0x06: DR1.b.bl = IS_CF_SET || IS_ZF_SET; break;
			case 0x07: DR1.b.bl = !IS_CF_SET && !IS_ZF_SET; break;
			case 0x08: DR1.b.bl = IS_SF_SET; break;
			case 0x09: DR1.b.bl = !IS_SF_SET; break;
			case 0x0a:
				e_printf("!!! SETp\n");
				DR1.b.bl = IS_PF_SET; break;
			case 0x0b:
				e_printf("!!! SETnp\n");
				DR1.b.bl = !IS_PF_SET; break;
			case 0x0c: DR1.b.bl = IS_SF_SET ^ IS_OF_SET; break;
			case 0x0d: DR1.b.bl = !(IS_SF_SET ^ IS_OF_SET); break;
			case 0x0e: DR1.b.bl = (IS_SF_SET ^ IS_OF_SET) || IS_ZF_SET; break;
			case 0x0f: DR1.b.bl = !(IS_SF_SET ^ IS_OF_SET) && !IS_ZF_SET; break;
		}
		}
		break;
	case O_BITOP: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		signed char o2 = Offs_From_Arg();
		register int flg;
		GTRACE3("O_BITOP",o2,0xff,o1);
		if (o1 == 0x1c || o1 == 0x1d) { /* bsf/bsr */
			if (mode & DATA16) DR1.d = DR1.w.l;
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
			DR2.d = o2 & ((mode & DATA16) ? 0x0f : 0x1f);
		else if (mode & DATA16)
		{
			DR2.d = CPUWORD(o2);
			if (mode & RM_REG)
				DR2.d &= 0x0f;
		} else {
			DR2.d = CPULONG(o2);
			if (mode & RM_REG)
				DR2.d &= 0x1f;
		}
		if ((mode & RM_REG) || o1 >= 0x20) {
		    switch (o1) {
		    case 0x03: case 0x20: flg = test_bit(DR2.d, &DR1.d); break;
		    case 0x0b: case 0x28: flg = set_bit(DR2.d, &DR1.d); break;
		    case 0x13: case 0x30: flg = clear_bit(DR2.d, &DR1.d); break;
		    case 0x1b: case 0x38: flg = change_bit(DR2.d, &DR1.d); break;
		    default: flg = 2;
		    }
		} else {
		    switch (o1) {
		    case 0x03: flg = test_bit(DR2.d, AR1.pdu); break;
		    case 0x0b: flg = set_bit(DR2.d, AR1.pdu); break;
		    case 0x13: flg = clear_bit(DR2.d, AR1.pdu); break;
		    case 0x1b: flg = change_bit(DR2.d, AR1.pdu); break;
		    default: flg = 2;
		    }
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
		shc &= 31;
		if (shc==0) break;
		RFL.mode = mode;
		RFL.valid = V_GEN;
		if (mode & DATA16) {
			if (l_r==0) {	// left:  <<reg|mem<<
				DR1.w.h = DR1.w.l;
				DR1.w.l = CPUWORD(o);
				RFL.cout = (uint32_t)DR1.w.h << 16;
				/* undocumented: works like rotate internally */
				DR1.d = (DR1.d << shc) | (DR1.d >> (32-shc));
				cy = DR1.d & 1;
				RFL.RES.d = DR1.w.l = DR1.w.h;
			}
			else {		// right: >>mem|reg>>
				DR1.w.h = CPUWORD(o);
				/* undocumented: works like rotate internally */
				DR1.d = (DR1.d >> shc) | (DR1.d << (32-shc));
				cy = (DR1.d >> 31) & 1;
				RFL.RES.d = DR1.w.l;
				RFL.cout = RFL.RES.d << 16;
			}
		}
		else {
			u_int64_u v;
			if (l_r==0) {	// left:  <<reg|mem<<
				v.t.tl = CPULONG(o);
				v.t.th = DR1.d;
				RFL.cout = v.t.th;
				cy = (v.td >> (64-shc)) & 1;
				v.td <<= shc;
				RFL.RES.d = DR1.d = v.t.th;
			}
			else {		// right: >>mem|reg>>
				v.t.th = CPULONG(o);
				v.t.tl = DR1.d;
				cy = (v.td >> (shc-1)) & 1;
				v.td >>= shc;
				RFL.RES.d = DR1.d = v.t.tl;
				RFL.cout = RFL.RES.d;
			}
		}
		SET_CF(cy);
		if (shc>1) RFL.mode |= IGNOVF;
		} break;

	case O_RDTSC: {		// dont trust this one
		hitimer_u t0, t1;
		GTRACE0("O_RDTSC");
		t0.td = GETTSC();
		if (eTimeCorrect >= 0) {
			t1.td = t0.td - TheCPU.EMUtime;
			TheCPU.EMUtime = t1.td;
		}
		CPULONG(Ofs_EAX) = t0.t.tl;
		CPULONG(Ofs_EDX) = t0.t.th;
		}
		break;

	case O_INPDX:
		GTRACE0("O_INPDX");
		DR2.d = CPULONG(Ofs_EDX);
		if (mode&MBYTE) {
			DR1.b.bl = port_inb(DR2.w.l);
			CPUBYTE(Ofs_AL) = DR1.b.bl;
		}
		else if (mode & DATA16) {
			DR1.w.l = port_inw(DR2.w.l);
			CPUWORD(Ofs_AX) = DR1.w.l;
		}
		else {
			DR1.d = port_ind(DR2.w.l);
			CPULONG(Ofs_EAX) = DR1.d;
		}
		break;
	case O_OUTPDX:
		GTRACE0("O_OUTPDX");
		DR2.d = CPULONG(Ofs_EDX);
		if (mode&MBYTE) {
			DR1.b.bl = CPUBYTE(Ofs_AL);
			port_outb(DR2.w.l,DR1.b.bl);
		}
		else if (mode & DATA16) {
			DR1.w.l = CPUWORD(Ofs_AX);
			port_outw(DR2.w.l,DR1.w.l);
		}
		else {
			DR1.d = CPULONG(Ofs_EAX);
			port_outd(DR2.w.l,DR1.d);
		}
		break;

	case JMP_LINK: {	// cond, dspt, retaddr, link
		/* evaluate cond at RUNTIME after exec'ing */
		int cond = va_arg(ap,int);
		P0 = va_arg(ap,unsigned int);
		unsigned int d_nt = va_arg(ap,unsigned int);
		if (cond == 0x11)
			PUSH(mode, d_nt);
		if (debug_level('e')>2) {
			if(cond == 0x11)
				dbug_printf("CALL: ret=%08x\n",d_nt);
			dbug_printf("** Jump taken to %08x\n",P0);
		} }
		break;

	case JF_LINK:
	case JB_LINK: {		// cond, PC, dspt, dspnt, link
		int cond = va_arg(ap,int);
		unsigned int PC = va_arg(ap,unsigned int);
		unsigned int j_t = va_arg(ap,unsigned int);
		unsigned int j_nt = va_arg(ap,unsigned int);
		(void)PC;
		switch(cond) {
		case 0x00:
			P0 = is_of_set() ? j_t : j_nt; break;
		case 0x01:
			P0 = !is_of_set() ? j_t : j_nt; break;
		case 0x02: P0 = IS_CF_SET ? j_t : j_nt; break;
		case 0x03: P0 = !IS_CF_SET ? j_t : j_nt; break;
		case 0x04: P0 = is_zf_set() ? j_t : j_nt; break;
		case 0x05: P0 = !is_zf_set() ? j_t : j_nt; break;
		case 0x06: P0 = IS_CF_SET || is_zf_set() ? j_t : j_nt; break;
		case 0x07: P0 = !IS_CF_SET && !is_zf_set() ? j_t : j_nt; break;
		case 0x08: P0 = is_sf_set() ? j_t : j_nt; break;
		case 0x09: P0 = !is_sf_set() ? j_t : j_nt; break;
		case 0x0a:
			e_printf("!!! JPset\n");
			FlagSync_AP();
			P0 = IS_PF_SET ? j_t : j_nt; break;
		case 0x0b:
			e_printf("!!! JPclr\n");
			FlagSync_AP();
			P0 = !IS_PF_SET ? j_t : j_nt; break;
		case 0x0c:
			P0 = is_sf_set() ^ is_of_set() ? j_t : j_nt; break;
		case 0x0d:
			P0 = !(is_sf_set() ^ is_of_set()) ? j_t : j_nt; break;
		case 0x0e:
			P0 = (is_sf_set() ^ is_of_set()) || is_zf_set() ? j_t : j_nt; break;
		case 0x0f:
			P0 = !(is_sf_set() ^ is_of_set()) && !is_zf_set() ? j_t : j_nt; break;
		case 0x31:	// JCXZ
			P0 = ((mode&ADDR16? rCX : rECX) == 0) ? j_t : j_nt; break;
		}
		if (debug_level('e')>2 && P0 == j_t) dbug_printf("** Jump taken to %08x\n",j_t);
		}
		break;
	case JLOOP_LINK: {	// cond, dspt, dspnt, link
		int cond = va_arg(ap,int);
		unsigned int j_t = va_arg(ap,unsigned int);
		unsigned int j_nt = va_arg(ap,unsigned int);
		int cxv = (mode&ADDR16? --rCX : --rECX);
		switch(cond) {
		case 0x20:	// LOOP
			P0 = cxv != 0 ? j_t : j_nt; break;
		case 0x24:	// LOOPZ
			P0 = cxv != 0 && is_zf_set() ? j_t : j_nt; break;
		case 0x25:	// LOOPNZ
			P0 = cxv != 0 && !is_zf_set() ? j_t : j_nt; break;
		}
		if (debug_level('e')>2 && P0 == j_t) dbug_printf("** Jump taken to %08x\n",j_t);
		}
		break;
	default:
		leavedos_main(0x494c4c);
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
	    dbug_printf("(R) RFL m=[%s] v=%d cout=%08x RES=%08x\n",
		showmode(RFL.mode),RFL.valid,RFL.cout,RFL.RES.d);
//	    if (debug_level('e')==9) dbug_printf("\n%s",e_print_regs());
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
	if (debug_level('e')) GenTime += (GETTSC() - t0);
#endif
}


/////////////////////////////////////////////////////////////////////////////


static unsigned int CloseAndExec_sim(unsigned int PC, int mode, int ln)
{
	unsigned int ret;
	if (debug_level('e')>1) {
	    if (TheCPU.sigalrm_pending>0) e_printf("** SIGALRM is pending\n");
	    if (debug_level('e')>2) {
		e_printf("== (%04d) == Closing sequence at %08x\n",ln,PC);
	    }
#if defined(SINGLESTEP)||defined(SINGLEBLOCK)
	    dbug_printf("\n%s",e_print_regs());
#endif
#ifndef DEBUG_MORE
	    if (debug_level('e')>3)
#endif
	    {
	    dbug_printf("(R) DR1=%08x DR2=%08x AR1=%08x AR2=%08x\n",
		DR1.d,DR2.d,AR1.d,AR2.d);
	    dbug_printf("(R) SR1=%08x TR1=%08x\n",
		SR1.d,TR1.d);
	    dbug_printf("(R) RFL m=[%s] v=%d cout=%08x RES=%08x\n\n",
		showmode(RFL.mode),RFL.valid,RFL.cout,RFL.RES.d);
	    }
	}

	if (signal_pending()) {
		CEmuStat|=CeS_SIGPEND;
	}
	TheCPU.sigalrm_pending = 0;
	if (eTimeCorrect >= 0)
	    TheCPU.EMUtime = GETTSC();
	if (P0 == (unsigned)-1)
		return PC;
	ret = P0;
	P0 = (unsigned)-1;
	return ret;
}


/////////////////////////////////////////////////////////////////////////////

