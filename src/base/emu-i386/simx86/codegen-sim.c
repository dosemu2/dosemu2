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
#include "dos2linux.h"
#include "vgaemu.h"
#include "video.h"
#include "msdoshlp.h"
#include "codegen.h"
#include "codegen-sim.h"

#undef	DEBUG_MORE

static unsigned char *CodeGen_sim(unsigned char *CodePtr, unsigned char *BaseGenBuf, const IGen *IG);
static unsigned Exec_sim(unsigned *mem_ref, unsigned long *flg,
			 unsigned char *ecpu, void *SeqStart,
			 unsigned short seqflg);

static unsigned char *currentIG = NULL;

/////////////////////////////////////////////////////////////////////////////

/* working registers of the host CPU */
static wkreg DR1;	// "eax"
static wkreg SR1;	// "ecx"
static flgtmp RFL;

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
	return RFL.res == 0;
}

static inline int is_cf_set(void)
{
	return RFL.cout >> LF_BIT_CF;
}

static inline int is_of_set(void)
{
	return (RFL.cout + LF_MASK_PO) >> LF_BIT_CF;
}

static inline int is_af_set(void)
{
	return (RFL.cout & LF_MASK_AF) != 0;
}

static inline void SET_CF(unsigned int c)
{
	// Always working on lazy flags: if CF changes, flip PO and CF
	RFL.cout ^= (c != is_cf_set()) * (LF_MASK_PO | LF_MASK_CF);
}

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
	if (wordsize == 32) RFL.cout = cout & ~(LF_MASK_SD|LF_MASK_PD);
	if (wordsize == 16) RFL.cout = (cout << 16) | (cout & LF_MASK_AF);
	if (wordsize == 8) RFL.cout = (cout << 24) | (cout & LF_MASK_AF);
	RFL.res = res;
}

static inline void FlagHandleSub(unsigned src1, unsigned src2, unsigned res,
				 int wordsize)
{
	unsigned int cout = (~src1 & src2) | ((~src1 ^ src2) & res);
	if (wordsize == 32) RFL.cout = cout & ~(LF_MASK_SD|LF_MASK_PD);
	if (wordsize == 16) RFL.cout = (cout << 16) | (cout & LF_MASK_AF);
	if (wordsize == 8) RFL.cout = (cout << 24) | (cout & LF_MASK_AF);
	RFL.res = res;
}

static inline void FlagHandleIncDec(unsigned low, unsigned high, int wordsize)
{
	unsigned int cout = low & ~high;
	int oldcy = is_cf_set();
	if (wordsize == 32) RFL.cout = cout & ~(LF_MASK_SD|LF_MASK_PD);
	if (wordsize == 16) RFL.cout = (cout << 16) | (cout & LF_MASK_AF);
	if (wordsize == 8) RFL.cout = (cout << 24) | (cout & LF_MASK_AF);
	SET_CF(oldcy);
}

static inline void FlagHandleRotate(int sh, int cy, uint32_t cout, int wordsize)
{
	if (sh == 1) {
		if (wordsize == 32) cout &= (LF_MASK_CF | LF_MASK_PO);
		if (wordsize == 16) cout <<= 16;
		if (wordsize == 8) cout <<= 24;
		RFL.cout = (RFL.cout & ~(LF_MASK_CF | LF_MASK_PO)) | cout;
	}
	if (debug_level('e')>1) dbug_printf("Sync C flag = %d\n", cy);
	SET_CF(cy);
}

static inline void FlagHandleShift(int sh, int cy, uint32_t cout, int wordsize,
				   unsigned res)
{
	FlagHandleRotate(sh, cy, cout, wordsize);
	RFL.res = res;
	RFL.cout &= ~(LF_MASK_SD|LF_MASK_PD);
}

static inline int FlagSync_S (void)
{
	return ((RFL.res>>(LF_BIT_RES_SF-X86_EFLAGS_SF_BIT)) ^ RFL.cout) &
	  EFLAGS_SF;
}

static inline int is_sf_set(void)
{
	return FlagSync_S() != 0;
}

static inline int FlagSync_O (void)
{
	int nf = is_of_set() << X86_EFLAGS_OF_BIT;
	if (debug_level('e')>1) e_printf("Sync O flag = %04x\n", nf);
	return nf;
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


static inline int FlagSync_P (void)
{
	return parity[RFL.res & 0xff] ^ (RFL.cout & LF_MASK_PD);
}

static inline int is_pf_set(void)
{
	return FlagSync_P() != 0;
}

static inline void SET_ZF(unsigned int c)
{
	// to set ZF in RFL.res we must transfer SF/PF to RFL.cout
	RFL.cout = (RFL.cout & ~(LF_MASK_PD | LF_MASK_SD)) |
	  ((FlagSync_S() | FlagSync_P()) ^ LF_MASK_PD);
	RFL.res = (!c) << 8;
}

static inline int FlagSync_SZAPC (void)
{
	/* AF/CF can be can be quickly obtained using a rotation */
	return (((RFL.cout << 1) | (RFL.cout >> 31)) & (EFLAGS_AF | EFLAGS_CF)) |
	  (is_zf_set() << X86_EFLAGS_ZF_BIT) |
	  FlagSync_S() | FlagSync_P();
}

static int FlagSync_All (void)
{
	int nf = FlagSync_SZAPC() | FlagSync_O();
	if (debug_level('e')>1) e_printf("Sync ALL flags = %04x\n", nf);
	return nf;
}


static void FlagSync_RFL (uint32_t flg)
{
	/* encode all CC flags into RFL */

	/* AF/CF via rotation */
	uint32_t cout = ((flg << 31) | (flg >> 1)) & (LF_MASK_AF | LF_MASK_CF);
	/* PO derived from CF^OF */
	cout |=  ((cout >> 1) ^ (flg << (LF_BIT_PO - X86_EFLAGS_OF_BIT))) & LF_MASK_PO;
	/* PF/SF in PD/SD; since parity of RFL.res is even, must flip PD */
	RFL.cout = cout | ((flg & (EFLAGS_SF|EFLAGS_PF)) ^ EFLAGS_PF);
	RFL.res = (!(flg & EFLAGS_ZF)) << 8;
}

/////////////////////////////////////////////////////////////////////////////

void InitGen_sim(void)
{
	Exec = Exec_sim;
	CodeGen = CodeGen_sim;
	UseLinker = 0;
	RFL.cout = RFL.res = 0;
}

static inline void check_v86_address_overflow(int mode, dosaddr_t offset)
{
	if (V86MODE() && (0 == (mode & (ADDR16 | MLEA))) && offset > 0xffff)
		TheCPU.err = EXCP0D_GPF;
}

static dosaddr_t AddrGen_sim(const IGen *IG)
{
	int op = IG->op;
	int mode = IG->mode;
	dosaddr_t mem_ref, offset;
	switch(op) {
	case A_DI_0:			// base(32), imm
	case A_DI_1: {			// base(32), {imm}, reg, {shift}
			long idsp=0;
			unsigned int ofs;
			ofs = IG->p0;
			if (mode & MLEA) {		// discard base	reg
				mem_ref = 0;	// ofs = Ofs_RZERO;
			}
			else mem_ref = CPULONG(ofs);

			idsp = IG->p1;
			if (op==A_DI_0) {
				GTRACE3("A_DI_0",0xff,0xff,idsp);
				offset = idsp;
				check_v86_address_overflow(mode, offset);
			}
			else if (mode & ADDR16) {
				unsigned int o = IG->p2;
				GTRACE3("A_DI_1",o,ofs,idsp);
				offset = (CPUWORD(o) + idsp) & 0xffff;
			}
			else {
				unsigned int o = IG->p2;
				GTRACE3("A_DI_1",o,ofs,idsp);
				offset = CPULONG(o) + idsp;
				check_v86_address_overflow(mode, offset);
			}
			mem_ref += offset;
		}
		break;
	case A_DI_2: {			// base(32), {imm}, reg, reg, {shift}
			long idsp=0;
			unsigned int ofs;
			ofs = IG->p0;
			if (mode & MLEA) {		// discard base	reg
				mem_ref = 0;	// ofs = Ofs_RZERO;
			}
			else mem_ref = CPULONG(ofs);

			idsp = IG->p1;
			if (mode & ADDR16) {
				unsigned int o1 = IG->p2;
				unsigned int o2 = IG->p3;
				GTRACE4("A_DI_2",o1,ofs,o2,idsp);
				offset = CPUWORD(o1) + CPUWORD(o2) + idsp;
				mem_ref += offset & 0xffff;
			}
			else {
				unsigned int o1 = IG->p2;
				unsigned int o2 = IG->p3;
				unsigned char sh;
				sh = (unsigned char)IG->p4;
				GTRACE5("A_DI_2",o1,ofs,o2,idsp,sh);
				offset = CPULONG(o1) +
				  (CPULONG(o2) << (sh & 0x1f)) + idsp;
				mem_ref += offset;
				check_v86_address_overflow(mode, offset);
			}
		}
		break;
	default:
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
			long idsp;
			unsigned char sh;
			unsigned int o;
			unsigned int ofs = IG->p0;
			if (mode & MLEA) {
				mem_ref = 0;
			}
			else {
				mem_ref = CPULONG(ofs);
			}
			idsp = IG->p1;
			o = IG->p2;
			sh = (unsigned char)IG->p3;
			GTRACE5("A_DI_2D",ofs,o,0xff,idsp,sh);
			offset = (CPULONG(o) << (sh & 0x1f)) + idsp;
			mem_ref += offset;
			check_v86_address_overflow(mode, offset);
		}
		break;
	case O_XLAT: {
		unsigned int ofs = IG->p0;
		unsigned int offset;
		GTRACE1("XLAT",ofs);
		mem_ref = CPULONG(ofs);
		offset = CPULONG(Ofs_EBX) + CPUBYTE(Ofs_AL);
		if (mode & ADDR16) {
			offset &= 0xFFFF;
		}
		mem_ref += offset;
		}
		break;
	case O_INT: {
		unsigned char intno = IG->p0;
		// Check bitmap, GPF if revectored
		if (test_bit(intno, &TheCPU.int_revectored)) {
			TheCPU.err = EXCP0D_GPF;
			mem_ref = (dosaddr_t)-1;
		}
		else {
			mem_ref = intno * 4;
		} }
		break;
	case O_MOVS_SetA: {
		unsigned int ofs = IG->p0;
		GTRACE1("O_MOVS_SetA",ofs);
		if (mode&ADDR16) {
		    if (mode&MOVSSRC) {
			mem_ref = CPULONG(ofs);
			mem_ref += CPUWORD(Ofs_SI);
		    } else {
			mem_ref = CPULONG(Ofs_XES);
			mem_ref += CPUWORD(Ofs_DI);

		    }
		}
		else {
		    if (mode&MOVSSRC) {
			mem_ref = CPULONG(ofs) + CPULONG(Ofs_ESI);
		    } else {
			mem_ref = CPULONG(Ofs_XES) + CPULONG(Ofs_EDI);
		    }
		}
		}
		break;
	}
	return mem_ref;
}

static unsigned int Gen_sim(const IGen *IG, dosaddr_t mem_ref)
{
	int op = IG->op;
	int mode = IG->mode;
	uint32_t S1, S2;
#if PROFILE >= 2
	hitimer_t t0 = 0;
	if (debug_level('e')) t0 = GETTSC();
#endif

	unsigned int P0 = (unsigned)-1;
	switch(op) {
	case A_SR_SH4: {	// real mode make base addr from seg
		unsigned int o = IG->p0;
		unsigned int v = CPUWORD(o);
		GTRACE1("A_SR_SH4",o);
		SetSegReal(DR1.w.l, o);
		DR1.d = v; // only needed for pushing old CS
		}
		break;
	case A_SR_PROT: {	// protected mode make base addr from seg
		unsigned int o = IG->p0;
		int e;
		GTRACE1("A_SR_PROT",o);
		e = SetSegProt_helper(DR1.w.l, o);
		if (e < 0)
			P0 = IG->p1;
		else
			DR1.d = e;
		}
		break;
	case L_NOP:
		GTRACE0("L_NOP");
		break;
	// Special case: CR0&0x3f
	case L_CR0:
		GTRACE0("L_CR0");
		DR1.d = CPULONG(Ofs_CR0) & 0x3f;
		break;

	case O_FOP: {
		unsigned char exop = (unsigned char)IG->p0;
		int reg = IG->p1;
		GTRACE2("O_FPOP",exop,reg);
		Fp87_op(exop, reg, mem_ref);
		int exs = TheCPU.fpus & 0x7f;
		if (debug_level('e')>3) {
		    e_printf("  %s\n", e_trace_fp());
		}
		if (exs) {
			e_printf("FPU: error status %02x\n",exs);
			if ((exs & ~TheCPU.fpuc) & 0x3f) {
				e_printf("FPU exception\n");
				TheCPU.err = EXCP10_COPR;
				P0 = FindPC((const unsigned char *)IG);
			}
		}
		}
		break;

	case L_REG: {
		unsigned int o = IG->p0;
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
		unsigned int o = IG->p0;
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
		unsigned int o1 = IG->p0;
		unsigned int o2 = IG->p1;
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
		unsigned int o = IG->p0;
		GTRACE1("S_DI_R",o);
		if (mode & DATA16)
			CPUWORD(o) = mem_ref;
		else
			CPULONG(o) = mem_ref;
		}
		break;
	case S_DI_IMM: {
		int v = IG->p0;
		dosaddr_t addr = mem_ref;
		if (mode&MBYTE) {
			GTRACE3("S_DI_IMM_B",0xff,0xff,v);
			sim_write_byte(addr, v);
		} else {
			GTRACE3("S_DI_IMM_WL",0xff,0xff,v);
			if (mode&DATA16) sim_write_word(addr, v);
			else sim_write_dword(addr, v);
		} }
		break;

	case L_IMM: {
		unsigned int o = IG->p0;
		int v = IG->p1;
		GTRACE3("L_IMM",o,0xff,v);
		if (mode & MBYTE) {
			CPUBYTE(o) = (signed char)v;
		}
		else if (mode & DATA16) {
			CPUWORD(o) = (short)v;
		}
		else {
			CPULONG(o) = v;
		} }
		break;
	case L_IMM_R1: {
		int v = IG->p0;
		GTRACE3("L_IMM_R1",0xff,0xff,v);
		if (mode & MBYTE) {
			DR1.b.bl = (signed char)v;
		}
		else if (mode & DATA16) {
			DR1.w.l = (short)v;
		}
		else {
			DR1.d = v;
		} }
		break;
	case L_MOVZS: {
		unsigned int o;
		int rcod = IG->p0;	// 0=z 1=s
		o = IG->p1;
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
		unsigned int o = IG->p0;
		GTRACE1("L_LXS1",o);
		if (mode&DATA16) {
		    CPUWORD(o) = DR1.w.l = sim_read_word(mem_ref);
		}
		else {
		    CPULONG(o) = DR1.d = sim_read_dword(mem_ref);
		} }
		break;
	case L_LXS2:	/* real mode segment base from segment value */
		GTRACE0("L_LXS2");
		DR1.d = sim_read_word(mem_ref + BT24(BitDATA16, mode));
		break;
	case L_ZXAX:
		GTRACE0("L_ZXAX");
		DR1.w.h = 0;
		break;

	case L_DI_R1: {
		dosaddr_t addr = mem_ref;
		GTRACE0("L_DI");
		if (mode & (MBYTE|MBYTX)) {
		    DR1.b.bl = sim_read_byte(addr);
		}
		else if (mode & DATA16) {
		    DR1.w.l = sim_read_word(addr);
		}
		else {
		    DR1.d = sim_read_dword(addr);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case S_DI: {
		dosaddr_t addr = mem_ref;
		GTRACE0("S_DI");
		if (mode&MBYTE) {
		    sim_write_byte(addr, DR1.b.bl);
		}
		else if (mode & DATA16) {
		    sim_write_word(addr, DR1.w.l);
		}
		else {
		    sim_write_dword(addr, DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;

	case O_ADD_R: {		// OSZAPC
		register wkreg v;
		v.d = IG->p0;
		if (mode & IMMED) {GTRACE3("O_ADD_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADD_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.bs.bl = S1 + S2;
		    FlagHandleAdd(S1, S2, DR1.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.ws.l = S1 + S2;
		    FlagHandleAdd(S1, S2, DR1.ws.l, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = S1 + S2;
		    FlagHandleAdd(S1, S2, DR1.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_OR_R: {		// O=0 SZP C=0
		int v = IG->p0;
		if (mode & IMMED) {GTRACE3("O_OR_R",0xff,0xff,v);}
		    else {GTRACE3("O_OR_R",v,0xff,v);}
		if (mode & MBYTE) {
		    if (!(mode & IMMED)) v = CPUBYTE(v);
		    RFL.res = DR1.bs.bl |= v;
		}
		else if (mode & DATA16) {
		    if (!(mode & IMMED)) v = CPUWORD(v);
		    RFL.res = DR1.ws.l |= v;
		}
		else {
		    if (!(mode & IMMED)) v = CPULONG(v);
		    RFL.res = DR1.d |= v;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_AND_R: {		// O=0 SZP C=0
		int v = IG->p0;
		if (mode & IMMED) {GTRACE3("O_AND_R",0xff,0xff,v);}
		    else {GTRACE3("O_AND_R",v,0xff,v);}
		if (mode & MBYTE) {
		    if (!(mode & IMMED)) v = CPUBYTE(v);
		    RFL.res = DR1.bs.bl &= v;
		}
		else if (mode & DATA16) {
		    if (!(mode & IMMED)) v = CPUWORD(v);
		    RFL.res = DR1.ws.l &= v;
		}
		else {
		    if (!(mode & IMMED)) v = CPULONG(v);
		    RFL.res = DR1.d &= v;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_XOR_R: {		// O=0 SZP C=0
		int v = IG->p0;
		if (mode & IMMED) {GTRACE3("O_XOR_R",0xff,0xff,v);}
		    else {GTRACE3("O_XOR_R",v,0xff,v);}
		if (mode & MBYTE) {
		    if (!(mode & IMMED)) v = CPUBYTE(v);
		    RFL.res = DR1.bs.bl ^= v;
		}
		else if (mode & DATA16) {
		    if (!(mode & IMMED)) v = CPUWORD(v);
		    RFL.res = DR1.ws.l ^= v;
		}
		else {
		    if (!(mode & IMMED)) v = CPULONG(v);
		    RFL.res = DR1.d ^= v;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_SUB_R: {		// OSZAPC
		register wkreg v;
		v.d = IG->p0;
		if (mode & IMMED) {GTRACE3("O_SUB_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_SUB_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.bs.bl = S1 - S2;
		    FlagHandleSub(S1, S2, DR1.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.ws.l = S1 - S2;
		    FlagHandleSub(S1, S2, DR1.ws.l, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = S1 - S2;
		    FlagHandleSub(S1, S2, DR1.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_CMP_R: {		// OSZAPC
		register wkreg v;
		v.d = IG->p0;
		if (mode & IMMED) {GTRACE3("O_CMP_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_CMP_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    FlagHandleSub(S1, S2, (int8_t)(S1 - S2), 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    FlagHandleSub(S1, S2, (int16_t)(S1 - S2), 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    FlagHandleSub(S1, S2, S1 - S2, 32);
		}
		}
		break;
	case O_ADC_R: {		// OSZAPC
		register wkreg v;
		int cy;
		v.d = IG->p0;
		cy = is_cf_set();
		if (mode & IMMED) {GTRACE3("O_ADC_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADC_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.bs.bl = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, DR1.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.ws.l = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, DR1.ws.l, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, DR1.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_SBB_R: {		// OSZAPC
		register wkreg v;
		int cy;
		v.d = IG->p0;
		cy = is_cf_set();
		if (mode & IMMED) {GTRACE3("O_SBB_R",0xff,0xff,v.d);}
		    else {GTRACE3("O_SBB_R",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = DR1.b.bl;
		    // movzbl v.b.bl->eax; add cy,eax; neg eax
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = CPUBYTE(v.bs.bl);
		    DR1.bs.bl = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, DR1.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = DR1.w.l;
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = CPUWORD(v.bs.bl);
		    DR1.ws.l = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, DR1.ws.l, 16);
		}
		else {
		    S1 = DR1.d;
		    if (mode & IMMED) S2 = v.d;
			else S2 = CPULONG(v.bs.bl);
		    DR1.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, DR1.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_CLEAR: {		// == XOR r,r
		unsigned int o = IG->p0;
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
		RFL.res = 0;
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_TEST: {		// == OR r,r
		unsigned int o = IG->p0;
		GTRACE1("O_TEST",o);
		if (mode & MBYTE) {
		    RFL.res = (int8_t)CPUBYTE(o);
		}
		else if (mode & DATA16) {
		    RFL.res = (int16_t)CPUWORD(o);
		}
		else {
		    RFL.res = CPULONG(o);
		}
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_SBSELF: {
		unsigned int o = IG->p0;
		GTRACE1("O_SBBSELF",o);
		// if CY=0 -> reg=0,  flag=xx46, OF=0
		// if CY=1 -> reg=-1, flag=xx97, OF=0
		if (is_cf_set()) {
		    RFL.res = 0xffffffff;
		    RFL.cout = LF_MASK_CF | LF_MASK_AF; // CF,AF set, OF=0
		}
		else {
		    RFL.res = 0;
		    RFL.cout = 0;
		}
		if (mode & MBYTE) {
		    CPUBYTE(o) = RFL.res;
		}
		else if (mode & DATA16) {
		    CPUWORD(o) = RFL.res;
		}
		else {
		    CPULONG(o) = RFL.res;
		}
		}
		break;
	case O_INC_R: {		// OSZAP
		unsigned int o = IG->p0;
		GTRACE1("O_INC_R",o);
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    CPUBYTE(o) = RFL.res = (int8_t)(S1 + 1);
		    FlagHandleIncDec(S1, RFL.res, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    CPUWORD(o) = RFL.res = (int16_t)(S1 + 1);
		    FlagHandleIncDec(S1, RFL.res, 16);
		}
		else {
		    S1 = CPULONG(o);
		    CPULONG(o) = RFL.res = S1 + 1;
		    FlagHandleIncDec(S1, RFL.res, 32);
		}
		}
		break;
	case O_DEC_R: {		// OSZAP
		unsigned int o = IG->p0;
		GTRACE1("O_DEC_R",o);
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    CPUBYTE(o) = RFL.res = (int8_t)(S1 - 1);
		    FlagHandleIncDec(RFL.res, S1, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    CPUWORD(o) = RFL.res = (int16_t)(S1 - 1);
		    FlagHandleIncDec(RFL.res, S1, 16);
		}
		else {
		    S1 = CPULONG(o);
		    CPULONG(o) = RFL.res = S1 - 1;
		    FlagHandleIncDec(RFL.res, S1, 32);
		}
		}
		break;
	case O_ADD_FR: {	// OSZAPC
		register wkreg v;
		unsigned int o = IG->p0;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		if (mode & IMMED) {GTRACE3("O_ADD_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADD_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = v.bs.bl = S1 + S2;
		    FlagHandleAdd(S1, S2, v.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = v.ws.l = S1 + S2;
		    FlagHandleAdd(S1, S2, v.ws.l, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = v.d = S1 + S2;
		    FlagHandleAdd(S1, S2, v.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_OR_FR: {		// O=0 SZP C=0
		register wkreg v;
		unsigned int o = IG->p0;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		if (mode & IMMED) {GTRACE3("O_OR_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_OR_FR",v.d,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.res = v.bs.bl = CPUBYTE(o) |= (mode & IMMED ? v.b.bl : DR1.b.bl);
		}
		else if (mode & DATA16) {
		    RFL.res = v.ws.l = CPUWORD(o) |= (mode & IMMED ? v.w.l : DR1.w.l);
		}
		else {
		    RFL.res = CPULONG(o) |= (mode & IMMED ? v.d : DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_ADC_FR: {	// OSZAPC
		register wkreg v;
		unsigned int o = IG->p0;
		int cy;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		cy = is_cf_set();
		if (mode & IMMED) {GTRACE3("O_ADC_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_ADC_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = v.bs.bl = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, v.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = v.ws.l = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, v.ws.l, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = v.d = S1 + S2 + cy;
		    FlagHandleAdd(S1, S2, v.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_SBB_FR: {	// OSZAPC
		register wkreg v;
		unsigned int o = IG->p0;
		int cy;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		cy = is_cf_set();
		if (mode & IMMED) {GTRACE3("O_SBB_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_SBB_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    // movzbl v.bs.bl->eax; add cy,eax; neg eax
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = v.bs.bl = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, v.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = v.ws.l = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, v.ws.l, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = v.d = S1 - S2 - cy;
		    FlagHandleSub(S1, S2, v.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_AND_FR: {		// O=0 SZP C=0
		register wkreg v;
		unsigned int o = IG->p0;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		if (mode & IMMED) {GTRACE3("O_AND_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_AND_FR",v.d,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.res = v.bs.bl = CPUBYTE(o) &= (mode & IMMED ? v.b.bl : DR1.b.bl);
		}
		else if (mode & DATA16) {
		    RFL.res = v.ws.l = CPUWORD(o) &= (mode & IMMED ? v.w.l : DR1.w.l);
		}
		else {
		    RFL.res = CPULONG(o) &= (mode & IMMED ? v.d : DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_SUB_FR: {	// OSZAPC
		register wkreg v;
		unsigned int o = IG->p0;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		if (mode & IMMED) {GTRACE3("O_SUB_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_SUB_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    CPUBYTE(o) = v.bs.bl = S1 - S2;
		    FlagHandleSub(S1, S2, v.bs.bl, 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    CPUWORD(o) = v.ws.l = S1 - S2;
		    FlagHandleSub(S1, S2, v.ws.l, 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    CPULONG(o) = v.d = S1 - S2;
		    FlagHandleSub(S1, S2, v.d, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		}
		break;
	case O_XOR_FR: {		// O=0 SZP C=0
		register wkreg v;
		unsigned int o = IG->p0;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		if (mode & IMMED) {GTRACE3("O_XOR_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_XOR_FR",v.d,0xff,v.d);}
		if (mode & MBYTE) {
		    RFL.res = v.bs.bl = CPUBYTE(o) ^= (mode & IMMED ? v.b.bl : DR1.b.bl);
		}
		else if (mode & DATA16) {
		    RFL.res = v.ws.l = CPUWORD(o) ^= (mode & IMMED ? v.w.l : DR1.w.l);
		}
		else {
		    RFL.res = CPULONG(o) ^= (mode & IMMED ? v.d : DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		RFL.cout &= LF_MASK_AF;
		}
		break;
	case O_CMP_FR: {	// OSZAPC
		register wkreg v;
		unsigned int o = IG->p0;
		v.d = 0;
		if (mode & IMMED) v.d = IG->p1;
		if (mode & IMMED) {GTRACE3("O_CMP_FR",0xff,0xff,v.d);}
		    else {GTRACE3("O_CMP_FR",v.bs.bl,0xff,v.d);}
		if (mode & MBYTE) {
		    S1 = CPUBYTE(o);
		    if (mode & IMMED) S2 = v.b.bl;
			else S2 = DR1.b.bl;
		    FlagHandleSub(S1, S2, (int8_t)(S1 - S2), 8);
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(o);
		    if (mode & IMMED) S2 = v.w.l;
			else S2 = DR1.w.l;
		    FlagHandleSub(S1, S2, (int16_t)(S1 - S2), 16);
		}
		else {
		    S1 = CPULONG(o);
		    if (mode & IMMED) S2 = v.d;
			else S2 = DR1.d;
		    FlagHandleSub(S1, S2, S1 - S2, 32);
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
		if (mode & MBYTE) {
			DR1.bs.bl = -(S2 = DR1.b.bl);
			FlagHandleSub(0, S2, DR1.bs.bl, 8);
		}
		else if (mode & DATA16) {
			DR1.ws.l = -(S2 = DR1.w.l);
			FlagHandleSub(0, S2, DR1.ws.l, 16);
		}
		else {
			DR1.d = -(S2 = DR1.d);
			FlagHandleSub(0, S2, DR1.d, 32);
		}
		break;
	case O_INC:		// OSZAP
		GTRACE0("O_INC");
		if (mode & MBYTE) {
			S1 = DR1.bs.bl;
			DR1.b.bl = RFL.res = (int8_t)(S1 + 1);
			FlagHandleIncDec(S1, RFL.res, 8);
		}
		else if (mode & DATA16) {
			S1 = DR1.ws.l;
			DR1.w.l = RFL.res = (int16_t)(S1 + 1);
			FlagHandleIncDec(S1, RFL.res, 16);
		}
		else {
			S1 = DR1.d;
			DR1.d = RFL.res = S1 + 1;
			FlagHandleIncDec(S1, RFL.res, 32);
		}
		break;
	case O_DEC:		// OSZAP
		GTRACE0("O_DEC");
		if (mode & MBYTE) {
			S1 = DR1.bs.bl;
			DR1.b.bl = RFL.res = (int8_t)(S1 - 1);
			FlagHandleIncDec(RFL.res, S1, 8);
		}
		else if (mode & DATA16) {
			S1 = DR1.ws.l;
			DR1.w.l = RFL.res = (int16_t)(S1 - 1);
			FlagHandleIncDec(RFL.res, S1, 16);
		}
		else {
			S1 = DR1.d;
			DR1.d = RFL.res = S1 - 1;
			FlagHandleIncDec(RFL.res, S1, 32);
		}
		break;
	case O_CMPXCHG: {		// OSZAPC
		unsigned int o = IG->p0;
		GTRACE1("O_CMPXCHG",o);
		if (mode & MBYTE) {
		    S1 = CPUBYTE(Ofs_AL);
		    S2 = DR1.b.bl;
		    FlagHandleSub(S1, S2, (int8_t)(S1 - S2), 8);
		    if (RFL.res == 0)
			DR1.b.bl = CPUBYTE(o);
		    else
			CPUBYTE(Ofs_AL) = DR1.b.bl;
		}
		else if (mode & DATA16) {
		    S1 = CPUWORD(Ofs_AX);
		    S2 = DR1.w.l;
		    FlagHandleSub(S1, S2, (int16_t)(S1 - S2), 16);
		    if (RFL.res == 0)
			DR1.w.l = CPUWORD(o);
		    else
			CPUWORD(Ofs_AX) = DR1.w.l;
		}
		else {
		    S1 = CPULONG(Ofs_EAX);
		    S2 = DR1.d;
		    FlagHandleSub(S1, S2, S1 - S2, 32);
		    if (RFL.res == 0)
			DR1.d = CPULONG(o);
		    else
			CPULONG(Ofs_EAX) = DR1.d;
		}
		}
		break;
	case O_XCHG: {
		unsigned int o = IG->p0;
		wkreg DR2;
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
		unsigned int o1 = IG->p0;
		unsigned int o2 = IG->p1;
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
		RFL.cout &= ~(LF_MASK_CF|LF_MASK_PO);
		if (of)
		    RFL.cout |= LF_MASK_CF; // also sets OF via PO
		}
		break;
	case O_IMUL: {		// OC
		int of;
		if (mode & MBYTE) {
		    if ((mode&(IMMED|DATA16))==(IMMED|DATA16)) {
			int b = IG->p0;
			unsigned int o = IG->p1;
			GTRACE3("O_IMUL",o,0xff,b);
			DR1.ds = (int)DR1.ws.l * b;
			CPUWORD(o) = DR1.w.l;
			DR1.ds >>= 15;
			of = ((DR1.ds!=0) && (DR1.ds!=-1));
		    }
		    else if ((mode&(IMMED|DATA16))==IMMED) {
			int b = IG->p0;
			unsigned int o = IG->p1;
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
			int b = IG->p0;
			unsigned int o = IG->p1;
			GTRACE3("O_IMUL",o,0xff,b);
		    	DR1.ds = (int)DR1.ws.l * b;
		    	CPUWORD(o) = DR1.w.l;
		    }
		    else if (mode&MEMADR) {
			unsigned int o = IG->p0;
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
			int b = IG->p0;
			unsigned int o = IG->p1;
			GTRACE3("O_IMUL",o,0xff,b);
			v = (int64_t)DR1.ds * b;
			CPULONG(o) = v & 0xffffffff;
		    }
		    else if (mode&MEMADR) {
			unsigned int o = IG->p0;
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
		RFL.cout &= ~(LF_MASK_CF|LF_MASK_PO);
		if (of)
		    RFL.cout |= LF_MASK_CF; // also sets OF via PO
		}
		break;

	case O_DIV:		// no flags
		GTRACE0("O_DIV");
		if (mode & MBYTE) {
			uint32_t v = CPUWORD(Ofs_AX);
			S1 = DR1.b.bl;
			if (S1==0)
			    TheCPU.err = EXCP00_DIVZ;
			else {
			    uint32_t rem = v % S1;
			    v /= S1;
			    if (v > 0xff)
				TheCPU.err = EXCP00_DIVZ;
			    else {
				CPUBYTE(Ofs_AL) = v;
				CPUBYTE(Ofs_AH) = rem;
			    }
			}
		}
		else {
			if (mode&DATA16) {
				uint32_t v;;
				v = ((uint32_t)CPUWORD(Ofs_DX) << 16) | CPUWORD(Ofs_AX);
				S1 = DR1.w.l;
				if (S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    uint32_t rem = v % S1;
				    v /= S1;
				    if (v > 0xffff)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPUWORD(Ofs_AX) = v;
					CPUWORD(Ofs_DX) = rem;
				    }
				}
			}
			else {
				u_int64_u v;
				v.t.tl = CPULONG(Ofs_EAX);
				v.t.th = CPULONG(Ofs_EDX);
				S1 = DR1.d;
				if (S1==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    uint32_t rem = v.td % S1;
				    v.td /= S1;
				    if (v.t.th)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPULONG(Ofs_EAX) = v.t.tl;
					CPULONG(Ofs_EDX) = rem;
				    }
		    		}
			}
		}
		if (TheCPU.err == EXCP00_DIVZ)
			P0 = LONG_CS + (dosaddr_t)IG->p0;
		break;
	case O_IDIV:		// no flags
		GTRACE0("O_IDIV");
		if (mode & MBYTE) {
			int32_t v;
			int32_t S = DR1.bs.bl;
			v = (signed short)CPUWORD(Ofs_AX);
			if (S==0)
			    TheCPU.err = EXCP00_DIVZ;
	    		else {
			    int32_t rem = v % S;
			    v /= S;
			    if (v > 127 || v < -128)
				TheCPU.err = EXCP00_DIVZ;
			    else {
				CPUBYTE(Ofs_AL) = v;
				CPUBYTE(Ofs_AH) = rem;
			    }
			}
		}
		else {
			if (mode&DATA16) {
				int32_t v;
				int32_t S = DR1.ws.l;
				v = ((uint32_t)CPUWORD(Ofs_DX) << 16) | CPUWORD(Ofs_AX);
				if (S==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    int32_t rem = v % S;
				    v /= S;
				    if (v > 32767 || v < -32768)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPUWORD(Ofs_AX) = v;
					CPUWORD(Ofs_DX) = rem;
				    }
		    		}
			}
			else {
				int64_t v;
				int32_t S = DR1.d;
				v = CPULONG(Ofs_EAX) |
				  ((uint64_t)CPULONG(Ofs_EDX) << 32);
				if (S==0)
				    TheCPU.err = EXCP00_DIVZ;
		    		else {
				    int32_t rem = v % S;
				    v /= S;
				    if (v > 0x7fffffffLL || v < -0x80000000LL)
					TheCPU.err = EXCP00_DIVZ;
				    else {
					CPULONG(Ofs_EAX) = v & 0xffffffff;
					CPULONG(Ofs_EDX) = rem;
				    }
		    		}
			}
		}
		if (TheCPU.err == EXCP00_DIVZ)
			P0 = LONG_CS + (dosaddr_t)IG->p0;
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
	case O_ROL: {		// O(if sh==1),C(if sh>0)
		unsigned int o = IG->p0;
		unsigned int sh, rbef, raft, cy;
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
			FlagHandleRotate(sh, cy, rbef, 8);
		}
		else if (mode & DATA16) {
			sh &= 15;
			rbef = DR1.w.l;
			raft = (rbef<<sh) | (rbef>>(16-sh));
			DR1.w.l = raft;
			cy = raft & 1;
			FlagHandleRotate(sh, cy, rbef, 16);
		}
		else {
			// sh != 0, else we "break"ed above.
			// sh < 32, so 32-sh is in range 1..31, which is OK.
			rbef = DR1.d;
			raft = (rbef<<sh) | (rbef>>(32-sh));
			DR1.d = raft;
			cy = raft & 1;
			FlagHandleRotate(sh, cy, rbef, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_RCL: {		// O(if sh==1),C(if sh>0)
		unsigned int o = IG->p0;
		unsigned int sh, rbef, raft, cy;
		GTRACE1("O_RCL",o);
		cy = is_cf_set();
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
			FlagHandleRotate(sh, cy, rbef, 8);
		}
		else if (mode & DATA16) {
			sh %= 17;
			if (!sh) break;
			rbef = DR1.w.l;
			raft = (rbef<<sh) | (rbef>>(17-sh)) | (cy<<(sh-1));
			DR1.w.l = raft;
			cy = (rbef>>(16-sh)) & 1;
			FlagHandleRotate(sh, cy, rbef, 16);
		}
		else {
			if (!sh) break;
			rbef = DR1.d;
			raft = (rbef<<sh) | (cy<<(sh-1));
			if (sh>1) raft |= (rbef>>(33-sh));
			DR1.d = raft;
			cy = (rbef>>(32-sh)) & 1;
			FlagHandleRotate(sh, cy, rbef, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_SHL: {		// O(if sh==1),SZPC(if sh>0)
		unsigned int o = IG->p0;
		unsigned int sh, rbef, raft=0, cy=0;
		GTRACE3("O_SHL",0xff,0xff,o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;	// happens on 286 and above, not on 8086
		if(!sh)
			// All flags unchanged (at least on PIII)
			break;

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

		if (mode & MBYTE) {
			cy = (raft & 0x100) != 0;
			FlagHandleShift(sh, cy, rbef, 8, DR1.bs.bl = raft);
		}
		else if (mode & DATA16) {
			cy = (raft & 0x10000) != 0;
			FlagHandleShift(sh, cy, rbef, 16, DR1.ws.l = raft);
		}
		else {
			cy = (rbef>>(32-sh)) & 1;
			FlagHandleShift(sh, cy, rbef, 32, DR1.d = raft);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_ROR: {		// O(if sh==1),C(if sh>0)
		unsigned int o = IG->p0;
		unsigned int sh, rbef, raft, cy;
		GTRACE1("O_ROR",o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if(!sh)
			break;	// All flags unmodified

		if (mode & MBYTE) {
			sh &= 7;
			rbef = DR1.b.bl;
			raft = (rbef>>sh) | (rbef<<(8-sh));
			DR1.b.bl = raft;
			cy = (raft & 0x80) != 0;
			FlagHandleRotate(sh, cy, raft, 8);
		}
		else if (mode & DATA16) {
			sh &= 15;
			rbef = DR1.w.l;
			raft = (rbef>>sh) | (rbef<<(16-sh));
			DR1.w.l = raft;
			cy = (raft & 0x8000) != 0;
			FlagHandleRotate(sh, cy, raft, 16);
		}
		else {
			// sh != 0, else we "break"ed above.
			// sh < 32, so 32-sh is in range 1..31, which is OK.
			rbef = DR1.d;
			raft = (rbef>>sh) | (rbef<<(32-sh));
			DR1.d = raft;
			cy = (raft & 0x80000000U) != 0;
			FlagHandleRotate(sh, cy, raft, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_RCR: {		// O(if sh==1),C(if sh>0)
		unsigned int o = IG->p0;
		unsigned int sh, rbef, raft, cy;
		GTRACE3("O_RCR",0xff,0xff,o);
		cy = is_cf_set();
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if(!sh)
			break;	// All flags unmodified

		if (mode & MBYTE) {
			sh %= 9;
			rbef = DR1.b.bl;
			raft = (rbef>>sh) | (rbef<<(9-sh)) | (cy<<(8-sh));
			DR1.b.bl = raft;
			if(sh)
				cy = (rbef>>(sh-1)) & 1;
			// else keep carry.
			FlagHandleRotate(sh, cy, raft, 8);
		}
		else if (mode & DATA16) {
			sh %= 17;
			rbef = DR1.w.l;
			raft = (rbef>>sh) | (rbef<<(17-sh)) | (cy<<(16-sh));
			DR1.w.l = raft;
			if(sh)
				cy = (rbef>>(sh-1)) & 1;
			FlagHandleRotate(sh, cy, raft, 16);
		}
		else {
			rbef = DR1.d;
			raft = (rbef>>sh) | (cy<<(32-sh));
			if (sh>1) raft |= (rbef<<(33-sh));
			DR1.d = raft;
			cy = (rbef>>(sh-1)) & 1;
			FlagHandleRotate(sh, cy, raft, 32);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_SHR: {		// O(if sh==1),SZPC(if sh>0)
		unsigned int o = IG->p0;
		unsigned int sh, rbef, raft, cy=0;
		GTRACE3("O_SHR",0xff,0xff,o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if (!sh)
			break;	// shift count 0, flags unchanged

		if (mode & MBYTE)
			rbef = DR1.b.bl;
		else if (mode & DATA16)
			rbef = DR1.w.l;
		else
			rbef = DR1.d;

		cy = (rbef >> (sh-1)) & 1;
		raft = rbef >> sh;

		if (mode & MBYTE)
			FlagHandleShift(sh, cy, raft, 8, DR1.bs.bl = raft);
		else if (mode & DATA16)
			FlagHandleShift(sh, cy, raft, 16, DR1.ws.l = raft);
		else
			FlagHandleShift(sh, cy, raft, 32, DR1.d = raft);
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;
	case O_SAR: {		// O(if sh==1),SZPC(if sh>0)
		unsigned int o = IG->p0;
		unsigned int sh, cy=0;
		signed int rbef, raft=0;
		GTRACE3("O_SHR",0xff,0xff,o);
		if (mode & IMMED) sh = o;
		  else sh = CPUBYTE(Ofs_CL);
		sh &= 31;
		if (!sh)
			break;	// shift count 0, flags unchanged

		if (mode & MBYTE)
			rbef = DR1.bs.bl;
		else if (mode & DATA16)
			rbef = DR1.ws.l;
		else
			rbef = DR1.ds;

		cy = (rbef >> (sh-1)) & 1;
		raft = rbef >> sh;

		if (mode & MBYTE)
			FlagHandleShift(sh, cy, raft, 8, DR1.bs.bl = raft);
		else if (mode & DATA16)
			FlagHandleShift(sh, cy, raft, 16, DR1.ws.l = raft);
		else
			FlagHandleShift(sh, cy, raft, 16, DR1.ds = raft);
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",raft);
		}
		break;

	case O_OPAX: {	/* used by DAA..AAD */
		int n =	IG->p0;
		// get n bytes from parameter stack
		unsigned char subop = IG->p1;
		GTRACE3("O_OPAX",0xff,0xff,n);
		uint32_t cy = is_cf_set();
		DR1.d = CPULONG(Ofs_EAX);
		switch (subop) {
			case DAA: {
				uint32_t cyaf = 0;
				unsigned char altmp = DR1.b.bl;
				if (((DR1.b.bl & 0x0f) > 9 ) || is_af_set()) {
					DR1.b.bl += 6;
					cyaf = ((uint32_t)(cy || (altmp > 0xf9))
						<< LF_BIT_CF) | LF_MASK_AF;
				}
				if ((altmp > 0x99) || cy) {
					DR1.b.bl += 0x60;
					cyaf |= LF_MASK_CF;
				}
				RFL.res = DR1.bs.bl; /* for flags */
				RFL.cout = cyaf;
				}
				break;
			case DAS: {
				uint32_t cyaf = 0;
				unsigned char altmp = DR1.b.bl;
				if (((altmp & 0x0f) > 9) || is_af_set()) {
					DR1.b.bl -= 6;
					cyaf = ((uint32_t)(cy || (altmp < 6))
						<< LF_BIT_CF) | LF_MASK_AF;
				}
				if ((altmp > 0x99) || cy) {
					DR1.b.bl -= 0x60;
					cyaf |= LF_MASK_CF;
				}
				RFL.res = DR1.bs.bl; /* for flags */
				RFL.cout = cyaf;
				}
				break;
			case AAA: {
				uint32_t cyaf;
				char icarry = (DR1.b.bl > 0xf9);
				if (((DR1.b.bl & 0x0f) > 9 ) || is_af_set()) {
					DR1.b.bl = (DR1.b.bl + 6) & 0x0f;
					DR1.b.bh = (DR1.b.bh + 1 + icarry);
					cyaf = LF_MASK_CF | LF_MASK_AF;
				} else {
					cyaf = 0;
					DR1.b.bl &= 0x0f;
				}
				RFL.cout = (RFL.cout &
				  ~(LF_MASK_AF|LF_MASK_CF|LF_MASK_PO)) | cyaf;
				}
				break;
			case AAS: {
				uint32_t cyaf;
				char icarry = (DR1.b.bl < 6);
				if (((DR1.b.bl & 0x0f) > 9 ) || is_af_set()) {
					DR1.b.bl = (DR1.b.bl - 6) & 0x0f;
					DR1.b.bh = (DR1.b.bh - 1 - icarry);
					cyaf = LF_MASK_CF | LF_MASK_AF;
				} else {
					cyaf = 0;
					DR1.b.bl &= 0x0f;
				}
				RFL.cout = (RFL.cout &
				  ~(LF_MASK_AF|LF_MASK_CF|LF_MASK_PO)) | cyaf;
				}
				break;
			case AAM: {
				signed char tmp = DR1.b.bl;
				int base = (signed char)IG->p2;
				DR1.b.bh = tmp / base;
				RFL.res = DR1.bs.bl = tmp % base;
				/* clear CF/OF/AF (undefined: found experimentally) */
				RFL.cout = 0;
				}
				break;
			case AAD: {
				int base = (signed char)IG->p2;
				DR1.w.l = ((DR1.b.bh * base) + DR1.b.bl) & 0xff;
				RFL.res = DR1.bs.bl; /* for flags */
				/* clear CF/OF/AF (undefined: found experimentally) */
				RFL.cout = 0;
				}
				break;
		}
		CPULONG(Ofs_EAX) = DR1.d;
		}
		break;

	case O_PUSH: {
		wkreg SR1, AR2;
		unsigned long stackm = CPULONG(Ofs_STACKM);
		GTRACE0("O_PUSH");
		if (mode & DATA16) {
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 2;
			SR1.d &= stackm;
			sim_write_word(AR2.d + SR1.d, DR1.w.l);
		}
		else {
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 4;
			SR1.d &= stackm;
			sim_write_dword(AR2.d + SR1.d, DR1.d);
		}
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
		SR1.d |= (CPULONG(Ofs_ESP) & ~stackm);
#endif
		CPULONG(Ofs_ESP) = SR1.d;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

/* PUSH derived (sub-)sequences: */
	case O_PUSH1:
		GTRACE0("O_PUSH1");
		SR1.d = CPULONG(Ofs_ESP);
		break;

	case O_PUSH2: {
		wkreg AR2;
		unsigned int o = IG->p0;
		unsigned long stackm = CPULONG(Ofs_STACKM);
		GTRACE1("O_PUSH2",o);
		AR2.d = CPULONG(Ofs_XSS);
		if (mode & DATA16) {
			DR1.w.l = CPUWORD(o);
			SR1.d -= 2;
			SR1.d &= stackm;
			sim_write_word(AR2.d + SR1.d, DR1.w.l);
		}
		else {
			DR1.d = (mode & SEGREG) ? CPUWORD(o) : CPULONG(o);
			SR1.d -= 4;
			SR1.d &= stackm;
			sim_write_dword(AR2.d + SR1.d, DR1.d);
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

	case O_PUSH3:
		GTRACE0("O_PUSH3");
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
		SR1.d |= (CPULONG(Ofs_ESP) & ~CPULONG(Ofs_STACKM));
#endif
		CPULONG(Ofs_ESP) = SR1.d;
		break;

	case O_PUSH2F: {
		wkreg SR1, AR2;
		unsigned long stackm = CPULONG(Ofs_STACKM);
		int ftmp;
		GTRACE0("O_PUSHF");
		ftmp = (CPULONG(Ofs_FLAGS) & ~EFLAGS_CC) | FlagSync_All();
#if 0		// unused "extended PVI", if used should move to separate op
		if (!V86MODE() && IOPL < 3 && (TheCPU.cr[4] & CR4_PVI))
			ftmp = (ftmp & ~(EFLAGS_IF|EFLAGS_VIF)) | ((ftmp & EFLAGS_VIF) ? EFLAGS_IF : 0);
#endif
		ftmp &= (RETURN_MASK|EFLAGS_IF);
		AR2.d = CPULONG(Ofs_XSS);
		SR1.d = CPULONG(Ofs_ESP);
		if (mode & DATA16) {
			SR1.d = (SR1.d - 2) & stackm;
			sim_write_word(AR2.d + SR1.d, ftmp);
		}
		else {
			SR1.d = (SR1.d - 4) & stackm;
			sim_write_dword(AR2.d + SR1.d, ftmp);
		}
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
		SR1.d |= (CPULONG(Ofs_ESP) & ~stackm);
#endif
		CPULONG(Ofs_ESP) = SR1.d;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",ftmp);
		} break;

	case O_PUSHI: {
		wkreg SR1, AR2;
		int v = IG->p0;
		unsigned long stackm = CPULONG(Ofs_STACKM);
		GTRACE3("O_PUSHI",0xff,0xff,v);
		if (mode & DATA16) {
			DR1.w.l = (short)v;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 2;
			SR1.d &= stackm;
			sim_write_word(AR2.d + SR1.d, DR1.w.l);
		}
		else {
			DR1.d = v;
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP) - 4;
			SR1.d &= stackm;
			sim_write_dword(AR2.d + SR1.d, DR1.d);
		}
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
		SR1.d |= (CPULONG(Ofs_ESP) & ~stackm);
#endif
		CPULONG(Ofs_ESP) = SR1.d;
		} break;

	case O_POP: {
		wkreg SR1, AR2;
		int imm16 = (mode&MRETISP) ? IG->p0 : 0;
		long stackm = CPULONG(Ofs_STACKM);
		GTRACE1("O_POP",imm16);
		if (mode & DATA16) {
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP);
			SR1.d &= stackm;
			DR1.w.l = sim_read_word(AR2.d + SR1.d);
			SR1.d += 2 + imm16;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
		}
		else {
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d = CPULONG(Ofs_ESP);
			SR1.d &= stackm;
			DR1.d = sim_read_dword(AR2.d + SR1.d);
			SR1.d += 4 + imm16;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
		}
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
		SR1.d |= (CPULONG(Ofs_ESP) & ~stackm);
#endif
		CPULONG(Ofs_ESP) = SR1.d;
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

/* POP derived (sub-)sequences: */
	case O_POP1:
		GTRACE0("O_POP1");
		SR1.d = CPULONG(Ofs_ESP);
		break;

	case O_POP2: {
		wkreg AR2;
		unsigned int o = (mode & MNOREG) ? 0 : IG->p0;
		long stackm = CPULONG(Ofs_STACKM);
		int imm16 = (mode&MRETISP) ? IG->p0 : 0;
		GTRACE2("O_POP2",o,imm16);
		AR2.d = CPULONG(Ofs_XSS);
		if (mode & DATA16) {
			SR1.d &= stackm;
			DR1.w.l = sim_read_word(AR2.d + SR1.d);
			if (!(mode & MNOREG))
				CPUWORD(o) = DR1.w.l;
			SR1.d += 2 + imm16;
		}
		else {
			SR1.d &= stackm;
			DR1.d = sim_read_dword(AR2.d + SR1.d);
			if (!(mode & MNOREG))
				CPULONG(o) = DR1.d;
			SR1.d += 4 + imm16;
		}
		if (debug_level('e')>3) dbug_printf("(V) %08x\n",DR1.d);
		} break;

	case O_POP3:
		GTRACE0("O_POP3");
#ifdef STACK_WRAP_MP	/* mask after incrementing */
		SR1.d &= CPULONG(Ofs_STACKM);
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
		SR1.d |= (CPULONG(Ofs_ESP) & ~CPULONG(Ofs_STACKM));
#endif
		CPULONG(Ofs_ESP) = SR1.d;
		break;

	case O_LEAVE: {
		wkreg SR1, AR2;
		long stackm = CPULONG(Ofs_STACKM);
		GTRACE0("O_LEAVE");
		if (mode & DATA16) {
			SR1.d = CPUWORD(Ofs_BP);
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d &= stackm;
			DR1.w.l = sim_read_word(AR2.d + SR1.d);
			CPUWORD(Ofs_BP) = DR1.w.l;
			SR1.d += 2;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
		}
		else {
			SR1.d = CPULONG(Ofs_EBP);
			AR2.d = CPULONG(Ofs_XSS);
			SR1.d &= stackm;
			DR1.d = sim_read_dword(AR2.d + SR1.d);
			CPULONG(Ofs_EBP) = DR1.d;
			SR1.d += 4;
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			SR1.d &= stackm;
#endif
		}
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
		SR1.d |= (CPULONG(Ofs_ESP) & ~stackm);
#endif
		CPULONG(Ofs_ESP) = SR1.d;
		}
		break;

	case O_SIM: {
		uint32_t flags = FlagSync_All();
		DR1.d = Sim_helper(mem_ref, DR1.d, mode,
				   &flags, IG->p0, IG->p1, currentIG);
		FlagSync_RFL(flags);
		if (TheCPU.err)
			P0 = IG->p2;
		break;
		}

	case O_MOVS_MovD: {
		wkreg SR1, DR2, AR1, AR2;
		unsigned int i;
		DR2.d = CPUWORD(Ofs_SI); /* for overflow calc */
		SR1.d = CPUWORD(Ofs_DI); /* for overflow calc */
		AR1.d = mem_ref;
		AR2.d = CPULONG(Ofs_XES);
		if (mode&ADDR16) {
		    AR2.d += CPUWORD(Ofs_DI);
		    i = (mode&(MREP|MREPNE)? CPUWORD(Ofs_CX) : 1);
		} else {
		    AR2.d += CPULONG(Ofs_EDI);
		    i = (mode&(MREP|MREPNE)? CPULONG(Ofs_ECX) : 1);
		}
		do {
		unsigned int minofs, bytesbefore, rest = 0;
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		dosaddr_t src, dest;
		GTRACE4("O_MOVS_MovD",0xff,0xff,df,i);
		if(i == 0)
		    break;
		if(mode & ADDR16) {
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
			    TheCPU.err = EXCP0D_GPF;
			    P0 = FindPC((const unsigned char *)IG);
			    break;
			}

			/* do maximal possible amount */
			possible = minofs / (OPSIZE(mode));
			if (df < 0)
			    possible++;
			rest = i - possible;
			i = possible;
		    }
		}
		dest = AR2.d;
		src = mem_ref;
		if (df<0) {
		    if (mode&MBYTE) {
			while (i--) sim_write_byte(dest--, sim_read_byte(src--));
		    }
		    else if (mode&DATA16) {
			while (i--) { sim_write_word(dest, sim_read_word(src));
			    dest -= 2; src -= 2; }
		    }
		    else {
			while (i--) { sim_write_dword(dest, sim_read_dword(src));
			    dest -= 4; src -= 4; }
		    }
		}
	    	else {
		    if (mode&MBYTE) {
			while (i--) sim_write_byte(dest++, sim_read_byte(src++));
		    }
		    else if (mode&DATA16) {
			while (i--) { sim_write_word(dest, sim_read_word(src));
			    dest += 2; src += 2; }
		    }
		    else {
			while (i--) { sim_write_dword(dest, sim_read_dword(src));
			    dest += 4; src += 4; }
		    }
		}
		i = rest;
		AR2.d = dest;
		AR1.d = src;
		if(rest == 0) break;
		/* emulate overflow */
		if (df == -1) {
		    if(SR1.d == minofs) {
			AR2.d -= df*0x10000;
			SR1.d = 0xffff;
			DR2.d -= minofs;
			if (DR2.d > 0)
			    DR2.d -= 1;
			else
			    DR2.d = 0xffff;
		    }
		    if(DR2.d == minofs) {
			AR1.d -= df*0x10000;
			DR2.d = 0xffff;
			SR1.d -= minofs;
			if (SR1.d > 0)
			    SR1.d -= 1;
			else
			    SR1.d = 0xffff;
		    }
		} else {
		    if(SR1.d == 0x10000 - minofs) {
			AR2.d -= df*0x10000;
			SR1.d = 0;
			DR2.d += minofs;
			DR2.d &= 0xffff;
		    }
		    if(DR2.d == 0x10000 - minofs) {
			AR1.d -= df*0x10000;
			DR2.d = 0;
			SR1.d += minofs;
			SR1.d &= 0xffff;
		    }
		}
		} while (1);
		}
		break;
	case O_MOVS_LodD: {
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		dosaddr_t addr;
		unsigned int i;
		if (mode&ADDR16)
		    i = (mode&(MREP|MREPNE)? CPUWORD(Ofs_CX) : 1);
		else
		    i = (mode&(MREP|MREPNE)? CPULONG(Ofs_ECX) : 1);
		GTRACE4("O_MOVS_LodD",0xff,0xff,df,i);
		if (mode&(MREP|MREPNE))	{
		    dbug_printf("odd: REP LODS %d\n",i);
		}
		addr = mem_ref;
		if (mode&MBYTE) {
		    while (i--) { DR1.b.bl = sim_read_byte(addr); addr += df; }
		}
		else if (mode&DATA16) {
		    while (i--) { DR1.w.l = sim_read_word(addr); addr += 2*df; }
		}
		else {
		    while (i--) { DR1.d = sim_read_dword(addr); addr += 4*df; }
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		}
		break;
	case O_MOVS_StoD: {
		wkreg SR1, AR1;
		unsigned int i;
		SR1.d = CPUWORD(Ofs_DI); /* for overflow calc */
		AR1.d = mem_ref;
		if (mode&ADDR16)
		    i = (mode&(MREP|MREPNE)? CPUWORD(Ofs_CX) : 1);
		else
		    i = (mode&(MREP|MREPNE)? CPULONG(Ofs_ECX) : 1);

		do {
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		dosaddr_t addr;
		unsigned int rest = 0;
		GTRACE4("O_MOVS_StoD",0xff,0xff,df,i);
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
				TheCPU.err = EXCP0D_GPF;
				P0 = FindPC((const unsigned char *)IG);
				break;
			}
			possible = minofs / (OPSIZE(mode));
			if (df < 0)
			    possible++;
			rest = i - possible;
			i = possible;
		    }
		}
		addr = AR1.d;
		if (mode&MBYTE) {
		    while (i--) { sim_write_byte(addr, DR1.b.bl); addr += df; }
		}
		else if (mode&DATA16) {
		    while (i--) { sim_write_word(addr, DR1.w.l); addr += 2*df; }
		}
		else {
		    while (i--) { sim_write_dword(addr, DR1.d); addr += 4*df; }
		}
		AR1.d = addr;
		i = rest;
		if (rest == 0) break;
		SR1.d = (df == -1 ? 0xffff : 0);
		AR1.d -= 0x10000*df;
		} while (1);
		}
		break;
	case O_MOVS_ScaD: {	// OSZAPC, never called with rep
		int df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		dosaddr_t addr;
		unsigned int i, di;
		char k, z;
		if (mode&ADDR16) {
		    i = CPUWORD(Ofs_CX);
		    di = CPUWORD(Ofs_DI);
		} else {
		    i = CPULONG(Ofs_ECX);
		    di = CPULONG(Ofs_EDI);
		}
		GTRACE4("O_MOVS_ScaD",0xff,0xff,df,i);
		if (i == 0) break; /* eCX = 0, no-op, no flags updated */
		z = k = (mode&MREP? 1:0);
		addr = mem_ref - di;
		while (i && (z==k)) {
		    if (mode&MBYTE) {
			S1 = DR1.b.bl;
			S2 = sim_read_byte(addr+di);
			FlagHandleSub(S1, S2, (int8_t)(S1 - S2), 8);
			di += df;
		    }
		    else if (mode&DATA16) {
			S1 = DR1.w.l;
			S2 = sim_read_word(addr+di);
			FlagHandleSub(S1, S2, (int16_t)(S1 - S2), 16);
			di += 2*df;
		    }
		    else {
			S1 = DR1.d;
			S2 = sim_read_dword(addr+di);
			FlagHandleSub(S1, S2, S1 - S2, 32);
			di += 4*df;
		    }
		    if (mode&ADDR16) di &= 0xffff;
		    z = (RFL.res==0);
		    i--;
		}
		if (mode&ADDR16) {
		    CPUWORD(Ofs_CX) = i;
		    CPUWORD(Ofs_DI) = di;
		}
		else {
		    CPULONG(Ofs_ECX) = i;
		    CPULONG(Ofs_EDI) = di;
		}
		}
		break;
	case O_MOVS_CmpD: {	// OSZAPC
		int df;
		dosaddr_t addr1, addr2;
		unsigned int i, si, di;
		char k, z;
		df = (CPUWORD(Ofs_FLAGS) & EFLAGS_DF? -1:1);
		addr1 = CPULONG(Ofs_XES);
		di = (mode&ADDR16) ? CPUWORD(Ofs_DI) : CPULONG(Ofs_EDI);
		if(!(mode & (MREP|MREPNE))) {
			GTRACE4("O_MOVS_CmpD",0xff,0xff,df,1);
			// assumes DR1=*AR1
			if (mode&MBYTE) {
				S1 = DR1.b.bl;
				S2 = sim_read_byte(addr1+di);
				FlagHandleSub(S1, S2, (int8_t)(S1 - S2), 8);
			} else if (mode&DATA16) {
				S1 = DR1.w.l;
				S2 = sim_read_word(addr1+di);
				FlagHandleSub(S1, S2, (int16_t)(S1 - S2), 16);
			} else {
				S1 = DR1.d;
				S2 = sim_read_dword(addr1+di);
				FlagHandleSub(S1, S2, S1 - S2, 32);
			}
			break;
		}
		i = (mode&ADDR16) ? CPUWORD(Ofs_CX) : CPULONG(Ofs_ECX);
		GTRACE4("O_MOVS_CmpD",0xff,0xff,df,i);
		if (i == 0) break; /* eCX = 0, no-op, no flags updated */
		z = k = (mode&MREP? 1:0);
		si = (mode&ADDR16) ? CPUWORD(Ofs_SI) : CPULONG(Ofs_ESI);
		addr2 = mem_ref - si;
		while (i && (z==k)) {
		    if (mode&MBYTE) {
			S1 = sim_read_byte(addr2+si);
			S2 = sim_read_byte(addr1+di);
			FlagHandleSub(S1, S2, (int8_t)(S1 - S2), 8);
			si += df; di += df;
		    }
		    else if (mode&DATA16) {
			S1 = sim_read_word(addr2+si);
			S2 = sim_read_word(addr1+di);
			FlagHandleSub(S1, S2, (int16_t)(S1 - S2), 16);
			si += 2*df; di += 2*df;
		    }
		    else {
			S1 = sim_read_dword(addr2+si);
			S2 = sim_read_dword(addr1+di);
			FlagHandleSub(S1, S2, S1 - S2, 32);
			si += 4*df; di += 4*df;
		    }
		    if (mode&ADDR16) {si &= 0xffff; di &= 0xffff;}
		    z = (RFL.res==0);
		    i--;
		}
		if (mode&ADDR16) {
		    CPUWORD(Ofs_SI) = si;
		    CPUWORD(Ofs_DI) = di;
		    CPUWORD(Ofs_CX) = i;
		} else {
		    CPULONG(Ofs_ESI) = si;
		    CPULONG(Ofs_EDI) = di;
		    CPULONG(Ofs_ECX) = i;
		}
		}
		break;

	case O_MOVS_SavA: {
		unsigned int ofs = IG->p0;
		GTRACE1("O_MOVS_SavA",ofs);
		if (!(mode&(MREP|MREPNE)) || !(mode&MREPCOND)) {
		    unsigned int count = (mode&(MREP|MREPNE)) ? CPULONG(Ofs_ECX):1;
		    wkreg DR2;
		    // %%edx set to DF's increment
		    DR2.d = (signed char)CPUBYTE(Ofs_DF_INCREMENTS+OPSIZEBIT(mode));
		    if(mode & MOVSSRC) {
			if (mode & ADDR16)
			    CPUWORD(Ofs_SI) += DR2.w.l * (count & 0xffff);
			else
			    CPULONG(Ofs_ESI) += DR2.d * count;
		    }
		    if(mode & MOVSDST) {
			if (mode & ADDR16)
			    CPUWORD(Ofs_DI) += DR2.w.l * (count & 0xffff);
			else
			    CPULONG(Ofs_EDI) += DR2.d * count;
		    }
		    if(mode&(MREP|MREPNE)) {
			if (mode & ADDR16)
			    CPUWORD(Ofs_CX) = 0;
			else
			    CPULONG(Ofs_ECX) = 0;
		    }
		}
		}
		break;
	case O_SLAHF: {
		int rcod = IG->p0&1;	// 0=LAHF 1=SAHF
		if (rcod==0) {		/* LAHF */
			GTRACE0("O_LAHF");
			CPUBYTE(Ofs_AH) = FlagSync_SZAPC();
		}
		else {			/* SAHF */
			GTRACE0("O_SAHF");
			FlagSync_RFL(FlagSync_O() | CPUBYTE(Ofs_AH));
		} }
		break;
	case O_SETFL: {
		unsigned char o1 = (unsigned char)IG->p0;
		switch(o1) {	// these are direct on x86
		case CMC:
			GTRACE0("O_CMC");
			RFL.cout ^= LF_MASK_CF | LF_MASK_PO;
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
		case CLI:
			CPULONG(Ofs_EFLAGS) &= ~EFLAGS_IF;
			break;
		case INT:
			GTRACE0("O_INT");
			CPUWORD(Ofs_FLAGS) &= ~(EFLAGS_IF|EFLAGS_TF);
			break;
		} }
		break;
	case O_BSWAP: {
		unsigned char o1 = (unsigned char)IG->p0;
		register long v;
		GTRACE1("O_BSWAP",o1);
		v = CPULONG(o1);
		v = ((v & 0xff00ff00) >> 8) | ((v & 0x00ff00ff) << 8);
		v = ((v & 0xffff0000) >> 16) | ((v & 0x0000ffff) << 16);
		CPULONG(o1) = v;
		}
		break;
	case O_SETCC: {
		unsigned char o1 = (unsigned char)IG->p0;
		GTRACE3("O_SETCC",0xff,0xff,o1);
		switch(o1) {
			case 0x00: DR1.b.bl = is_of_set(); break;
			case 0x01: DR1.b.bl = !is_of_set(); break;
			case 0x02: DR1.b.bl = is_cf_set(); break;
			case 0x03: DR1.b.bl = !is_cf_set(); break;
			case 0x04: DR1.b.bl = is_zf_set(); break;
			case 0x05: DR1.b.bl = !is_zf_set(); break;
			case 0x06: DR1.b.bl = is_cf_set() || is_zf_set(); break;
			case 0x07: DR1.b.bl = !is_cf_set() && !is_zf_set(); break;
			case 0x08: DR1.b.bl = is_sf_set(); break;
			case 0x09: DR1.b.bl = !is_sf_set(); break;
			case 0x0a:
				e_printf("!!! SETp\n");
				DR1.b.bl = is_pf_set(); break;
			case 0x0b:
				e_printf("!!! SETnp\n");
				DR1.b.bl = !is_pf_set(); break;
			case 0x0c: DR1.b.bl = is_sf_set() ^ is_of_set(); break;
			case 0x0d: DR1.b.bl = !(is_sf_set() ^ is_of_set()); break;
			case 0x0e: DR1.b.bl = (is_sf_set() ^ is_of_set()) || is_zf_set(); break;
			case 0x0f: DR1.b.bl = !(is_sf_set() ^ is_of_set()) && !is_zf_set(); break;
		}
		}
		break;
	case O_BITOP: {
		wkreg DR2;
		unsigned char o1 = (unsigned char)IG->p0;
		unsigned int o2 = IG->p1;
		GTRACE3("O_BITOP",o2,0xff,o1);
		if (o1 == 0x1c || o1 == 0x1d) { /* bsf/bsr */
			if (mode & DATA16) DR1.d = DR1.w.l;
			DR1.d = o1 == 0x1c ? find_bit(DR1.d) : find_bit_r(DR1.d);
			if (DR1.d == -1) {
				SET_ZF(1);
			} else {
				SET_ZF(0);
				if (mode & DATA16)
					CPUWORD(o2) = DR1.d;
				else
					CPULONG(o2) = DR1.d;
			}
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
		if (!(mode & RM_REG) && o1 < 0x20) {
			/* add bit offset to effective address */
			mem_ref += (mode&DATA16) ? 2*(DR2.d>>4) : 4*(DR2.d>>5);
			if (mode & DATA16) {
				DR1.d = sim_read_word(mem_ref);
			}
			else {
				DR1.d = sim_read_dword(mem_ref);
			}
		}
		if (mode & DATA16) DR1.d = DR1.w.l;
		if ((o1 & ~0x18) == 0x03 || (o1 & ~0x18) == 0x20) {
			SET_CF((DR1.d >> DR2.d) & 1);
			switch (o1 & 0x18) {
			case 0x00: break;
			case 0x08: DR1.d |= 1U << DR2.d; break;
			case 0x10: DR1.d &= ~(1U << DR2.d); break;
			case 0x18: DR1.d ^= 1U << DR2.d; break;
			default: break;
			}
		}
		if (!(mode & RM_REG) && o1 < 0x20 && o1 != 0x03) {
			if (mode & DATA16) {
				sim_write_word(mem_ref, DR1.w.l);
			}
			else {
				sim_write_dword(mem_ref, DR1.d);
			}
		}
		} break;
	case O_SHFD: {
		unsigned char l_r = (unsigned char)IG->p0&8;
		unsigned int o = IG->p1;
		unsigned char shc;
		int cy;
		if (mode & IMMED) {
			shc = (unsigned char)IG->p2&0x1f;
			GTRACE4("O_SHFD",o,0xff,l_r,shc);
		}
		else {
			shc = CPUBYTE(Ofs_CL)&0x1f;
			GTRACE3("O_SHFD",o,0xff,l_r);
		}
		shc &= 31;
		if (shc==0) break;
		RFL.cout &= LF_MASK_AF;
		if (mode & DATA16) {
			if (l_r==0) {	// left:  <<reg|mem<<
				DR1.w.h = DR1.w.l;
				DR1.w.l = CPUWORD(o);
				RFL.cout |= (uint32_t)DR1.w.h << 16;
				/* undocumented: works like rotate internally */
				DR1.d = (DR1.d << shc) | (DR1.d >> (32-shc));
				cy = DR1.d & 1;
				RFL.res = DR1.ws.l = DR1.ws.h;
			}
			else {		// right: >>mem|reg>>
				DR1.w.h = CPUWORD(o);
				/* undocumented: works like rotate internally */
				DR1.d = (DR1.d >> shc) | (DR1.d << (32-shc));
				cy = (DR1.d >> 31) & 1;
				RFL.res = DR1.ws.l;
				RFL.cout |= RFL.res << 16;
			}
		}
		else {
			u_int64_u v;
			if (l_r==0) {	// left:  <<reg|mem<<
				v.t.tl = CPULONG(o);
				v.t.th = DR1.d;
				RFL.cout |= v.t.th & (LF_MASK_CF | LF_MASK_PO);
				cy = (v.td >> (64-shc)) & 1;
				v.td <<= shc;
				RFL.res = DR1.d = v.t.th;
			}
			else {		// right: >>mem|reg>>
				v.t.th = CPULONG(o);
				v.t.tl = DR1.d;
				cy = (v.td >> (shc-1)) & 1;
				v.td >>= shc;
				RFL.res = DR1.d = v.t.tl;
				RFL.cout |= RFL.res & (LF_MASK_CF | LF_MASK_PO);
			}
		}
		SET_CF(cy);
		} break;

	case O_RDTSC: {		// don't trust this one
#if 0
		hitimer_u t0, t1;
		GTRACE0("O_RDTSC");
		t0.td = GETTSC();
		if (eTimeCorrect >= 0) {
			t1.td = t0.td - TheCPU.EMUtime;
			TheCPU.EMUtime = t1.td;
		}
		CPULONG(Ofs_EAX) = t0.t.tl;
		CPULONG(Ofs_EDX) = t0.t.th;
#else
		error("rdtsc not implemented\n");
#endif
		}
		break;

	case JMP_TAILCODE: {	// retaddr
		P0 = (unsigned int)IG->p0;
		if (debug_level('e')>2) {
			dbug_printf("** Tail code: return from %08x\n",P0);
		} }
		break;

	case JMP_INDIRECT:
		P0 = LONG_CS + ((mode & DATA16) ? DR1.w.l : DR1.d);
		if (debug_level('e')>2)
			dbug_printf("** Jump taken to %08x\n",P0);
		break;

	case JMP_LINK:		// dspt
		P0 = (unsigned int)IG->p0;
		if (debug_level('e')>2) {
			dbug_printf("** Jump taken to %08x\n",P0);
		}
		break;

	case JF_LINK:
	case JB_LINK: {		// opc, PC, dspt, dspnt, link
		int opc = IG->p0;
		unsigned int j_t = (unsigned int)IG->p1;
		unsigned int j_nt = (unsigned int)IG->p2;
		switch(opc) {
		case JO:      P0 = is_of_set() ? j_t : j_nt; break;
		case JNO:     P0 = !is_of_set() ? j_t : j_nt; break;
		case JB_JNAE: P0 = is_cf_set() ? j_t : j_nt; break;
		case JNB_JAE: P0 = !is_cf_set() ? j_t : j_nt; break;
		case JE_JZ:   P0 = is_zf_set() ? j_t : j_nt; break;
		case JNE_JNZ: P0 = !is_zf_set() ? j_t : j_nt; break;
		case JBE_JNA: P0 = is_cf_set() || is_zf_set() ? j_t : j_nt; break;
		case JNBE_JA: P0 = !is_cf_set() && !is_zf_set() ? j_t : j_nt; break;
		case JS:      P0 = is_sf_set() ? j_t : j_nt; break;
		case JNS:     P0 = !is_sf_set() ? j_t : j_nt; break;
		case JP_JPE:
			e_printf("!!! JPset\n");
			P0 = is_pf_set() ? j_t : j_nt; break;
		case JNP_JPO:
			e_printf("!!! JPclr\n");
			P0 = !is_pf_set() ? j_t : j_nt; break;
		case JL_JNGE:
			P0 = is_sf_set() ^ is_of_set() ? j_t : j_nt; break;
		case JNL_JGE:
			P0 = !(is_sf_set() ^ is_of_set()) ? j_t : j_nt; break;
		case JLE_JNG:
			P0 = (is_sf_set() ^ is_of_set()) || is_zf_set() ? j_t : j_nt; break;
		case JNLE_JG:
			P0 = !(is_sf_set() ^ is_of_set()) && !is_zf_set() ? j_t : j_nt; break;
		case JCXZ:
			P0 = ((mode&ADDR16? rCX : rECX) == 0) ? j_t : j_nt; break;
		}
		if (debug_level('e')>2 && P0 == j_t) dbug_printf("** Jump taken to %08x\n",j_t);
		}
		break;
	case JLOOP_LINK: {	// opc, dspt, dspnt, link
		int opc = IG->p0;
		unsigned int j_t = (unsigned int)IG->p1;
		unsigned int j_nt = (unsigned int)IG->p2;
		int cxv = (mode&ADDR16? --rCX : --rECX);
		switch(opc) {
		case LOOP:
			P0 = cxv != 0 ? j_t : j_nt; break;
		case LOOPZ_LOOPE:
			P0 = cxv != 0 && is_zf_set() ? j_t : j_nt; break;
		case LOOPNZ_LOOPNE:
			P0 = cxv != 0 && !is_zf_set() ? j_t : j_nt; break;
		}
		if (debug_level('e')>2 && P0 == j_t) dbug_printf("** Jump taken to %08x\n",j_t);
		}
		break;
	default:
		leavedos_main(0x494c4c);
		break;

	}

#ifdef DEBUG_MORE
	if (debug_level('e')>3) {
#else
	if (debug_level('e')>6) {
#endif
	    dbug_printf("(R) DR1=%08x AR1=%08x\n",
		DR1.d,mem_ref);
	    dbug_printf("(R) RFL cout=%08x RES=%08x\n",
		RFL.cout,RFL.res);
//	    if (debug_level('e')==9) dbug_printf("\n%s",e_print_regs());
	}

#if PROFILE >= 2
	if (debug_level('e')) GenTime += (GETTSC() - t0);
#endif
	return P0;
}


/////////////////////////////////////////////////////////////////////////////


static unsigned char *CodeGen_sim(unsigned char *CodePtr, unsigned char *BaseGenBuf, const IGen *IG)
{
	memcpy(CodePtr, IG, sizeof(*IG));
	return CodePtr + sizeof(*IG);
}

static unsigned Exec_sim(unsigned *pmem_ref, unsigned long *flg,
			 unsigned char *ecpu, void *SeqStart,
			 unsigned short seqflg)
{
	IGen *IG = SeqStart;
	unsigned int P0;
	dosaddr_t mem_ref = 0;

	if (seqflg & F_FPOP)
		/* mask all exceptions, and set rounding properly */
		fp87_mask_except();

	FlagSync_RFL(*flg);
	do {
		if (IG->op <= O_MOVS_SetA) {
			mem_ref = AddrGen_sim(IG);
			if (TheCPU.err == EXCP0D_GPF) {
				if (IG->op == O_INT)
					P0 = (dosaddr_t)IG->p1;
				else
					P0 =  FindPC((unsigned char *)IG);
				break;
			}
			IG++;
		} // no need for "else", a non-addr op always follows
		currentIG = (unsigned char *)IG;
		P0 = Gen_sim(IG, mem_ref);
		IG++;
	} while (P0 == (unsigned int)-1);
	currentIG = NULL;
	*pmem_ref = mem_ref;
	*flg = FlagSync_All();

#ifdef DEBUG_MORE
	if (debug_level('e')>1)
#else
	if (debug_level('e')>3)
#endif
	{
	    dbug_printf("(R) DR1=%08x AR1=%08x\n",
		DR1.d,mem_ref);
	    dbug_printf("(R) RFL cout=%08x RES=%08x\n\n",
		RFL.cout,RFL.res);
	}

	return P0;
}

static void emu_pagefault_handler(dosaddr_t addr, int err, uint32_t op, int len)
{
	if (!in_emu_cpu()) {
		default_sim_pagefault_handler(addr, err, op, len);
		return;
	}
	/* err = -1 is special, hitting a page with code that's been converted
	   to intermediate ops */
	if (err == -1) {
		if (e_querymark(addr, len))
			InvalidateNodeRange(addr, len, currentIG);
		return;
	}
	if ((err & 2) && msdos_ldt_access(addr)) {
		emu_ldt_write(addr, op, len);
		return;
	}
	/* trigger an exception in DPMI */
	/* Need to shutdown prejitter to touch TheCPU.err
	   to return to _Interp86() */
	prejit_sync();
	TheCPU.err = EXCP0E_PAGE;
	TheCPU.scp_err = err;
	TheCPU.cr[2] = addr;
	assert (currentIG);
	TheCPU.eip = FindPC(currentIG) - LONG_CS;
	if (CONFIG_CPUSIM)
		EFLAGS = (EFLAGS & ~EFLAGS_CC) | FlagSync_All();
	// Sim_helper can call from JIT, with synced EFLAGS
	longjmp(jmp_env, 1);
}

uint8_t sim_read_byte(dosaddr_t x)
{
	return do_read_byte(x, emu_pagefault_handler);
}

uint16_t sim_read_word(dosaddr_t x)
{
	return do_read_word(x, emu_pagefault_handler);
}

uint32_t sim_read_dword(dosaddr_t x)
{
	return do_read_dword(x, emu_pagefault_handler);
}

uint64_t sim_read_qword(dosaddr_t x)
{
	return do_read_qword(x, emu_pagefault_handler);
}

void sim_write_byte(dosaddr_t x, uint8_t y)
{
	do_write_byte(x, y, emu_pagefault_handler);
}

void sim_write_word(dosaddr_t x, uint16_t y)
{
	do_write_word(x, y, emu_pagefault_handler);
}

void sim_write_dword(dosaddr_t x, uint32_t y)
{
	do_write_dword(x, y, emu_pagefault_handler);
}

void sim_write_qword(dosaddr_t x, uint64_t y)
{
	do_write_qword(x, y, emu_pagefault_handler);
}

/////////////////////////////////////////////////////////////////////////////

static __inline__ void SetCPU_WL(int m, signed char o, unsigned long v)
{
	if (m&DATA16) CPUWORD(o)=v; else CPULONG(o)=v;
}

/* This generic helper function is called from JIT generated code and from
 * Gen_sim to simulate hard-to-compile and rarely used ops.
 * parameters:
 * mem_ref: linear address, passed via %edi in JIT
 * data: loaded data, passed via %eax in JIT or DR1.d in Sim (instruction-dependent)
 * mode: operand size, address size, etc.
 * flags: pointer to EFLAGS (on stack in JIT, calculated from lazy flags in Sim)
 * opc: original opcode, 0x1ab denotes 0x0f prefixed opcode 0xab (from IG->p0)
 * arg: instruction-dependent argument passed via IG->p1 at compile time
 * returns data (could be modified), so compiled code can write it back
 */
unsigned int Sim_helper(unsigned int mem_ref, unsigned int data, int mode,
			uint32_t *flags, unsigned int opc, unsigned int arg,
			unsigned char *eip)
{
	/* needed in emu_pagefault_handler */
	currentIG = eip;
	EFLAGS = (EFLAGS & ~EFLAGS_CC) | (*flags & EFLAGS_CC);

	unsigned int temp;
	switch (opc) {
/*9c*/	case PUSHF:	/* flag handling for VME-case only,
			    not used presently with IOPL==3 */
			assert(V86MODE() && IOPL<3 && (TheCPU.cr[4] & CR4_VME));
			data = (EFLAGS|IOPL_MASK) & RETURN_MASK;
			if (EFLAGS & VIF) data |= EFLAGS_IF;
			if (debug_level('e')>1)
				e_printf("Pushing flags %08x fl=%08x\n",
					 data,EFLAGS);
			break;
/*62*/	case BOUND:    {
			signed int lo, hi, r = data;
			lo = DataGetWL_S(mode, mem_ref);
			mem_ref += BT24(BitDATA16, mode);
			hi = DataGetWL_S(mode, mem_ref);
			if(r < lo || r > hi)
			{
				e_printf("Bound interrupt 05\n");
				TheCPU.err=EXCP05_BOUND;
			}
			break;
		       }
/*63*/	case ARPL:	{
			unsigned short dest, src = data;
			unsigned int reg3 = arg;
			if (reg3) {
				dest = CPUWORD(reg3);
			} else {
				dest = sim_read_word(mem_ref);
			}
			if ((dest & 3) < (src & 3)) {
				*flags |= EFLAGS_ZF;
				dest = (dest & ~3) | (src & 3);
				if (reg3) {
					CPUWORD(reg3) = dest;
				} else {
					sim_write_word(mem_ref, dest);
				}
			} else {
				*flags &= ~EFLAGS_ZF;
			}
			break;
			}
/*c8*/	case ENTER: {
			// This simulates only the middle part for level >=2
			unsigned int bp, sp, val;
			int level, ds;
			level = arg;
			assert(level > 1); // level 0 and 1 are compiled
			ds = BT24(BitDATA16, mode);
			bp = rEBP & TheCPU.StackMask;
			while (--level) {
				bp -= ds;
				bp &= TheCPU.StackMask;
				val = (mode&DATA16) ?
				     sim_read_word(LONG_SS + bp) :
				     sim_read_dword(LONG_SS + bp);
				sp = (rESP - ds) & TheCPU.StackMask;
				if (mode&DATA16)
					sim_write_word(LONG_SS + sp, val);
				else
					sim_write_dword(LONG_SS + sp, val);
				rESP = sp | (rESP&~TheCPU.StackMask);
			}
			}
			break;
/*ce*/	case INTO:
			if(*flags & EFLAGS_OF)
			{
				e_printf("Overflow interrupt 04\n");
				TheCPU.err=EXCP04_INTO;
			}
			break;
/*cd*/	case INT:      {
			/* flag handling for non-revectored VME-case only,
			   not used presently with IOPL=3 */
			int inum = arg;
			unsigned int temp;
			assert(V86MODE() && IOPL < 3 && (TheCPU.cr[4] & CR4_VME)
			       && !test_bit(inum, &vm86s.int_revectored));
			temp = (EFLAGS & ~EFLAGS_CC) | (*flags & EFLAGS_CC);
			temp = (temp|IOPL_MASK) & (RETURN_MASK|EFLAGS_IF);
			if (IOPL<3) {
				temp &= ~EFLAGS_IF;
				if (EFLAGS & VIF)
					temp |= EFLAGS_IF;
			}
			data = temp;
			EFLAGS &= ~(TF|RF|AC|NT);
			EFLAGS &= ~(IOPL==3 ? EFLAGS_IF : EFLAGS_VIF);
			if (debug_level('e')>1)
				dbug_printf("EMU86: directly calling int "
					    "%#x ax=%#x at %#x:%#x\n",
					    inum, _AX, _CS, _IP);
			break;
		       }
/*cf*/	case IRET: {	/* restartable */
			/* GPF handled in interpreter */
			assert(!(V86MODE() && IOPL!=3 && !(TheCPU.cr[4] & CR4_VME)));
			temp = data;
			/* IRET always returns with the new PC, we need to
			   flag that TF is set via an exception code that doesn't
			   interrupt the IRET */
			data = (temp & TF) ? EXCP_TFSET : 0;
			if (debug_level('e')>1) {
				e_printf("IRET: ret=%04x:%08x\n",TheCPU.cs,TheCPU.eip);
			}
			if (!(mode & DATA16)) {
			    temp &= EFLAGS_ALL & ~(VM|VIF|VIP);
			    temp |= EFLAGS & (VM|VIF|VIP);
			}
			/* in 16bit mode the manual doesn't seem to ask to
			 * clear reserved bits... But bit1 is always set! */
			if (REALMODE())
			    FLAGS = temp | 2;
			else if (V86MODE()) {
			    goto stack_return_from_vm86;
			}
			else {
			    /* if (EFLAGS&EFLAGS_NT) goto task_return */
			    /* if (temp&EFLAGS_VM) goto stack_return_to_vm86 */
			    /* else stack_return */
			    int amask = (CPL==0? 0:(EFLAGS_IOPL_MASK|VIF|VIP)) |
					(CPL<=IOPL? 0:EFLAGS_IF);
			    if (mode & DATA16)
				FLAGS = (FLAGS&amask) | ((temp&0x7fd7)&~amask) | 2;
			    else	/* should use eTSSMASK */
				EFLAGS = (EFLAGS&amask) |
					 ((temp&(eTSSMASK|0xfd7))&~amask) | 2;
			    TheCPU.df_increments = (EFLAGS&DF)?0xfcfeff:0x040201;
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x->{r=%08x v=%08x}\n",temp,EFLAGS,get_FLAGS(EFLAGS));
			}
			*flags = (EFLAGS & EFLAGS_CC) | (*flags & ~EFLAGS_CC);
			} break;
/*9d*/	case POPF: {
			/* GPF handled in interpreter */
			assert(!(V86MODE() && IOPL!=3 && !(TheCPU.cr[4] & CR4_VME)));
			temp = data;
			if (temp & TF)
			    TheCPU.err = EXCP_TFSET;
			if (V86MODE()) {
			    int is_tf;
stack_return_from_vm86:
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x fl=%08x\n",
					temp,EFLAGS);
			    is_tf = !!(EFLAGS & TF);
			    if (IOPL==3) {	/* Intel manual */
				/* keep reserved bits + IOPL,VIP,VIF,VM,RF */
				if (mode & DATA16)
				    FLAGS &= ~(SAFE_MASK|EFLAGS_IF);
				else
				    EFLAGS &= ~(SAFE_MASK|EFLAGS_IF);
				EFLAGS |= (temp & (SAFE_MASK|EFLAGS_IF)) | 2;
			    }
			    else {
				/* virtual-8086 monitor */
				/* move mask from pop{e}flags to regs->eflags */
				if (mode & DATA16)
				    FLAGS &= ~SAFE_MASK;
				else
				    EFLAGS &= ~SAFE_MASK;
				EFLAGS |= (temp & SAFE_MASK) | 2;
				if (temp & EFLAGS_IF)
				    EFLAGS |= EFLAGS_VIF;
			    }
			    TheCPU.df_increments = (EFLAGS&DF)?0xfcfeff:0x040201;
			    if (temp & EFLAGS_IF) {
				if (vm86s.regs.eflags & VIP) {
				    if (debug_level('e')>1)
					e_printf("Return for STI fl=%08x\n",
						 EFLAGS);
				    if (opc == POPF)
					TheCPU.err = (is_tf ? EXCP01_SSTP : EXCP_STISIGNAL);
				    else
					data = (is_tf ? EXCP01_SSTP : EXCP_STISIGNAL);
				}
			    }
			}
			else {
			    int is_tf = !!(EFLAGS & TF);
			    int amask = (CPL==0? 0:EFLAGS_IOPL_MASK) |
					(CPL<=IOPL? 0:EFLAGS_IF) |
					(EFLAGS_VM|EFLAGS_RF);
			    if (mode & DATA16)
				FLAGS = (FLAGS&amask) | ((temp&0x7fd7)&~amask) | 2;
			    else
				EFLAGS = (EFLAGS&amask) |
					 ((temp&(eTSSMASK|0xfd7))&~amask) | 2;
			    // unused "extended PVI" since real PVI does not
			    // affect POPF
			    if (IOPL<3 && (TheCPU.cr[4]&CR4_PVI)) {
				if (temp & EFLAGS_IF)
				    EFLAGS |= EFLAGS_VIF;
				else
				    EFLAGS &= ~EFLAGS_VIF;
			    }
			    if (debug_level('e')>1)
				e_printf("Popped flags %08x->{r=%08x v=%08x}\n",temp,EFLAGS,_EFLAGS);
			    TheCPU.df_increments = (EFLAGS&DF)?0xfcfeff:0x040201;
			    if ((EFLAGS & EFLAGS_IF) && isset_VIP()) {
				if (debug_level('e')>1)
				    e_printf("Return for STI fl=%08x\n",
					    EFLAGS);
				TheCPU.err = (is_tf ? EXCP01_SSTP : EXCP_STISIGNAL);
			    }
			}
			*flags = (EFLAGS & EFLAGS_CC) | (*flags & ~EFLAGS_CC);
			} break;
/*fa*/	case CLI:
			/* only for PVI/VME IOPL<3 CLI, not used presently */
			assert(!(REALMODE() || (CPL <= IOPL) || (IOPL==3)) &&
			       !((V86MODE() && !(TheCPU.cr[4] & CR4_VME)) ||
				 (!V86MODE() && !(TheCPU.cr[4] & CR4_PVI))));
			/* virtual-8086 monitor */
			if (debug_level('e')>2) {
			    if (V86MODE())
				e_printf("Virtual VM86 CLI\n");
			    else
				e_printf("Virtual DPMI CLI\n");
			}
			EFLAGS &= ~EFLAGS_VIF;
			break;
/*fb*/	case STI:
			if (V86MODE()) {    /* traps always (Intel man) */
				/* virtual-8086 monitor */
				if (IOPL==3)
				    EFLAGS |= EFLAGS_IF;
				else
				    EFLAGS |= EFLAGS_VIF;
				if (vm86s.regs.eflags & VIP) {
				    if (debug_level('e')>1)
					e_printf("Return for STI fl=%08x\n",
					    EFLAGS);
				    TheCPU.err=EXCP_STISIGNAL;
				}
			}
			else {
			    if (REALMODE() || (CPL <= IOPL) || (IOPL==3)) {
				EFLAGS |= EFLAGS_IF;
			    }
			    else {
				if (debug_level('e')>2) e_printf("Virtual DPMI STI\n");
				EFLAGS |= EFLAGS_VIF;
			    }
			    if (isset_VIP()) {
				if (debug_level('e')>1)
				    e_printf("Return for STI ASAP fl=%08x\n",
					    EFLAGS);
				/* force exit after next compiled block execution */
				exit_pending_or(CeS_RPIC);
				TheCPU.err = EXCP_STISHADOW;
			    }
			}
			break;
/*6c*/	case INSb: {
			unsigned short a;
			unsigned int rd;
			a = rDX;
			if (!test_ioperm(a)) goto not_permitted_sim;
			rd = (mode&ADDR16? rDI:rEDI);
			sim_write_byte(LONG_ES+rd, port_inb(a));
			if (EFLAGS & EFLAGS_DF) rd--; else rd++;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			} break;
/*ec*/	case INvb: {
			unsigned short a = rDX;
			int uc;
			if ((CEmuStat & CeS_INSTREMU) &&
			    (uc=VGA_emulate_inb(a, NULL)) != -1) {
				rAL = uc;
				break;
			}
			if (!test_ioperm(a)) goto not_permitted_sim;
			rAL = port_inb(a);
			}
			break;
/*e4*/	case INb: {
			unsigned short a = arg;
			if (!test_ioperm(a)) goto not_permitted_sim;
			rAL = port_inb(a);
			} break;
/*6d*/	case INSw: {
			unsigned int rd;
			int dp;
			if (!test_ioperm(rDX)) goto not_permitted_sim;
			rd = (mode&ADDR16? rDI:rEDI);
			if (mode&DATA16) {
				sim_write_word(LONG_ES+rd, port_inw(rDX)); dp=2;
			}
			else {
				sim_write_dword(LONG_ES+rd, port_ind(rDX)); dp=4;
			}
			if (EFLAGS & EFLAGS_DF) rd-=dp; else rd+=dp;
			if (mode&ADDR16) rDI=rd; else rEDI=rd;
			} break;
/*ed*/	case INvw:
			if (!test_ioperm(rDX)) goto not_permitted_sim;
			if (mode&DATA16) rAX = port_inw(rDX);
			else rEAX = port_ind(rDX);
			break;
/*e5*/	case INw: {
			unsigned short a = arg;
			if (!test_ioperm(a)) goto not_permitted_sim;
			if (mode&DATA16) rAX = port_inw(a);
			else rEAX = port_ind(a);
			} break;

/*6e*/	case OUTSb: {
			unsigned short a = rDX;
			unsigned long rs;
			if (!test_ioperm(a)) goto not_permitted_sim;
			rs = (mode&ADDR16? rSI:rESI);
			port_outb(a,sim_read_byte(LONG_DS+rs));
			if (EFLAGS & EFLAGS_DF) rs--; else rs++;
			if (mode&ADDR16) rSI=rs; else rESI=rs;
			} break;
/*ee*/	case OUTvb: {
			unsigned short a = rDX;
			/* Note that we short circuit for vgaemu planar */
			if ((CEmuStat & CeS_INSTREMU) &&
			    VGA_emulate_outb(a, rAL, NULL) != -1) {
				break;
			}
			if (!test_ioperm(a)) goto not_permitted_sim;
			port_outb(a,rAL);
			}
			break;
/*e6*/	case OUTb:  {
			unsigned short a = arg;
			if (!test_ioperm(a)) goto not_permitted_sim;
			port_outb(a,rAL);
			} break;
/*ef*/	case OUTvw: {
			unsigned short a;
			a = rDX;
			if ((CEmuStat & CeS_INSTREMU) &&
			    VGA_emulate_outb(a, rAL, NULL) != -1 &&
			    VGA_emulate_outb(a+1, rAH, NULL) != -1) {
				break;
			}
			if (!test_ioperm(a)) goto not_permitted_sim;
			if (mode&DATA16) port_outw(a,rAX); else port_outd(a,rEAX);
			}
			break;

/*e7*/	case OUTw:  {
			unsigned short a = arg;
			if (!test_ioperm(a)) goto not_permitted_sim;
			if (mode&DATA16) port_outw(a,rAX); else port_outd(a,rEAX);
			} break;

/*d9*/	case ESC1:
/*dd*/	case ESC5:
			Fp87_op(arg, mode, mem_ref);
			break;
/*100*/	case 0x100: {	/* GRP6 - Extended Opcode 20 */
			unsigned char opm = arg;
			switch (opm) {
			case 0: /* SLDT */
			    data = TheCPU.LDT_SEL;
			    break;
			case 1: /* STR */
			    /* Store Task Register */
			    data = TheCPU.TR_SEL;
			    break;
			case 4:   /* VERR */
			case 5: { /* VERW */
			    unsigned short sv = data; int tmp;
			    tmp = opm == 4 ? hsw_verr(sv) : hsw_verw(sv);
			    *flags &= ~EFLAGS_ZF;
			    if (tmp) *flags |= EFLAGS_ZF;
			    }
			    break;
			    }
			break;
			}
/*102*/	case 0x102:   /* LAR */ /* Load Access Rights Byte */
/*103*/	case 0x103: { /* LSL */ /* Load Segment Limit */
			unsigned short sv = data; int tmp;
			if (!e_larlsl(mode, sv)) {
			    *flags &= ~EFLAGS_ZF;
			}
			else {
			    if (opc==0x102) {	/* LAR */
				tmp = GetSelectorFlags(sv);
				if (mode&DATA16) tmp &= 0xff;
				tmp <<= 8;
				if (tmp) SetFlagAccessed(sv);
			    }
			    else {		/* LSL */
				tmp = GetSelectorByteLimit(sv);
			    }
			    *flags |= EFLAGS_ZF;
			    SetCPU_WL(mode, arg, tmp);
			} }
			break;
/*106*/	case 0x106: /* CLTS */ /* Privileged */
			/* Clear Task State Register */
			TheCPU.cr[0] &= ~8;
			break;
/*120*/	case 0x120:   /* MOVcdrd */ /* Privileged */
/*122*/	case 0x122:   /* MOVrdcd */ /* Privileged */
/*121*/	case 0x121:   /* MOVddrd */ /* Privileged */
/*123*/	case 0x123:   /* MOVrddd */ /* Privileged */
/*124*/	case 0x124:   /* MOVtdrd */ /* Privileged */
/*126*/	case 0x126: { /* MOVrdtd */ /* Privileged */
			int reg; unsigned char opd,opc2;
			opc2 = opc - 0x100;
			reg = arg;
			opd = opc2&2;
			if (opc2&1) {
			    if (opd) TheCPU.dr[reg] = data;
				else data = TheCPU.dr[reg];
			}
			else if (opc2&4) {
			    reg-=6;
			    if (opd) TheCPU.tr[reg] = data;
				else data = TheCPU.tr[reg];
			}
			else {
			    if (opd) {	/* write to CRs */
				if (reg==0) {
				    if ((TheCPU.cr[0] ^ data) & 1) {
					dbug_printf("RM/PM switch not allowed\n");
					break;
				    }
				    TheCPU.cr[0] = (data&0xe005002f)|0x10;
				}
				else TheCPU.cr[reg] = data;
			    } else {
				data = TheCPU.cr[reg];
			    }
			}
			} break;
/*1a2*/	case 0x1a2:	/* CPUID */
			if (rEAX==0) {
				/* "GenuineIntel" */
				rEAX = 1; rEBX = 0x756e6547;
				rECX = 0x6c65746e; rEDX = 0x49656e69;
			}
			else if (rEAX==1) {
				/* family 5, model 2, stepping 12 =
				   Pentium 133-200MHz (no MMX) */
				rEAX = 0x052c; rEBX = rECX = 0;
				/* 0x1bf */
				rEDX = CPUID_FEATURE_FPU | CPUID_FEATURE_VME |
				  CPUID_FEATURE_DBGE | CPUID_FEATURE_PGSZE |
				  CPUID_FEATURE_TSC  | CPUID_FEATURE_MSR |
				  CPUID_FEATURE_MCK  | CPUID_FEATURE_CPMX;
			}
			break;
/*1c7*/	case 0x1c7: { /* Code Extension 23 - 01=CMPXCHG8B mem */
			uint64_t edxeax, m;
			edxeax = ((uint64_t)rEDX << 32) | rEAX;
			m = sim_read_qword(mem_ref);
			if (edxeax == m)
			{
				*flags |= EFLAGS_ZF;
				m = ((uint64_t)rECX << 32) | rEBX;
			} else {
				*flags &= ~EFLAGS_ZF;
				rEDX = m >> 32;
				rEAX = m & 0xffffffff;
			}
			sim_write_qword(mem_ref, m);
			break;
			}
	}
	return data;

not_permitted_sim:
	if (debug_level('e')>1) e_printf("!!! Not permitted %02x\n",opc);
	TheCPU.err = EXCP0D_GPF;
	return data;
}
