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

/////////////////////////////////////////////////////////////////////////////

#define L_LITERAL	0

#define LEA_DI_R	1
#define A_DI_0		2
#define A_DI_1		3
#define A_DI_2		4
#define A_DI_2D		5
#define A_SR_SH4	6

#define L_REG		7
#define L_SI_R1		8
#define L_DI_R1		9
#define S_DI		10
#define S_SI		11
#define S_REG		12
#define L_IMM		13
#define L_IMM_R1	14
#define S_DI_R		15
#define L_XREG1		16
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
#define O_MOVS_SetA	85
#define O_MOVS_MovD	86
#define O_MOVS_SavA	87
#define O_MOVS_LodD	88
#define O_MOVS_StoD	89
#define O_MOVS_ScaD	90
#define O_MOVS_CmpD	91
#define O_RDTSC		92

#define O_PUSH1		93
#define O_PUSH2		94
#define O_PUSH3		95
#define O_POP1		96
#define O_POP2		97
#define O_POP3		98

#ifdef OPTIMIZE_BACK_JUMPS
#define JB_LOCAL	100
#define JCXZ_LOCAL	101
#define JLOOP_LOCAL	102
#endif
#ifdef OPTIMIZE_FW_JUMPS
#define JF_LOCAL	103
#endif

/////////////////////////////////////////////////////////////////////////////
//
#define ADDR16	0x00000001
#define BitADDR16	0
#define DATA16	0x00000002
#define BitDATA16	1
#define MBYTE	0x00000004
#define IMMED	0x00000008
#define STACK16	0x00000010
#define RSHIFT	0x00000020
#define SEGREG	0x00000040
#define MLEA	0x00000080
#define MREP	0x00000100
#define MREPNE	0x00000200
#define MEMADR	0x00000400
#define NOFLDR	0x00000800
#define XECFND	0x00001000
#define MBYTX	0x00002000

#define DSPSTK	0x00080000

// as seqflg takes mode>>16, these must go together in pairs
// (bits 0-3 are accumulated in the sequence head node):
#define M_FPOP	0x00010000
#define F_FPOP	0x0001
#define M_BJMP	0x00100000
#define F_BJMP	0x0010
#define M_FJMP	0x00200000
#define F_FJMP	0x0020

/////////////////////////////////////////////////////////////////////////////

/* x386 */
#define GetSWord(w)	*((unsigned short *)(w))=*((unsigned short *)(LONG_SS+sp))
#define GetSLong(l)	*((unsigned long *)(l))=*((unsigned long *)(LONG_SS+sp))
#define PutSWord(w)	*((unsigned short *)(LONG_SS+sp))=*((unsigned short *)(w))
#define PutSLong(l)	*((unsigned long *)(LONG_SS+sp))=*((unsigned long *)(l))

static __inline__ void PUSH(int m, void *w)
{
	if (m&STACK16) {
		unsigned short sp = *((unsigned short *)&TheCPU.esp)-2;
		if (m&DATA16) PutSWord(w);
		else {
			sp-=2; PutSLong(w);
		}
		*((unsigned short *)&TheCPU.esp) = sp;
	}
	else {
		unsigned long sp = TheCPU.esp-2;
		if (m&DATA16) PutSWord(w);
		else {
			sp-=2; PutSLong(w);
		}
		TheCPU.esp = sp;
	}
}

static __inline__ void POP(int m, void *w)
{
	if (m&STACK16) {
		unsigned short sp = *((unsigned short *)&TheCPU.esp);
		if (m&DATA16) {
			GetSWord(w); sp+=2;
		}
		else {
			GetSLong(w); sp+=4;
		}
		*((unsigned short *)&TheCPU.esp) = sp;
	}
	else {
		unsigned long sp = TheCPU.esp;
		if (m&DATA16) {
			GetSWord(w); sp+=2;
		}
		else {
			GetSLong(w); sp+=4;
		}
		TheCPU.esp = sp;
	}
}

static __inline__ void TOS_WORD(int m, void *w)		// for segments
{
	if (m&STACK16) {
		unsigned short sp = *((unsigned short *)&TheCPU.esp);
		GetSWord(w);
	}
	else {
		unsigned long sp = TheCPU.esp;
		GetSWord(w);
	}
}

static __inline__ void NOS_WORD(int m, void *w)		// for segments
{
	if (m&STACK16) {
		unsigned short sp = *((unsigned short *)&TheCPU.esp)+(m&DATA16? 2:4);
		GetSWord(w);
	}
	else {
		unsigned long sp = TheCPU.esp+(m&DATA16? 2:4);
		GetSWord(w);
	}
}

static __inline__ void POP_ONLY(int m)
{
	if (m&STACK16) {
		unsigned short *sp = (unsigned short *)&TheCPU.esp;
		*sp += (m&DATA16? 2:4);
	}
	else {
		TheCPU.esp += (m&DATA16? 2:4);
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// Tree node key definition.
// The 64-bit key is composed of two parts: the most sigificant (a)
// is the (almost)bit-reversal of the program counter, the least
// significant (c) keeps the four code bytes found at that address.
// This way we can identify almost all the cases when the code at
// a given address changes (by overwriting or self-modifying). Being
// limited to the first four bytes means, however, that this is not
// 100% foolproof, especially if the code sequence is quite long.
// Using e.g. the checksum of the full code block would be safer, but
// doesn't look very efficient.
//
typedef	union {
	struct { long c, a; } sk;
    	long long lk;
} GKey;

typedef struct _imeta {
	unsigned char *addr, *npc, *jtgt;
	struct _imeta *fwref;
	unsigned short ncount, len, flags;
} IMeta;

typedef struct _tnode {
	struct _tnode *root, *left, *right, *prev, *next;
	GKey key;
	unsigned char *addr, *npc;
	int jcount;
	unsigned short len, flags;
} TNode;

#define MAXGNODES	512
extern IMeta InstrMeta[];
extern IMeta *LastIMeta;
extern IMeta *ForwIRef;

#define CODEBUFSIZE	16384

extern unsigned char CodeBuf[];
extern unsigned char *CodePtr;
extern unsigned char *PrevCodePtr;
extern unsigned char *MaxCodePtr;
extern unsigned char TailCode[8];

/* Code generation macros for x86 */
#define	G1(b)		*CodePtr++=(unsigned char)(b)
#define	G2(w)		{*((unsigned short *)CodePtr)=(w);CodePtr+=2;}
#define	G2M(c,b)	{*((unsigned short *)CodePtr)=((b)<<8)|(c);CodePtr+=2;}
#define	G3(l)		{*((unsigned long *)CodePtr)=(l);CodePtr+=3;}
#define	G3M(c,b1,b2)	{*((unsigned long *)CodePtr)=((b2)<<16)|((b1)<<8)|(c);CodePtr+=3;}
#define	G4(l)		{*((unsigned long *)CodePtr)=(l);CodePtr+=4;}
#define	G4M(c,b1,b2,b3)	{*((unsigned long *)CodePtr)=((b3)<<24)|((b2)<<16)|((b1)<<8)|(c);\
				CodePtr+=4;}
#define GNX(v)		{memcpy(CodePtr,&(v),sizeof(v));CodePtr+=sizeof(v);}

//
TNode *FindTree(unsigned char *addr);
TNode *Move2ITree(void);
//
void GCPrint(unsigned char *cp, int len);
//
void InitGen(void);
void InitTrees(void);
IMeta *NewIMeta(unsigned char *newa, int mode, int *rc, void *aux);
void Gen(int op, int mode, ...);
void AddrGen(int op, int mode, ...);
int  Fp87_op(int exop, int reg);
//void InvalidateITree(unsigned long lo_a, unsigned long hi_a);
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

extern char InterOps[];

#endif
