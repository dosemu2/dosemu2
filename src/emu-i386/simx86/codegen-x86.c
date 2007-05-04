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
 * It translates the intermediate ops (defined in codegen.h) to their
 * final binary form and stores the generated code into a temporary
 * buffer (CodeBuf).
 * These intermediate ops are still being reworked and grow in an
 * incremental way; I hope they will converge to some better defined
 * set as soon as I'll start coding for some other processor.
 *
 * There should be other similar modules, one for each target. So you
 * can have codegen-ppc.c or codegen-emulated.c or whatever else.
 *
 * This module generates x86 code. Hey, wait... x86 from x86?
 * Actually the generated code runs always in 32-bit mode, so in a way
 * the 16-bit V86 mode is "emulated".
 * 
 * All instructions operate on a virtual CPU image in memory ("TheCPU"),
 * and are completely self-contained. They read from TheCPU registers,
 * operate, and store back to the same registers. There's an exception -
 * FLAGS, which are not stored back until the end of a code block.
 * In fact, you will note that there's NO flag handling here, because on
 * x86 we use the real hardware to calculate them, and this speeds up
 * things a lot compared to a full interpreter. Flags will be a nightmare
 * for non-x86 host CPUs.
 *
 * This only applies to the condition code flags though, OF, SF, ZF, AF,
 * PF and CF (0x8d5). All other flags are stored in EFLAGS=TheCPU.eflags,
 * including DF. Normally the real DF is clear for compatibility with C
 * code; it is only temporarily set during string instructions.
 *
 * There is NO optimization for the produced code. It is a very pipeline-
 * unconscious code full of register dependencies and reloadings.
 * Clearly we hope that the 1st level cache of the host CPU works as
 * advertised ;-)
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

/***************************************************************************
 *
 * Registers on enter:
 *	ebx		pointer to SynCPU (must not be changed)
 *	flags		from cpu->eflags
 *
 * Registers used by the 32-bit machine:
 *	eax		scratch, data
 *	ebx		pointer to SynCPU (must not be changed)
 *	ecx		scratch, address/count
 *	edx		scratch, data
 *	esi		scratch, address
 *	edi		memory/register address
 *	esp		not modified
 *	flags		modified
 *
 * Registers on exit:
 *	eax		PC for the next instruction
 *	edx		flags
 *	edi		last memory address
 *
 ***************************************************************************/
//

#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "emu86.h"
#include "dlmalloc.h"

#ifdef HOST_ARCH_X86
#include "codegen-x86.h"

static void Gen_x86(int op, int mode, ...);
static void AddrGen_x86(int op, int mode, ...);
static unsigned char *CloseAndExec_x86(unsigned char *PC, TNode *G, int mode, int ln);

/* Buffer and pointers to store generated code */
unsigned char *CodePtr = NULL;

CodeBuf *GenCodeBuf = NULL;
unsigned char *BaseGenBuf = NULL;
int GenBufSize = 0;

hitimer_u TimeStartExec;

/////////////////////////////////////////////////////////////////////////////

#define	Offs_From_Arg()		(char)(va_arg(ap,int))

/* This code is appended at the end of every instruction sequence. It
 * passes back the IP of the next instruction after the sequence.
 * (the one where we switch back to interpreted code).
 *
 *		movl #return_PC,eax
 *		popl edx (flags)
 *		ret
 */
unsigned char TailCode[TAILSIZE+1] =
	{ 0xb8,0,0,0,0,0x5a,0xc3,0xf4 };

/*
 * This function is only here for looking at the generated binary code
 * with objdump.
 */
#if GCC_VERSION_CODE >= 3003
static void _test_(void) __attribute__((used));
#else
static void _test_(void) __attribute__((unused));
#endif

static void _test_(void)
{
	__asm__ __volatile__ (" \
		nop \
		" : : : "memory" );
}

/////////////////////////////////////////////////////////////////////////////

/* empirical!! */
static int goodmemref(long m)
{
	if ((m>0) && (m<0x110000)) return 1;
	if ((m>0x40000000) && (m<mMaxMem)) return 1;
	return 0;
}

/////////////////////////////////////////////////////////////////////////////


void InitGen_x86(void)
{
	Gen = Gen_x86;
	AddrGen = AddrGen_x86;
	CloseAndExec = CloseAndExec_x86;
	UseLinker = USE_LINKER;
	GenCodeBuf = NULL;
	BaseGenBuf = NULL;
	GenBufSize = 0;
	InitTrees();
}


/////////////////////////////////////////////////////////////////////////////

/* NOTE: parameters IG->px must be the last argument in a Gn() macro
 * because of the OR operator, which would cause trouble if the parameter
 * is negative */

static void CodeGen(IMeta *I, int j)
{
	/* evil hack, keeping state from MOVS_SavA to MOVS_SetA in
	   a static variable */
	static unsigned char * rep_retry_ptr = (char*)0xdeadbeef;
	IGen *IG = &(I->gen[j]);
	register unsigned char *Cp = CodePtr;
	unsigned char * CpTemp;
	int mode = IG->mode;
	int rcod;
#ifdef PROFILE
	hitimer_t t0 = GETTSC();
#endif

	switch(IG->op) {
	case LEA_DI_R:
		if (mode&IMMED)	{
			// leal immed(%%edi),%%edi: 8d 7f xx | bf xxxxxxxx
			GenLeaEDI(IG->p0);
		}
		else {
			// leal offs(%%ebx),%%edi
			G3M(0x8d,0x7b,IG->p0,Cp);
		}
		break;
	case A_DI_0:			// base(32), imm
		// movl $imm,%%edi
		G1(0xbf,Cp); G4(IG->p1,Cp);
		if (!(mode & MLEA)) {
			// addl offs(%%ebx),%%edi (seg reg offset)
			G3M(0x03,0x7b,IG->p0,Cp);
		}
		break;
	case A_DI_1: {			// base(32), {imm}, reg
		int idsp=IG->p1;

		if (mode & ADDR16) {
			// movzwl offs(%%ebx),%%edi
			G4M(0x0f,0xb7,0x7b,IG->p2,Cp);
			if ((mode&IMMED) && (idsp!=0)) {
				// addw $immed,%%di
				G3(0xc78166,Cp); G2(idsp,Cp);
			}
		}
		else {
			// movl offs(%%ebx),%%edi
			G3M(0x8b,0x7b,IG->p2,Cp);
			if ((mode&IMMED) && (idsp!=0)) {
				GenLeaEDI(idsp);
			}
		}
		if (!(mode & MLEA)) {
			// addl offs(%%ebx),%%edi (seg reg offset)
			G3M(0x03,0x7b,IG->p0,Cp);
		} }
		break;
	case A_DI_2: {			// base(32), {imm}, reg, reg, {shift}
		int idsp=IG->p1;

		if (mode & ADDR16) {
			// movzwl offs(%%ebx),%%edi
			G4M(0x0f,0xb7,0x7b,IG->p3,Cp);
			// addw offs(%%ebx),%%di
			G4M(0x66,0x03,0x7b,IG->p2,Cp);
			if ((mode&IMMED) && (idsp!=0)) {
				// addw $immed,%%di
				G3(0xc78166,Cp); G2(idsp,Cp);
			}
		}
		else {
			// movl offs(%%ebx),%%edi
			G3M(0x8b,0x7b,IG->p3,Cp);
			if (mode & RSHIFT) {
			    unsigned char sh = IG->p4;
			    if (sh) {
				// shll $1,%%edi
				if (sh==1) { G2(0xe7d1,Cp); }
				// shll $count,%%edi
				else { G2(0xe7c1,Cp); G1(sh,Cp); }
			    }
			}
			// addl offs(%%ebx),%%edi
			G3M(0x03,0x7b,IG->p2,Cp);
			if ((mode&IMMED) && idsp) {
			    GenLeaEDI(idsp);
			}
		}
		if (!(mode & MLEA)) {
			// addl offs(%%ebx),%%edi (seg reg offset)
			G3M(0x03,0x7b,IG->p0,Cp);
		} }
		break;
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
		int idsp = IG->p0;

		if (!(mode & IMMED)) {
			unsigned char sh = IG->p2;
			// movl offs(%%ebx),%%edi
			G3M(0x8b,0x7b,IG->p1,Cp);
			// shll $count,%%ecx
			if (sh)	{
			    // shll $1,%%edi
			    if (sh==1) { G2(0xe7d1,Cp); }
			    // shll $count,%%edi
			    else { G2(0xe7c1,Cp); G1(sh,Cp); }
			}
			if (idsp) {
			    GenLeaEDI(idsp);
			}
		} else {
			// movl $idsp,%%edi
			G1(0xbf,Cp); G4(idsp,Cp);
		}
		if (!(mode & MLEA)) {
			// addl offs(%%ebx),%%edi (seg reg offset)
			G3M(0x03,0x7b,IG->ovds,Cp);
		} }
		break;
	case A_SR_SH4: {	// real mode make base addr from seg
		// movzwl ofs(%%ebx),%%eax
		G4M(0x0f,0xb7,0x43,IG->p0,Cp);
		// shll $4,%%eax
		G3M(0xc1,0xe0,0x04,Cp);
		// movl %%eax,ofs(%%ebx)
		G3M(0x89,0x43,IG->p1,Cp);
		// addl $0xffff,%eax
		G1(0x05,Cp); G4(0x0000ffff,Cp);
		// movl %%eax,ofs(%%ebx)
		G3M(0x89,0x43,IG->p1+4,Cp);
		}
		break;
	case L_NOP:
		G1(0x90,Cp);
		break;
	// Special case: CR0&0x3f
	case L_CR0:
		// movl Ofs_CR0(%%ebx),%%eax
		G3M(0x8b,0x43,Ofs_CR0,Cp);
		// andl $0x3f,%%eax
		G3(0x3fe083,Cp);
		break;
	case O_FOP:
		if (Fp87_op(IG->p0, IG->p1)) TheCPU.err = -96;
		Cp = CodePtr;
		break;

	case L_REG: {
		if (mode&MBYTE)	{
			// movb offs(%%ebx),%%al
			G3M(0x8a,0x43,IG->p0,Cp);
		}
		else {
			// mov{wl} offs(%%ebx),%%{e}ax
			Gen66(mode,Cp); G3M(0x8b,0x43,IG->p0,Cp);
		} }
		break;
	case S_REG: {
		if (mode&MBYTE)	{
			// movb %%al,offs(%%ebx)
			G3M(0x88,0x43,IG->p0,Cp);
		}
		else {
			// mov{wl} %%{e}ax,offs(%%ebx)
			Gen66(mode,Cp); G3M(0x89,0x43,IG->p0,Cp);
		} }
		break;
	case L_REG2REG: {
		if (mode&MBYTE) {
			G3M(0x8a,0x43,IG->p0,Cp);	// rsrc
			G3M(0x88,0x43,IG->p1,Cp);	// rdest
		} else {
			Gen66(mode,Cp);	G3M(0x8b,0x43,IG->p0,Cp);
			Gen66(mode,Cp);	G3M(0x89,0x43,IG->p1,Cp);
		} }
		break;
	case S_DI_R: {
		// mov{wl} %%{e}di,offs(%%ebx)
		Gen66(mode,Cp);	G3M(0x89,0x7b,IG->p0,Cp);
		}
		break;
	case S_DI_IMM: {
		if (mode&MBYTE) {
			// movb $xx,(%%edi)
			G1(0xb0,Cp); G1(IG->p0,Cp);
			STD_WRITE_B;
		} else {
			// mov{wl} $xx,(%%edi)
			G1(0xb8,Cp); G4(IG->p0,Cp);
			STD_WRITE_WL(mode);
		} }
		break;

	case L_IMM: {
		if (mode&MBYTE)	{
			// movb $immed,offs(%%ebx)
			G3M(0xc6,0x43,IG->p0,Cp); G1(IG->p1,Cp);
		}
		else {
			// mov{wl} $immed,offs(%%ebx)
			Gen66(mode,Cp);
			G3M(0xc7,0x43,IG->p0,Cp); G2_4(mode,IG->p1,Cp);
		} }
		break;
	case L_IMM_R1: {
		if (mode&MBYTE)	{
			// movb $immed,%%al
			G1(0xb0,Cp); G1(IG->p0,Cp);
		}
		else {
			// mov{wl} $immed,%%{e}ax
			Gen66(mode,Cp);	G1(0xb8,Cp); G2_4(mode,IG->p0,Cp);
		} }
		break;
	case L_MOVZS: {
		if (mode & MBYTX) {
			// mov{sz}bw (%%edi),%%ax
			Gen66(mode,Cp);
			G3M(0x0f,(0xb6|IG->p0),0x07,Cp);
		}
		else {
			// mov{sz}wl (%%edi),%%eax
			Gen66(mode,Cp);
			G3M(0x0f,(0xb7|IG->p0),0x07,Cp);
		}
		// mov{wl} %%{e}ax,offs(%%ebx)
		Gen66(mode,Cp); G3M(0x89,0x43,IG->p1,Cp);
		}
		break;

	case L_LXS1: {
		// mov{wl} (%%edi),%%{e}ax
		Gen66(mode,Cp); G2M(0x8b,0x07,Cp);
		// mov{wl} %%{e}ax,offs(%%ebx)
		Gen66(mode,Cp);	G3M(0x89,0x43,IG->p0,Cp);
		// leal {2|4}(%%edi),%%edi
		G2M(0x8d,0x7f,Cp); G1((mode&DATA16? 2:4),Cp);
		}
		break;
	case L_LXS2: {	/* real mode segment base from segment value */
		// movzwl (%%edi),%%eax
		G3M(0x0f,0xb7,0x07,Cp);
		// movw %%ax,ofs(%%ebx)
		G4M(0x66,0x89,0x43,IG->p0,Cp);
		// shll $4,%%eax
		G3M(0xc1,0xe0,0x04,Cp);
		// movl %%eax,ofs(%%ebx)
		G3M(0x89,0x43,IG->p1,Cp);
		// addl $0xffff,%eax
		G1(0x05,Cp); G4(0x0000ffff,Cp);
		// movl %%eax,ofs(%%ebx)
		G3M(0x89,0x43,IG->p1+4,Cp);
		}
		break;
	case L_ZXAX:
		// movzwl %%ax,%%eax
		G3(0xc0b70f,Cp);
		break;

//	case L_DI_R1:
	case L_VGAREAD:
		if (!(TheCPU.mode&RM_REG) && TrapVgaOn && vga.inst_emu &&
			(IG->ovds!=Ofs_XCS) && (IG->ovds!=Ofs_XSS)) {
		    // movl %%edi,%%eax
		    G2M(0x89,0xf8,Cp);
		    // subl vga.bank_base(%%ebx),%%eax
		    G2(0x832b,Cp); G4((char *)&vga.mem.bank_base-CPUOFFS(0),Cp);
		    // cmpl vga.bank_len(%%ebx),%%eax
		    G2(0x833b,Cp); G4((char *)&vga.mem.bank_len-CPUOFFS(0),Cp);
		    // jnb normal_read
		    // pushl mode
	       	    G3(0x6a1073,Cp); G1(mode,Cp);
#ifdef __x86_64__
		    // pop %%rsi; push %%rdi
		    G5(0x8b8d48575e,Cp);
	    	    // lea e_VgaRead(%%rbx),%%rcx; call %%rcx; pop %%rdi
		    G4((char *)e_VgaRead-CPUOFFS(0),Cp); G3(0x5fd1ff,Cp);
		    // must be the same amount of ins bytes as i386!!
#else
		    // pushl %%edi
		    G2(0xb957,Cp);
	    	    // call e_VgaRead
		    G4((long)e_VgaRead,Cp); G2(0xd1ff,Cp);
	    	    // add $8,%%esp; nop
		    G4(0x9008c483,Cp);
#endif
	    	    // jmp (skip normal read)
		    G2M(0xeb,((mode&(DATA16|MBYTE))==DATA16? 3:2),Cp);
		}
		if (mode&MBYTE) {
		    G2(0x078a,Cp);
		}
		else {
		    Gen66(mode,Cp);
		    G2(0x078b,Cp);
		}
		break;
//	case S_DI:
	case L_VGAWRITE:
		if (!(TheCPU.mode&RM_REG) && TrapVgaOn && vga.inst_emu &&
			(IG->ovds!=Ofs_XCS) && (IG->ovds!=Ofs_XSS)) {
		    // movl %%edi,%%ecx
		    G2M(0x89,0xf9,Cp);
		    // subl vga.mem.bank_base(%%ebx),%%ecx
		    G2(0x8b2b,Cp); G4((char *)&vga.mem.bank_base-CPUOFFS(0),Cp);
		    // cmpl vga.mem.bank_len(%%ebx),%%ecx
		    G2(0x8b3b,Cp); G4((char *)&vga.mem.bank_len-CPUOFFS(0),Cp);
		    // jnb normal_write
		    // pushl mode
	       	    G3(0x6a1273,Cp); G1(mode,Cp);
#ifdef __x86_64__
		    // pop %%rdx; mov %%eax, %%esi; push %%rdi
		    G4(0x57c6895a,Cp);
	    	    // lea e_VgaWrite(%%ebx),%%ecx; call %%ecx; pop %%rdi
		    G3(0x8b8d48,Cp);
		    G4((char *)e_VgaWrite-CPUOFFS(0),Cp); G3(0x5fd1ff,Cp);
		    // must be the same amount of ins bytes as i386!!
#else
		    // pushl %%eax
		    // pushl %%edi
		    G3(0xb95750,Cp);
	    	    // call e_VgaWrite
		    G4((long)e_VgaWrite,Cp); G2(0xd1ff,Cp);
	    	    // add $0c,%%esp; nop; nop
		    G4(0x900cc483,Cp); G1(0x90,Cp);
#endif
	    	    // jmp (skip normal write)
		    G2(0x03eb,Cp);
		}
		if (mode&MBYTE) {
		    STD_WRITE_B;
		}
		else {
		    STD_WRITE_WL(mode);
		}
		break;

	case O_ADD_R:				// acc = acc op	reg
		rcod = ADDbtrm; goto arith0;
	case O_OR_R:
		rcod = ORbtrm; goto arith0;
	case O_ADC_R:
		rcod = ADCbtrm; goto arith0;
	case O_SBB_R:
		rcod = SBBbtrm; goto arith0;
	case O_AND_R:
		rcod = ANDbtrm; goto arith0;
	case O_SUB_R:
		rcod = SUBbtrm; goto arith0;
	case O_XOR_R:
		rcod = XORbtrm; goto arith0;
	case O_CMP_R:
		rcod = CMPbtrm; goto arith0;
	case O_INC_R:
		rcod = GRP2brm; goto arith0;
	case O_DEC_R:
		rcod = 0x08fe;
arith0:		{
		G1(POPF,Cp);	// get flags from stack
		if (mode & MBYTE) {
			if (mode & IMMED) {
				// OPb $immed,%%al
				G1(rcod+2,Cp); G1(IG->p0,Cp);
			}
			else {
				// OPb offs(%%ebx),%%al
				G2(0x4300|rcod,Cp); G1(IG->p0,Cp);
			}
		}
		else {
			if (mode & IMMED) {
				// OP{lw} $immed,%%{e}ax
				Gen66(mode,Cp);
				G1(rcod+3,Cp); G2_4(mode,IG->p0,Cp);
			}
			else {
				// OP{wl} offs(%%ebx),%%{e}ax
				Gen66(mode,Cp);
				G2(0x4301|rcod,Cp); G1(IG->p0,Cp);
			}
		}
		G1(PUSHF,Cp);	// flags back on stack
		}
		break;
	case O_CLEAR:
		G3M(POPF,0x31,0xc0,Cp);	// get flags; xorl %%eax,%%eax
		if (mode & MBYTE) {
			// movb %%al,offs(%%ebx)
			G3M(0x88,0x43,IG->p0,Cp);
		}
		else {
			// mov{wl} %%{e}ax,offs(%%ebx)
			Gen66(mode,Cp); G3M(0x89,0x43,IG->p0,Cp);
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;
	case O_TEST:
		G1(POPF,Cp);
		if (mode & MBYTE) {
			// testb $0xff,offs(%%ebx)
			G4M(0xf6,0x43,IG->p0,0xff,Cp);
		}
		else if (mode&DATA16) {
			// testw $0xffff,offs(%%ebx)
			G4M(0x66,0xf7,0x43,IG->p0,Cp); G2(0xffff,Cp);
		}
		else {
			// test $0xffffffff,offs(%%ebx)
			G3M(0xf7,0x43,IG->p0,Cp); G4(0xffffffff,Cp);
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;
	case O_SBSELF:
		// if CY=0 -> reg=0,  flag=xx46
		// if CY=1 -> reg=-1, flag=xx97
		G3M(POPF,0x19,0xc0,Cp);	// get flags; sbbl %%eax,%%eax
		if (mode & MBYTE) {
			// movb %%al,offs(%%ebx)
			G3M(0x88,0x43,IG->p0,Cp);
		}
		else {
			// mov{wl} %%{e}ax,offs(%%ebx)
			Gen66(mode,Cp); G3M(0x89,0x43,IG->p0,Cp);
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;
	case O_SBB_FR:
		rcod = SBBbfrm; /* 0x18 */ goto arith1;
	case O_SUB_FR:
		rcod = SUBbfrm; /* 0x28 */ goto arith1;
	case O_CMP_FR:
		rcod = CMPbfrm; /* 0x38 */
arith1:
		G1(POPF,Cp);	// get flags from stack
		if (mode & MBYTE) {
			if (mode & IMMED) {
				// OPb $immed,Ofs_EAX(%%ebx)
				G4M(0x80,0x43|rcod,Ofs_EAX,IG->p0,Cp);
			}
			else {
				// OPb %%al,offs(%%ebx)
				G2(0x4300|rcod,Cp); G1(IG->p0,Cp);
			}
		}
		else {
			if (mode & IMMED) {
				// OP{wl} $immed,Ofs_EAX(%%ebx)
				Gen66(mode,Cp);
				G3M(0x81,0x43|rcod,Ofs_EAX,Cp);
				G2_4(mode,IG->p0,Cp);
			}
			else {
				// OP{wl} %%eax,offs(%%ebx)
				Gen66(mode,Cp);
				G2(0x4301|rcod,Cp); G1(IG->p0,Cp);
			}
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;
	case O_NOT:
		if (mode & MBYTE) {
			// notb %%al
			G2M(0xf6,0xd0,Cp);
		}
		else {
			// NOT{wl} %%(e)ax
			Gen66(mode,Cp);
			G2M(0xf7,0xd0,Cp);
		}
		break;
	case O_NEG:
		G1(POPF,Cp);	// get flags from stack
		if (mode & MBYTE) {
			// negb %%al
			G2M(0xf6,0xd8,Cp);
		}
		else {
			// neg{wl} (%%edi)
			Gen66(mode,Cp);
			G2M(0xf7,0xd8,Cp);
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;
	case O_INC:
		G1(POPF,Cp);	// get flags from stack
		if (mode & MBYTE) {
			// incb %%al
			G2M(0xfe,0xc0,Cp);
		}
		else if (mode & DATA16) {
			// inc %%ax
#ifdef __x86_64__ // 0x40 is a REX byte, not inc
			G3M(0x66,0xff,0xc0,Cp);
#else
			G2M(0x66,0x40,Cp);
#endif
		}
		else {
			// inc %%eax
#ifdef __x86_64__
			G2M(0xff,0xc0,Cp);
#else
			G1(0x40,Cp);
#endif
		}
		G1(PUSHF,Cp);	// flags back on stack before writing
		break;
	case O_DEC:
		G1(POPF,Cp);	// get flags from stack
		if (mode & MBYTE) {
			// decb %%al
			G2M(0xfe,0xc8,Cp);
		}
		else if (mode & DATA16) {
			// dec %%ax
#ifdef __x86_64__ // 0x48 is a REX byte, not dec
			G3M(0x66,0xff,0xc8,Cp);
#else
			G2M(0x66,0x48,Cp);
#endif
		}
		else {
			// dec %%eax
#ifdef __x86_64__
			G2M(0xff,0xc8,Cp);
#else
			G1(0x48,Cp);
#endif
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;
	case O_XCHG: {
		if (mode & MBYTE) {
			// xchgb offs(%%ebx),%%al
			G3M(0x86,0x43,IG->p0,Cp);
		}
		else {
			// xchg{wl} offs(%%ebx),%%{e}ax
			Gen66(mode,Cp);	G3M(0x87,0x43,IG->p0,Cp);
		} }
		break;
	case O_XCHG_R: {
		// mov{wl} offs1(%%ebx),%%{e}ax
		Gen66(mode,Cp);	G3M(0x8b,0x43,IG->p0,Cp);
		// xchg{wl} offs2(%%ebx),%%{e}ax
		Gen66(mode,Cp); G3M(0x87,0x43,IG->p1,Cp);
		// mov{wl} %%{e}ax,offs1(%%ebx)
		Gen66(mode,Cp);	G3M(0x89,0x43,IG->p0,Cp);
		}
		break;
	case O_MUL:
		G1(POPF,Cp);	// get flags from stack
		if (mode & MBYTE) {
			// mulb Ofs_AL(%%ebx),%%al
			G3M(0xf6,0x63,Ofs_AL,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
		}
		else if (mode&DATA16) {
			// mulw Ofs_AX(%%ebx),%%ax
			G4M(OPERoverride,0xf7,0x63,Ofs_AX,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
			// movw %%dx,Ofs_DX(%%ebx)
			G4M(OPERoverride,0x89,0x53,Ofs_DX,Cp);
		}
		else {
			// mull Ofs_EAX(%%ebx),%%eax
			G3M(0xf7,0x63,Ofs_EAX,Cp);
			// movl %%eax,Ofs_EAX(%%ebx)
			G3M(0x89,0x43,Ofs_EAX,Cp);
			// movl %%edx,Ofs_EDX(%%ebx)
			G3M(0x89,0x53,Ofs_EDX,Cp);
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;
	case O_IMUL:
		G1(POPF,Cp);	// get flags from stack
		if (mode & MBYTE) {
			if ((mode&(IMMED|DATA16))==(IMMED|DATA16)) {
				// imul $immed,%%ax,%%ax
				G3M(OPERoverride,0x6b,0xc0,Cp); G1(IG->p0,Cp);
				// movw %%ax,offs(%%ebx)
				G4M(OPERoverride,0x89,0x43,IG->p1,Cp);
			}
			else if ((mode&(IMMED|DATA16))==IMMED) {
				// imul $immed,%%eax,%%eax
				G2M(0x6b,0xc0,Cp); G1(IG->p0,Cp);
				// movl %%eax,offs(%%ebx)
				G3M(0x89,0x43,IG->p1,Cp);
			}
			else {
				// imul Ofs_AL(%%ebx),%%al
				G3M(0xf6,0x6b,Ofs_AL,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
			}
		}
		else if (mode&DATA16) {
			if (mode&IMMED) {
				// imul $immed,%%ax,%%ax
				G3M(OPERoverride,0x69,0xc0,Cp); G2(IG->p0,Cp);
				// movw %%ax,offs(%%ebx)
				G4M(OPERoverride,0x89,0x43,IG->p1,Cp);
			}
			else if (mode&MEMADR) {
				// imul offs(%%ebx),%%ax
				G4M(OPERoverride,0x0f,0xaf,0x43,Cp);
				G1(IG->p0,Cp);
				// movw %%ax,offs(%%ebx)
				G4M(OPERoverride,0x89,0x43,IG->p0,Cp);
			}
			else {
				// imul Ofs_AX(%%ebx),%%ax
				G4M(OPERoverride,0xf7,0x6b,Ofs_AX,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
				// movw %%dx,Ofs_DX(%%ebx)
				G4M(OPERoverride,0x89,0x53,Ofs_DX,Cp);
			}
		}
		else {
			if (mode&IMMED) {
				// imul $immed,%%eax,%%eax
				G2M(0x69,0xc0,Cp); G4(IG->p0,Cp);
				// movl %%eax,offs(%%ebx)
				G3M(0x89,0x43,IG->p1,Cp);
			}
			else if (mode&MEMADR) {
				// imul offs(%%ebx),%%eax
				G4M(0x0f,0xaf,0x43,IG->p0,Cp);
				// movl %%eax,offs(%%ebx)
				G3M(0x89,0x43,IG->p0,Cp);
			}
			else {
				// imul Ofs_EAX(%%ebx),%%eax
				G3M(0xf7,0x6b,Ofs_EAX,Cp);
				// movl %%eax,Ofs_EAX(%%ebx)
				G3M(0x89,0x43,Ofs_EAX,Cp);
				// movl %%edx,Ofs_EDX(%%ebx)
				G3M(0x89,0x53,Ofs_EDX,Cp);
			}
		}
		G1(PUSHF,Cp);	// flags back on stack
		break;

	case O_DIV: {
		G1(POPF,Cp);	// get flags from stack
		G2(0xc189,Cp);  // movl %%eax,%%ecx
		if (mode & MBYTE) {
			// movw Ofs_AX(%%ebx),%%ax
			G4M(OPERoverride,0x8b,0x43,Ofs_AX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2M(0xc7,0x43,Cp); G1(Ofs_CR2,Cp); G4(IG->p0,Cp);
			// div %%cl,%%al
			G2M(0xf6,0xf1,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
		}
		else if (mode&DATA16) {
			// movw Ofs_AX(%%ebx),%%ax
			G4M(OPERoverride,0x8b,0x43,Ofs_AX,Cp);
			// movw Ofs_DX(%%ebx),%%dx
			G4M(OPERoverride,0x8b,0x53,Ofs_DX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(IG->p0,Cp);
			// div %%cx,%%ax
			G3M(OPERoverride,0xf7,0xf1,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
			// movw %%dx,Ofs_DX(%%ebx)
			G4M(OPERoverride,0x89,0x53,Ofs_DX,Cp);
		}
		else {
			// movl Ofs_EAX(%%ebx),%%eax
			G3M(0x8b,0x43,Ofs_EAX,Cp);
			// movl Ofs_EDX(%%ebx),%%edx
			G3M(0x8b,0x53,Ofs_EDX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(IG->p0,Cp);
			// div %%ecx,%%eax
			G2M(0xf7,0xf1,Cp);
			// movl %%eax,Ofs_EAX(%%ebx)
			G3M(0x89,0x43,Ofs_EAX,Cp);
			// movl %%edx,Ofs_EDX(%%ebx)
			G3M(0x89,0x53,Ofs_EDX,Cp);
		}
		G1(PUSHF,Cp);	// flags back on stack
		}
		break;
	case O_IDIV: {
		G1(POPF,Cp);	// get flags from stack
		G2(0xc189,Cp);  // movw %%eax,%%ecx
		if (mode & MBYTE) {
			// movw Ofs_AX(%%ebx),%%ax
			G4M(OPERoverride,0x8b,0x43,Ofs_AX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(IG->p0,Cp);
			// idiv %%cl,%%al
			G2M(0xf6,0xf9,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
		}
		else if (mode&DATA16) {
			// movw Ofs_AX(%%ebx),%%ax
			G4M(OPERoverride,0x8b,0x43,Ofs_AX,Cp);
			// movw Ofs_DX(%%ebx),%%dx
			G4M(OPERoverride,0x8b,0x53,Ofs_DX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(IG->p0,Cp);
			// idiv %%cx,%%ax
			G3M(OPERoverride,0xf7,0xf9,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G4M(OPERoverride,0x89,0x43,Ofs_AX,Cp);
			// movw %%dx,Ofs_DX(%%ebx)
			G4M(OPERoverride,0x89,0x53,Ofs_DX,Cp);
		}
		else {
			// movl Ofs_EAX(%%ebx),%%eax
			G3M(0x8b,0x43,Ofs_EAX,Cp);
			// movl Ofs_EDX(%%ebx),%%edx
			G3M(0x8b,0x53,Ofs_EDX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(IG->p0,Cp);
			// idiv %%ecx,%%eax
			G2M(0xf7,0xf9,Cp);
			// movl %%eax,Ofs_EAX(%%ebx)
			G3M(0x89,0x43,Ofs_EAX,Cp);
			// movl %%edx,Ofs_EDX(%%ebx)
			G3M(0x89,0x53,Ofs_EDX,Cp);
		}
		G1(PUSHF,Cp);	// flags back on stack
		}
		break;

	case O_CBWD:
		// movl Ofs_EAX(%%ebx),%%eax
		G3M(0x8b,0x43,Ofs_EAX,Cp);
		if (mode & MBYTE) {		/* 0x98: CBW,CWDE */
			if (mode & DATA16) {	// AL->AX
				// cbw
				G2(0x9866,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G4M(0x66,0x89,0x43,Ofs_AX,Cp);
			}
			else {			// AX->EAX
				// cwde
				// movl %%eax,Ofs_EAX(%%ebx)
				G4M(0x98,0x89,0x43,Ofs_EAX,Cp);
			}
		}
		else if	(mode &	DATA16)	{	/* 0x99: AX->DX:AX */
			// cwd
			G2(0x9966,Cp);
			// movw %%dx,Ofs_DX(%%ebx)
			G4M(0x66,0x89,0x53,Ofs_DX,Cp);
		}
		else {	/* 0x99: EAX->EDX:EAX */
			// cdq
			// movl %%edx,Ofs_EDX(%%ebx)
			G4M(0x99,0x89,0x53,Ofs_EDX,Cp);
		}
		break;
	case O_XLAT:
		// movl OVERR_DS(%%ebx),%%edi
		G2(0x7b8b,Cp); G1(IG->ovds,Cp);
		if (mode & DATA16) {
			// movzwl Ofs_BX(%%ebx),%%ecx
			G4M(0x0f,0xb7,0x4b,Ofs_BX,Cp);
		}
		else {
			// movl Ofs_EBX(%%ebx),%%ecx
			G3M(0x8b,0x4b,Ofs_EBX,Cp);
		}
		// leal (%%ecx,%%edi,1),%%edi
		G3(0x393c8d,Cp);
		// movzbl Ofs_AL(%%ebx),%%ecx
		G4M(0x0f,0xb6,0x4b,Ofs_AL,Cp);
		// leal (%%ecx,%%edi,1),%%edi
		G4(0x8a393c8d,Cp);
		// movb (%%edi),%%al
		// movb %%al,Ofs_AL(%%ebx)
		G4M(0x07,0x88,0x43,Ofs_AL,Cp);
		break;

	case O_ROL:
		rcod = 0x00; goto shrot0;
	case O_ROR:
		rcod = 0x08; goto shrot0;
	case O_RCL:
		rcod = 0x10; goto shrot0;
	case O_RCR:
		rcod = 0x18; goto shrot0;
	case O_SHL:
		rcod = 0x20; goto shrot0;
	case O_SHR:
		rcod = 0x28; goto shrot0;
	case O_SAR:
		rcod = 0x38;
shrot0:
		G1(0x9d,Cp);	// get flags from stack
		if (mode & MBYTE) {
			// op [edi],1:	d0 07+r
			// op [edi],n:	c0 07+r	n
			// op [edi],cl:	d2 07+r
			if (mode & IMMED) {
				unsigned char sh = IG->p0;
				G1(sh==1? 0xd0:0xc0,Cp);
				G1(0x07	| rcod,Cp);
				if (sh!=1) G1(sh,Cp);
			}
			else {
				// movb Ofs_CL(%%ebx),%%cl
				G3M(0x8a,0x4b,Ofs_CL,Cp);
				// OPb %%cl,(%%edi)
				G1(0xd2,Cp); G1(0x07 | rcod,Cp);
			}
		}
		else {
			// op [edi],1:	(66) d1	07+r
			// op [edi],n:	(66) c1	07+r n
			// op [edi],cl:	(66) d3	07+r
			if (mode & IMMED) {
				unsigned char sh = IG->p0;
				Gen66(mode,Cp);	G1(sh==1? 0xd1:0xc1,Cp);
				G1(0x07	| rcod,Cp);
				if (sh!=1) G1(sh,Cp);
			}
			else {
				// movb Ofs_CL(%%ebx),%%cl
				G3M(0x8a,0x4b,Ofs_CL,Cp);
				// OP{wl} %%cl,(%%edi)
				Gen66(mode,Cp);
				G1(0xd3,Cp); G1(0x07 | rcod,Cp);
			}
		}
		G1(0x9c,Cp);	// flags back on stack
		break;

	case O_OPAX: {	/* used by DAA,DAS,AAA,AAS,AAM,AAD */
			// movl Ofs_EAX(%%ebx),%%eax
			G4M(0x9d,0x8b,0x43,Ofs_EAX,Cp);
#ifdef __x86_64__ /* have to emulate all of them... */
			switch(IG->p1) {
			case DAA:
			case DAS: {
				int op = (IG->p1 == DAS ? 0x28 : 0);
				const static char pseq[] = {
				// pushf; mov %al,%cl; add $0x66,%al
				0x9c,0x88,0xc1,0x04,0x66,
				// pushf; pop %rax; pop %rdx
				0x9c,0x58,0x5a,
				// or %dl,%al; and $0x11,%al // combine AF/CF
				0x08,0xd0,0x24,0x11,
				// mov %al,%dl; rol $4,%al
				0x88,0xc2,0xc0,0xc0,0x04,
				// imul $6,%eax // multiply 0 CF 0 0 0 AF by 6
				0x6b,0xc0,0x06};

				GNX(Cp, pseq, sizeof(pseq));
				// add/sub %al,%cl; pushf // Combine flags from
				G3M(op,0xc1,0x9c,Cp);
				// or %dl,(%rsp) // add/sub with AF/CF
				G3M(0x08,0x14,0x24,Cp);
				// movb %%cl,Ofs_EAX(%%ebx)
				G3M(0x88,0x4b,Ofs_EAX,Cp);
				break;
			}
			case AAA:
			case AAS: {
				int op = (IG->p1 == AAS ? 0x28 : 0);
				const static char pseq[] = {
				// pushf; mov %eax,%ecx; and 0xf,%al
				0x9c,0x89,0xc1,0x24,0x0f,
				// add $6,%al; pop %edx
				0x04,0x06,0x5a,
				// or %dl,%al; and $0xee,%dl // ~(AF|CF)
				0x08,0xd0,0x80,0xe2,0xee,
				// and $0x10,%al; xchg %eax,%ecx; jz 1f
				0x24,0x10,0x91,0x74,0x07};
				GNX(Cp, pseq, sizeof(pseq));

				// add/sub $0x106, %ax
				G4M(0x66,op+0x05,0x06,0x01,Cp);
				// or $0x11,%dl; (AF|CF) 1: and $0xf,%al
				G3M(0x80,0xca,0x11,Cp); G2M(0x24,0x0f,Cp);
				// push %rdx; movl %%eax,Ofs_EAX(%%ebx)
				G4M(0x52,0x89,0x43,Ofs_EAX,Cp);
				break;
			}
			case AAM:
				// mov $0,%ah; mov p2,%cl
				G4M(0xb4,0x00,0xb1,IG->p2,Cp);
				// div %cl; xchg %al,%ah
				G4M(0xf6,0xf1,0x86,0xc4,Cp);
				// orb %al,%al (for flags)
				G2M(0x08,0xc0,Cp);
				// movl %%eax,Ofs_EAX(%%ebx)
				G4M(0x89,0x43,Ofs_EAX,0x9c,Cp);
				break;
			case AAD:
				// mov %al,%cl; mov %ah,%al
				G4M(0x88,0xc1,0x88,0xe0,Cp);
				// mov p2,%ah; mul %ah
				G4M(0xb4,IG->p2,0xf6,0xe4,Cp);
				// add %cl,%al; mov $0,%ah
				G4M(0x00,0xc8,0xb4,0x00,Cp);
				// movl %%eax,Ofs_EAX(%%ebx)
				G4M(0x89,0x43,Ofs_EAX,0x9c,Cp);
				break;
			default:
				error("Unimplemented O_OPAX instruction\n");
				leavedos(99);
			}
#else
			// get n>0,n<3 bytes from parameter stack
			G1(IG->p1,Cp);
			if (IG->p0==2) { G1(IG->p2,Cp); }
			// movl %%eax,Ofs_EAX(%%ebx)
			G4M(0x89,0x43,Ofs_EAX,0x9c,Cp);
#endif
		}
		break;

	case O_PUSH: {
		static char pseq16[] = {
			// movw (%%edi),%%ax
			0x66,0x8b,0x07,0x90,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -2(%%ecx),%%ecx
			0x8d,0x49,0xfe,
			// 16-bit stack seg w/underflow (RM)
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movw %%ax,(%%esi,%%ecx,1)
			0x66,0x89,0x04,0x0e,
			// do 16-bit PM apps exist which use a 32-bit stack seg?
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl (%%edi),%%eax
			0x90,0x8b,0x07,0x90,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -4(%%ecx),%%ecx
			0x8d,0x49,0xfc,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl %%eax,(%%esi,%%ecx,1)
			0x89,0x04,0x0e,
#if 0	/* keep high 16-bits of ESP in small-stack mode */
			// movl StackMask(%%ebx),%%edx
			0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ecx
			0x09,0xd1,
#endif
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		register char *p, *q; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		q=Cp; GNX(Cp, p, sz);
		if (!(mode&MEMADR)) {
			// mov{wl} offs(%%ebx),%%{e}ax
			q[2] = 0x43;
			q[3] = IG->p0;
		}
		} break;

/* PUSH derived (sub-)sequences: */
	case O_PUSH1: {
		static char pseq[] = {
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS
		};
		GNX(Cp, pseq, sizeof(pseq));
		} break;

	case O_PUSH2: {		/* register push only */
		static char pseq16[] = {
			// movl offs(%%ebx),%%eax
/*00*/			0x8b,0x43,0x00,
			// leal -2(%%ecx),%%ecx
			0x8d,0x49,0xfe,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movw %%ax,(%%esi,%%ecx,1)
			0x66,0x89,0x04,0x0e,
		};
		static char pseq32[] = {
			// movl offs(%%ebx),%%eax
/*00*/			0x8b,0x43,0x00,
			// leal -4(%%ecx),%%ecx
			0x8d,0x49,0xfc,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl %%eax,(%%esi,%%ecx,1)
			0x89,0x04,0x0e,
		};
		register char *p, *q; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		q=Cp; GNX(Cp, p, sz);
		q[2] = IG->p0;
		} break;

	case O_PUSH3:
		// movl %%ecx,Ofs_ESP(%%ebx)
		G3M(0x89,0x4b,Ofs_ESP,Cp);
		break;

	case O_PUSH2F: {
		static char pseqpre[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -4(%%ecx),%%ecx
/*08*/			0x8d,0x49,0xfc,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl (%%esp),%%edx	(get flags on stack)
			0x8b,0x14,0x24,
			// movl Ofs_FLAGS(%%ebx),%%eax
			0x8b,0x43,Ofs_EFLAGS,
			// andw EFLAGS_CC,%%dx	(0x8d5: OF/SF/ZF/AF/PF/CF)
			0x66,0x81,0xe2,0xd5,0x08,
			// andw	~EFLAGS_CC,%%ax
			0x66,0x25,0x2a,0xf7,
			// orw %%dx,%%ax
			0x66,0x09,0xd0,
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		char *q=Cp; GNX(Cp, pseqpre, sizeof(pseqpre));
		if (mode&DATA16) q[8] = 0xfe; /* use -2 in lea ins */
		if (in_dpmi) {
		    /* This solves the DOSX 'System test 8' error.
		     * The virtualized IF is pushed instead of the
		     * real one (which is always 1). This way, tests
		     * on the pushed value of the form
		     *		cli
		     *		pushf
		     *		test 0x200,(esp)
		     * don't fail anymore. It is not clear, apart this
		     * special test case, whether pushing the virtual
		     * IF is actually useful; probably not. In any
		     * case, POPF ignores this IF on stack.
		     * Since PUSHF doesn't trap in PM, non-cpuemued
		     * dosemu will always fail this particular test.
		     */
			// rcr $10,%%eax	(IF->cy)
			G3M(0xc1,0xd8,0x0a,Cp);
			// bt $19,(_EFLAGS-TheCPU)(%ebx) (test for VIF)
			G3M(0x0f,0xba,0xa3,Cp);
			/* relative ebx offset works on x86-64 too */
			G4((char *)&_EFLAGS-CPUOFFS(0),Cp);
			// (19 from bt); rcl $10,%%eax
			G4M(0x13,0xc1,0xd0,0x0a,Cp);
		}
		if (mode&DATA16) {
			// movw %%ax,(%%esi,%%ecx,1)
			G4M(0x66,0x89,0x04,0x0e,Cp);
		} else {
			// movl %%eax,(%%esi,%%ecx,1)
			G4M(0x89,0x04,0x0e,NOP,Cp);
		}
		/* nop to make space for a code patch */
		G1(NOP, Cp);
		} break;

	case O_PUSHI: {
		static char pseq16[] = {
			// movw $immed,%%ax
/*00*/			0xb8,0,0,0,0,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -2(%%ecx),%%ecx
			0x8d,0x49,0xfe,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movw %%ax,(%%esi,%%ecx,1)
			0x66,0x89,0x04,0x0e,
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl $immed,%%eax
/*00*/			0xb8,0,0,0,0,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -4(%%ecx),%%ecx
			0x8d,0x49,0xfc,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl %%eax,(%%esi,%%ecx,1)
			0x89,0x04,0x0e,
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		register char *p, *q; int sz;
		if (mode&DATA16) {
			p = pseq16,sz=sizeof(pseq16);
		}
		else {
			p = pseq32,sz=sizeof(pseq32);
		}
		q=Cp; GNX(Cp, p, sz);
		*((int *)(q+1)) = IG->p0;
		} break;

	case O_PUSHA: {
		/* push order: eax ecx edx ebx esp ebp esi edi */
		static char pseq16[] = {	// wrong if SP wraps!
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -16(%%ecx),%%ecx
			0x8d,0x49,0xf0,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// pushl %%esi; leal (%%esi,%%ecx,1),%%esi
			0x56,0x8d,0x34,0x0e,
			// movl Ofs_EDI(%%ebx),%%eax
			// movl Ofs_ESI(%%ebx),%%edx
			0x8b,0x43,Ofs_EDI,0x8b,0x53,Ofs_ESI,
			// movw %%ax,0(%%esi)
			// movw %%dx,2(%%esi)
			0x66,0x89,0x46,0x00,0x66,0x89,0x56,0x02,
			// movl Ofs_EBP(%%ebx),%%eax
			// movl Ofs_ESP(%%ebx),%%edx
			0x8b,0x43,Ofs_EBP,0x8b,0x53,Ofs_ESP,
			// movw %%ax,4(%%esi)
			// movw %%dx,6(%%esi)
			0x66,0x89,0x46,0x04,0x66,0x89,0x56,0x06,
			// movl Ofs_EBX(%%ebx),%%eax
			// movl Ofs_EDX(%%ebx),%%edx
			0x8b,0x43,Ofs_EBX,0x8b,0x53,Ofs_EDX,
			// movw %%ax,8(%%esi)
			// movw %%dx,10(%%esi)
			0x66,0x89,0x46,0x08,0x66,0x89,0x56,0x0a,
			// movl Ofs_ECX(%%ebx),%%eax
			// movl Ofs_EAX(%%ebx),%%edx
			0x8b,0x43,Ofs_ECX,0x8b,0x53,Ofs_EAX,
			// movw %%ax,12(%%esi)
			// movw %%dx,14(%%esi)
			0x66,0x89,0x46,0x0c,0x66,0x89,0x56,0x0e,
			// popl %%esi; movl %%ecx,Ofs_ESP(%%ebx)
			0x5e,0x89,0x4b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%edi
			0x8b,0x7b,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -32(%%ecx),%%ecx
			0x8d,0x49,0xe0,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// leal (%%edi,%%ecx,1),%%edi
			0x8d,0x3c,0x0f,
			// cld; leal Ofs_EDI(%%ebx),%%esi
			0xfc,0x8d,0x73,Ofs_EDI,
			// push %%ecx; mov $8,%%ecx
			0x51,0xb9,8,0,0,0,
			// rep; movsl; pop %%ecx
			0xf3,0xa5,0x59,
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		register char *p; int sz;
		if (mode&DATA16) {
			p = pseq16,sz=sizeof(pseq16);
		}
		else {
			p = pseq32,sz=sizeof(pseq32);
		}
		GNX(Cp, p, sz);
		} break;

	case O_POP: {
		static char pseq16[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movw (%%esi,%%ecx,1),%%ax
			0x66,0x8b,0x04,0x0e,
			// movw %%ax,offs(%%ebx)
/*0d*/			0x66,0x89,0x43,0x00,
			// leal 2(%%ecx),%%ecx
			0x8d,0x49,0x02,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
#endif
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl (%%esi,%%ecx,1),%%eax
			0x90,0x8b,0x04,0x0e,
			// movl %%eax,offs(%%ebx)
/*0d*/			0x90,0x89,0x43,0x00,
			// leal 4(%%ecx),%%ecx
			0x8d,0x49,0x04,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			// movl StackMask(%%ebx),%%edx
			0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ecx
			0x09,0xd1,
#endif
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		register char *p, *q; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		// for popping into memory the sequence is:
		//	first pop, then adjust stack, then
		//	do address calculation and last store data
		q=Cp; GNX(Cp, p, sz);
		if (mode & MEMADR)
			*((int *)(q+0x0d)) = 0x90909090;
		else {
			q[0x10] = IG->p0;
		}
		} break;

/* POP derived (sub-)sequences: */
	case O_POP1: {
		static char pseq[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP
		};
		GNX(Cp, pseq, sizeof(pseq));
		} break;

	case O_POP2: {
		static char pseq16[] = {
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movw (%%esi,%%ecx,1),%%ax
			0x66,0x8b,0x04,0x0e,
			// movw %%ax,offs(%%ebx)
/*07*/			0x66,0x89,0x43,0x00,
			// leal 2(%%ecx),%%ecx
			0x8d,0x49,0x02
		};
		static char pseq32[] = {
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl (%%esi,%%ecx,1),%%eax
			0x90,0x8b,0x04,0x0e,
			// movl %%eax,offs(%%ebx)
/*07*/			0x90,0x89,0x43,0x00,
			// leal 4(%%ecx),%%ecx
			0x8d,0x49,0x04,
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			// movl StackMask(%%ebx),%%edx
			0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ecx
			0x09,0xd1,
#endif
		};
		register char *p, *q; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		// for popping into memory the sequence is:
		//	first pop, then adjust stack, then
		//	do address calculation and last store data
		q=Cp; GNX(Cp, p, sz);
		if (mode & MEMADR)
			*((int *)(q+0x07)) = 0x90909090;
		else {
			q[0x0a] = IG->p0;
		}
		} break;

	case O_POP3:
		// movl %%ecx,Ofs_ESP(%%ebx)
		G3M(0x89,0x4b,Ofs_ESP,Cp);
		break;

	case O_POPA: {
		static char pseq16[] = {	// wrong if SP wraps!
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// pushl %%esi; leal (%%esi,%%ecx,1),%%esi
			0x56,0x8d,0x34,0x0e,
			// movw 0(%%esi),%%ax
			// movw 2(%%esi),%%dx
			0x66,0x8b,0x46,0x00,0x66,0x8b,0x56,0x02,
			// movw %%ax,Ofs_DI(%%ebx)
			// movw %%dx,Ofs_SI(%%ebx)
			0x66,0x89,0x43,Ofs_DI,0x66,0x89,0x53,Ofs_SI,
			// movw 4(%%esi),%%ax
			0x66,0x8b,0x46,0x04,
			// movw %%ax,Ofs_BP(%%ebx)
			0x66,0x89,0x43,Ofs_BP,
			// movw 8(%%esi),%%ax
			// movw 10(%%esi),%%dx
			0x66,0x8b,0x46,0x08,0x66,0x8b,0x56,0x0a,
			// movw %%ax,Ofs_BX(%%ebx)
			// movw %%dx,Ofs_DX(%%ebx)
			0x66,0x89,0x43,Ofs_BX,0x66,0x89,0x53,Ofs_DX,
			// movw 12(%%esi),%%ax
			// movw 14(%%esi),%%dx
			0x66,0x8b,0x46,0x0c,0x66,0x8b,0x56,0x0e,
			// movw %%ax,Ofs_CX(%%ebx)
			// movw %%dx,Ofs_AX(%%ebx)
			0x66,0x89,0x43,Ofs_CX,0x66,0x89,0x53,Ofs_AX,
			// popl %%esi; leal 16(%%ecx),%%ecx
			0x5e,0x8d,0x49,0x10,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
#endif
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// leal (%%esi,%%ecx,1),%%esi
			0x8d,0x34,0x0e,
			// cld; leal Ofs_EDI(%%ebx),%%edi
			0xfc,0x8d,0x7b,Ofs_EDI,
			// here ESP is overwritten, BUT it has been saved
			// locally in %%ebp and will be rewritten later
			// push %%ecx; mov $8,%%ecx
			0x51,0xb9,8,0,0,0,
			// rep; movsl; pop %%ecx
			0xf3,0xa5,0x59,
			// leal 32(%%ecx),%%ecx
			0x8d,0x49,0x20,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			// movl StackMask(%%ebx),%%edx
			0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ecx
			0x09,0xd1,
#endif
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		register char *p; int sz;
		if (mode&DATA16) {
			p = pseq16,sz=sizeof(pseq16);
		}
		else {
			p = pseq32,sz=sizeof(pseq32);
		}
		GNX(Cp, p, sz);
		}
		break;

	case O_LEAVE: {
		static char pseq16[] = {
			// movzwl Ofs_BP(%%ebx),%%ecx
			0x0f,0xb7,0x4b,Ofs_EBP,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movw (%%esi,%%ecx,1),%%ax
			0x66,0x8b,0x04,0x0e,
			// movw %%ax,Ofs_BP(%%ebx)
			0x66,0x89,0x43,Ofs_BP,
			// leal 2(%%ecx),%%ecx
			0x8d,0x49,0x02,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
#endif
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_EBP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_EBP,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl (%%esi,%%ecx,1),%%eax
			0x8b,0x04,0x0e,
			// movl %%eax,Ofs_EBP(%%ebx)
			0x89,0x43,Ofs_EBP,
			// leal 4(%%ecx),%%ecx
			0x8d,0x49,0x04,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			// movl StackMask(%%ebx),%%edx
			0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ecx
			0x09,0xd1,
#endif
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		register char *p; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		GNX(Cp, p, sz);
		} break;

	case O_MOVS_SetA:
		if (mode&ADDR16) {
		    /* The CX load has to be before the address reloads */
		    if (mode&(MREP|MREPNE)) {
			// movzwl Ofs_CX(%%ebx),%%ecx
			G4M(0x0f,0xb7,0x4b,Ofs_CX,Cp);
			rep_retry_ptr = Cp;
		    }
		    if (mode&MOVSSRC) {
			// movzwl Ofs_SI(%%ebx),%%esi
			G4M(0x0f,0xb7,0x73,Ofs_SI,Cp);
		    	if(mode & (MREPNE|MREP))
		    	{
			    /* EAX: iterations possible until address overflow
			            if DF is set. */
			    // movl %%esi,%%eax
			    G2M(MOVwfrm,0xF0,Cp);
			    if(!(mode & MBYTE))
			    {
			    	if(mode & DATA16)
			    	{
				    // shrl $1,%%eax
				    G2M(SHIFTw,0xE8,Cp);
				}
				else
				{
				    // shrl $2,%%eax
				    G3M(SHIFTwi,0xE8,2,Cp);
				}
			    }
			    // incl %%eax
#ifdef __x86_64__ // 0x40 is a REX byte, not inc
			    G2M(0xff,0xc0,Cp);
#else
			    G1(INCax,Cp);
#endif
			}
			// addl OVERR_DS(%%ebx),%%esi
			G3M(0x03,0x73,IG->ovds,Cp);
		    }
		    if (mode&MOVSDST) {
			// movzwl Ofs_DI(%%ebx),%%edi
			G4M(0x0f,0xb7,0x7b,Ofs_DI,Cp);
		    	if(mode & (MREPNE|MREP))
		    	{
			    /* EDX: iterations possible until address overflow
			            if DF is set. */
			    // movl %%edi,%%edx
			    G2M(MOVwfrm,0xFA,Cp);
			    if(!(mode & MBYTE))
			    {
			    	if(mode & DATA16)
			    	{
				    // shrl $1,%%edx
				    G2M(SHIFTw,0xEA,Cp);
				}
				else
				{
				    // shrl $2,%%edx
				    G3M(SHIFTwi,0xEA,2,Cp);
				}
			    }
			    // incl %%edx
#ifdef __x86_64__ // 0x42 is a REX byte, not inc
			    G2M(0xff,0xc2,Cp);
#else
			    G1(INCdx,Cp);
#endif
			}
			// addl Ofs_XES(%%ebx),%%edi
			G3M(0x03,0x7b,Ofs_XES,Cp);
		    }
		    if (mode&(MREP|MREPNE)) {
			/* Address overflow detection */
			// testl $4,Ofs_EFLAGS+1(%%ebx)
			G4M(GRP1brm,0x43,Ofs_EFLAGS+1,0x4,Cp);
			// jnz 0f (distance is 8 bytes per limit to adjust)
			G2M(JNE_JNZ,(mode&(MOVSDST|MOVSSRC)) == (MOVSDST|MOVSSRC) ?
			                0x10:0x08,Cp);
			/* correct for cleared DF */
			if(mode&MOVSSRC)
			{
			    // negl %%eax
			    G2M(GRP1wrm,0xD8,Cp);
			    // addl $(0x10000/opsize+1),%%eax
			    G2M(IMMEDwrm,0xC0,Cp); G4(0x10000/OPSIZE(mode)+1,Cp);
			}
			if(mode&MOVSDST)
			{
			    // negl %%edx
			    G2M(GRP1wrm,0xDA,Cp);
			    // addl $(0x10000/opsize+1),%%edx
			    G2M(IMMEDwrm,0xC2,Cp); G4(0x10000/OPSIZE(mode)+1,Cp);
			}
			// 0:
			/* consolidate limits to edx */
			switch(mode&(MOVSDST|MOVSSRC))
			{
			    case MOVSDST:
				/* nothing to do, limit already in edx */
				break;
			    case MOVSSRC:
			    	/* limit in eax, want it in edx */
				// xchg %%eax,%%edx	
			        G1(XCHGdx,Cp);
			        break;
			    case MOVSSRC | MOVSDST:
				/* smaller limit to edx */
				// cmp %%eax,%%edx
				G2M(CMPwtrm,0xD0,Cp);
				// jb 0f
				G2M(JB_JNAE,0x01,Cp);
				// xchg %%eax,%%edx
				G1(XCHGdx,Cp);
				break;
			}
			// cmp %%ecx,%%edx
			G2M(CMPwfrm,0xCA,Cp);
			// jbe 0f
			G2M(JBE_JNA,0x02,Cp);
			// mov %%ecx,%%edx
			G2M(MOVwfrm,0xCA,Cp);
			// 0: 
			// xchg %%ecx,%%edx
			G2M(XCHGwrm,0xCA,Cp);
			// sub %%ecx, %%edx
			G2M(SUBwfrm,0xCA,Cp);
		    }
		}
		else {
		    if (mode&MOVSSRC) {
			// movl OVERR_DS(%%ebx),%%esi
			G2(0x738b,Cp); G1(IG->ovds,Cp);
			// addl Ofs_ESI(%%ebx),%%esi
			G3M(0x03,0x73,Ofs_ESI,Cp);
		    }
		    if (mode&MOVSDST) {
			// movl Ofs_XES(%%ebx),%%edi
			G3M(0x8b,0x7b,Ofs_XES,Cp);
			// addl Ofs_EDI(%%ebx),%%edi
			G3M(0x03,0x7b,Ofs_EDI,Cp);
		    }
		    if (mode&(MREP|MREPNE)) {
			// movl Ofs_ECX(%%ebx),%%ecx
			G3M(0x8b,0x4b,Ofs_ECX,Cp);
		    }
		}
		break;

	case O_MOVS_MovD:
		GetDF(Cp);
		if (mode&(MREP|MREPNE))	{ G3M(NOP,NOP,REP,Cp); }
		if (mode&MBYTE)	{ G1(MOVSb,Cp); }
		else {
			Gen66(mode,Cp);
			G1(MOVSw,Cp);
		}
		if (!(mode&(MREP|MREPNE))) { G4(0x90909090,Cp); }
		G1(CLD,Cp);
		break;
	case O_MOVS_LodD:
		GetDF(Cp);
		if (mode&(MREP|MREPNE))	{ G1(REP,Cp); }
		if (mode&MBYTE)	{ G1(LODSb,Cp); }
		else {
			Gen66(mode,Cp);
			G1(LODSw,Cp);
		}
		G1(CLD,Cp);
		break;
	case O_MOVS_StoD:
		GetDF(Cp);
		if (mode&(MREP|MREPNE))	{ G3M(NOP,NOP,REP,Cp); }
		if (mode&MBYTE)	{ G1(STOSb,Cp); }
		else {
			Gen66(mode,Cp);
			G1(STOSw,Cp);
		}
		if (!(mode&(MREP|MREPNE))) { G4(0x90909090,Cp); }
		G1(CLD,Cp);
		break;
	case O_MOVS_ScaD:
		CpTemp = NULL;
		if(mode & (MREP|MREPNE))
		{
			G2M(JCXZ,00,Cp);
			// Pointer to the jecxz distance byte
			CpTemp = Cp-1;
		}
		GetDF(Cp);
		if (mode&MREP) { G1(REP,Cp); }
			else if	(mode&MREPNE) {	G1(REPNE,Cp); }
		if (mode&MBYTE)	{ G1(SCASb,Cp); }
		else {
			Gen66(mode,Cp);
			G1(SCASw,Cp);
		}
		G3M(CLD,POPsi,PUSHF,Cp); // replace flags back on stack,esi=dummy
		if(mode & (MREP|MREPNE))
			*CpTemp = (Cp-(CpTemp+1));
		break;
	case O_MOVS_CmpD:
		CpTemp = NULL;
		if(mode & (MREP|MREPNE))
		{
			G2M(JCXZ,00,Cp);
			// Pointer to the jecxz distance byte
			CpTemp = Cp-1;
		}
		GetDF(Cp);
		if (mode&MREP) { G1(REP,Cp); }
			else if	(mode&MREPNE) {	G1(REPNE,Cp); }
		if (mode&MBYTE)	{ G1(CMPSb,Cp); }
		else {
			Gen66(mode,Cp);
			G1(CMPSw,Cp);
		}
		G3M(CLD,POPax,PUSHF,Cp); // replace flags back on stack,eax=dummy
		if(mode & (MREP|MREPNE))
			*CpTemp = (Cp-(CpTemp+1));
		break;

	case O_MOVS_SavA:
		if (mode&ADDR16) {
		    if(mode & MREPCOND)
		    {
			/* it is important to *NOT* destroy the flags here, so
			   use lea instead of add. Flags are needed for termination
			   detection */
			// lea 0(%%edx,%%ecx),%%ecx	; add remaining to cx
			G3M(0x8D,0x0C,0x11,Cp);
			/* terminate immediately if rep was stopped by flags */
			// j[n]z 0f
			G2M((mode&MREP)?JE_JZ:JNE_JNZ,0x02,Cp);
			// xor %%edx,%%edx	; clear remaining
			G2M(XORwtrm,0xD2,Cp);
			// 0:
		    }
		    else if(mode & (MREP|MREPNE))
		    {
			/* use shorter add instruction for nonconditional reps */
			// add %%edx,%%ecx
			G2M(ADDwtrm,0xCA,Cp);
		    }
		    if (mode&MOVSSRC) {
			// esi = base1 + CPU_(e)SI +- n
			// subl OVERR_DS(%%ebx),%%esi
			G2(0x732b,Cp); G1(IG->ovds,Cp);
			// movw %%si,Ofs_SI(%%ebx)
			G4M(0x66,0x89,0x73,Ofs_SI,Cp);
		    }
		    if (mode&MOVSDST) {
			// edi = base2 + CPU_(e)DI +- n
			// subl Ofs_XES(%%ebx),%%edi
			G3M(0x2b,0x7b,Ofs_XES,Cp);
			// movw %%di,Ofs_DI(%%ebx)
			G4M(0x66,0x89,0x7b,Ofs_DI,Cp);
		    }
		    // continue after SI/DI overflow; store ecx
		    if (mode&(MREP|MREPNE)) {
			unsigned char * jmpbackbase;
			// or %%edx,%%edx
			G2M(ORwtrm,0xD2,Cp);
			// jnz retry
			jmpbackbase = Cp;
			G2M(JNE_JNZ,(rep_retry_ptr-jmpbackbase-2)&0xFF,Cp);
			// movw %%cx,Ofs_CX(%%ebx)
			G4M(0x66,0x89,0x4b,Ofs_CX,Cp);
		    }
		}
		else {
		    if (mode&(MREP|MREPNE)) {
			// movl %%ecx,Ofs_ECX(%%ebx)
			G3M(0x89,0x4b,Ofs_ECX,Cp);
		    }
		    if (mode&MOVSSRC) {
			// esi = base1 + CPU_(e)SI +- n
			// subl OVERR_DS(%%ebx),%%esi
			G2(0x732b,Cp); G1(IG->ovds,Cp);
			// movl %%esi,Ofs_ESI(%%ebx)
			G3M(0x89,0x73,Ofs_ESI,Cp);
		    }
		    if (mode&MOVSDST) {
			// edi = base2 + CPU_(e)DI +- n
			// subl Ofs_XES(%%ebx),%%edi
			G3M(0x2b,0x7b,Ofs_XES,Cp);
			// movl %%edi,Ofs_EDI(%%ebx)
			G3M(0x89,0x7b,Ofs_EDI,Cp);
		    }
		}
		break;

	case O_SLAHF:
		rcod = IG->p0;		// 0=LAHF 1=SAHF
		if (rcod==0) {		/* LAHF */
			// movb 0(%%esp),%%al
			G3M(0x8a,0x04,0x24,Cp);
			// movb %%al,Ofs_AH(%%ebx)
			G3M(0x88,0x43,Ofs_AH,Cp);
		}
		else {			/* SAHF */
			// movb Ofs_AH(%%ebx),%%al
			G3M(0x8a,0x43,Ofs_AH,Cp);
			// movb %%al,0(%%esp)
			G3M(0x88,0x04,0x24,Cp);
		}
		break;
	case O_SETFL: {
		unsigned char o1 = IG->p0;
		switch(o1) {	// these are direct on x86
		case CMC:	// xorb $1,0(%%esp)
			G4M(0x80,0x34,0x24,0x01,Cp); break;
		case CLC:	// andb $0xfe,(%%esp)
			G4M(0x80,0x24,0x24,0xfe,Cp); break;
		case STC:	// orb $1,0(%%esp)
			G4M(0x80,0x0c,0x24,0x01,Cp); break;
		case CLD:
			// andb $0xfb,EFLAGS+1(%%ebx)
			G4M(0x80,0x63,Ofs_EFLAGS+1,0xfb,Cp);
			break;
		case STD:
			// orb $4,EFLAGS+1(%%ebx)
			G4M(0x80,0x4b,Ofs_EFLAGS+1,0x04,Cp);
			break;
		} }
		break;
	case O_BSWAP: {
		// movl offs(%%ebx),%%eax
		G3M(0x8b,0x43,IG->p0,Cp);
		// bswap %%eax
		G2M(0x0f,0xc8,Cp);
		// movl %%eax,offs(%%ebx)
		G3M(0x89,0x43,IG->p0,Cp);
		}
		break;
	case O_SETCC: {
		unsigned char n = IG->p0;
		PopPushF(Cp);	// get flags from stack
		// setcc (%%edi)
		G3M(0x0f,(0x90|(n&15)),0x07,Cp);
		}
		break;
	case O_BITOP: {
		unsigned char n = IG->p0;
		G1(0x9d,Cp);	// get flags from stack
		switch (n) {
		case 0x03: /* BT */
		case 0x0b: /* BTS */
		case 0x13: /* BTR */
		case 0x1b: /* BTC */
			// mov{wl} offs(%%ebx),%%{e}ax
			Gen66(mode,Cp);	G3M(0x8b,0x43,IG->p1,Cp);
			// OP{wl} %%{e}ax,(%%edi)
			Gen66(mode,Cp);	G3M(0x0f,(n+0xa0),0x07,Cp);
			break;
		case 0x1c: /* BSF */
		case 0x1d: /* BSR */
			// OP{wl} %%{e}ax,(%%edi)
			Gen66(mode,Cp); G3M(0x0f,(n+0xa0),0x07,Cp);
			// jz 1f
			G2M(0x74,(mode&DATA16)?0x04:0x03,Cp);
			// mov{wl} %%{e}ax,offs(%%ebx) 1:
			Gen66(mode,Cp); G3M(0x89,0x43,IG->p1,Cp);
			break;
		case 0x20: /* BT  imm8 */
		case 0x28: /* BTS imm8 */
		case 0x30: /* BTR imm8 */
		case 0x38: /* BTC imm8 */
			// OP{wl} $immed,(%%edi)
			Gen66(mode,Cp);	G4M(0x0f,0xba,(n|0x07),IG->p1,Cp);
			break;
		}
		G1(0x9c,Cp);	// flags back on stack
		} break;

	case O_SHFD: {
		unsigned char l_r = IG->p0;
		G1(0x9d,Cp);	// get flags from stack
		if (mode & IMMED) {
			unsigned char shc = IG->p2;
			// mov{wl} offs(%%ebx),%%{e}ax
			Gen66(mode,Cp);	G3M(0x8b,0x43,IG->p1,Cp);
			// sh{lr}d $immed,%%{e}ax,(%%edi)
			Gen66(mode,Cp);	G4M(0x0f,(0xa4|l_r),0x07,shc,Cp);
		}
		else {
			// mov{wl} offs(%%ebx),%%{e}ax
			Gen66(mode,Cp);	G3M(0x8b,0x43,IG->p1,Cp);
			// movl Ofs_ECX(%%ebx),%%ecx
			G3M(0x8b,0x4b,Ofs_ECX,Cp);
			// sh{lr}d %%cl,%%{e}ax,(%%edi)
			Gen66(mode,Cp);	G3M(0x0f,(0xa5|l_r),0x07,Cp);
		}
		G1(0x9c,Cp);	// flags back on stack
		} break;

	case O_RDTSC: {
		// rdtsc
		G2(0x310f,Cp);
		// movl	%%eax,%%ecx
		// movl	%%edx,%%edi
		G4(0xd789c189,Cp);
		// subl	TimeStartExec.t.tl(%%ebx),%%eax
		// sbbl	TimeStartExec.t.th(%%ebx),%%edx
		G2(0x832b,Cp); G4((char *)&TimeStartExec.t.tl-CPUOFFS(0),Cp);
		G2(0x931b,Cp); G4((char *)&TimeStartExec.t.th-CPUOFFS(0),Cp);
		// addl	TheCPU.EMUtime(%%ebx),%%eax
		// adcl	TheCPU.EMUtime+4(%%ebx),%%edx
		G3M(0x03,0x43,Ofs_ETIME,Cp);
		G3M(0x13,0x53,Ofs_ETIME+4,Cp);
		// movl	%%ecx,TimeStartExec.t.tl(%%ebx)
		// movl	%%edi,TimeStartExec.t.th(%%ebx)
		G2(0x8b89,Cp); G4((char *)&TimeStartExec.t.tl-CPUOFFS(0),Cp);
		G2(0xbb89,Cp); G4((char *)&TimeStartExec.t.th-CPUOFFS(0),Cp);
		// movl	%%eax,TheCPU.EMUtime(%%ebx)
		// movl	%%edx,TheCPU.EMUtime+4(%%ebx)
		G3M(0x89,0x43,Ofs_ETIME,Cp);
		G3M(0x89,0x53,Ofs_ETIME+4,Cp);
		// movl %%eax,Ofs_EAX(%%ebx)
		// movl %%edx,Ofs_EDX(%%ebx)
		G3M(0x89,0x43,Ofs_EAX,Cp);
		G3M(0x89,0x53,Ofs_EDX,Cp);
		}
		break;

	case O_INPDX:
		// movl Ofs_EDX(%%ebx),%%edx
		G3M(0x8b,0x53,Ofs_EDX,Cp);
		if (mode&MBYTE) {
			// inb (%%dx),%%al; movb %%al,Ofs_AL(%%ebx)
			G4M(0xec,0x88,0x43,Ofs_AL,Cp);
		}
		else {
			// in{wl} (%%dx),%%{e}ax
			Gen66(mode,Cp);	G1(0xed,Cp);
			// mov{wl} %%{e}ax,Ofs_EAX(%%ebx)
			Gen66(mode,Cp);	G3M(0x89,0x43,Ofs_EAX,Cp);
		}
		break;
	case O_OUTPDX:
		// movl Ofs_EDX(%%ebx),%%edx
		G3M(0x8b,0x53,Ofs_EDX,Cp);
		if (mode&MBYTE) {
			// movb Ofs_AL(%%ebx),%%al; outb %%al,(%%dx)
			G4M(0x8a,0x43,Ofs_AL,0xee,Cp);
		}
		else {
			// movl Ofs_EAX(%%ebx),%%eax
			G3M(0x8b,0x43,Ofs_EAX,Cp);
			// out{wl} %%{e}ax,(%%dx)
			Gen66(mode,Cp);	G1(0xef,Cp);
		}
		break;

	case JMP_LINK: {	// cond, dspt, retaddr, link
		static char pseq16[] = {
			// movw $RA,%%ax
/*00*/			0xb8,0,0,0,0,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -2(%%ecx),%%ecx
			0x8d,0x49,0xfe,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movw %%ax,(%%esi,%%ecx,1)
			0x66,0x89,0x04,0x0e,
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl $RA,%%eax
/*00*/			0xb8,0,0,0,0,
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ecx
			0x8b,0x4b,Ofs_ESP,
			// leal -4(%%ecx),%%ecx
			0x8d,0x49,0xfc,
			// andl StackMask(%%ebx),%%ecx
			0x23,0x4b,Ofs_STACKM,
			// movl %%eax,(%%esi,%%ecx,1)
			0x89,0x04,0x0e,
			// movl %%ecx,Ofs_ESP(%%ebx)
			0x89,0x4b,Ofs_ESP
		};
		unsigned char cond = IG->p0;
		int dspt = IG->p1;
		int dspnt = IG->p2;
		linkdesc *lt = IG->lt;
		if (cond == 0x11) {	// call
			register char *p, *q; int sz;
			if (mode&DATA16) {
				p=pseq16,sz=sizeof(pseq16);
			}
			else {
				p=pseq32,sz=sizeof(pseq32);
			}
			q = Cp; GNX(Cp, p, sz);
			*((int *)(q+1)) = dspnt;
			if (debug_level('e')>1) e_printf("CALL: ret=%08x\n",dspnt);
		}
		// t:	b8 [exit_pc] c3
		G1(0xb8,Cp);
		lt->t_type = JMP_LINK;
		/* {n}t_link = offset from codebuf start to immed value */
		lt->t_link.rel = Cp-BaseGenBuf;
		lt->nt_link.abs = 0;
		G4(dspt,Cp); G2(0xc35a,Cp);
		if (debug_level('e')>2) e_printf("JMP_Link %08x:%08x lk=%d:%08x:%08lx\n",
			dspt,dspnt,lt->t_type,lt->t_link.rel,(long)lt->nt_link.abs);
		}
		break;

	case JF_LINK:
	case JB_LINK: {		// cond, PC, dspt, dspnt, link
		unsigned char cond = IG->p0;
		int jpc = IG->p1;
		int dspt = IG->p2;
		int dspnt = IG->p3;
		linkdesc *lt = IG->lt;
		int sz;
		//	JCXZ:	8b 4b Ofs_ECX {66} e3 06
		//	JCC:	7x 06
		// nt:	b8 [nt_pc] c3
		// t:	8b 0d [sig] e3 06
		//	b8 [sig_pc] c3
		//	b8 [t_pc] c3
		sz = TAILSIZE + (mode & CKSIGN? 12:0);
		if (cond==0x31) {
			if (mode&ADDR16) {
			    G4M(0x0f,0xb7,0x4b,Ofs_ECX,Cp);
			}
			else {
			    G3M(0x8b,0x4b,Ofs_ECX,Cp);
			}
			G2M(0xe3,sz,Cp);	// jecxz
		}
		else {
			PopPushF(Cp);	// get flags from stack
			G2M(0x70|cond,sz,Cp);	// normal cond
		}
		if (mode & CKSIGN) {
		    // check signal on NOT TAKEN branch
		    // for backjmp-after-jcc:
		    // decb Ofs_SIGAPEND(%%ebx)
		    G3M(0xfe,0x4b,Ofs_SIGAPEND,Cp);
		    // jne {continue}: exit if sigpend was 1
		    G2M(0x75,TAILSIZE,Cp);
		    // movl {exit_addr},%%eax; pop %%edx; ret
		    G1(0xb8,Cp); G4(jpc,Cp); G2(0xc35a,Cp);
	        }
		lt->t_type = IG->op;
		// not taken: continue with next instr
		G1(0xb8,Cp);
		/* {n}t_link = offset from codebuf start to immed value */
		lt->nt_link.rel = Cp-BaseGenBuf;
		G4(dspnt,Cp); G2(0xc35a,Cp);
		if (IG->op==JB_LINK) {
		    // check signal on TAKEN branch for back jumps
		    G3M(0xfe,0x4b,Ofs_SIGAPEND,Cp);
		    G2M(0x75,TAILSIZE,Cp);
		    G1(0xb8,Cp); G4(jpc,Cp); G2(0xc35a,Cp);
	        }
		// taken
		G1(0xb8,Cp);
		lt->t_link.rel = Cp-BaseGenBuf;
		G4(dspt,Cp); G2(0xc35a,Cp);
		if (debug_level('e')>2) e_printf("J_Link %08x:%08x lk=%d:%08x:%08x\n",
			dspt,dspnt,lt->t_type,lt->t_link.rel,lt->nt_link.rel);
		}
		break;

	case JLOOP_LINK: {	// cond, PC, dspt, dspnt, link
		unsigned char cond = IG->p0;
		int dspt = IG->p1;
		int dspnt = IG->p2;
		linkdesc *lt = IG->lt;
		//	{66} dec Ofs_ECX(ebx)
		//	LOOP:	jnz t
		//	LOOPZ:  jz  nt; test 0x40,dl; jnz t
		//	LOOPNZ: jz  nt; test 0x40,dl; jz  t
		// nt:	b8 [nt_pc] c3
		// t:	8b 0d [sig] jcxz t2
		//	{66} inc Ofs_ECX(ebx)
		//	b8 [sig_pc] c3
		// t2:	b8 [t_pc] c3
		if (mode&ADDR16) {
			G4M(OPERoverride,0xff,0x4b,Ofs_ECX,Cp);
		}
		else {
			G3M(0xff,0x4b,Ofs_ECX,Cp);
		}
		/*
		 * 20 LOOP   taken = (e)cx		nt=cxz
		 * 24 LOOPZ  taken = (e)cx &&  ZF	nt=cxz||!ZF
		 * 25 LOOPNZ taken = (e)cx && !ZF	nt=cxz|| ZF
		 */
		if (cond==0x24) {		// loopz
			G2M(0x74,0x06,Cp);		// jz->nt
			// test flags (on stack)
			G4M(0xf6,0x04,0x24,0x40,Cp);
			G2M(0x75,TAILSIZE,Cp);	// jnz->t
		}
		else if (cond==0x25) {		// loopnz
			G2M(0x74,0x06,Cp);		// jz->nt
			// test flags (on stack)
			G4M(0xf6,0x04,0x24,0x40,Cp);
			G2M(0x74,TAILSIZE,Cp);	// jz->t
		}
		else {
			G2M(0x75,TAILSIZE,Cp);	// jnz->t
		}
		lt->t_type = JLOOP_LINK;
		// not taken: continue with next instr
		G1(0xb8,Cp);
		/* {n}t_link = offset from codebuf start to immed value */
		lt->nt_link.rel = Cp-BaseGenBuf;
		G4(dspnt,Cp); G2(0xc35a,Cp);
		// taken
		G1(0xb8,Cp);
		lt->t_link.rel = Cp-BaseGenBuf;
		G4(dspt,Cp); G2(0xc35a,Cp);
		if (debug_level('e')>2) e_printf("JLOOP_Link %08x:%08x lk=%d:%08x:%08x\n",
			dspt,dspnt,lt->t_type,lt->t_link.rel,lt->nt_link.rel);
		}
		break;

	}
	CodePtr = Cp;
#ifdef PROFILE
	GenTime += (GETTSC() - t0);
#endif
}


/*
 * address generator unit
 * careful - do not use eax, and NEVER change any flag!
 */
static void AddrGen_x86(int op, int mode, ...)
{
	va_list	ap;
	IMeta *I;
	IGen *IG;
#ifdef PROFILE
	hitimer_t t0 = GETTSC();
#endif

	if (CurrIMeta<0) {
		CurrIMeta=0; InstrMeta[0].ngen=0;
		GenCodeBuf=NULL;
		BaseGenBuf=NULL; GenBufSize=0;
	}
	I = &InstrMeta[CurrIMeta];
	IG = &(I->gen[I->ngen]);
	if (debug_level('e')>6) dbug_printf("AGEN: %3d %6x\n",op,mode);

	va_start(ap, mode);
	IG->op = op;
	IG->mode = mode;
	IG->ovds = OVERR_DS;
	GenBufSize += GendBytesPerOp[op];

	switch(op) {
	case LEA_DI_R:
		if (mode&IMMED)
			IG->p0 = va_arg(ap,int);
		else {
			signed char o = Offs_From_Arg();
			IG->p0 = o;
		}
		break;
	case A_DI_0:			// base(32), imm
	case A_DI_1: {			// base(32), {imm}, reg, {shift}
		signed char ofs = (char)va_arg(ap,int);
		signed char o;
		IG->p0 = ofs;
		IG->p1 = 0;
		if (mode&IMMED)	{
			IG->p1 = va_arg(ap,int);
			if (op==A_DI_0)	break;
		}
		o = Offs_From_Arg();
		IG->p2 = o;
		}
		break;
	case A_DI_2: {			// base(32), {imm}, reg, reg, {shift}
		signed char o1,o2;
		signed char ofs = (char)va_arg(ap,int);
		IG->p0 = ofs;
		IG->p1 = 0;
		if (mode&IMMED)	{
			IG->p1 = va_arg(ap,int);
		}
		o1 = Offs_From_Arg();
		o2 = Offs_From_Arg();
		IG->p2 = o1;
		IG->p3 = o2;
		if (!(mode&ADDR16)) {
			if (mode & RSHIFT) {
			    unsigned char sh = (unsigned char)(va_arg(ap,int));
			    IG->p4 = sh;
			}
		} }
		break;
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
		IG->p0 = va_arg(ap,int);
		if (!(mode & IMMED)) {
			signed char o = Offs_From_Arg();
			unsigned char sh;
			IG->p1 = o;
			sh = (unsigned char)(va_arg(ap,int));
			IG->p2 = sh;
		} }
		break;
	case A_SR_SH4: {	// real mode make base addr from seg
		signed char o1 = Offs_From_Arg();
		signed char o2 = Offs_From_Arg();
		IG->p0 = o1;
		IG->p1 = o2;
		}
		break;
	}
	va_end(ap);
	I->ngen += 1;
	if (I->ngen >= NUMGENS) leavedos(0xbac1);
#ifdef PROFILE
	GenTime += (GETTSC() - t0);
#endif
}


static void Gen_x86(int op, int mode, ...)
{
	int rcod=0;
	va_list	ap;
	IMeta *I;
	IGen *IG;
#ifdef PROFILE
	hitimer_t t0 = GETTSC();
#endif

	if (CurrIMeta<0) {
		CurrIMeta=0; InstrMeta[0].ngen=0;
		GenCodeBuf=NULL;
		BaseGenBuf=NULL; GenBufSize = 0;
	}
	I = &InstrMeta[CurrIMeta];
	IG = &(I->gen[I->ngen]);
	if (debug_level('e')>6) dbug_printf("CGEN: %3d %6x\n",op,mode);

	va_start(ap, mode);
	IG->op = op;
	IG->mode = mode;
	IG->ovds = OVERR_DS;
	GenBufSize += GendBytesPerOp[op];

	switch(op) {
	case L_NOP:
	case L_CR0:
	case L_ZXAX:
//	case L_DI_R1:
	case L_VGAREAD:
//	case S_DI:
	case L_VGAWRITE:
	case O_NOT:
	case O_NEG:
	case O_INC:
	case O_DEC:
	case O_MUL:
	case O_CBWD:
	case O_XLAT:
	case O_PUSH1:
	case O_PUSH2F:
	case O_PUSH3:
	case O_PUSHA:
	case O_POP1:
	case O_POP3:
	case O_POPA:
	case O_LEAVE:
	case O_MOVS_SetA:
	case O_MOVS_MovD:
	case O_MOVS_LodD:
	case O_MOVS_StoD:
	case O_MOVS_ScaD:
	case O_MOVS_CmpD:
	case O_MOVS_SavA:
	case O_RDTSC:
	case O_INPDX:
	case O_OUTPDX:
		break;

	case L_REG:
	case S_REG:
	case S_DI_R:
	case L_LXS1:
	case O_XCHG:
	case O_CLEAR:
	case O_TEST:
	case O_SBSELF:
	case O_BSWAP: {
		signed char o = Offs_From_Arg();
		IG->p0 = o;
		}
		break;

	case L_REG2REG:
	case L_LXS2:	/* real mode segment base from segment value */
	case O_XCHG_R: {
		signed char o1 = Offs_From_Arg();
		signed char o2 = Offs_From_Arg();
		IG->p0 = o1;
		IG->p1 = o2;
		}
		break;

	case S_DI_IMM:
	case L_IMM_R1:
	case O_ADD_R:				// acc = acc op	reg
	case O_OR_R:
	case O_ADC_R:
	case O_SBB_R:
	case O_AND_R:
	case O_SUB_R:
	case O_XOR_R:
	case O_CMP_R:
	case O_SBB_FR:
	case O_SUB_FR:
	case O_CMP_FR:
	case O_INC_R:
	case O_DEC_R:
	case O_DIV:
	case O_IDIV:
	case O_PUSHI: {
		int v = va_arg(ap,int);
		IG->p0 = v;
		}
		break;

	case L_IMM: {
		signed char o = Offs_From_Arg();
		int v = va_arg(ap,int);
		IG->p0 = o;
		IG->p1 = v;
		}
		break;

	case L_MOVZS: {
		signed char o;
		rcod = (va_arg(ap,int)&1)<<3;	// 0=z 8=s
		o = Offs_From_Arg();
		IG->p0 = rcod;
		IG->p1 = o;
		}
		break;

	case O_IMUL:
		if (mode&IMMED) {
			int v = va_arg(ap,int);
			signed char o = Offs_From_Arg();
			IG->p0 = v;
			IG->p1 = o;
		}
		if (!(mode&MBYTE)) {
			if (mode&MEMADR) {
				signed char o = Offs_From_Arg();
				IG->p0 = o;
			}
		}
		break;

	case O_ROL:
	case O_ROR:
	case O_RCL:
	case O_RCR:
	case O_SHL:
	case O_SHR:
	case O_SAR:
		if (mode & IMMED) {
			unsigned char sh = (unsigned char)va_arg(ap,int);
			IG->p0 = sh;
		}
		break;

	case O_OPAX: {	/* used by DAA..AAD */
			int n =	va_arg(ap,int);
			IG->p0 = n;
			// get n>0,n<3 bytes from parameter stack
			IG->p1 = va_arg(ap,int);
			if (n==2) IG->p2 = va_arg(ap,int);
		}
		break;

	case O_PUSH:
	case O_PUSH2:
	case O_POP:
	case O_POP2:
		if (!(mode&MEMADR)) {
			signed char o = Offs_From_Arg();
			IG->p0 = o;
		}
		break;

	case O_SLAHF:
		rcod = va_arg(ap,int)&1;	// 0=LAHF 1=SAHF
		IG->p0 = rcod;
		break;

	case O_SETFL:
	case O_SETCC: {
		unsigned char n = (unsigned char)va_arg(ap,int);
		IG->p0 = n;
		}
		break;

	case O_FOP: {
		unsigned char exop = (unsigned char)va_arg(ap,int);
		IG->p0 = exop;
		IG->p1 = va_arg(ap,int);	// reg
		}
		break;

	case O_BITOP: {
		unsigned char n = (unsigned char)va_arg(ap,int);
		signed char o = Offs_From_Arg();
		IG->p0 = n;
		IG->p1 = o;
		} break;

	case O_SHFD: {
		unsigned char l_r = (unsigned char)va_arg(ap,int)&8;
		signed char o = Offs_From_Arg();
		IG->p0 = l_r;
		IG->p1 = o;
		if (mode & IMMED) {
			unsigned char shc = (unsigned char)va_arg(ap,int)&0x1f;
			IG->p2 = shc;
		}
		} break;

	case JMP_LINK:		// cond, dspt, retaddr, link
	case JLOOP_LINK: {
		unsigned char cond = (unsigned char)va_arg(ap,int);
		IG->p0 = cond;
		IG->p1 = va_arg(ap,int);	// dspt
		IG->p2 = va_arg(ap,int);	// dspnt
		IG->lt = va_arg(ap,linkdesc *);	// lt
		}
		break;

	case JF_LINK:
	case JB_LINK: {		// cond, PC, dspt, dspnt, link
		unsigned char cond = (unsigned char)va_arg(ap,int);
		IG->p0 = cond;
		IG->p1 = va_arg(ap,int);	// jpc
		IG->p2 = va_arg(ap,int);	// dspt
		IG->p3 = va_arg(ap,int);	// dspnt
		IG->lt = va_arg(ap,linkdesc *);	// lt
		}
		break;

	}

	va_end(ap);
	I->ngen += 1;
	if (I->ngen >= NUMGENS) leavedos(0xbac2);
#ifdef PROFILE
	GenTime += (GETTSC() - t0);
#endif
}


/////////////////////////////////////////////////////////////////////////////


static void ProduceCode(unsigned char *PC)
{
	int i,j,nap,mall_req;
	unsigned char *adr_lo=0, *adr_hi=0, *cp1;
	IMeta *I0 = &InstrMeta[0];

	if (debug_level('e')>1) {
	    e_printf("---------------------------------------------\n");
	    e_printf("ProduceCode: CurrIMeta=%d\n",CurrIMeta);
	}
	if (CurrIMeta < 0) leavedos(0xbac3);

	/* reserve space for auto-ptr and info structures */
	nap = I0->ncount+1;

	/* allocate the actual code buffer here; size is a worst-case
	 * estimate based on measured bytes per opcode.
	 *
	 * Code buffer layout:
	 *	0000	(GenCodeBuf) pointed from {TNode}.mblock
	 *		contains a back pointer to the TNode
	 * 0008/0004	self-pointer (address of this location)
	 * 0010/0008	Addr2Pc table (nap) pointed from {TNode}.pmeta
	 *	nap+10/8 actual code produced (BaseGenBuf)
	 *		plus tail code
	 * Only the code part is filled here.
	 * GenBufSize contain a first guess of the amount of space required
	 *
	 */
	mall_req = GenBufSize + offsetof(CodeBuf,meta[nap]) + 32;// 32 for tail
	GenCodeBuf = dlmalloc(mall_req);
	/* actual code buffer starts from here */
	BaseGenBuf = CodePtr = (unsigned char *)&GenCodeBuf->meta[nap];
	I0->addr = BaseGenBuf;
	if (debug_level('e')>1)
	    e_printf("CodeBuf=%p siz %d CodePtr=%p\n",GenCodeBuf,GenBufSize,CodePtr);

	for (i=0; i<CurrIMeta; i++) {
	    IMeta *I = &InstrMeta[i];
	    if (i==0) {
		adr_lo = adr_hi = I->npc;
	    }
	    else {
		if (I->npc < adr_lo) adr_lo = I->npc;
		    else if (I->npc > adr_hi) adr_hi = I->npc;
	    }
	    I->addr = cp1 = CodePtr;
	    for (j=0; j<I->ngen; j++) {
		CodeGen(I, j);
		if (debug_level('e')>1) {
		    IGen *IG = &(I->gen[j]);
		    int dg = CodePtr-cp1;
		    e_printf("PGEN(%02d,%02d) %3d %6x %2d %08x %08x %08x %08x %08x\n",
			i,j,IG->op,IG->mode,dg,
			IG->p0,IG->p1,IG->p2,IG->p3,IG->p4);
		    cp1 = CodePtr;
		    if (dg > GendBytesPerOp[IG->op]) {
/**/			dbug_printf("Gend[%d] = %d\n",IG->op,dg);
			GendBytesPerOp[IG->op] = dg;
		    }
		}
	    }
	    I->len = CodePtr - I->addr;
	    if (debug_level('e')>3) GCPrint(I->addr, BaseGenBuf, I->len);
	}
	if (debug_level('e')>1)
	    e_printf("Size=%td guess=%d\n",(CodePtr-BaseGenBuf),GenBufSize);
/**/ if ((CodePtr-BaseGenBuf) > GenBufSize) leavedos(0x535347);
	if (PC < adr_lo) adr_lo = PC;
	    else if (PC > adr_hi) adr_hi = PC;
	InstrMeta[0].seqbase = adr_lo;
	InstrMeta[0].seqlen  = adr_hi - adr_lo;

	if (debug_level('e')>1)
	    e_printf("---------------------------------------------\n");
}


/////////////////////////////////////////////////////////////////////////////
/*
 * The node linker.
 *
 * A code sequence can have one of two termination types:
 *
 *	1) straight end (no jump), or unconditional jump or call
 *
 *	key:	|
 *		|
 *		|
 *		mov $next_addr,eax
 *		ret
 *
 *	2) conditional jump or loop
 *
 *	key:	|
 *		|
 *		|
 *		jcond taken
 *		mov $not_taken_addr,eax
 *		ret
 *	taken:  mov $taken_addr,eax
 *		ret
 *
 * In the first case, there's only one linking point; in the second, two.
 * Linking means replacing the "mov addr,eax" instruction with a direct
 * jump to the start point of the next code fragment.
 * The parameters used are (t_ means taken, nt_ means not taken):
 *	t_ref,nt_ref	pointers to next node
 *	t_link,nt_link	addresses of the patch point
 *	t_undo,nt_undo	saves the previous data for unlinking
 * Since a node can be referred from many others, we need to keep
 * "back-references" in a list in order to unlink it.
 */
static void _nodelinker2(TNode *LG, TNode *G)
{
	int *lp;
	linkdesc *T = &G->clink;
	backref *B;

	if (debug_level('e')>8) e_printf("nodelinker2: %08x->%08x\n",LG->key,G->key);

	if (LG && (LG->alive>0)) {
	    int ra;
	    linkdesc *L = &LG->clink;
	    if (L->t_type) {	// node ends with links
		lp = L->t_link.abs;		// check 'taken' branch
		if (*lp==G->key && ((unsigned char*)lp)[-1] == 0xb8) {		// points to current node?
		    if (L->t_ref!=0) {
			dbug_printf("Linker: t_ref at %08x busy\n",LG->key);
			leavedos(0x8102);
		    }
		    L->t_undo = *lp;
		    // b8 [npc] -> e9/eb reladr
		    ra = G->addr - (unsigned char *)L->t_link.abs;
		    if ((ra > -127) && (ra < 128)) {
			ra -= 1; ((char *)lp)[-1] = 0xeb;
		    }
		    else {
			ra -= 4; ((char *)lp)[-1] = 0xe9;
		    }
		    *lp = ra;
		    L->t_ref = &G->mblock->bkptr;
		    B = calloc(1,sizeof(backref));
		    // head insertion
		    B->next = T->bkr.next;
		    T->bkr.next = B;
		    B->ref = &LG->mblock->bkptr;
		    B->branch = 'T';
		    T->nrefs++;
		    if (G==LG) {
			G->flags |= F_SLFL;
			if (debug_level('e')>1) {
			    e_printf("Linker: node (%08lx:%08x:%08lx) SELF link\n"
				"\t\tjmp %08x, undo=%08x, t_ref %d=%08lx->%08lx\n",
				(long)G,G->key,(long)G->addr,
				ra, L->t_undo, T->nrefs, (long)L->t_ref, (long)*L->t_ref);
			}
		    }
		    else if (debug_level('e')>1) {
			e_printf("Linker: previous node (%08lx:%08x:%08lx)\n"
			    "\t\tlinked to (%08lx:%08x:%08lx)\n"
			    "\t\tjmp %08x, undo=%08x, t_ref %d=%08lx->%08lx\n",
			    (long)LG,LG->key,(long)LG->addr,
			    (long)G,G->key,(long)G->addr,
			    ra, L->t_undo, T->nrefs, (long)L->t_ref, (long)*L->t_ref);
		    }
		    if (debug_level('e')>8) { backref *bk = T->bkr.next;
#ifdef DEBUG_LINKER
			if (bk==NULL) { dbug_printf("bkr null\n"); leavedos(0x8108); }
#endif
			while (bk) { dbug_printf("bkref=%c%08lx->%08lx\n",bk->branch,
			(long)bk->ref,(long)*bk->ref); bk=bk->next; }
		    }
		}
		if (L->t_type>JMP_LINK) {	// if it has a 'not taken' link
		    lp = L->nt_link.abs;	// check 'not taken' branch
		    if (*lp==G->key && ((unsigned char*)lp)[-1] == 0xb8) {		// points to current node?
			if (L->nt_ref!=0) {
			    dbug_printf("Linker: nt_ref at %08x busy\n",LG->key);
			    leavedos(0x8103);
			}
			L->nt_undo = *lp;
			// b8 [npc] -> e9/eb reladr
			ra = G->addr - (unsigned char *)L->nt_link.abs;
			if ((ra > -127) && (ra < 128)) {
			    ra -= 1; ((char *)lp)[-1] = 0xeb;
			}
			else {
			    ra -= 4; ((char *)lp)[-1] = 0xe9;
			}
			*lp = ra;
			L->nt_ref = &G->mblock->bkptr;
			B = calloc(1,sizeof(backref));
			// head insertion
			B->next = T->bkr.next;
			T->bkr.next = B;
			B->ref = &LG->mblock->bkptr;
			B->branch = 'N';
			T->nrefs++;
			if (G==LG) {
			    G->flags |= F_SLFL;
			    if (debug_level('e')>1) {
				e_printf("Linker: node (%08lx:%08x:%08lx) SELF link\n"
				"\t\tjmp %08x, undo=%08x, nt_ref %d=%08lx->%08lx\n",
				(long)G,G->key,(long)G->addr,
				ra, L->nt_undo, T->nrefs, (long)L->nt_ref, (long)*L->nt_ref);
			    }
			}
			else if (debug_level('e')>1) {
			    e_printf("Linker: previous node (%08lx:%08x:%08lx)\n"
				"\t\tlinked to (%08lx:%08x:%08lx)\n"
				"\t\tjmp %08x, undo=%08x, nt_ref %d=%08lx->%08lx\n",
				(long)LG,LG->key,(long)LG->addr,
				(long)G,G->key,(long)G->addr,
				ra, L->nt_undo, T->nrefs, (long)L->nt_ref, (long)*L->nt_ref);
			}
			if (debug_level('e')>8) { backref *bk = T->bkr.next;
#ifdef DEBUG_LINKER
			    if (bk==NULL) { dbug_printf("bkr null\n"); leavedos(0x8109); }
#endif
				while (bk) { dbug_printf("bkref=%c%08lx->%08lx\n",bk->branch,
				(long)bk->ref,(long)*bk->ref); bk=bk->next; }
			}
		    }
		}
	    }
	}
}

static void NodeLinker(TNode *G)
{
#ifdef PROFILE
	hitimer_t t0;
#endif

#if !defined(SINGLESTEP)&&!defined(SINGLEBLOCK)
	if (!UseLinker)
#endif
	    return;
#ifdef PROFILE
	t0 = GETTSC();
#endif
	/* check links FROM LastXNode TO current node */
	if (G != LastXNode) _nodelinker2(LastXNode, G);

	/* check links INSIDE current node */
	_nodelinker2(G, G);

#ifdef PROFILE
	LinkTime += (GETTSC() - t0);
#endif
}


void NodeUnlinker(TNode *G)
{
	int *lp;
	linkdesc *T = &G->clink;
	backref *B = T->bkr.next;
#ifdef PROFILE
	hitimer_t t0;
#endif

#if !defined(SINGLESTEP)&&!defined(SINGLEBLOCK)
	if (!UseLinker)
#endif
	    return;
#ifdef PROFILE
	t0 = GETTSC();
#endif
	// unlink backward references (from other nodes to the current
	// node)
	if (debug_level('e')>8)
	    e_printf("Unlinker: bkr.next=%08lx\n",(long)B);
	while (B) {
	    backref *b2 = B;
	    if (B->branch=='T') {
		TNode *H = *B->ref;
		linkdesc *L = &H->clink;
		if (debug_level('e')>2) e_printf("Unlinking T ref from node %08lx(%08x) to %08x\n",
			(long)H, L->t_undo, G->key);
		if (L->t_undo != G->key) {
		    dbug_printf("Unlinker: BK ref error u=%08x k=%08x\n",
			L->t_undo, G->key);
		    leavedos(0x8110);
		}
		lp = L->t_link.abs;
		((char *)lp)[-1] = 0xb8;
		*lp = L->t_undo;
		L->t_ref = NULL; L->t_undo = 0;
		T->nrefs--;
	    }
	    else if (B->branch=='N') {
		TNode *H = *B->ref;
		linkdesc *L = &H->clink;
		if (debug_level('e')>2) e_printf("Unlinking N ref from node %08lx(%08x) to %08x\n",
			(long)H, L->nt_undo, G->key);
		if (L->nt_undo != G->key) {
		    dbug_printf("Unlinker: BK ref error u=%08x k=%08x\n",
			L->nt_undo, G->key);
		    leavedos(0x8110);
		}
		lp = L->nt_link.abs;
		((char *)lp)[-1] = 0xb8;
		*lp = L->nt_undo;
		L->nt_ref = NULL; L->nt_undo = 0;
		T->nrefs--;
	    }
	    else {
		e_printf("Invalid unlink [%c] ref %08lx from node ?(?) to %08x\n",
			B->branch, (long)B->ref, G->key);
		leavedos(0x8116);
	    }
	    B = B->next;
	    free(b2);
	}

	if (G==LastXNode) LastXNode=NULL;
	if (T->nrefs) {
	    dbug_printf("Unlinker: nrefs error\n");
	    leavedos(0x8115);
	}

	// unlink forward references (from the current node to other
	// nodes), which are backward refs for the other nodes
	if (debug_level('e')>8)
	    e_printf("Unlinker: refs=T%08lx N%08lx\n",(long)T->t_ref,(long)T->nt_ref);
	if (T->t_ref) {
	    TNode *Gt = *T->t_ref;
	    backref *Btq = &Gt->clink.bkr;
	    backref *Bt  = Gt->clink.bkr.next;
	    if (debug_level('e')>2) e_printf("Unlink fwd T ref to node %08lx(%08x)\n",(long)Gt,
		Gt->key);
	    while (Bt) {
		if (*Bt->ref==G) {
			Btq->next = Bt->next;
			Gt->clink.nrefs--;
			free(Bt);
			break;
		}
		Btq = Bt;
		Bt = Bt->next;
	    }
	    if (Bt==NULL) {	// not found...
		dbug_printf("Unlinker: FW T ref error\n");
		leavedos(0x8111);
	    }
	    T->t_ref = NULL;
	}
	if (T->nt_ref) {
	    TNode *Gn = *T->nt_ref;
	    backref *Bnq = &Gn->clink.bkr;
	    backref *Bn  = Gn->clink.bkr.next;
	    if (debug_level('e')>2) e_printf("Unlink fwd N ref to node %08lx(%08x)\n",(long)Gn,
		Gn->key);
	    while (Bn) {
		if (*Bn->ref==G) {
			Bnq->next = Bn->next;
			Gn->clink.nrefs--;
			free(Bn);
			break;
		}
		Bnq = Bn;
		Bn = Bn->next;
	    }
	    if (Bn==NULL) {	// not found...
		dbug_printf("Unlinker: FW N ref error\n");
		leavedos(0x8112);
	    }
	    T->nt_ref = NULL;
	}
	memset(T, 0, sizeof(linkdesc));
#ifdef PROFILE
	LinkTime += (GETTSC() - t0);
#endif
}


/////////////////////////////////////////////////////////////////////////////
/*
 * This is the function which actually executes the generated code.
 *
 * It can take two paths depending on the XECFND ("eXEC block FouND")
 * flag in the ubiquitous parameter "mode":
 * 1) if XECFND is 0 we are ending a code generation phase, and our code
 *	is still in the CodeBuf together with all its detailed info stored
 *	in InstrMeta. We close the sequence adding the TailCode, execute
 *	it, and then move it to the collecting tree and clear the temporary
 *	structures.
 *	The PC parameter is the address in the source code of the next
 *	instruction following the end of the code block. It will be stored
 *	into the TailCode of the block.
 * 2) if XECFND is not 0 we are executing a sequence found in the collecting
 *	tree. The PC parameter is NULL and G is the node we found (possibly
 *	the start of a chain of linked code sequences).
 *
 * When the code is executed, it returns in eax the source address of the
 * next instruction to find/parse.
 *
 */

static unsigned char *CloseAndExec_x86(unsigned char *PC, TNode *G, int mode, int ln)
{
	unsigned long flg;
	unsigned char *ecpu;
	long mem_ref;
	unsigned char *ePC;
	unsigned short seqflg, fpuc, ofpuc;
	unsigned char *SeqStart;
	hitimer_u TimeEndExec;

	if (mode & XECFND) {		// we found an existing node
		PC = G->addr;
		SeqStart = PC;
		seqflg = mode >> 16;
		NodesExecd++;
#ifdef PROFILE
		TotalNodesExecd++;
#endif
	}
	else if (CurrIMeta > 0) {	// we're creating a new node
		IMeta *I0 = &InstrMeta[0];
		unsigned char *p;

		if (debug_level('e')>2) {
		    e_printf("== (%d) == Closing sequence at %08lx\n",ln,(long)PC);
		}

		ProduceCode(PC);

		p = CodePtr;
		/* If the code doesn't terminate with a jump/loop instruction
		 * it still lacks the tail code; add it here */
		if (I0->clink.t_type==0) {
		    /* copy tail instructions to the end of the code block */
		    memcpy(p, TailCode, TAILSIZE);
		    p += TAILFIX;
		    I0->clink.t_link.abs = (int *)p;
		    *((int *)p) = (long)PC;
		    CodePtr += TAILSIZE;
		}

		/* show jump+tail code */
 		if ((debug_level('e')>6) && (CurrIMeta>0)) {
		    IMeta *GL = &InstrMeta[CurrIMeta-1];
		    char *pl = GL->addr+GL->len;
		    GCPrint(pl, BaseGenBuf, (long)CodePtr - (long)pl);
		}

		I0->totlen = CodePtr - BaseGenBuf;
		if (debug_level('e')>3)
		    e_printf("Seq len %#x:%#x\n",I0->seqlen,I0->totlen);

		seqflg = I0->flags;
		NodesParsed++;
#ifdef PROFILE
		TotalNodesParsed++;
#endif
		G = Move2Tree();	/* when is G==NULL? */
		/* InstrMeta will be zeroed at this point */
		/* mprotect the page here; a page fault will be triggered
		 * if some other code tries to write over the page including
		 * this node */
		e_markpage((void *)G->seqbase, G->seqlen);
		e_mprotect((void *)G->seqbase, G->seqlen);
		SeqStart = G->addr;
	}
	else {
/**/		e_printf("(X) Nothing to exec at %p\n",PC);
		return PC;
	}

	/*
	 * LastXNode stuff: history. We keep in every node the address of
	 * the next node executed, in historical order. This speeds up
	 * finding a node in the tree a lot, hits are always in the
	 * 70-85% range!
	 */
	if (LastXNode && (LastXNode->alive>0)) {
	    LastXNode->nxnode = G;	// can be relocated in the tree!
	    LastXNode->nxkey  = G->key;
	    if (debug_level('e')>2) e_printf("History: from %08x to %08x\n",LastXNode->key,G->key);
	}

	ecpu = CPUOFFS(0);
	if (debug_level('e')>1) {
		if (TheCPU.sigalrm_pending>0) e_printf("** SIGALRM is pending\n");
		e_printf("== (%d) == Executing code at %08lx flg=%04x\n",
			ln,(long)SeqStart,seqflg);
	}
#ifdef ASM_DUMP
	fprintf(aLog,"%08lx: exec\n",G->key);
#endif
	if (seqflg & F_FPOP) {
		fpuc = (TheCPU.fpuc & 0x1f00) | 0xff;
		__asm__ __volatile__ (" \
			fstcw	%0\n \
			fldcw	%1" \
			: "=m"(ofpuc) : "m"(fpuc): "memory" );
	}
	/* get the protected mode flags. Note that RF and VM are cleared
	 * by pushfd (but not by ints and traps) */
	flg = getflags();

	/* pass TF=0, IF=1, DF=0 */
	flg = (flg & ~(EFLAGS_CC|EFLAGS_IF|EFLAGS_DF|EFLAGS_TF)) |
	       (EFLAGS & EFLAGS_CC) | EFLAGS_IF;

	/* This is for exception processing */
	InCompiledCode = 1;

	/* stack frame for compiled code:
	 * esp+00	TheCPU flags
	 *     04/08	return address
	 *     08/10	dosemu flags
	 *     14/18	ebx
	 *     18/20...	locals of CloseAndExec
	 */

#ifdef __x86_64__
#define RE_REG(r) "%%r"#r
#else
#define RE_REG(r) "%%e"#r
	if (config.cpuprefetcht0)
#endif
	    __asm__ __volatile__ (
"		prefetcht0 %0\n"
		: : "m"(*ecpu) );

	__asm__ __volatile__ (
"		push   "RE_REG(bx)"\n"
"		pushf\n"
"		call	1f\n"
"		jmp	2f\n"
"1:		push	%0\n"		/* push and get TheCPU flags    */
"		rdtsc\n"
"		movl	%%eax,%3\n"	/* save time before execution   */
"		movl	%%edx,%4\n"
"		mov	%1,"RE_REG(bx)"\n"/* address of TheCPU(+0x80!)  */
"		jmp	*%2\n"		/* call SeqStart                */
"2:		mov    "RE_REG(dx)",%0\n"/* save flags			*/
"		mov    "RE_REG(ax)",%1\n"/* save PC at block exit	*/
"		rdtsc\n"
"		popf\n"
"		pop    "RE_REG(bx) 	/* restore regs                 */
		: "=S"(flg),"=c"(ePC),"=D"(mem_ref),
		  "=m"(TimeStartExec.t.tl),"=m"(TimeStartExec.t.th),
		  "=&a"(TimeEndExec.t.tl),"=&d"(TimeEndExec.t.th)
		: "1"(ecpu),"0"(flg),"2"(SeqStart)
		: "memory"
#ifdef __x86_64__ /* Generated code calls C functions which clobber ... */
		  ,"r8","r9","r10","r11"
#endif
		);

	InCompiledCode = 0;

	TimeEndExec.td -= TimeStartExec.td;
	EFLAGS = (EFLAGS & ~EFLAGS_CC) | (flg &	EFLAGS_CC);
	TheCPU.mem_ref = mem_ref;

	/* was there at least one FP op in the sequence? */
	if (seqflg & F_FPOP) {
		int exs = TheCPU.fpus & 0x7f;
		__asm__ __volatile__ (" \
			fnclex\n \
			fldcw	%0" \
			: : "m"(ofpuc): "memory" );
		if (exs) {
			e_printf("FPU: error status %02x\n",exs);
			if ((exs & ~TheCPU.fpuc) & 0x3f) {
				e_printf("FPU exception\n");
				/* TheCPU.err = EXCP10_COPR; */
			}
		}
	}

	
	TheCPU.EMUtime += TimeEndExec.td;
#ifdef PROFILE
	ExecTime += TimeEndExec.td;
#endif
	if (debug_level('e')>1) {
		e_printf("** End code, PC=%08lx sig=%x\n",(long)ePC,
		    TheCPU.sigalrm_pending);
		if ((debug_level('e')>3) && (seqflg & F_FPOP)) {
		    e_printf("  %s\n", e_trace_fp());
		}
		/* DANGEROUS - can crash dosemu! */
		if ((debug_level('e')>4) && goodmemref(mem_ref)) {
		    TryMemRef = 1;
		    e_printf("*mem_ref [%08lx] = %08x\n",mem_ref,
			*((unsigned int *)mem_ref));
		    TryMemRef = 0;
		}
	}
	/* signal_pending at this point is 1 if there was ANY signal,
	 * not just a SIGALRM
	 */
	if (signal_pending) {
		CEmuStat|=CeS_SIGPEND;
	}
	/* sigalrm_pending at this point can be:
	 *  000 - if there was no signal
	 *  0ff - if there was no signal and a jmp instruction tested it
	 *  101 - if there was a signal but no jmp instruction tested it
	 *  100 - if there was a signal and a jmp instruction tested it
	 * .. so reset it for next try
	 */
	TheCPU.sigalrm_pending = 0;

#if defined(SINGLESTEP)||defined(SINGLEBLOCK)
	avltr_delete(G->key);
	if (debug_level('e')>1) e_printf("\n%s",e_print_regs());
#else
	/*
	 * After execution comes the linker stage.
	 * So the order is:
	 *	1) build code sequence in the IMeta buffer
	 *	2) move buffer to a newly allocated node in the tree
	 *	3) execute it, always returning back at the end
	 *	4) link it to other nodes
	 * LastXNode stuff: linking. A node is linked with the next
	 * one in execution order, provided that the end source address
	 * of the preceding node matches the start source address of the
	 * following (i.e. no interpreted instructions in between).
	 */
	if (G && (G->alive>0)) {
	    if (UseLinker) NodeLinker(G);
	    LastXNode = G;
	    if (debug_level('e')>2) e_printf("New LastXNode=%08x\n",G->key);
	}
	else
#endif
	    LastXNode = NULL;

	return ePC;
}

/////////////////////////////////////////////////////////////////////////////

#endif
