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

#include <stddef.h>
#include "emu86.h"

#ifdef HOST_ARCH_X86

#include "codegen.h"
#include <math.h>

static int Fp87_op_x86(int exop, int reg);

/*
 * Tags are not completely implemented, and also all the stuff is in
 * 64-bit, not 80-bit, precision, so no wonder if you get errors in FP
 * test programs. It was meant to be moved one day to other CPUs that
 * have no idea of an 80-bit FP type.
 */

unsigned short nxta[8] =
{	0x0801,0x1002,0x1803,0x2004,0x2805,0x3006,0x3807,0x0000 };

unsigned short nxta2[8] =
{	0x1002,0x1803,0x2004,0x2805,0x3006,0x3807,0x0000,0x0801 };

unsigned short prva[8] =
{	0x3807,0x0000,0x0801,0x1002,0x1803,0x2004,0x2805,0x3006 };

#define FPSELEM		sizeof(double)

/*
 * What's the trick here?
 * the FP regs are aligned on a 256-byte boundary, so that the lowest
 * byte of FP0 is 0, of FP1 is 8, etc. and we can adjust the pointer
 * FPRSTT only by changing its lowest byte.
 */
//	movl	FPRSTT(ebx),esi
#define FTOS2ESI	{G2(0xb38b,Cp);G4((char *)&FPRSTT-CPUOFFS(0),Cp);}
//	movl	FPRSTT(ebx),ecx
#define FTOS2ECX	{G2(0x8b8b,Cp);G4((char *)&FPRSTT-CPUOFFS(0),Cp);}
//	ffree st(0)
//	fincstp
#define DISCARD		G4(0xf7d9c0dd,Cp)

// if r!=0
//	leal	n*8(esi),ecx
//	andb	0x3f,cl
// if r==0
//	movl	esi,ecx
#define FSRN2CX(r)	{if (r) {G2(0x4e8d,Cp);G1((r)*FPSELEM,Cp);G3(0x3fe180,Cp);}\
			 else G2(0xf189,Cp);}
//	movl	FPRSTT,ecx
// if r!=0
//	addb	(reg*8),cl
//	andb	0x3f,cl
#define FTOS2CX(r)	{FTOS2ECX; if (r) {G2(0xc180,Cp);G1((r)*FPSELEM,Cp);G3(0x3fe180,Cp);}}

//	fldl	(esi)
#define FLDfromESI	G2(0x06dd,Cp)
//	fstpl	(esi)
#define FSTPfromESI	G2(0x1edd,Cp)
//	fldl	(ecx)
#define FLDfromECX	G2(0x01dd,Cp)
//	fstpl	(ecx)
#define FSTPfromECX	G2(0x19dd,Cp)

// ! replace TOS field !
//	fstcw	FPUS(ebx)
#define	SAVESW		{G3(0x7bdd9b,Cp);G1(Ofs_FPUS,Cp);}

#if __x86_64__
// leaq	&x(rbx),rcx
#define GETADDR_IN_CX(x,Cp) G3M(0x48,0x8d,0x8b,Cp);G4((char *)&x-CPUOFFS(0),Cp);
#else
// movl	&x,ecx
#define GETADDR_IN_CX(x,Cp) G1(0xb9,Cp);G4((long)&x,Cp);
#endif


//	movzbl	FPSTT(ebx),eax
//	movl	&nxta,ecx
//	movw	(ecx,eax,2),ax
//	movb	al,FPSTT(ebx)
//	movb	ah,FPRSTT(ebx)
#define INCFSP	{G3(0x43b60f,Cp);G1(Ofs_FPSTT,Cp);GETADDR_IN_CX(nxta,Cp);\
		G4(0x41048b66,Cp);G2(0x4388,Cp);G1(Ofs_FPSTT,Cp);G2(0xa388,Cp);\
		G4((char *)&FPRSTT-CPUOFFS(0),Cp);}
//	movzbl	FPSTT(ebx),eax
//	movl	&nxta2,ecx
//	movw	(ecx,eax,2),ax
//	movb	al,FPSTT(ebx)
//	movb	ah,FPRSTT(ebx)
#define INCFSP2	{G3(0x43b60f,Cp);G1(Ofs_FPSTT,Cp);GETADDR_IN_CX(nxta2,Cp);\
		G4(0x41048b66,Cp);G2(0x4388,Cp);G1(Ofs_FPSTT,Cp);G2(0xa388,Cp);\
		G4((char *)&FPRSTT-CPUOFFS(0),Cp);}
//	movzbl	FPSTT(ebx),eax
//	movl	&prva,ecx
//	movw	(ecx,eax,2),ax
//	movb	al,FPSTT(ebx)
//	movb	ah,FPRSTT(ebx)
#define DECFSP	{G3(0x43b60f,Cp);G1(Ofs_FPSTT,Cp);GETADDR_IN_CX(prva,Cp);\
		G4(0x41048b66,Cp);G2(0x4388,Cp);G1(Ofs_FPSTT,Cp);G2(0xa388,Cp);\
		G4((char *)&FPRSTT-CPUOFFS(0),Cp);}

/* required to align the 64-byte FP register block to a 256-byte boundary */
static char _fparea[320];

double *FPRSTT;

void init_emu_npu_x86 (void)
{
	int i;
	Fp87_op = Fp87_op_x86;

	/* align 64-byte FP regs on a 256-byte boundary */
	TheCPU.fpregs = (double *)(((long)_fparea+0x100)&~0xff);
	e_printf("FPU: register area %08lx\n",(long)TheCPU.fpregs);
	for (i=0; i<8; i++) {
		unsigned long a = (unsigned long)(&TheCPU.fpregs[i]);
		nxta[(i-1)&7] = prva[(i+1)&7] = nxta2[(i-2)&7] =
			(a&0xff)<<8 | i;
	}
	FPRSTT = TheCPU.fpregs;
	TheCPU.fpus = 0;
	TheCPU.fpstt = 0;
	TheCPU.fpuc = 0x37f;
	TheCPU.fptag = 0xffff;
	__asm__ __volatile__ ("fninit");
}

static int Fp87_op_x86(int exop, int reg)
{
	unsigned char rcod;
	register unsigned char *Cp = CodePtr;

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
//	27	DF xx100nnn	FBLD		!! err - uses 80 bits
/*2b*/	case 0x2b:
/*2f*/	case 0x2f:
//	2B	DB xx101nnn	FLD	ext
//	2F	DF xx101nnn	FILD	qw
		DECFSP;	FTOS2ESI;
		G2M(0xd8+(exop&7),(exop&0x38)|7,Cp);	// Fop (edi)
		SAVESW;	FSTPfromESI;
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
		FTOS2ESI; FLDfromESI;
		G2M(0xd8+(exop&7),(exop&0x38)|7,Cp);	// Fop (edi)
		SAVESW;	FSTPfromESI;
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
		FTOS2ESI; FLDfromESI;
		G2M(0xd8+(exop&7),0x17,Cp);	// Fop (edi), no pop
		SAVESW; DISCARD;
		if (exop&8) INCFSP;
		break;

/*37*/	case 0x37:
//	37	DF xx110nnn	FBSTP		!! err - uses 80 bits
		rcod = 0x37; goto fpp002;
/*3b*/	case 0x3b:
/*3f*/	case 0x3f:
//	3B	DB xx111nnn	FSTP	ext
//	3F	DF xx111nnn	FISTP	qw
		rcod = 0x3f;
fpp002:
		FTOS2ESI; FLDfromESI;
		G2M(0xd8+(exop&7),rcod,Cp);	// Fop (edi), pop
		SAVESW; INCFSP;
		break;

/*29*/	case 0x29:
//*	29	D9 xx101nnn	FLDCW	2b
		// movw	(edi),ax
		G3(0x078b66,Cp);
		// movl	eax,ecx
		G2(0xc189,Cp);
		// andb	0x1f,ah
		G3(0x1fe480,Cp);
		// movb 0xff,al
		G2(0xffb0,Cp);
		// movw	ax,FPUC(ebx)		
		G3(0x438966,Cp); G1(Ofs_FPUC,Cp);
		// fldcw FPUC(ebx)
		G2(0x6bd9,Cp); G1(Ofs_FPUC,Cp);
		// movw	cx,FPUC(ebx)
		G3(0x4b8966,Cp); G1(Ofs_FPUC,Cp);
		break;
/*39*/	case 0x39:
//*	39	D9 xx111nnn	FSTCW	2b
		G3(0x438b66,Cp); G1(Ofs_FPUC,Cp); G3(0x078966,Cp);
		break;

/*67*/	case 0x67: if (reg!=0) goto fp_notok;
//	67.0	DF 11000000	FSTSW	ax
/*3d*/	case 0x3d:
//	3D	DD xx111nnn	FSTSW	2b
		// movw	FPUS(ebx),ax
		G3(0x438b66,Cp); G1(Ofs_FPUS,Cp);
		// andw	0xc7ff,ax
		// movw	FPSTT(ebx),cx
		G3(0xff2566,Cp); G4(0x4b8b66c7,Cp); G1(Ofs_FPSTT,Cp);
		// andw	0x07,cx
		// shll	11,ecx
		// orl	ecx,eax
		G4(0x07e18366,Cp); G4(0x090be1c1,Cp); G1(0xc8,Cp);
		if (exop==0x3d) {
			// movw	ax,(edi)
			G3(0x078966,Cp);
		}
		else {
			G3(0x438966,Cp); G1(Ofs_AX,Cp);
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
		FTOS2ESI;		// &e_st(0) -> esi
		FSRN2CX(reg);		// &e_st(n) -> ecx
		FLDfromESI;		// (esi) -> st(0)
		G2M(0xdc,0x01|(exop&0x38),Cp);	// op (ecx)
		SAVESW;
		FSTPfromESI;		// st(0) -> (esi)
		break;

/*50*/	case 0x50:
/*58*/	case 0x58:
//*	50	D8 11010nnn	FCOM	st,st(n)
//*	58	D8 11011nnn	FCOMP	st,st(n)
		FTOS2ESI; FSRN2CX(reg);
		FLDfromESI; G2M(0xdc,0x11,Cp);	// fcom (ecx)
		SAVESW;	DISCARD;
		if (exop&8) { INCFSP; }
		break;

/*6a*/	case 0x6a: if (reg!=1) goto fp_notok;
/*6d*/	case 0x6d:
/*65*/	case 0x65:
//	65	DD 11000nnn	FUCOM	st(n),st(0)
//	6D	DD 11101nnn	FUCOMP	st(n)
//	6A.1	DA 11101001	FUCOMPP
		FTOS2ESI; FSRN2CX(reg);
		FLDfromECX;	// (ecx)->st(1)
		FLDfromESI;	// (esi)->st(0)
		G2M(0xda,0xe9,Cp);	// fucompp
		SAVESW;
		if (exop==0x6a) { INCFSP; }
		if (exop>=0x6a) { INCFSP; }
		break;

//	73	DB 11000nnn	FCOMI	st(0),st(n)
//	77	DF 11000nnn	FCOMIP	st(0),st(n)
//	6B	DB 11101nnn	FUCOMI	st(0),st(n)
//	6F	DF 11101nnn	FUCOMIP	st(0),st(n)

/*5e*/	case 0x5e: if (reg==1) {
//	5E.1	DE 11011001	FCOMPP
			FTOS2ESI; FSRN2CX(1);
			FLDfromESI; G2M(0xdc,0x11,Cp);
			SAVESW;	DISCARD;
			INCFSP2;
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
		FTOS2ESI; FSRN2CX(reg);
		FLDfromECX;
		G2M(0xdc,0x06|(exop&0x38),Cp);	// op (esi)
		SAVESW; FSTPfromECX;
		if (exop&2) { INCFSP; }
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
		FTOS2ESI; FSRN2CX(reg);
		FLDfromECX;
		G2M(0xdc,0x06|((exop&0x38)^8),Cp);
		SAVESW; FSTPfromECX;
		if (exop&2) { INCFSP; }
		break;

/*41*/	case 0x41:
//*	41	D9 11000nnn	FLD	st(n)
		FTOS2CX(reg);		// &e_st(n) -> ecx
		FLDfromECX; SAVESW;
		DECFSP;	FTOS2ESI; FSTPfromESI;
		break;

/*45*/	case 0x45: reg=0;
/*51*/	case 0x51: if (reg==0) {
//	45	DD 11000nnn	FFREE	st(n)		set tag(n) empty
//*	51.0	D9 11010000	FNOP
			G1(NOP,Cp);
		   }
		   else goto fp_notok;
		   break;

/*49*/	case 0x49:
//*	49	D9 11001nnn	FXCH	st,st(n)
		FTOS2ESI; FSRN2CX(reg);
//		FLDfromESI; FLDfromECX;
//		FSTPfromESI; FSTPfromECX;
/*integer*/	G4(0x118b068b,Cp); G4(0x16890189,Cp);
		G4(0x8b04468b,Cp); G4(0x41890451,Cp); G4(0x04568904,Cp);
		break;

/*55*/	case 0x55:
/*5d*/	case 0x5d:
//	55	DD 11010nnn	FST	st(n)
//	5D	DD 11011nnn	FSTP	st(n)
		FTOS2ESI; FSRN2CX(reg);
		FLDfromESI; SAVESW; FSTPfromECX;
		if (exop==0x5d) { INCFSP; }
		break;

/*61*/	case 0x61:
//*	61.0	D9 11100000	FCHS
//*	61.1	D9 11100001	FABS
//*	61.4	D9 11100100	FTST
//*	61.5	D9 11100101	FXAM
		FTOS2ESI; FLDfromESI;
		switch(reg) {
		   case 0:		/* FCHS */
			G2(0xe0d9,Cp); break;
		   case 1:		/* FABS */
			G2(0xe1d9,Cp); break;
		   case 4:		/* FTST */
			G2(0xe4d9,Cp); break;
		   case 5:		/* FXAM */
			G2(0xe5d9,Cp); break;
		   default:
			goto fp_notok;
		}
		SAVESW; FSTPfromESI;
		break;

/*63*/	case 0x63: switch(reg) {
//	63.2*	DB 11000010	FCLEX
//	63.3*	DB 11000011	FINIT
		   case 2:		/* FCLEX */
			TheCPU.fpus &= 0x7f00;
			break;
		   case 3:		/* FINIT */
			FPRSTT = TheCPU.fpregs;
			TheCPU.fpus = 0;
			TheCPU.fpstt = 0;
			TheCPU.fpuc = 0x37f;
			TheCPU.fptag = 0xffff;
			__asm__ __volatile__ ("fninit");
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
			if (reg==7) goto fp_notok;
			DECFSP; FTOS2ESI;
			G2M(0xd9,0xe8|reg,Cp); SAVESW;
			FSTPfromESI;
		   }
		   break;

/*71*/	case 0x71: switch(reg) {
//	71.0	D9 11110000	FX2M1	st(0)
//	71.1	D9 11110001	FYL2X	st(1)*l2(st(0))->st(1),pop
//	71.2	D9 11110010	FPTAN	st(0),push 1
//	71.3	D9 11110011	FPATAN	st(1)/st(0)->st(1),pop
//	71.4	D9 11110100	FXTRACT	exp->st(0),push signif
//	71.5	D9 11110101	FPREM1	st(0)/st(1)->st(0)
//	71.6	D9 11110110	FDECSTP
//	71.7	D9 11110111	FINCSTP
		   case 0:		/* FX2M1 */
			FTOS2ESI; FLDfromESI;
			G2(0xf0d9,Cp); SAVESW;
			FSTPfromESI; break;
		   case 1:		/* FYL2X */
		   case 3:		/* FPATAN */
			FTOS2ESI; FSRN2CX(1);
			FLDfromECX;	// (ecx)->st(1)
			FLDfromESI;	// (esi)->st(0)
			G2M(0xd9,0xf0|reg,Cp); SAVESW;
			FSTPfromECX;	// st(0)->(ecx)
			INCFSP; break;
		   case 2:		/* FPTAN */
		   case 4:		/* FXTRACT */
			FTOS2ESI; FLDfromESI;
			G2M(0xd9,0xf0|reg,Cp); SAVESW;
			G2(0xc9d9,Cp);	// xchg: st(1)->(esi)
			FSTPfromESI; DECFSP; FTOS2ESI; FSTPfromESI;
			break;
		   case 5:		/* FPREM1 */
			FTOS2ESI; FSRN2CX(1);
			FLDfromECX;	// (ecx)->st(1)
			FLDfromESI;	// (esi)->st(0)
			G2(0xf5d9,Cp); SAVESW;
			FSTPfromESI; DISCARD;
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
		   case 5:		/* FSCALE */
			FTOS2ESI; FSRN2CX(1);
			FLDfromECX;	// (ecx)->st(1)
			FLDfromESI;	// (esi)->st(0)
			G2M(0xd9,0xf8|reg,Cp); SAVESW;
			FSTPfromESI; DISCARD;
			break;
		   case 1:		/* FYL2XP1 */
			FTOS2ESI; FSRN2CX(1);
			FLDfromECX;	// (ecx)->st(1)
			FLDfromESI;	// (esi)->st(0)
			G2(0xf9d9,Cp);
			FSTPfromECX;	// st(0)->(ecx)
			SAVESW; INCFSP; break;
		   case 2:		/* FSQRT */
		   case 4:		/* FRNDINT */
		   case 6:		/* FSIN */
		   case 7:		/* FCOS */
			FTOS2ESI; FLDfromESI;
			G2M(0xd9,0xf8|reg,Cp);
			SAVESW; FSTPfromESI; break;
		   case 3:		/* FSINCOS */
			FTOS2ESI; FLDfromESI;
			G2(0xfbd9,Cp); SAVESW;
			G2(0xc9d9,Cp);	// xchg: st(1)->(esi)
			FSTPfromESI; DECFSP; FTOS2ESI; FSTPfromESI;
			break;
		   }
		   break;

/*21*/	case 0x21:
/*25*/	case 0x25: {
//*	21	D9 xx100nnn	FLDENV	14/28byte
//	25	DD xx100nnn	FRSTOR	94/108byte
		    unsigned short *p = (unsigned short *)TheCPU.mem_ref;
		    char *q;
		    if (reg&DATA16) {
			TheCPU.fpuc = p[0]; TheCPU.fpus = p[1]; TheCPU.fptag = p[2];
			q = (char *)(p+7);
		    }
		    else {
			TheCPU.fpuc = p[0]; TheCPU.fpus = p[2]; TheCPU.fptag = p[4];
			q = (char *)(p+14);
		    }
		    TheCPU.fpstt = (TheCPU.fpus>>11)&7;
		    FPRSTT = &TheCPU.fpregs[TheCPU.fpstt];
		    if (exop==0x25) {
			int i, k;
			k = TheCPU.fpstt;
			for (i=0; i<8; i++) {
			    double *sf = &TheCPU.fpregs[k];
			    __asm__ __volatile__ (" \
				fldt	%1\n \
				fstpl	%0" \
				: "=m"(*sf) : "m"(*q));
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
		    char *q;
//*	31	D9 xx110nnn	FSTENV	14/28byte
//	35	DD xx110nnn	FSAVE	94/108byte
		    TheCPU.fpus = (TheCPU.fpus & ~0x3800) | (TheCPU.fpstt<<11);
//
#if 0
		    { int i, fptag, ntag;
		    fptag = TheCPU.fptag; ntag=0;
		    for (i=7; i>=0; --i) {
	    		unsigned short *sf = (unsigned short *)&(TheCPU.fpregs[i]);
			ntag <<= 2;
			if ((fptag & 0xc000) == 0xc000) ntag |= 3;
			else {
			    if (!L_ISZERO(sf)) {
				ntag |= 1;
			    }
			    else {
				unsigned short sf2 = sf[4]&0x7fff;
				if ((sf2==0)||(sf2==0x7fff)||((sf[3]&0x8000)==0)) {
				    ntag |= 2;
				}	/* else tag=00 valid */
			    }
			}
			fptag <<= 2;
		    }
		    TheCPU.fptag = ntag; }
#endif
//
		    if (reg&DATA16) {
			unsigned short *p = LINEAR2UNIX(TheCPU.mem_ref);
			p[0] = TheCPU.fpuc; p[1] = TheCPU.fpus; p[2] = TheCPU.fptag;
			/* IP,OP,opcode: n.i. */
			p[3] = p[4] = p[5] = p[6] = 0; q = (char *)(p+7);
		    }
		    else {
			uint32_t *p = LINEAR2UNIX(TheCPU.mem_ref);
			p[0] = TheCPU.fpuc; p[1] = TheCPU.fpus; p[2] = TheCPU.fptag;
			/* IP,OP,opcode: n.i. */
			p[3] = p[4] = p[5] = p[6] = 0; q = (char *)(p+7);
		    }
		    TheCPU.fpuc |= 0x3f;
		    if (exop==0x35) {
			int i, k;
			k = TheCPU.fpstt;
			for (i=0; i<8; i++) {
			    double *sf = &TheCPU.fpregs[k];
			    __asm__ __volatile__ (" \
				fldl	%1\n \
				fstpt	%0" \
				: "=m"(*q) : "m"(*sf) : "memory");
			    k = (k+1)&7; q += 10;
			}
			FPRSTT = TheCPU.fpregs;
			TheCPU.fpus = 0;
			TheCPU.fpstt = 0;
			TheCPU.fpuc = 0x37f;
			TheCPU.fptag = 0xffff;
			__asm__ __volatile__ ("fninit");
		    }
		   }
		   break;

/*xx*/	default:
fp_notok:
	CodePtr = Cp;
	return -1;
	}
fp_ok:
	CodePtr = Cp;
	return 0;
}

#endif
