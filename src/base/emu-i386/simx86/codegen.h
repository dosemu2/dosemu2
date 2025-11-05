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

#ifndef _EMU86_CODEGEN_H
#define _EMU86_CODEGEN_H

#include <string.h>
#include "syncpu.h"
#include "trees.h"
#include "emudpmi.h"
#include "dos2linux.h"

/////////////////////////////////////////////////////////////////////////////

/* #define LEA_DI_R	0 */
#define A_DI_0		1
#define A_DI_1		2
#define A_DI_2		3
#define A_DI_2D		4
#define O_XLAT		5
#define O_INT		6
#define O_MOVS_SetA	7
/* The above all generate an address, the below only consume it
   (except for string instructions O_MOVS_* in the JIT only) */

#define A_SR_SH4	8
#define A_SR_PROT	9

#define L_REG		10
#define S_REG		11
#define L_REG2REG	12
#define L_IMM		13
#define L_IMM_R1	14
#define S_DI_IMM	15
#define S_DI_R		16
#define L_MOVZS		17
#define L_LXS1		18
#define L_LXS2		19
#define L_CR0		21
#define L_DI_R1		22
#define S_DI		23
#define L_NOP		24

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
#define O_SBB_FR	40
#define O_SUB_FR	41
#define O_CMP_FR	42
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
#define O_FOP		62
#define O_CJMP		63	// unused
#define O_SLAHF		64
#define O_SETFL		65
#define O_BSWAP		66
#define O_SETCC		67
#define O_BITOP		68
#define O_SHFD		69
#define O_CLEAR		70	// xor r,r
#define O_TEST		71	// and r,r; or r,r
#define O_SBSELF	72	// sbb r,r
#define O_CMPXCHG	73

#define O_ADD_FR	75
#define O_OR_FR		76
#define O_ADC_FR	77
#define O_AND_FR	78
#define O_XOR_FR	79

#define O_POP		82
#define O_PUSH1		85
#define O_PUSH2		86
#define O_PUSH2F	87
#define O_PUSH3		88
#define O_POP1		89
#define O_POP2		90
#define O_POP3		91
#define O_LEAVE		92

#define O_SIM		100

#define O_MOVS_MovD	101
#define O_MOVS_SavA	102
#define O_MOVS_LodD	103
#define O_MOVS_StoD	104
#define O_MOVS_ScaD	105
#define O_MOVS_CmpD	106
#define O_RDTSC		107

#define JMP_TAILCODE	112

#define JMP_INDIRECT	113
#define JMP_LINK	114
#define JB_LINK		115
#define JF_LINK		116
#define JLOOP_LINK	117

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
#define RM_REG	0x00000010
#define NOFLDR	0x00000020
#define SEGREG	0x00000040
#define MLEA	0x00000080
#define MREP	0x00000100
#define MREPNE	0x00000200
#define MEMADR	0x00000400
#define MLOAD	0x00000800
#define MSTORE	0x00001000
#define MBYTX	0x00002000
#define MOVSSRC	0x00004000
#define MOVSDST	0x00008000
#define MPOPRM	0x00010000
#define MRETISP	0x00020000
#define MREALA	0x00040000
#define MBIGCS	0x00080000

#define CKSIGN	0x00100000	// check signal: for jumps
#define MNOREG  0x00200000
#define MPATCH	0x00400000	// patch existing code in JMP_LINK
#define MLINK	0x00800000	// convert to link in JMP_LINK
// for HOST_ARCH_X86
#define MREPCOND 0x01000000	// this is SCASx or CMPSx, REP can be terminated
				// by flags
#define MSSTP	0x02000000	// generate only one instruction
#define MTRAP	0x04000000	// INT01 Sstep active: generate EXCP01_SSTP
#define MINHI	0x08000000	// inhibits IRQs (MOVss/POPss/STI)
#define MOPT	0x10000000	// optimize this op

// values for TNode.flags and IMeta.flags
#define F_FPOP	0x0001	// has at least one FP instruction
#define F_SPEC	0x0002	// start speculative prejit when executing this node
#define F_SLFL	0x0004	// links to itself
#define F_PREJ	0x0010	// generated by any prejit thread
#define F_SPRJ	0x0020	// generated by speculative prejit thread
#define F_LEAV	0x0040	// leave instremu after executing this node
#define F_SLFJ	0x0080	// self-jump (forever loop)
#define F_LJMP	0x0100	// node ends with direct far jmp

/////////////////////////////////////////////////////////////////////////////

// returns 1(16 bit), 0(32 bit)
#define BTA(bpos, mode) (((mode) >> (bpos)) & 1)

// returns 2(16 bit), 4(32 bit)
#define BT24(bpos, mode) (4 - (((mode) << (1-(bpos))) & 2))

static __inline__ int FastLog2(int v)
{
	return fls(v) - 1;
}

/////////////////////////////////////////////////////////////////////////////

void InitGen(void);
int NewIMeta(int npc, int override);
extern int CurrIMeta;
extern void Gen(int op, int mode, ...);
extern void AddrGen(int op, int mode, ...);
extern void (*Fp87_op)(int exop, int reg, unsigned mem_ref);
extern int Fp87_illegal_op(int exop, int reg);
TNode *Close(unsigned int PC, unsigned int Interp_LONGCS, int mode, int flags);
extern unsigned char * (*CodeGen)(unsigned char *CodePtr,
				  unsigned char *BaseGenBuf, const IGen *IG);
extern unsigned (*Exec)(unsigned *mem_ref, unsigned long *flg,
			unsigned char *ecpu, void *SeqStart,
			unsigned short seqflg, unsigned *seqbase);
unsigned int DoExec(TNode *G, unsigned *LastXKey);
void EndGen(void);
extern void fp87_mask_except(void);
extern void fp87_save_except(void);
//
static __inline__ int GoodNode(TNode *G)
{
	if (G->cs != LONG_CS) {
		/* CS mismatch can confuse relative jump/call */
		e_printf("cs mismatch at %08x: old=%x new=%x\n",
					G->key, G->cs, LONG_CS);
		return 0;
	}
	if (G->mode != TheCPU.mode) {
		/* mode mismatch can be 32/16(MBIGCS), MREALA, or MSSTP */
		e_printf("mode mismatch at %08x: old=%x new=%x\n",
					G->key, G->mode, TheCPU.mode);
		return 0;
	}
	return 1;
}

extern unsigned char InterOps[];
extern char RmIsReg[];
extern char OpIsPush[];
extern char OpSize[];
extern char OpSizeBit[];

#define OPSIZE(m) (OpSize[(m)&(DATA16|MBYTE)])
#define OPSIZEBIT(m) (OpSizeBit[(m)&(DATA16|MBYTE)])

#endif
