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
 ***************************************************************************/

#ifndef _EMU86_CODEGEN_H
#define _EMU86_CODEGEN_H

#include "syncpu.h"
#include "trees.h"

/////////////////////////////////////////////////////////////////////////////

#define L_LITERAL	0

#define LEA_DI_R	1
#define A_DI_0		2
#define A_DI_1		3
#define A_DI_2		4
#define A_DI_2D		5
#define A_SR_SH4	6

#define L_IMM		13
#define L_IMM_R1	14
#define S_DI_R		15
#define L_MOVZS		17
#define L_LXS1		18
#define L_LXS2		19
#define L_ZXAX		20
#define L_CR0		21

#define O_ADD_R		30
#define O_OR_R		31
#define O_ADC_R		32
#define O_SBB_R		33
#define O_AND_R		34
#define O_SUB_R		35
#define O_XOR_R		36
#define O_CMP_R		37
#define O_INC_R		38
#define O_DEC_R		39
#define O_SBB_M		40
#define O_SUB_M		41
#define O_CMP_M		42
#define O_CBWD		43
#define O_XCHG		44
#define O_XCHG_R	45
#define O_INC		46
#define O_DEC		47
#define O_NOT		48
#define O_NEG		49
#define O_MUL		50
#define O_IMUL		51
#define O_DIV		52
#define O_IDIV		53
#define O_ROL		54
#define O_ROR		55
#define O_RCL		56
#define O_RCR		57
#define O_SHL		58
#define O_SHR		59
#define O_SAR		60
#define O_OPAX		61
#define O_XLAT		62
#define O_CJMP		63
#define O_SLAHF		64
#define O_SETFL		65
#define O_BSWAP		66
#define O_SETCC		67
#define O_BITOP		68
#define O_SHFD		69

#define O_PUSH		80
#define O_PUSHI		81
#define O_POP		82
#define O_PUSHA		83
#define O_POPA		84
#define O_PUSH1		85
#define O_PUSH2		86
#define O_PUSH2F	87
#define O_PUSH3		88
#define O_POP1		89
#define O_POP2		90
#define O_POP3		91

#define O_MOVS_SetA	100
#define O_MOVS_MovD	101
#define O_MOVS_SavA	102
#define O_MOVS_LodD	103
#define O_MOVS_StoD	104
#define O_MOVS_ScaD	105
#define O_MOVS_CmpD	106
#define O_RDTSC		107

#define O_INPDX		108
#define O_INPPC		109
#define O_OUTPDX	110
#define O_OUTPPC	111

#define JB_LOCAL	112
#define JCXZ_LOCAL	113
#define JLOOP_LOCAL	114
#define JF_LOCAL	115

/////////////////////////////////////////////////////////////////////////////
//
#define ADDR16	0x00000001
#define ADDR32	0x00000000
#define BitADDR16	0
#define DATA16	0x00000002
#define DATA32	0x00000000
#define BitDATA16	1
#define MBYTE	0x00000004
#define IMMED	0x00000008
#define MCEXEC	0x00000010
#define RSHIFT	0x00000020
#define SEGREG	0x00000040
#define MLEA	0x00000080
#define MREP	0x00000100
#define MREPNE	0x00000200
#define MEMADR	0x00000400
#define NOFLDR	0x00000800
#define XECFND	0x00001000
#define MBYTX	0x00002000
#define MOVSSRC	0x00004000
#define MOVSDST	0x00008000

#define DSPSTK	0x00080000

// as seqflg takes mode>>16, these must go together in pairs
// (bits 0-3 are accumulated in the sequence head node):
#define M_FPOP	0x00010000
#define F_FPOP	0x0001
#define M_BJMP	0x00100000
#define F_BJMP	0x0010
#define M_FJMP	0x00200000
#define F_FJMP	0x0020
#define M_SUSP	0x00400000
#define F_SUSP	0x0040

/////////////////////////////////////////////////////////////////////////////

/* x386 */
#define GetSWord(w)	*((unsigned short *)(w))=*((unsigned short *)(LONG_SS+sp))
#define GetSLong(l)	*((unsigned long *)(l))=*((unsigned long *)(LONG_SS+sp))
#define PutSWord(w)	*((unsigned short *)(LONG_SS+sp))=*((unsigned short *)(w))
#define PutSLong(l)	*((unsigned long *)(LONG_SS+sp))=*((unsigned long *)(l))

static __inline__ void PUSH(int m, void *w)
{
	unsigned long sp = (TheCPU.esp-2) & TheCPU.StackMask;
	if (m&DATA16) PutSWord(w);
	else {
		sp = (sp-2)&TheCPU.StackMask; PutSLong(w);
	}
	TheCPU.esp = (sp&TheCPU.StackMask) | (TheCPU.esp&~TheCPU.StackMask);
}

static __inline__ void POP(int m, void *w)
{
	unsigned long sp = TheCPU.esp & TheCPU.StackMask;
	if (m&DATA16) {
		GetSWord(w); sp+=2;
	}
	else {
		GetSLong(w); sp+=4;
	}
	TheCPU.esp = (sp&TheCPU.StackMask) | (TheCPU.esp&~TheCPU.StackMask);
}

static __inline__ void TOS_WORD(int m, void *w)		// for segments
{
	unsigned long sp = TheCPU.esp & TheCPU.StackMask;
	GetSWord(w);
}

static __inline__ void NOS_WORD(int m, void *w)		// for segments
{
	unsigned long sp = (TheCPU.esp+(m&DATA16? 2:4)) & TheCPU.StackMask;
	GetSWord(w);
}

static __inline__ void POP_ONLY(int m)
{
	unsigned long sp = TheCPU.esp + (m&DATA16? 2:4);
	TheCPU.esp = (sp&TheCPU.StackMask) | (TheCPU.esp&~TheCPU.StackMask);
}


/////////////////////////////////////////////////////////////////////////////

/* Code generation macros for x86 */
#define	G1(b,p)		*(p)++=(unsigned char)(b)
#define	G2(w,p)		{*((unsigned short *)(p))=(w);(p)+=2;}
#define	G2M(c,b,p)	{*((unsigned short *)(p))=((b)<<8)|(c);(p)+=2;}
#define	G3(l,p)		{*((unsigned long *)(p))=(l);(p)+=3;}
#define	G3M(c,b1,b2,p)	{*((unsigned long *)(p))=((b2)<<16)|((b1)<<8)|(c);(p)+=3;}
#define	G4(l,p)		{*((unsigned long *)(p))=(l);(p)+=4;}
#define	G4M(c,b1,b2,b3,p) {*((unsigned long *)(p))=((b3)<<24)|((b2)<<16)|((b1)<<8)|(c);\
				(p)+=4;}
#define	G5(l1,b2,p)	{*((unsigned long *)(p))=(l1);*((p)+4)=(unsigned char)(b2);\
				(p)+=5;}
#define	G6(l1,w2,p)	{*((unsigned long *)(p))=(l1);\
			 *((unsigned short *)((p)+4))=(w2);(p)+=6;}
#define	G7(l1,l2,p)	{*((unsigned long *)(p))=(l1);\
			 *((unsigned long *)((p)+4))=(l2);(p)+=7;}
#define	G8(l1,l2,p)	{*((unsigned long *)(p))=(l1);\
			 *((unsigned long *)((p)+4))=(l2);(p)+=8;}
#define GNX(d,s,l)	{__memcpy((d),(s),(l));(d)+=(l);}

/////////////////////////////////////////////////////////////////////////////
//
void InitGen(void);
IMeta *NewIMeta(unsigned char *newa, int mode, int *rc, void *aux);
void Gen(int op, int mode, ...);
void AddrGen(int op, int mode, ...);
int  Fp87_op(int exop, int reg);
unsigned char *CloseAndExec(unsigned char *newa, int mode);
void EndGen(void);
//
static __inline__ void ResetCodeBuf(void)
{
	CodePtr = PrevCodePtr = CodeBuf;
}

static __inline__ int IsCodeBufEmpty(void)
{
	return (CodePtr == CodeBuf);
}

static __inline__ int IsCodeInBuf(void)
{
	return (CodePtr > CodeBuf);
}

extern TNode *CurrXNode;

extern char InterOps[];

#endif
