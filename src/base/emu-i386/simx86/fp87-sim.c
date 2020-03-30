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

#include <stddef.h>
#include "dos2linux.h"
#include "emu86.h"
#include "codegen.h"
#include "codegen-sim.h"
#include <stdlib.h>
#include <math.h>

int (*Fp87_op)(int exop, int reg);
static int Fp87_op_sim(int exop, int reg);

static long double WFR0, WFR1;
static unsigned short WFRS;

#define S_next(r)	(((r)+1)&7)
#define S_prev(r)	(((r)-1)&7)
#define S_reg(r,n)	(((r)+(n))&7)
#define ST0		(&TheCPU.fpregs[TheCPU.fpstt])
#define ST1		(&TheCPU.fpregs[S_next(TheCPU.fpstt)])
#define STn(n)		(&TheCPU.fpregs[S_reg(TheCPU.fpstt,n)])

/* Note: 0 / special values for tag word are only done for FSTENV and FSAVE */
#define FREETAG(n)	TheCPU.fptag |= (3 << 2*S_reg(TheCPU.fpstt,n))
#define UNFREETAG(n)	TheCPU.fptag &= ~(3 << 2*S_reg(TheCPU.fpstt,n))
#define SYNCFSP		TheCPU.fpus=(TheCPU.fpus&0xc7ff)|(TheCPU.fpstt<<11)
#define INCFSP		TheCPU.fpstt=S_next(TheCPU.fpstt),SYNCFSP
#define INCFSPP		FREETAG(0),TheCPU.fpstt=S_next(TheCPU.fpstt),SYNCFSP
#define DECFSP		TheCPU.fpstt=S_prev(TheCPU.fpstt),SYNCFSP
#define DECFSPP		TheCPU.fpstt=S_prev(TheCPU.fpstt),SYNCFSP,UNFREETAG(0)

static long double _fparea[8];

/*
 * This function is only here for looking at the generated binary code
 * with objdump.
 */
static void _test_(void) __attribute__((used));

static void _test_(void)
{
	__asm__ __volatile__ (" \
		nop \
		" : : : "memory" );
}

void init_emu_npu (void)
{
	int i;
#ifdef HOST_ARCH_X86
	if (!config.cpusim) {
		init_emu_npu_x86();
		return;
	}
#endif
	Fp87_op = Fp87_op_sim;
	TheCPU.fpregs = _fparea;
	for (i=0; i<8; i++) TheCPU.fpregs[i] = 0.0;
	TheCPU.fpus  = 0;
	TheCPU.fpstt = 0;
	TheCPU.fpuc  = 0x37f;
	TheCPU.fptag = 0xffff;
	__asm__ __volatile__ ("fninit");
	WFR0 = WFR1 = 0.0;
}

static void ftest(long double d)
{
	register unsigned short fps;
	__asm__ __volatile__ (
	"fldt	%1\n"
	"fxam\n"
	"fnstsw\n"
	"fstp	%%st(0)" : "=a"(fps) : "m"(d) );
	TheCPU.fpus = (fps&0xc7df)|(TheCPU.fpstt<<11);
}

static inline void fssync(void)
{
	TheCPU.fpus = (WFRS&0xc7df)|(TheCPU.fpstt<<11);
}

/* round long double to integer depending on rounding mode */
static inline void round_double(void)
{
	switch (TheCPU.fpuc & 0xc00) {
	case 0x000: WFR0 = rintl(WFR0); break;
	case 0x400: WFR0 = floorl(WFR0); break;
	case 0x800: WFR0 = ceill(WFR0); break;
	default:    WFR0 = truncl(WFR0); break;
	}
}

static float read_float(dosaddr_t addr)
{
	union { uint32_t u32; float f; } x = {.u32 = read_dword(addr)};
	return x.f;
}

static double read_double(dosaddr_t addr)
{
	union { uint64_t u64; double d; } x = {.u64 = read_qword(addr)};
	return x.d;
}

static long double read_long_double(dosaddr_t addr)
{
	union { uint32_t u32[3]; long double ld; } x;
	x.u32[0] = read_dword(addr);
	x.u32[1] = read_dword(addr+4);
	x.u32[2] = read_word(addr+8);
	return x.ld;
}

static void write_float(dosaddr_t addr, float f)
{
	union { uint32_t u32; float f; } x = {.f = f};
	write_dword(addr, x.u32);
}

static void write_double(dosaddr_t addr, double d)
{
	union { uint64_t u64; double d; } x = {.d = d};
	write_qword(addr, x.u64);
}

static void write_long_double(dosaddr_t addr, long double ld)
{
	union { uint32_t u32[3]; long double ld; } x = {.ld = ld};
	write_dword(addr, x.u32[0]);
	write_dword(addr+4, x.u32[1]);
	write_word(addr+8, x.u32[2]);
}

static int Fp87_op_sim(int exop, int reg)
{
//	42	DA 11000nnn	FCMOVB	st(0),st(n)
//	43	DB 11000nnn	FCMOVNB	st(0),st(n)
//	4A	DA 11001nnn	FCMOVE	st(0),st(n)
//	4B	DB 11001nnn	FCMOVNE	st(0),st(n)
//	52	DA 11010nnn	FCMOVBE	st(0),st(n)
//	53	DB 11010nnn	FCMOVNBE st(0),st(n)
//	5A	DA 11011nnn	FCMOVU	st(0),st(n)
//	5B	DB 11011nnn	FCMOVNU	st(0),st(n)

	e_printf("FPop %x.%d\n", exop, reg);

	switch(exop) {
/*01*/	case 0x01:
/*03*/	case 0x03:
/*05*/	case 0x05:
/*07*/	case 0x07:
//*	01	D9 xx000nnn	FLD	mem32r
//	03	DB xx000nnn	FILD	dw
//	05	DD xx000nnn	FLD	dr
//	07	DF xx000nnn	FILD	w
/*27*/	case 0x27:
//	27	DF xx100nnn	FBLD
/*2b*/	case 0x2b:
/*2f*/	case 0x2f:
//	2B	DB xx101nnn	FLD	ext
//	2F	DF xx101nnn	FILD	qw
		DECFSPP;
		switch(exop) {		// Fop (edi)
		case 0x01: WFR0 = read_float(DOSADDR_REL(AR1.pu)); break;
		case 0x03: WFR0 = (int32_t)read_dword(DOSADDR_REL(AR1.pu)); break;
		case 0x05: WFR0 = read_double(DOSADDR_REL(AR1.pu)); break;
		case 0x07: WFR0 = (int16_t)read_word(DOSADDR_REL(AR1.pu)); break;
		case 0x27: {
			dosaddr_t p = DOSADDR_REL(AR1.pu);
			long long b = 0;
			int i;
			for (i = 8; i >= 0; i--) {
				uint8_t u = read_byte(p+i);
				b = (b * 100) + (u >> 4) * 10 + (u & 0xf);
			}
			WFR0 = ((read_byte(p+9) & 0x80) ? -1 : 1) * b;
			break;
		}
		case 0x2b: WFR0 = read_long_double(DOSADDR_REL(AR1.pu)); break;
		case 0x2f: WFR0 = (int64_t)read_qword(DOSADDR_REL(AR1.pu)); break;
		}
		*ST0 = WFR0;
		break;

/*00*/	case 0x00:
/*02*/	case 0x02:
/*04*/	case 0x04:
/*06*/	case 0x06:
//*	00	D8 xx000nnn	FADD	mem32r
//	02	DA xx000nnn	FIADD	dw
//	04	DC xx000nnn	FADD	dr
//	06	DE xx000nnn	FIADD	w
/*08*/	case 0x08:
/*0a*/	case 0x0a:
/*0c*/	case 0x0c:
/*0e*/	case 0x0e:
//*	08	D8 xx001nnn	FMUL	mem32r
//	0A	DA xx001nnn	FIMUL	dw
//	0C	DC xx001nnn	FMUL	dr
//	0E	DE xx001nnn	FIMUL	w
/*20*/	case 0x20:
/*22*/	case 0x22:
/*24*/	case 0x24:
/*26*/	case 0x26:
//*	20	D8 xx100nnn	FSUB	mem32r
//	22	DA xx100nnn	FISUB	dw
//	24	DC xx100nnn	FSUB	dr
//	26	DE xx100nnn	FISUB	w
/*28*/	case 0x28:
/*2a*/	case 0x2a:
/*2c*/	case 0x2c:
/*2e*/	case 0x2e:
//*	28	D8 xx101nnn	FSUBR	mem32r
//	2A	DA xx101nnn	FISUBR	dw
//	2C	DC xx101nnn	FSUBR	dr
//	2E	DE xx101nnn	FISUBR	w
/*30*/	case 0x30:
/*32*/	case 0x32:
/*34*/	case 0x34:
/*36*/	case 0x36:
//*	30	D8 xx110nnn	FDIV	mem32r
//	32	DA xx110nnn	FIDIV	dw
//	34	DC xx110nnn	FDIV	dr
//	36	DE xx110nnn	FIDIV	w
/*38*/	case 0x38:
/*3a*/	case 0x3a:
/*3c*/	case 0x3c:
/*3e*/	case 0x3e:
//*	38	D8 xx111nnn	FDIVR	mem32r
//	3A	DA xx111nnn	FIDIVR	dw
//	3C	DC xx111nnn	FDIVR	dr
//	3E	DE xx111nnn	FIDIVR	w
		WFR0 = *ST0;
		switch(exop) {		// Fop (edi)
		case 0x00: WFR0 += read_float(DOSADDR_REL(AR1.pu)); break;
		case 0x02: WFR0 += (int32_t)read_dword(DOSADDR_REL(AR1.pu)); break;
		case 0x04: WFR0 += read_double(DOSADDR_REL(AR1.pu)); break;
		case 0x06: WFR0 += (int16_t)read_word(DOSADDR_REL(AR1.pu)); break;
		case 0x08: WFR0 *= read_float(DOSADDR_REL(AR1.pu)); break;
		case 0x0a: WFR0 *= (int32_t)read_dword(DOSADDR_REL(AR1.pu)); break;
		case 0x0c: WFR0 *= read_double(DOSADDR_REL(AR1.pu)); break;
		case 0x0e: WFR0 *= (int16_t)read_word(DOSADDR_REL(AR1.pu)); break;
		case 0x20: WFR0 -= read_float(DOSADDR_REL(AR1.pu)); break;
		case 0x22: WFR0 -= (int32_t)read_dword(DOSADDR_REL(AR1.pu)); break;
		case 0x24: WFR0 -= read_double(DOSADDR_REL(AR1.pu)); break;
		case 0x26: WFR0 -= (int16_t)read_word(DOSADDR_REL(AR1.pu)); break;
		case 0x28: WFR0 = (long double)read_float(DOSADDR_REL(AR1.pu)) - WFR0; break;
		case 0x2a: WFR0 = (long double)(int32_t)read_dword(DOSADDR_REL(AR1.pu)) - WFR0; break;
		case 0x2c: WFR0 = (long double)read_double(DOSADDR_REL(AR1.pu)) - WFR0; break;
		case 0x2e: WFR0 = (long double)(int16_t)read_word(DOSADDR_REL(AR1.pu)) - WFR0; break;
		case 0x30: WFR0 /= read_float(DOSADDR_REL(AR1.pu)); break;
		case 0x32: WFR0 /= (int32_t)read_dword(DOSADDR_REL(AR1.pu)); break;
		case 0x34: WFR0 /= read_double(DOSADDR_REL(AR1.pu)); break;
		case 0x36: WFR0 /= (int16_t)read_word(DOSADDR_REL(AR1.pu)); break;
		case 0x38: WFR0 = (long double)read_float(DOSADDR_REL(AR1.pu)) / WFR0; break;
		case 0x3a: WFR0 = (long double)(int32_t)read_dword(DOSADDR_REL(AR1.pu)) / WFR0; break;
		case 0x3c: WFR0 = (long double)read_double(DOSADDR_REL(AR1.pu)) / WFR0; break;
		case 0x3e: WFR0 = (long double)(int16_t)read_word(DOSADDR_REL(AR1.pu)) / WFR0; break;
		}
		ftest(WFR0);
		*ST0 = WFR0;
		break;

/*10*/	case 0x10:
/*11*/	case 0x11:
/*12*/	case 0x12:
/*13*/	case 0x13:
/*14*/	case 0x14:
/*15*/	case 0x15:
/*16*/	case 0x16:
/*17*/	case 0x17:
/*18*/	case 0x18:
/*19*/	case 0x19:
/*1a*/	case 0x1a:
/*1b*/	case 0x1b:
/*1c*/	case 0x1c:
/*1d*/	case 0x1d:
/*1e*/	case 0x1e:
/*1f*/	case 0x1f:
//*	10	D8 xx010nnn	FCOM	mem32r
//*	11	D9 xx010nnn	FST	mem32r
//	12	DA xx010nnn	FICOM	dw
//	13	DB xx010nnn	FIST	dw
//	14	DC xx010nnn	FCOM	dr
//	15	DD xx010nnn	FST	dr
//	16	DE xx010nnn	FICOM	w
//	17	DF xx010nnn	FIST	w
//*	18	D8 xx011nnn	FCOMP	mem32r
//*	19	D9 xx011nnn	FSTP	mem32r
//	1A	DA xx011nnn	FICOMP	dw
//	1B	DB xx011nnn	FISTP	dw
//	1C	DC xx011nnn	FCOMP	dr
//	1D	DD xx011nnn	FSTP	dr
//	1E	DE xx011nnn	FICOMP	w
//	1F	DF xx011nnn	FISTP	w
		WFR0 = *ST0;
		switch(exop) {		// Fop (edi), no pop
		case 0x10:
		case 0x18: WFR1 = (long double)read_float(DOSADDR_REL(AR1.pu)); goto fcom00;
		case 0x12:
		case 0x1a: WFR1 = (long double)(int32_t)read_dword(DOSADDR_REL(AR1.pu)); goto fcom00;
		case 0x14:
		case 0x1c: WFR1 = (long double)read_double(DOSADDR_REL(AR1.pu)); goto fcom00;
		case 0x16:
		case 0x1e: WFR1 = (long double)(int16_t)read_word(DOSADDR_REL(AR1.pu));
fcom00:			TheCPU.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
			if (WFR0 < WFR1)
			    TheCPU.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
				else if (WFR0 == WFR1)
				    TheCPU.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
				else if (WFR0 > WFR1);  /* do nothing */
			        else /* not comparable */ TheCPU.fpus |= 0x4500;
			    break;
		case 0x11:
		case 0x19: write_float(DOSADDR_REL(AR1.pu), WFR0); break;
		case 0x15:
		case 0x1d: write_double(DOSADDR_REL(AR1.pu), WFR0); break;
		case 0x13:
		case 0x1b:
		case 0x17:
		case 0x1f: {
			round_double();
			if (exop & 4) {
			    if (isnan(WFR0) || isinf(WFR0) ||
				WFR0 < (long double)-0x8000 ||
				WFR0 > (long double)0x7fff) {
				TheCPU.fpus |= 1;
				WFR0 = (long double)-0x8000;
			    }
			    write_word(DOSADDR_REL(AR1.pu), (int16_t)WFR0); break;
			}
			if (isnan(WFR0) || isinf(WFR0) ||
			    WFR0 < -(long double)0x80000000 ||
			    WFR0 >  (long double)0x7fffffff) {
			    TheCPU.fpus |= 1;
			    WFR0 = -(long double)0x80000000;
			}
			write_dword(DOSADDR_REL(AR1.pu), (int32_t)WFR0); break; }
		}
		if (exop&8) INCFSPP;
		break;

/*37*/	case 0x37: {
//	37	DF xx110nnn	FBSTP
		dosaddr_t p = DOSADDR_REL(AR1.pu);
		long long b;
		int i;
		WFR0 = *ST0;
		round_double();
		b = WFR0;
		write_byte(p+9, b < 0 ? 0x80 : 0);
		b = llabs(b);
		for (i=0; i < 9; i++) {
			uint8_t u = b % 10;
			b /= 10;
			write_byte(p+i, u | ((b % 10) << 4));
			b /= 10;
		}
		INCFSPP;
		}
		break;
/*3b*/	case 0x3b:
//	3B	DB xx111nnn	FSTP	ext
		WFR0 = *ST0;
		write_long_double(DOSADDR_REL(AR1.pu), WFR0);
		INCFSPP;
		break;
/*3f*/	case 0x3f: {
//	3F	DF xx111nnn	FISTP	qw
		WFR0 = *ST0;
		round_double();
		if (isnan(WFR0) || isinf(WFR0) ||
		    WFR0 < (long double)(long long)0x8000000000000000ULL ||
		    WFR0 > (long double)(long long)0x7fffffffffffffffULL) {
		    TheCPU.fpus |= 1;
		    WFR0 = (long double)(long long)0x8000000000000000ULL;
		}
		write_qword(DOSADDR_REL(AR1.pu), (int64_t)WFR0);
		INCFSPP;
		}
		break;
/*29*/	case 0x29:
//*	29	D9 xx101nnn	FLDCW	2b
		TheCPU.fpuc = read_word(DOSADDR_REL(AR1.pu)) | 0x40;
		break;
/*39*/	case 0x39:
//*	39	D9 xx111nnn	FSTCW	2b
		write_word(DOSADDR_REL(AR1.pu), TheCPU.fpuc);
		break;

/*67*/	case 0x67: if (reg!=0) goto fp_notok;
//	67.0	DF 11000000	FSTSW	ax
/*3d*/	case 0x3d:
//	3D	DD xx111nnn	FSTSW	2b
		// movw	FPUS(ebx),ax
		// andw	0xc7ff,ax
		// movw	FPSTT(ebx),cx
		// andw	0x07,cx
		// shll	11,ecx
		// orl	ecx,eax
		SYNCFSP;
		if (exop==0x3d) {
			// movw	ax,(edi)
			write_word(DOSADDR_REL(AR1.pu), TheCPU.fpus);
		}
		else {
			CPUWORD(Ofs_AX) = TheCPU.fpus;
		}
		break;

/*40*/	case 0x40:
/*48*/	case 0x48:
/*60*/	case 0x60:
/*68*/	case 0x68:
/*70*/	case 0x70:
/*78*/	case 0x78:
//*	40	D8 11000nnn	FADD	st,st(n)
//*	48	D8 11001nnn	FMUL	st,st(n)
//*	60	D8 11100nnn	FSUB	st,st(n)
//*	68	D8 11101nnn	FSUBR	st,st(n)
//*	70	D8 11110nnn	FDIV	st,st(n)
//*	78	D8 11111nnn	FDIVR	st,st(n)
		WFR0 = *ST0;
		WFR1 = *STn(reg);
		switch (exop) {
		case 0x40: WFR0 += WFR1; break;
		case 0x48: WFR0 *= WFR1; break;
		case 0x60: WFR0 -= WFR1; break;
		case 0x68: WFR0  = WFR1 - WFR0; break;
		case 0x70: WFR0 /= WFR1; break;
		case 0x78: WFR0  = WFR1 / WFR0; break;
		}
		ftest(WFR0);
		*ST0 = WFR0;
		break;

/*50*/	case 0x50:
/*58*/	case 0x58:
//*	50	D8 11010nnn	FCOM	st,st(n)
//*	58	D8 11011nnn	FCOMP	st,st(n)
		WFR0 = *ST0;
		WFR1 = *STn(reg);
		TheCPU.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
		if (WFR0 < WFR1)
		    TheCPU.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
			else if (WFR0 == WFR1)
			    TheCPU.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
			else if (WFR0 > WFR1); /* do nothing */
			else /* not comparable */ TheCPU.fpus |= 0x4500;
		if (exop&8) INCFSPP;
		break;

/*6a*/	case 0x6a: if (reg!=1) goto fp_notok;
/*6d*/	case 0x6d:
/*65*/	case 0x65:
//	65	DD 11000nnn	FUCOM	st(n),st(0)
//	6D	DD 11101nnn	FUCOMP	st(n)
//	6A.1	DA 11101001	FUCOMPP
		WFR0 = *ST0;
		WFR1 = *STn(reg);
		TheCPU.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
		if (WFR0 < WFR1)
		    TheCPU.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
			else if (WFR0 == WFR1)
			    TheCPU.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
			else if (WFR0 > WFR1); /* do nothing */
			else /* not comparable */ TheCPU.fpus |= 0x4500;
		if (exop==0x6a) INCFSPP;
		if (exop>=0x6a) INCFSPP;
		break;

//	73	DB 11000nnn	FCOMI	st(0),st(n)
//	77	DF 11000nnn	FCOMIP	st(0),st(n)
//	6B	DB 11101nnn	FUCOMI	st(0),st(n)
//	6F	DF 11101nnn	FUCOMIP	st(0),st(n)

/*5e*/	case 0x5e: if (reg==1) {
//	5E.1	DE 11011001	FCOMPP
			WFR0 = *ST0;
			WFR1 = *ST1;
			TheCPU.fpus &= (~0x4500);	/* (C3,C2,C0) <-- 000 */
			if (WFR0 < WFR1)
			    TheCPU.fpus |= 0x100;	/* (C3,C2,C0) <-- 001 */
				else if (WFR0 == WFR1)
				    TheCPU.fpus |= 0x4000; /* (C3,C2,C0) <-- 100 */
				else if (WFR0 > WFR1); /* do nothing */
				else /* not comparable */ TheCPU.fpus |= 0x4500;
			INCFSPP;
			INCFSPP;
		   }
		   else goto fp_notok;
		   break;

/*44*/	case 0x44:
/*4c*/	case 0x4c:
/*46*/	case 0x46:
/*4e*/	case 0x4e:
//	44	DC 11000nnn	FADD	st(n),st
//	4C	DC 11001nnn	FMUL	st(n),st
//	46	DE 11000nnn	FADDP	st(n),st
//	4E	DE 11001nnn	FMULP	st(n),st
		WFR0 = *ST0;
		WFR1 = *STn(reg);
		switch (exop) {
		case 0x44:
		case 0x46: WFR1 += WFR0; break;
		case 0x4c:
		case 0x4e: WFR1 *= WFR0; break;
		}
		*STn(reg) = WFR1;
		if (exop&2) INCFSPP;
		ftest(WFR1);
		break;

/*64*/	case 0x64:
/*6c*/	case 0x6c:
/*74*/	case 0x74:
/*7c*/	case 0x7c:
/*66*/	case 0x66:
/*6e*/	case 0x6e:
/*76*/	case 0x76:
/*7e*/	case 0x7e:
//	64	DC 11100nnn	FSUBR	st(n),st(0)
//	6C	DC 11101nnn	FSUB	st(n),st(0)
//	74	DC 11110nnn	FDIVR	st(n),st(0)
//	7C	DC 11111nnn	FDIV	st(n),st(0)
//	66	DE 11100nnn	FSUBRP	st(n),st(0)
//	6E	DE 11101nnn	FSUBP	st(n),st(0)
//	76	DE 11110nnn	FDIVRP	st(n),st(0)
//	7E	DE 11111nnn	FDIVP	st(n),st(0)
		WFR0 = *ST0;
		WFR1 = *STn(reg);
		switch (exop) {
		case 0x64:
		case 0x66: WFR1  = WFR0 - WFR1; break;
		case 0x6c:
		case 0x6e: WFR1 -= WFR0; break;
		case 0x74:
		case 0x76: WFR1  = WFR0 / WFR1; break;
		case 0x7c:
		case 0x7e: WFR1 /= WFR0; break;
		}
		*STn(reg) = WFR1;
		if (exop&2) INCFSPP;
		ftest(WFR1);
		break;

/*41*/	case 0x41:
//*	41	D9 11000nnn	FLD	st(n)
		WFR0 = *STn(reg);
		DECFSPP;
		*ST0 = WFR0;
		break;

/*45*/	case 0x45:
//	45	DD 11000nnn	FFREE	st(n)		set tag(n) empty
		FREETAG(reg);
		break;
/*51*/	case 0x51: if (reg==0) {
//*	51.0	D9 11010000	FNOP
			break;		// nop
		   }
		   else goto fp_notok;
		   break;

/*49*/	case 0x49:
//*	49	D9 11001nnn	FXCH	st,st(n)
		{ long double t;
		  memcpy(&t, ST0, sizeof t);
		  memcpy(ST0, STn(reg), sizeof t);
		  memcpy(STn(reg), &t, sizeof t);
		}
		break;

/*55*/	case 0x55:
/*5d*/	case 0x5d:
//	55	DD 11010nnn	FST	st(n)
//	5D	DD 11011nnn	FSTP	st(n)
		*STn(reg) = *ST0;
		UNFREETAG(reg);
		if (exop==0x5d) INCFSPP;
		break;

/*61*/	case 0x61:
//*	61.0	D9 11100000	FCHS
//*	61.1	D9 11100001	FABS
//*	61.4	D9 11100100	FTST
//*	61.5	D9 11100101	FXAM
		WFR0 = *ST0;
		switch(reg) {
		   case 0:		/* FCHS */
			WFR0 = -WFR0; break;
		   case 1:		/* FABS */
			WFR0 = fabsl(WFR0); break;
		   case 4:		/* FTST */
		   	TheCPU.fpus &= (~0x4500);
			if (WFR0 < 0.0) TheCPU.fpus |= 0x100;
			  else if (WFR0 == 0.0) TheCPU.fpus |= 0x4000;
			  else if (WFR0 > 0.0); /* do nothing; */
			  else /* not comparable */ TheCPU.fpus |= 0x4500;
			break;
		   case 5:		/* FXAM */
			break;
		   default:
			goto fp_notok;
		}
		if (reg != 4) ftest(WFR0);
		*ST0 = WFR0;
		break;

/*63*/	case 0x63: switch(reg) {
//	63.2*	DB 11000010	FCLEX
//	63.3*	DB 11000011	FINIT
		   case 2:		/* FCLEX */
			TheCPU.fpus &= 0x7f00;
			__asm__ __volatile__ ("fnclex");
			break;
		   case 3:		/* FINIT */
			TheCPU.fpus  = 0;
			TheCPU.fpstt = 0;
			TheCPU.fpuc  = 0x37f;
			TheCPU.fptag = 0xffff;
			__asm__ __volatile__ ("fninit");
			WFR0 = WFR1 = 0.0;
			break;
		   default: /* FNENI,FNDISI: 8087 */
			    /* FSETPM,FRSTPM: 80287 */
			goto fp_ok;	// do nothing
		   }
		   break;

/*69*/	case 0x69: {
//*	69.0	D9 11101000	FLD1
//*	69.1	D9 11101001	FLDL2T
//*	69.2	D9 11101010	FLDL2E
//*	69.3	D9 11101011	FLDPI
//*	69.4	D9 11101100	FLDLG2
//*	69.5	D9 11101101	FLDLN2
//*	69.6	D9 11101110	FLDZ
			DECFSPP;
			switch (reg) {
			case 0: WFR0 = 1.0; break;
			case 1: WFR0 = M_LN10/M_LN2; break;
			case 2: WFR0 = M_LOG2E; break;
			case 3: WFR0 = M_PI; break;
			case 4: WFR0 = M_LN2/M_LN10; break;
			case 5: WFR0 = M_LN2; break;
			case 6: WFR0 = 0.0; break;
			default: goto fp_notok;
			}
			ftest(WFR0);
			*ST0 = WFR0;
		   }
		   break;

/*71*/	case 0x71: switch(reg) {
//	71.0	D9 11110000	F2XM1	st(0)
//	71.1	D9 11110001	FYL2X	st(1)*l2(st(0))->st(1),pop
//	71.2	D9 11110010	FPTAN	st(0),push 1
//	71.3	D9 11110011	FPATAN	st(1)/st(0)->st(1),pop
//	71.4	D9 11110100	FXTRACT	exp->st(0),push signif
//	71.5	D9 11110101	FPREM1	st(0)/st(1)->st(0)
//	71.6	D9 11110110	FDECSTP
//	71.7	D9 11110111	FINCSTP
		   case 0:		/* F2XM1 */
	   		WFR0 = *ST0;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"f2xm1\n"
			"fnstsw	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR0) : "memory" );
			fssync();
			*ST0 = WFR0;
			break;
		   case 1:		/* FYL2X */
	   		WFR0 = *ST0;
			WFR1 = *ST1;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fldt	%3\n"
			"fyl2x\n"
			"fnstsw	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR1),"m"(WFR0) : "memory" );
			INCFSPP;
			fssync();
			*ST0 = WFR0;
			break;
		   case 3:		/* FPATAN */
	   		WFR0 = *ST0;
			WFR1 = *ST1;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fldt	%3\n"
			"fpatan\n"
			"fnstsw	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR1),"m"(WFR0) : "memory" );
			INCFSPP;
			fssync();
			*ST0 = WFR0;
			break;
		   case 2:		/* FPTAN */
	   		WFR0 = *ST0;
			__asm__ __volatile__ (
			"fldt	%3\n"
			"fptan\n"
			"fnstsw	%2\n"
			"fstpt	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=m"(WFR1),"=g"(WFRS) : "m"(WFR0) : "memory" );
			*ST0 = WFR0; DECFSPP;
			fssync();
			*ST0 = WFR1;
			break;
		   case 4:		/* FXTRACT */
	   		WFR0 = *ST0;
			__asm__ __volatile__ (
			"fldt	%3\n"
			"fxtract\n"
			"fnstsw	%2\n"
			"fstpt	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=m"(WFR1),"=g"(WFRS) : "m"(WFR0) : "memory" );
			*ST0 = WFR0; DECFSPP;
			fssync();
			*ST0 = WFR1;
			break;
		   case 5:		/* FPREM1 */
	   		WFR0 = *ST0;
			WFR1 = *ST1;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fldt	%3\n"
			"fprem1\n"
			"fnstsw	%1\n"
			"fstpt	%0\n"
			"fstp	%%st(0)" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR1),"m"(WFR0) : "memory" );
			fssync();
			*ST0 = WFR0;
			break;
		   case 6:		/* FDECSTP */
			DECFSP; break;
		   case 7:		/* FINCSTP */
			INCFSP; break;
		   }
		   break;

/*79*/	case 0x79: switch(reg) {
//	79.0	D9 11111000	FPREM	st(0)/st(1)->st(0)
//	79.1	D9 11111001	FYL2XP1	st(1)*lg(st(0))->st(1),pop
//	79.2	D9 11111010	FSQRT	st(0)
//	79.3	D9 11111011	FSINCOS	sin->st(0), push cos
//	79.4	D9 11111100	FRNDINT	st(0)
//	79.5	D9 11111101	FSCALE	st(0) by st(1)
//	79.6	D9 11111110	FSIN	st(0)
//	79.7	D9 11111111	FCOS	st(0)
		   case 0:		/* FPREM */
	   		WFR0 = *ST0;
			WFR1 = *ST1;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fldt	%3\n"
			"fprem\n"
			"fnstsw	%1\n"
			"fstpt	%0\n"
			"fstp	%%st(0)" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR1),"m"(WFR0) : "memory" );
			fssync();
			*ST0 = WFR0;
			break;
		   case 5:		/* FSCALE */
	   		WFR0 = *ST0;
			WFR1 = *ST1;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fldt	%3\n"
			"fscale\n"
			"fnstsw	%1\n"
			"fstpt	%0\n"
			"fstp	%%st(0)" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR1),"m"(WFR0) : "memory" );
			fssync();
			*ST0 = WFR0;
			break;
		   case 1:		/* FYL2XP1 */
	   		WFR0 = *ST0;
			WFR1 = *ST1;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fldt	%3\n"
			"fyl2xp1\n"
			"fnstsw	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR1),"m"(WFR0) : "memory" );
			INCFSPP;
			fssync();
			*ST0 = WFR0;
			break;
		   case 2:		/* FSQRT */
	   		WFR0 = *ST0;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fsqrt\n"
			"fnstsw	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR0) : "memory" );
			fssync();
			*ST0 = WFR0;
			break;
		   case 4:		/* FRNDINT */
	   		WFR0 = *ST0;
			round_double();
			ftest(WFR0);
			*ST0 = WFR0;
			break;
		   case 6:		/* FSIN */
	   		WFR0 = *ST0;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fsin\n"
			"fnstsw	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR0) : "memory" );
			fssync();
			*ST0 = WFR0;
			break;
		   case 7:		/* FCOS */
	   		WFR0 = *ST0;
			__asm__ __volatile__ (
			"fldt	%2\n"
			"fcos\n"
			"fnstsw	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=g"(WFRS) : "m"(WFR0) : "memory" );
			fssync();
			*ST0 = WFR0;
			break;
		   case 3:		/* FSINCOS */
	   		WFR0 = *ST0;
			__asm__ __volatile__ (
			"fldt	%3\n"
			"fsincos\n"
			"fnstsw	%2\n"
			"fstpt	%1\n"
			"fstpt	%0" : "=m"(WFR0),"=m"(WFR1),"=g"(WFRS) : "m"(WFR0) : "memory" );
			*ST0 = WFR0; DECFSPP;
			fssync();
			*ST0 = WFR1;
			break;
		   }
		   break;

/*21*/	case 0x21:
/*25*/	case 0x25: {
//*	21	D9 xx100nnn	FLDENV	14/28byte
//	25	DD xx100nnn	FRSTOR	94/108byte
		    dosaddr_t p = TheCPU.mem_ref, q;
		    TheCPU.fpuc = read_word(p) | 0x40;
		    if (reg&DATA16) {
			TheCPU.fpus = read_word(p+2); TheCPU.fptag = read_word(p+4);
			q = p+14;
		    }
		    else {
			TheCPU.fpus = read_word(p+4); TheCPU.fptag = read_word(p+8);
			q = p+28;
		    }
		    TheCPU.fpstt = (TheCPU.fpus>>11)&7;
		    if (exop==0x25) {
			int i, k;
			k = TheCPU.fpstt;
			for (i=0; i<8; i++) {
			    TheCPU.fpregs[k] = read_long_double(q);
			    k = (k+1)&7; q += 10;
			}
		    }
		   }
		   break;

/*
 * FSAVE: 4 modes - followed by FINIT
 *
 * A) 16-bit real (94 bytes)
 *	(00-01)		Control Word
 *	(02-03)		Status Word
 *	(04-05)		Tag Word
 *	(06-07)		IP 15..00
 *	(08-09)		IP 19..16,0,Opc 10..00
 *	(0a-0b)		OP 15..00
 *	(0c-0d)		OP 19..16,0...
 *	(0e-5d)		FP registers
 *
 * B) 16-bit protected (94 bytes)
 *	(00-01)		Control Word
 *	(02-03)		Status Word
 *	(04-05)		Tag Word
 *	(06-07)		IP offset
 *	(08-09)		IP selector
 *	(0a-0b)		OP offset
 *	(0c-0d)		OP selector
 *	(0e-5d)		FP registers
 *
 * C) 32-bit real (108 bytes)
 *	(00-01)		Control Word		(02-03) reserved
 *	(04-05)		Status Word		(06-07) reserved
 *	(08-09)		Tag Word		(0a-0b) reserved
 *	(0c-0d)		IP 15..00		(0e-0f) reserved
 *	(10-13)		0,0,0,0,IP 31..16,0,Opc 10..00
 *	(14-15)		OP 15..00		(16-17) reserved
 *	(18-1b)		0,0,0,0,OP 31..16,0...
 *	(1c-6b)		FP registers
 *
 * D) 32-bit protected (108 bytes)
 *	(00-01)		Control Word		(02-03) reserved
 *	(04-05)		Status Word		(06-07) reserved
 *	(08-09)		Tag Word		(0a-0b) reserved
 *	(0c-0d)		IP offset		(0e-0f) reserved
 *	(10-13)		0,0,0,0,Opc 10..00,IP selector
 *	(14-15)		OP offset		(16-17) reserved
 *	(18-19)		OP selector		(1a-1b) reserved
 *	(1c-6b)		FP registers
 */
/*31*/	case 0x31:
/*35*/	case 0x35: {
		    dosaddr_t q;
		    int i;
		    unsigned fptag, ntag;
//*	31	D9 xx110nnn	FSTENV	14/28byte
//	35	DD xx110nnn	FSAVE	94/108byte
		    TheCPU.fpus = (TheCPU.fpus & ~0x3800) | (TheCPU.fpstt<<11);
//
		    fptag = TheCPU.fptag; ntag=0;
		    for (i=7; i>=0; --i) {
			long double d = TheCPU.fpregs[i];
			ntag <<= 2;
			if ((fptag & 0xc000) == 0xc000) ntag |= 3;
			else if (isnan(d) || !isnormal(d) || isinf(d)) ntag |= 2;
			else if (d == 0) ntag |= 1;
			fptag <<= 2;
		    }
		    TheCPU.fptag = ntag;
		    dosaddr_t p = TheCPU.mem_ref;
		    if (reg&DATA16) {
			write_word(p, TheCPU.fpuc);
			write_word(p+2, TheCPU.fpus);
			write_word(p+4, TheCPU.fptag);
			/* IP,OP,opcode: n.i. */
			write_qword(p+6, 0);
			q = p+14;
		    }
		    else {
			dosaddr_t p = TheCPU.mem_ref;
			write_dword(p, TheCPU.fpuc);
			write_dword(p+4, TheCPU.fpus);
			write_dword(p+8, TheCPU.fptag);
			/* IP,OP,opcode: n.i. */
			write_qword(p+12, 0);
			write_qword(p+20, 0);
			q = p+28;
		    }
		    TheCPU.fpuc |= 0x3f;
		    if (exop==0x35) {
			int i, k;
			k = TheCPU.fpstt;
			for (i=0; i<8; i++) {
			    write_long_double(q, TheCPU.fpregs[k]);
			    k = (k+1)&7; q += 10;
			}
			TheCPU.fpus  = 0;
			TheCPU.fpstt = 0;
			TheCPU.fpuc  = 0x37f;
			TheCPU.fptag = 0xffff;
			__asm__ __volatile__ ("fninit");
			WFR0 = WFR1 = 0.0;
		    }
		   }
		   break;

/*xx*/	default:
fp_notok:
	return -1;
	}
fp_ok:
	return 0;
}

