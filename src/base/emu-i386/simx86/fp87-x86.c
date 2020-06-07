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
#include "emu86.h"

#ifdef HOST_ARCH_X86

#include "codegen-x86.h"

static int Fp87_op_x86_sim(int exop, int reg);

/*
 * Only mask bits 0-5 of the control word (fpuc),
 * and memory and ax (for fstsw) references are emulated.
 * All other FPU state is native.
 */

struct float_env16 {
	uint16_t fpuc;
	uint16_t fpus;
	uint16_t fptag;
	uint16_t ignored[4];
	unsigned char fpregs[80];
};

struct float_env32 {
	uint16_t fpuc;
	uint16_t dummy1;
	uint16_t fpus;
	uint16_t dummy2;
	uint16_t fptag;
	uint16_t dummy3;
	uint32_t ignored[4];
	unsigned char fpregs[80];
};

void init_emu_npu_x86 (void)
{
	Fp87_op = Fp87_op_x86_sim;
	TheCPU.fpuc = 0x37f;
}

unsigned char *Fp87_op_x86(unsigned char *CodePtr, int exop, int reg)
{
	register unsigned char *Cp = CodePtr;

//	0B	DB xx001nnn	FISTTP	dw // SSE3 capable CPUs
//	0D	DD xx001nnn	FISTTP	qw // SSE3 capable CPUs
//	0F	DF xx001nnn	FISTTP	w  // SSE3 capable CPUs

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

/*37*/	case 0x37:
//	37	DF xx110nnn	FBSTP
/*3b*/	case 0x3b:
/*3f*/	case 0x3f:
//	3B	DB xx111nnn	FSTP	ext
//	3F	DF xx111nnn	FISTP	qw
fp_mem:
		G2M(0xd8+(exop&7),(exop&0x38)|7,Cp);	// Fop (edi)
		break;

/*29*/	case 0x29:
//*	29	D9 xx101nnn	FLDCW	2b
		// movw	(edi),ax
		G3(0x078b66,Cp);
		// movl	eax,ecx
		G2(0xc189,Cp);
		// orb 0x3f,al
		G2(0x3f0c,Cp);
		// movw	ax,FPUC(ebx)
		G3(0x438966,Cp); G1(Ofs_FPUC,Cp);
		// fldcw FPUC(ebx)
		G2(0x6bd9,Cp); G1(Ofs_FPUC,Cp);
		// movw	cx,FPUC(ebx)
		G3(0x4b8966,Cp); G1(Ofs_FPUC,Cp);
		break;
/*39*/	case 0x39:
//*	39	D9 xx111nnn	FSTCW	2b
		// movb FPUC(ebx),cl
		G2(0x4b8a,Cp); G1(Ofs_FPUC,Cp);
		// andb 0x3f,cl
		G3(0x3fe180,Cp);
		// fstcw FPUC(ebx)
		G2(0x7bd9,Cp); G1(Ofs_FPUC,Cp);
		// movw	FPUC(ebx),ax
		G3(0x438b66,Cp); G1(Ofs_FPUC,Cp);
		// andb ~0x3f,al
		G2(0xc024,Cp);
		// orb cl,al
		G2(0xc808,Cp);
		// movw	ax,FPUC(ebx)
		G3(0x438966,Cp); G1(Ofs_FPUC,Cp);
		// movw ax,(edi)
		G3(0x078966,Cp);
		break;

/*3d*/	case 0x3d: goto fp_mem;
/*67*/	case 0x67: if (reg!=0) goto fp_notok;
//	67.0	DF 11000000	FSTSW	ax
//	3D	DD xx111nnn	FSTSW	2b
		G3M(0xdd,0x7b,Ofs_AX,Cp);	// FSTSW Ofs_AX(%%ebx)
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

/*50*/	case 0x50:
/*58*/	case 0x58:
//*	50	D8 11010nnn	FCOM	st,st(n)
//*	58	D8 11011nnn	FCOMP	st,st(n)
		goto fp_op;

/*6a*/	case 0x6a: if (reg!=1) goto fp_notok;
/*6d*/	case 0x6d:
/*65*/	case 0x65:
//	65	DD 11000nnn	FUCOM	st(n),st(0)
//	6D	DD 11101nnn	FUCOMP	st(n)
//	6A.1	DA 11101001	FUCOMPP

//	73	DB 11000nnn	FCOMI	st(0),st(n)
//	77	DF 11000nnn	FCOMIP	st(0),st(n)
//	6B	DB 11101nnn	FUCOMI	st(0),st(n)
//	6F	DF 11101nnn	FUCOMIP	st(0),st(n)
		goto fp_op;

/*5e*/	case 0x5e: if (reg==1) {
//	5E.1	DE 11011001	FCOMPP
			goto fp_op;
		   }
		   else goto fp_notok;

/*44*/	case 0x44:
/*4c*/	case 0x4c:
/*46*/	case 0x46:
/*4e*/	case 0x4e:
//	44	DC 11000nnn	FADD	st(n),st
//	4C	DC 11001nnn	FMUL	st(n),st
//	46	DE 11000nnn	FADDP	st(n),st
//	4E	DE 11001nnn	FMULP	st(n),st

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

/*41*/	case 0x41:
//*	41	D9 11000nnn	FLD	st(n)
		goto fp_op;

/*45*/	case 0x45: goto fp_op;
/*51*/	case 0x51: if (reg==0) {
//	45	DD 11000nnn	FFREE	st(n)		set tag(n) empty
//*	51.0	D9 11010000	FNOP
			goto fp_op;
		   }
		   else goto fp_notok;
		   break;

/*49*/	case 0x49:
//*	49	D9 11001nnn	FXCH	st,st(n)

/*55*/	case 0x55:
/*5d*/	case 0x5d:
//	55	DD 11010nnn	FST	st(n)
//	5D	DD 11011nnn	FSTP	st(n)
		goto fp_op;

/*61*/	case 0x61:
//*	61.0	D9 11100000	FCHS
//*	61.1	D9 11100001	FABS
//*	61.4	D9 11100100	FTST
//*	61.5	D9 11100101	FXAM
		switch(reg) {
		   case 0:		/* FCHS */
		   case 1:		/* FABS */
		   case 4:		/* FTST */
		   case 5:		/* FXAM */
			goto fp_op;
		   default:
			goto fp_notok;
		}
		break;

/*63*/	case 0x63: switch(reg) {
//	63.2*	DB 11000010	FCLEX
//	63.3*	DB 11000011	FINIT
		   case 2:		/* FCLEX */
			goto fp_op;
		   case 3:		/* FINIT */
			// movw	0x37f,FPUC(ebx)
			G3M(0x66,0xc7,0x43,Cp); G1(Ofs_FPUC,Cp); G2(0x37f,Cp);
			goto fp_op;
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
			if (reg==7) goto fp_notok;
		   }
		   goto fp_op;

/*71*/	case 0x71:
//	71.0	D9 11110000	FX2M1	st(0)
//	71.1	D9 11110001	FYL2X	st(1)*l2(st(0))->st(1),pop
//	71.2	D9 11110010	FPTAN	st(0),push 1
//	71.3	D9 11110011	FPATAN	st(1)/st(0)->st(1),pop
//	71.4	D9 11110100	FXTRACT	exp->st(0),push signif
//	71.5	D9 11110101	FPREM1	st(0)/st(1)->st(0)
//	71.6	D9 11110110	FDECSTP
//	71.7	D9 11110111	FINCSTP

/*79*/	case 0x79:
//	79.0	D9 11111000	FPREM	st(0)/st(1)->st(0)
//	79.1	D9 11111001	FYL2XP1	st(1)*lg(st(0))->st(1),pop
//	79.2	D9 11111010	FSQRT	st(0)
//	79.3	D9 11111011	FSINCOS	sin->st(0), push cos
//	79.4	D9 11111100	FRNDINT	st(0)
//	79.5	D9 11111101	FSCALE	st(0) by st(1)
//	79.6	D9 11111110	FSIN	st(0)
//	79.7	D9 11111111	FCOS	st(0)
	fp_op:
		G2M(0xd8+(exop&7),0xc0|(exop&0x38)|reg,Cp);	// Fop (st(reg))
		break;

/*xx*/	default:
fp_notok:
	return NULL;
	}
fp_ok:
	return Cp;
}

static int Fp87_op_x86_sim(int exop, int reg)
{
	e_printf("FPop %x.%d\n", exop, reg);

	switch(exop) {
/*21*/	case 0x21:
/*25*/	case 0x25: {
//*	21	D9 xx100nnn	FLDENV	14/28byte
//	25	DD xx100nnn	FRSTOR	94/108byte
		    void *p = LINEAR2UNIX(TheCPU.mem_ref);
		    if (reg&DATA16) {
			struct float_env16 q;
		        memcpy(&q, p, (exop == 0x21 ? 14 : 94));
			TheCPU.fpuc = q.fpuc;
			/* mask exceptions in real FPU control word */
			q.fpuc |= 0x3f;
			if (exop==0x21)
			    __asm__ __volatile__ ("data16 fldenv %0\n" :: "m"(q));
			else
			    __asm__ __volatile__ ("data16 frstor %0\n" :: "m"(q));
		    }
		    else {
			struct float_env32 q;
		        memcpy(&q, p, (exop == 0x21 ? 28 : 108));
			TheCPU.fpuc = q.fpuc;
			/* mask exceptions in real FPU control word */
			q.fpuc |= 0x3f;
			if (exop==0x21)
			    __asm__ __volatile__ ("fldenv	%0\n" :: "m"(q));
			else
			    __asm__ __volatile__ ("frstor	%0\n" :: "m"(q));
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
//*	31	D9 xx110nnn	FSTENV	14/28byte
//	35	DD xx110nnn	FSAVE	94/108byte
		    if (exop==0x31) {
			struct float_env16 *p = (struct float_env16 *)LINEAR2UNIX(TheCPU.mem_ref);
			if (reg&DATA16)
			    __asm__ __volatile__ ("data16 fnstenv %0\n":"=m"(*p));
			else
			    __asm__ __volatile__ ("fnstenv	%0\n" : "=m"(*p));
			p->fpuc = (p->fpuc & ~0x3f) | (TheCPU.fpuc & 0x3f);
		    }
		    else {
			struct float_env32 *p = (struct float_env32 *)LINEAR2UNIX(TheCPU.mem_ref);
			if (reg&DATA16)
			    __asm__ __volatile__ ("data16 fnsave %0\n" : "=m"(*p));
			else
			    __asm__ __volatile__ ("fnsave	%0\n" : "=m"(*p));
			p->fpuc = (p->fpuc & ~0x3f) | (TheCPU.fpuc & 0x3f);
		    }
		    TheCPU.fpuc |= 0x3f;
		    if (exop==0x35) {
			TheCPU.fpuc = 0x37f;
			__asm__ __volatile__ ("fninit");
		    }
		   }
		   break;

/*xx*/	default:
	return -1;
	}
	return 0;
}

#endif
