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
 *	ebp		stack pointer
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
#include "codegen.h"

/* If you define this, in 16-bit stack mode the stack pointer will be
 * masked after every inc/dec operation. If undefined (faster) it can
 * temporarily over- or underflow (but this should trigger a fault
 * anyway). For 32-bit stacks this masking is only a waste of time. */
#undef  STACK_WRAP_MP

/* If you undefine this, in 16-bit stack mode the high 16 bits of ESP
 * will be zeroed after every push/pop operation. There's a small
 * possibility of breaking some code, you can easily figure out how.
 * For 32-bit stacks, keeping ESP is also a waste of time. */
#undef KEEP_ESP

extern int NextFreeIMeta;

/* Buffer and pointers to store generated code */
unsigned char CodeBuf[CODEBUFSIZE];
unsigned char *CodePtr = NULL;
unsigned char *PrevCodePtr = NULL;
unsigned char *MaxCodePtr = NULL;

unsigned long e_vga_base, e_vga_end;
int TrapVgaOn = 0;

/////////////////////////////////////////////////////////////////////////////

#define	Offs_From_Arg(p)	G1(va_arg(ap,int),(p))

/* WARNING - these are signed char offsets, NOT pointers! */
char OVERR_DS=Ofs_XDS, OVERR_SS=Ofs_XSS;

/* This code is appended at the end of every instruction sequence. It saves
 * the flags and passes back the IP of the next instruction after the sequence
 * (the one where we switch back to interpreted code).
 *		pushfd
 *		pop	edx
 *		mov #return_PC, eax
 *		ret
 */
unsigned char TailCode[8] =
	{ 0x9c,0x5a,0xb8,0,0,0,0,0xc3 };

#ifndef __KERNEL__
/* This comes from the kernel, include/asm/string.h */
static inline void * __memcpy(void * to, const void * from, size_t n)
{
int d0, d1, d2;
__asm__ __volatile__(
	"rep ; movsl\n\t"
	"testb $2,%b4\n\t"
	"je 1f\n\t"
	"movsw\n"
	"1:\ttestb $1,%b4\n\t"
	"je 2f\n\t"
	"movsb\n"
	"2:"
	: "=&c" (d0), "=&D" (d1), "=&S" (d2)
	:"0" (n/4), "q" (n),"1" ((long) to),"2" ((long) from)
	: "memory");
return (to);
}
#endif

/*
 * This function is here just for looking at the generated binary code
 * with objdump.
 */
static void _test_(void)
{
	__asm__ __volatile__ ("
		.byte	0xec
		nop
		" : : : "memory" );
}

/////////////////////////////////////////////////////////////////////////////

unsigned e_VgaRead(unsigned offs, int mode);
void e_VgaWrite(unsigned offs, unsigned u, int mode);

#if X_GRAPHICS
static inline void TRAPVGAR(unsigned char *Cp, int mode)
{
	if (!Cp) {	/* gcc-2.95 trick */
_cf_0s:	__asm__ ("
		pushfl
		cmpl	e_vga_end,%%edi
		jge	1f
		cmpl	e_vga_base,%%edi
		jl	1f
		pushl	%%ebp" : : );
_cf_01:	__asm__ ("
		pushl	$0
		movl	$e_VgaRead,%%ecx
		pushl	%%edi
		call	*%%ecx
		addl	$0x08,%%esp
		popl	%%ebp
1:		popfl"	: : );
	}
_cf_0e:	if (TrapVgaOn) {
		int SIZ = (&&_cf_0e - &&_cf_0s);
		*((char *)(&&_cf_01+1)) = (char)mode;
		__memcpy(Cp,&&_cf_0s,SIZ); Cp += SIZ;
	}
}

static inline void TRAPVGAW(unsigned char *Cp, int mode)
{
	if (!Cp) {	/* gcc-2.95 trick */
_cf_0s:	__asm__ ("
		pushfl
		cmpl	e_vga_end,%%edi
		jge	1f
		cmpl	e_vga_base,%%edi
		jl	1f
		pushl	%%ebp" : : );
_cf_01:	__asm__ ("
		pushl	$0
		pushl	%%eax
		movl	$e_VgaWrite,%%ecx
		pushl	%%edi
		call	*%%ecx
		addl	$0x0c,%%esp
		popl	%%ebp
1:		popfl"	: : );
	}
_cf_0e:	if (TrapVgaOn) {
		int SIZ = (&&_cf_0e - &&_cf_0s);
		*((char *)(&&_cf_01+1)) = (char)mode;
		__memcpy(Cp,&&_cf_0s,SIZ); Cp += SIZ;
	}
}
#else
#define TRAPVGAR(c)
#define TRAPVGAW(c)
#endif

#define GenLeaECX(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0x498d,Cp); G1((o),Cp); } else {\
			G2(0x898d,Cp); G4((o),Cp); }

#define GenLeaEDI(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0x7f8d,Cp); G1((o),Cp); } else {\
			G2(0xbf8d,Cp); G4((o),Cp); }

#define StackMaskEBP	{G3(0x6b239c,Cp); G1(Ofs_STACKM,Cp); G1(0x9d,Cp);}

/////////////////////////////////////////////////////////////////////////////
//
//	given addr in edi and data in eax:
//		mov	edi,edx
//		sub	#vga_base,edx	if >=0 this is over 0xa0000
//		jl	isnovga
//		cmp	#vga_size,edx	if > this is over vga
//		jge	isnovga
//	call Logical_VGA_{read|write}(edx,eax)
//	else mov eax,[edi]
//
/////////////////////////////////////////////////////////////////////////////


void InitGen(void)
{
	e_printf("Code buffer at %08lx\n",(long)CodeBuf);

	CodePtr = PrevCodePtr = CodeBuf;
	MaxCodePtr = CodeBuf + CODEBUFSIZE - 256;
	InitTrees();
}

/*
 * address generator unit
 * careful - do not use eax, and NEVER change any flag!
 */
void AddrGen(int op, int mode, ...)
{
	va_list	ap;
	register unsigned char *Cp = CodePtr;

	va_start(ap, mode);
	switch(op) {
	case LEA_DI_R:
		if (mode&IMMED)	{
			int o = va_arg(ap,int);
			// leal immed(%%edi),%%edi: 8d 7f xx | bf xxxxxxxx
			GenLeaEDI(o);
		}
		else {
			// leal offs(%%ebx),%%edi
			G2M(0x8d,0x7b,Cp); Offs_From_Arg(Cp);
		}
		break;
	case A_DI_0:			// base(32), imm
	case A_DI_1:			// base(32), {imm}, reg, {shift}
	case A_DI_2: {			// base(32), {imm}, reg, reg, {shift}
			long idsp=0;
			if (mode & MLEA) {		// discard base	reg
				idsp = va_arg(ap,int);
				// can't use xor because it changes flags
				// movl	$0,%%edi
				G1(0xbf,Cp); G4(0,Cp);
			}
			else {
				// movl offs(%%ebx),%%edi (seg reg offset)
				G2M(0x8b,0x7b,Cp); Offs_From_Arg(Cp);
			}
			if (mode&IMMED)	{
				idsp = va_arg(ap,int);
				if (op==A_DI_0) {
					if (idsp) { GenLeaEDI(idsp); }
					break;
				}
			}
			if (mode & ADDR16) {
				int k = 0;
				// movzwl offs(%%ebx),%%ecx
				G3M(0x0f,0xb7,0x4b,Cp); Offs_From_Arg(Cp);
				if (op==A_DI_2)	{
					// movzwl offs(%%ebx),%%edx
					G3M(0x0f,0xb7,0x53,Cp); Offs_From_Arg(Cp);
					// leal (%%ecx,%%edx,1),%%ecx
					G3M(0x8d,0x0c,0x11,Cp); k=1;
				}
				if (mode&IMMED)	{
					if (idsp) {
					    /*unsigned?*/ short ds = idsp;
					    // leal immed(%%ecx),%%ecx
					    GenLeaECX(ds);
					}
					k=1;
				}
				if (k) {
					// movzwl %%cx,%%ecx
					G3M(0x0f,0xb7,0xc9,Cp);
				}
				// leal (%%edi,%%ecx,1),%%edi
				G3M(0x8d,0x3c,0x39,Cp);
			}
			else {
				// movl offs(%%ebx),%%ecx
				G2M(0x8b,0x4b,Cp); Offs_From_Arg(Cp);
				// leal (%%edi,%%ecx,1),%%edi
				G3M(0x8d,0x3c,0x39,Cp);
				if (op==A_DI_2)	{
					// movl offs(%%ebx),%%ecx
					G2M(0x8b,0x4b,Cp); Offs_From_Arg(Cp);
					if (mode & RSHIFT) {
					    unsigned char sh = (unsigned char)(va_arg(ap,int));
					    if (sh) {
						// pushfl; shll $1,%%ecx; popfl
						if (sh==1) { G3M(0x9c,0xd1,0xe1,Cp); G1(0x9d,Cp); }
						// pushfl; shll $count,%%ecx; popfl
						else { G3M(0x9c,0xc1,0xe1,Cp); G1(sh,Cp); G1(0x9d,Cp); }
					    }
					}
					// leal (%%edi,%%ecx,1),%%edi
					G3M(0x8d,0x3c,0x39,Cp);
				}
				if ((mode&IMMED) && idsp) {
				    GenLeaEDI(idsp);
				}
			}
		}
		break;
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
			long idsp;
			if (mode & MLEA) {
				// movl $0,%%edi
				G1(0xbf,Cp); G4(0,Cp);
			}
			else {
				// movl offs(%%ebx),%%edi (seg reg offset)
				G2M(0x8b,0x7b,Cp); G1(OVERR_DS,Cp);
			}
			idsp = va_arg(ap,int);
			if (idsp) {
				GenLeaEDI(idsp);
			}
			if (!(mode & IMMED)) {
				unsigned char sh;
				// movl offs(%%ebx),%%ecx
				G2M(0x8b,0x4b,Cp); Offs_From_Arg(Cp);
				// shll $count,%%ecx
				sh = (unsigned char)(va_arg(ap,int));
				if (sh)	{
				    // pushfl; shll $1,%%ecx; popfl
				    if (sh==1) { G3M(0x9c,0xd1,0xe1,Cp); G1(0x9d,Cp); }
				    // pushfl; shll $count,%%ecx; popfl
				    else { G3M(0x9c,0xc1,0xe1,Cp); G1(sh,Cp); G1(0x9d,Cp); }
				}
				// leal (%%edi,%%ecx,1),%%edi
				G3M(0x8d,0x3c,0x39,Cp);
			}
		}
		break;
	case A_SR_SH4:	// real mode make base addr from seg
		// pushfl; movzwl offs(%%ebx),%%edx
		G4(0x53b70f9c,Cp); Offs_From_Arg(Cp);
		// shll  $4,%%edx; popfl
		G4(0x9d04e2c1,Cp);
		// movl	 %%edx,offs(%%ebx)
		G2(0x5389,Cp); Offs_From_Arg(Cp);
		break;
	}
	CodePtr = Cp;
	va_end(ap);
}


void Gen(int op, int mode, ...)
{
	int rcod=0;
	va_list	ap;
	register unsigned char *Cp = CodePtr;

	va_start(ap, mode);
	switch(op) {
	// This is the x86-on-x86 lazy way to generate code...
	case L_LITERAL:
		rcod = va_arg(ap,int);	// n.bytes
		while (rcod >= 4) {
			G4(va_arg(ap,int),Cp); rcod-=4;
		}
		*((unsigned long *)Cp)=va_arg(ap,int); Cp+=rcod;
		break;
	// Load Accumulator from CPU register
	case L_XREG1:
		// movl $immed,%%eax
		G1(0xb8,Cp); G4(va_arg(ap,int),Cp);
		break;
	// Special case: CR0&0x3f
	case L_CR0:
		// movl Ofs_CR0(%%ebx),%%eax
		G2(0x438b,Cp); G1(Ofs_CR0,Cp);
		// pushfl; andl $0x3f,%%eax; popfl
		G4(0x3fe0839c,Cp); G1(0x9d,Cp);
		break;

	// mov eax, reg
	// mov eax,[esi]	8b 06
	// mov ax,[esi]		66 8b 06
	// mov al,[esi]		8a 06
	case L_DI_R1:				// [edi]->ax
		rcod |= 0x1;
	case L_SI_R1:				// [esi]->ax
		TRAPVGAR(Cp,mode); /* check address in VGA range if under X */
		if (mode&MBYTE)	{
			// movb (%%e{ds}i),%%al
			G2M(0x8a,rcod+6,Cp);
		}
		else {
			// mov{wl} (%%e{ds}i),%%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x8b,rcod+6,Cp);
		}
		break;
	// mov [esi],eax	89 06
	// mov [esi],ax		66 89 06
	// mov [esi],al		88 06
	case S_DI:					// al,(e)ax->[edi]
		rcod = 0x1;
	case S_SI:					// al,(e)ax->[esi]
		TRAPVGAW(Cp,mode); /* check address in VGA range if under X */
		if (mode&MBYTE)	{
			// movb %%al,(%e{ds}i)
			G2M(0x88,rcod+6,Cp);
		}
		else {
			// mov{wl} %%{e}ax,(%%e{ds}i)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x89,rcod+6,Cp);
		}
		break;
	case S_DI_R:
		// mov{wl} %%{e}ax,offs(%%ebx)
		if (mode&DATA16) G1(OPERoverride,Cp);
		G2M(0x89,0x7b,Cp); Offs_From_Arg(Cp);
		break;

	case L_IMM:
		if (mode&MBYTE)	{
			// movb $immed,offs(%%ebx)
			G2M(0xc6,0x43,Cp); Offs_From_Arg(Cp);
			G1(va_arg(ap,int),Cp);	// immediate code byte
		}
		else if	(mode&DATA16) {
			// movw $immed,offs(%%ebx)
			G3M(OPERoverride,0xc7,0x43,Cp); Offs_From_Arg(Cp);
			G2(va_arg(ap,int),Cp);	// immediate code word
		}
		else {
			// movl $immed,offs(%%ebx)
			G2M(0xc7,0x43,Cp); Offs_From_Arg(Cp);
			G4(va_arg(ap,int),Cp);	// immediate code long
		}
		break;
	case L_IMM_R1:
		if (mode&MBYTE)	{
			// movb $immed,%%al
			G1(0xb0,Cp); G1(va_arg(ap,int),Cp); // immediate code byte
		}
		else if	(mode&DATA16) {
			// movw $immed,%%ax
			G2M(OPERoverride,0xb8,Cp);
			G2(va_arg(ap,int),Cp);	// immediate code word
		}
		else {
			// movl $immed,%%eax
			G1(0xb8,Cp); G4(va_arg(ap,int),Cp); // immediate code long
		}
		break;
	case L_MOVZS:
		rcod = (va_arg(ap,int)&1)<<3;	// 0=z 8=s
		if (mode & MBYTX) {
			// mov{sz}bw (%%edi),%%ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G3M(0x0f,(0xb6|rcod),0x07,Cp);
		}
		else {
			// mov{sz}wl (%%edi),%%eax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G3M(0x0f,(0xb7|rcod),0x07,Cp);
		}
		// mov{wl} %%{e}ax,offs(%%ebx)
		if (mode&DATA16) G1(OPERoverride,Cp);
		G2M(0x89,0x43,Cp); Offs_From_Arg(Cp);
		break;

	case L_LXS1:
		// mov{wl} (%%edi),%%{e}ax
		if (mode&DATA16) G1(OPERoverride,Cp);
		G2M(0x8b,0x07,Cp);
		// mov{wl} %%{e}ax,offs(%%ebx)
		if (mode&DATA16) G1(OPERoverride,Cp);
		G2M(0x89,0x43,Cp); Offs_From_Arg(Cp);
		// leal {2|4}(%%edi),%%edi
		G2M(0x8d,0x7f,Cp);
		G1((mode&DATA16? 2:4),Cp);
		break;
	case L_LXS2:	/* real mode segment base from segment value */
		// pushfl; movzwl (%%edi),%%eax
		G4(0x07b70f9c,Cp);
		// movw %%ax,offs(%%ebx)
		G3(0x438966,Cp); Offs_From_Arg(Cp);
		// shll $4,%%eax; popfl
		G4(0x9d04e0c1,Cp);
		// movl %%eax,offs(%%ebx)
		G2(0x4389,Cp); Offs_From_Arg(Cp);
		break;
	case L_ZXAX:
		// movzwl %%ax,%%eax
		G3(0xc0b70f,Cp);
		break;

	case L_REG:
		rcod = 0x8a; goto arith0;
	case S_REG:
		rcod = 0x88; goto arith0;
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
arith0:
		if (mode & MBYTE) {
			if (mode & IMMED) {
				// OPb $immed,%%al
				G1(rcod+2,Cp);
				G1(va_arg(ap,int),Cp); // immediate code byte
			}
			else {
				// OPb offs(%%ebx),%%al
				G2(0x4300|rcod,Cp); Offs_From_Arg(Cp);
			}
		}
		else {
			if (mode & IMMED) {
				if (mode&DATA16) {
					// OPw $immed,%%ax
					G1(OPERoverride,Cp); G1(rcod+3,Cp);
					G2(va_arg(ap,int),Cp);
				}
				else {
					// OPl $immed,%%eax
					G1(rcod+3,Cp);
					G4(va_arg(ap,int),Cp);
				}
			}
			else {
				// OP{wl} offs(%%ebx),%%{e}ax
				if (mode&DATA16) G1(OPERoverride,Cp);
				G2(0x4301|rcod,Cp); Offs_From_Arg(Cp);
			}
		}
		break;
	case O_SBB_M:
		rcod = 0x1a; goto arith1;
	case O_SUB_M:
		rcod = 0x2a; goto arith1;
	case O_CMP_M:
		rcod = 0x3a;
arith1:
		if (mode & MBYTE) {
			if (mode & IMMED) {
				// OPb $immed,%%al
				G1(rcod+2,Cp);
				G1(va_arg(ap,int),Cp);
			}
			else {
				// OPb (%%edi),%%al
				G2(0x0700|rcod,Cp);
			}
		}
		else {
			if (mode & IMMED) {
				if (mode&DATA16) {
					// OPw $immed,%%ax
					G1(OPERoverride,Cp); G1(rcod+3,Cp);
					G2(va_arg(ap,int),Cp);
				}
				else {
					// OPl $immed,%%eax
					G1(rcod+3,Cp);
					G4(va_arg(ap,int),Cp);
				}
			}
			else {
				// OP{wl} (%%edi),%%eax
				if (mode&DATA16) G1(OPERoverride,Cp);
				G2(0x0701|rcod,Cp);
			}
		}
		break;
	case O_NOT:
		if (mode & MBYTE) {
			// notb %%al
			G2M(0xf6,0x17,Cp);
		}
		else {
			// NOT{wl} %%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0xf7,0x17,Cp);
		}
		break;
	case O_NEG:
		if (mode & MBYTE) {
			// negb %%al
			G2M(0xf6,0x1f,Cp);
		}
		else {
			// neg{wl} %%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0xf7,0x1f,Cp);
		}
		break;
	case O_INC:
		if (mode & MBYTE) {
			// incb (%%edi)
			G2M(GRP2brm,0x07,Cp);
		}
		else {
			// inc{wl} (%%edi)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(GRP2wrm,0x07,Cp);
		}
		break;
	case O_DEC:
		if (mode & MBYTE) {
			// decb (%%edi)
			G2M(GRP2brm,0x0f,Cp);
		}
		else {
			// dec{wl} (%%edi)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(GRP2wrm,0x0f,Cp);
		}
		break;
	case O_XCHG: {
			unsigned char o1;
			o1 = (unsigned char)va_arg(ap,int);
			if (mode & MBYTE) {
				// movb offs(%%ebx),%%al
				G2M(0x8a,0x43,Cp); G1(o1,Cp);
				// movb (%%edi),%%dl; movb %%dl,offs(%%ebx)
				G4M(0x8a,0x17,0x88,0x53,Cp); G1(o1,Cp);
				// movb %%al,(%%edi)
				G2M(0x88,0x07,Cp);
			}
			else {
				// mov{wl} offs(%%ebx),%%{e}ax
				if (mode&DATA16) G1(OPERoverride,Cp);
				G2M(0x8b,0x43,Cp); G1(o1,Cp);
				// mov{wl} (%%edi),%%{e}dx
				if (mode&DATA16) G1(OPERoverride,Cp);
				G2M(0x8b,0x17,Cp);
				// mov{wl} %%{e}dx,offs(%%ebx)
				if (mode&DATA16) G1(OPERoverride,Cp);
				G2M(0x89,0x53,Cp); G1(o1,Cp);
				// movl %%{e}ax,(%%edi)
				if (mode&DATA16) G1(OPERoverride,Cp);
				G2M(0x89,0x07,Cp);
			}
		}
		break;
	case O_XCHG_R: {
			unsigned char o1,o2;
			o1 = (unsigned char)va_arg(ap,int);
			o2 = (unsigned char)va_arg(ap,int);
			// mov{wl} offs1(%%ebx),%%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x8b,0x43,Cp); G1(o1,Cp);
			// mov{wl} offs2(%%ebx),%%{e}dx
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x8b,0x53,Cp); G1(o2,Cp);
			// mov{wl} %%{e}ax,offs2(%%ebx)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x89,0x43,Cp); G1(o2,Cp);
			// mov{wl} %%{e}dx,offs1(%%ebx)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x89,0x53,Cp); G1(o1,Cp);
		}
		break;
	case O_MUL:
		if (mode & MBYTE) {
			// movb Ofs_AL(%%ebx),%%al
			G2M(0x8a,0x43,Cp); G1(Ofs_AL,Cp);
			// mulb (%%edi),%%al
			G2M(0xf6,0x27,Cp);
			// movb %%al,Ofs_AX(%%ebx)
			G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
		}
		else {
			if (mode&DATA16) {
				// movw Ofs_AX(%%ebx),%%ax
				G3M(OPERoverride,0x8b,0x43,Cp); G1(Ofs_AX,Cp);
				// mulw (%%edi),%%ax
				G3M(OPERoverride,0xf7,0x27,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
				// movw %%dx,Ofs_DX(%%ebx)
				G3M(OPERoverride,0x89,0x53,Cp); G1(Ofs_DX,Cp);
			}
			else {
				// movl Ofs_EAX(%%ebx),%%eax
				G2M(0x8b,0x43,Cp); G1(Ofs_EAX,Cp);
				// mull (%%edi),%%eax
				G2M(0xf7,0x27,Cp);
				// movl %%eax,Ofs_EAX(%%ebx)
				G2M(0x89,0x43,Cp); G1(Ofs_EAX,Cp);
				// movl %%edx,Ofs_EDX(%%ebx)
				G2M(0x89,0x53,Cp); G1(Ofs_EDX,Cp);
			}
		}
		break;
	case O_IMUL:
		if (mode & MBYTE) {
			if ((mode&(IMMED|DATA16))==(IMMED|DATA16)) {
              			// movw (%%edi),%%dx
				G3M(OPERoverride,0x8b,0x17,Cp);
				// imul $immed,%%dx,%%ax
				G3M(OPERoverride,0x6b,0xc2,Cp);
				G1(va_arg(ap,int),Cp);
				// movw %%ax,offs(%%ebx)
				G3M(OPERoverride,0x89,0x43,Cp); Offs_From_Arg(Cp);
			}
			else if ((mode&(IMMED|DATA16))==IMMED) {
              			// movl (%%edi),%%edx
				G2M(0x8b,0x17,Cp);
				// imul $immed,%%edx,%%eax
				G2M(0x6b,0xc2,Cp);
				G1(va_arg(ap,int),Cp);
				// movl %%eax,offs(%%ebx)
				G2M(0x89,0x43,Cp); Offs_From_Arg(Cp);
			}
			else {
				// movb Ofs_AL(%%ebx),%%al
				G2M(0x8a,0x43,Cp); G1(Ofs_AL,Cp);
				// imul (%%edi),%%al
				G2M(0xf6,0x2f,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
			}
		}
		else {
			if (mode&DATA16) {
				if (mode&IMMED) {
	              			// movw (%%edi),%%dx
					G3M(OPERoverride,0x8b,0x17,Cp);
					// imul $immed,%%dx,%%ax
					G3M(OPERoverride,0x69,0xc2,Cp);
					G2(va_arg(ap,int),Cp);
					// movw %%ax,offs(%%ebx)
					G3M(OPERoverride,0x89,0x43,Cp); Offs_From_Arg(Cp);
				}
				else if (mode&MEMADR) {
					unsigned char o1;
					o1 = (unsigned char)va_arg(ap,int);
					// movw offs(%%ebx),%%ax
					G3M(OPERoverride,0x8b,0x43,Cp); G1(o1,Cp);
					// imul (%%edi),%%ax
					G4M(OPERoverride,0x0f,0xaf,0x07,Cp);
					// movw %%ax,offs(%%ebx)
					G3M(OPERoverride,0x89,0x43,Cp); G1(o1,Cp);
				}
				else {
					// movw Ofs_AX(%%ebx),%%ax
					G3M(OPERoverride,0x8b,0x43,Cp); G1(Ofs_AX,Cp);
					// imul (%%edi),%%ax
					G3M(OPERoverride,0xf7,0x2f,Cp);
					// movw %%ax,Ofs_AX(%%ebx)
					G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
					// movw %%dx,Ofs_DX(%%ebx)
					G3M(OPERoverride,0x89,0x53,Cp); G1(Ofs_DX,Cp);
				}
			}
			else {
				if (mode&IMMED) {
	              			// movl (%%edi),%%edx
					G2M(0x8b,0x17,Cp);
					// imul $immed,%%edx,%%eax
					G2M(0x69,0xc2,Cp);
					G4(va_arg(ap,int),Cp);
					// movl %%eax,offs(%%ebx)
					G2M(0x89,0x43,Cp); Offs_From_Arg(Cp);  // movw (e)ax,o(ebx)
				}
				else if (mode&MEMADR) {
					unsigned char o1;
					o1 = (unsigned char)va_arg(ap,int);
					// movl offs(%%ebx),%%eax
					G2M(0x8b,0x43,Cp); G1(o1,Cp);
					// imul (%%edi),%%eax
					G3M(0x0f,0xaf,0x07,Cp);
					// movl %%eax,offs(%%ebx)
					G2M(0x89,0x43,Cp); G1(o1,Cp);
				}
				else {
					// movl Ofs_EAX(%%ebx),%%eax
					G2M(0x8b,0x43,Cp); G1(Ofs_EAX,Cp);
					// imul (%%edi),%%eax
					G2M(0xf7,0x2f,Cp);
					// movl %%eax,Ofs_EAX(%%ebx)
					G2M(0x89,0x43,Cp); G1(Ofs_EAX,Cp);
					// movl %%edx,Ofs_EDX(%%ebx)
					G2M(0x89,0x53,Cp); G1(Ofs_EDX,Cp);
				}
			}
		}
		break;
	case O_DIV:
		if (mode & MBYTE) {
			// movw Ofs_AX(%%ebx),%%ax
			G3M(OPERoverride,0x8b,0x43,Cp); G1(Ofs_AX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2M(0xc7,0x43,Cp); G1(Ofs_CR2,Cp); G4(va_arg(ap,int),Cp);
			// div (%%edi),%%al
			G2M(0xf6,0x37,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
		}
		else {
			if (mode&DATA16) {
				// movw Ofs_AX(%%ebx),%%ax
				G3M(OPERoverride,0x8b,0x43,Cp); G1(Ofs_AX,Cp);
				// movw Ofs_DX(%%ebx),%%dx
				G3M(OPERoverride,0x8b,0x53,Cp); G1(Ofs_DX,Cp);
				/* exception trap: save current PC */
				// movl $eip,Ofs_CR2(%%ebx)
				G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(va_arg(ap,int),Cp);
				// div (%%edi),%%ax
				G3M(OPERoverride,0xf7,0x37,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
				// movw %%dx,Ofs_DX(%%ebx)
				G3M(OPERoverride,0x89,0x53,Cp); G1(Ofs_DX,Cp);
			}
			else {
				// movl Ofs_EAX(%%ebx),%%eax
				G2M(0x8b,0x43,Cp); G1(Ofs_EAX,Cp);
				// movl Ofs_EDX(%%ebx),%%edx
				G2M(0x8b,0x53,Cp); G1(Ofs_EDX,Cp);
				/* exception trap: save current PC */
				// movl $eip,Ofs_CR2(%%ebx)
				G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(va_arg(ap,int),Cp);
				// div (%%edi),%%eax
				G2M(0xf7,0x37,Cp);
				// movl %%eax,Ofs_EAX(%%ebx)
				G2M(0x89,0x43,Cp); G1(Ofs_EAX,Cp);
				// movl %%edx,Ofs_EDX(%%ebx)
				G2M(0x89,0x53,Cp); G1(Ofs_EDX,Cp);
			}
		}
		break;
	case O_IDIV:
		if (mode & MBYTE) {
			// movw Ofs_AX(%%ebx),%%ax
			G3M(OPERoverride,0x8b,0x43,Cp); G1(Ofs_AX,Cp);
			/* exception trap: save current PC */
			// movl $eip,Ofs_CR2(%%ebx)
			G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(va_arg(ap,int),Cp);
			// idiv (%%edi),%%al
			G2M(0xf6,0x3f,Cp);
			// movw %%ax,Ofs_AX(%%ebx)
			G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
		}
		else {
			if (mode&DATA16) {
				// movw Ofs_AX(%%ebx),%%ax
				G3M(OPERoverride,0x8b,0x43,Cp); G1(Ofs_AX,Cp);
				// movw Ofs_DX(%%ebx),%%dx
				G3M(OPERoverride,0x8b,0x53,Cp); G1(Ofs_DX,Cp);
				/* exception trap: save current PC */
				// movl $eip,Ofs_CR2(%%ebx)
				G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(va_arg(ap,int),Cp);
				// idiv (%%edi),%%ax
				G3M(OPERoverride,0xf7,0x3f,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G3M(OPERoverride,0x89,0x43,Cp); G1(Ofs_AX,Cp);
				// movw %%dx,Ofs_DX(%%ebx)
				G3M(OPERoverride,0x89,0x53,Cp); G1(Ofs_DX,Cp);
			}
			else {
				// movl Ofs_EAX(%%ebx),%%eax
				G2M(0x8b,0x43,Cp); G1(Ofs_EAX,Cp);
				// movl Ofs_EDX(%%ebx),%%edx
				G2M(0x8b,0x53,Cp); G1(Ofs_EDX,Cp);
				/* exception trap: save current PC */
				// movl $eip,Ofs_CR2(%%ebx)
				G2(0x43c7,Cp); G1(Ofs_CR2,Cp); G4(va_arg(ap,int),Cp);
				// idiv (%%edi),%%eax
				G2M(0xf7,0x3f,Cp);
				// movl %%eax,Ofs_EAX(%%ebx)
				G2M(0x89,0x43,Cp); G1(Ofs_EAX,Cp);
				// movl %%edx,Ofs_EDX(%%ebx)
				G2M(0x89,0x53,Cp); G1(Ofs_EDX,Cp);
			}
		}
		break;
	case O_CBWD:
		// movl Ofs_EAX(%%ebx),%%eax
		G2(0x438b,Cp); G1(Ofs_EAX,Cp);
		if (mode & MBYTE) {		/* 0x98: CBW,CWDE */
			if (mode & DATA16) {	// AL->AX
				// cbw
				G2(0x9866,Cp);
				// movw %%ax,Ofs_AX(%%ebx)
				G3(0x438966,Cp); G1(Ofs_AX,Cp);
			}
			else {			// AX->EAX
				// cwde
				G1(0x98,Cp);
				// movl %%eax,Ofs_EAX(%%ebx)
				G2(0x4389,Cp); G1(Ofs_EAX,Cp);
			}
		}
		else if	(mode &	DATA16)	{	/* 0x99: AX->DX:AX */
			// cwd
			G2(0x9966,Cp);
			// movw %%dx,Ofs_DX(%%ebx)
			G3(0x538966,Cp); G1(Ofs_DX,Cp);
		}
		else {	/* 0x99: EAX->EDX:EAX */
			// cdq
			G1(0x99,Cp);
			// movl %%edx,Ofs_EDX(%%ebx)
			G2(0x5389,Cp); G1(Ofs_EDX,Cp);
		}
		break;
	case O_XLAT:
		// movl OVERR_DS(%%ebx),%%edi
		G2(0x7b8b,Cp); G1(OVERR_DS,Cp);
		if (mode & DATA16) {
			// movzwl Ofs_BX(%%ebx),%%ecx
			G3(0x4bb70f,Cp); G1(Ofs_BX,Cp);
		}
		else {
			// movl Ofs_EBX(%%ebx),%%ecx
			G2(0x4b8b,Cp); G1(Ofs_EBX,Cp);
		}
		// leal (%%ecx,%%edi,1),%%edi
		G3(0x393c8d,Cp);
		// movzbl Ofs_AL(%%ebx),%%ecx
		G3(0x4bb60f,Cp); G1(Ofs_AL,Cp);
		// leal (%%ecx,%%edi,1),%%edi
		G3(0x393c8d,Cp);
		// movb (%%edi),%%al
		// movb %%al,Ofs_AL(%%ebx)
		G4(0x4388078a,Cp); G1(Ofs_AL,Cp);
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
		if (mode & MBYTE) {
			unsigned char o1 = (unsigned char)va_arg(ap,int);
			// op [edi],1:	d0 07+r
			// op [edi],n:	c0 07+r	n
			// op [edi],cl:	d2 07+r
			if (mode & IMMED) {
				G1(o1==1? 0xd0:0xc0,Cp);
				G1(0x07	| rcod,Cp);
				if (o1!=1) G1(o1,Cp);
			}
			else {
				// movb Ofs_CL(%%ebx),%%cl
				G2(0x4b8a,Cp); G1(Ofs_CL,Cp);
				// OPb %%cl,(%%edi)
				G1(0xd2,Cp); G1(0x07 | rcod,Cp);
			}
		}
		else {
			unsigned char o1 = (unsigned char)va_arg(ap,int);
			// op [edi],1:	(66) d1	07+r
			// op [edi],n:	(66) c1	07+r n
			// op [edi],cl:	(66) d3	07+r
			if (mode & IMMED) {
				if (mode & DATA16) G1(OPERoverride,Cp);
				G1(o1==1? 0xd1:0xc1,Cp);
				G1(0x07	| rcod,Cp);
				if (o1!=1) G1(o1,Cp);
			}
			else {
				// movb Ofs_CL(%%ebx),%%cl
				G2(0x4b8a,Cp); G1(Ofs_CL,Cp);
				// OP{wl} %%cl,(%%edi)
				if (mode & DATA16) G1(OPERoverride,Cp);
				G1(0xd3,Cp); G1(0x07 | rcod,Cp);
			}
		}
		break;
	case O_OPAX: {	/* used by DAA..AAD */
			int n =	va_arg(ap,int);
			// movl Ofs_EAX(%%ebx),%%eax
			G2(0x438b,Cp); G1(Ofs_EAX,Cp);
			// get n bytes from parameter stack
			while (n--) Offs_From_Arg(Cp);
			// movl %%eax,Ofs_EAX(%%ebx)
			G2(0x4389,Cp); G1(Ofs_EAX,Cp);
		}
		break;

	case O_PUSH: {
		static char pseq16[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// leal -2(%%ebp),%%ebp
			0x8d,0x6d,0xfe,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// movw (%%edi),%%ax
/*0e*/			0x66,0x8b,0x07,0x90,
			// movw %%ax,(%%esi,%%ebp,1)
			0x66,0x89,0x04,0x2e,
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// leal -4(%%ebp),%%ebp
			0x8d,0x6d,0xfc,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// movl (%%edi),%%eax
/*0e*/			0x90,0x8b,0x07,0x90,
			// movl %%eax,(%%esi,%%ebp,1)
			0x89,0x04,0x2e,
#if 0	/* keep high 16-bits of ESP in small-stack mode */
			// pushfl; movl StackMask(%%ebx),%%edx
			0x9c,0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ebp; popfl
			0x09,0xd5,
			0x9d,
#endif
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
		};
		register char *p; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		if (mode&MEMADR) {
			*((short *)(p+0x10)) = 0x9007;
		}
		else {
			// mov{wl} offs(%%ebx),%%{e}ax
			p[0x10] = 0x43;
			p[0x11] = va_arg(ap,char);
		}
		GNX(Cp, p, sz);
		} break;

/* PUSH derived (sub-)sequences: */
	case O_PUSH1: {
		static char pseq[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
#ifndef STACK_WRAP_MP	/* mask before decrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d
#endif
		};
		GNX(Cp, pseq, sizeof(pseq));
		} break;

	case O_PUSH2: {
		static char pseq16[] = {
			// leal -2(%%ebp),%%ebp
			0x8d,0x6d,0xfe,
#ifdef STACK_WRAP_MP	/* mask after decrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
#endif
			// movw (%%edi),%%ax
/*03|08*/		0x66,0x8b,0x07,0x90,
			// movw %%ax,(%%esi,%%ebp,1)
			0x66,0x89,0x04,0x2e
		};
		static char pseq32[] = {
			// leal -4(%%ebp),%%ebp
			0x8d,0x6d,0xfc,
#ifdef STACK_WRAP_MP	/* mask after decrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
#endif
			// movw (%%edi),%%ax
/*03|08*/		0x90,0x8b,0x07,0x90,
			// movw %%ax,(%%esi,%%ebp,1)
			0x89,0x04,0x2e
		};
		register char *p; int sz;
#ifdef STACK_WRAP_MP
		const int ix = 10;
#else
		const int ix = 5;
#endif
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		if (mode&MEMADR) {
			*((short *)(p+ix)) = 0x9007;
		}
		else {
			// mov{wl} offs(%%ebx),%%{e}ax
			p[ix] = 0x43;
			p[ix+1] = va_arg(ap,char);
		}
		GNX(Cp, p, sz);
		} break;

	case O_PUSH3:
		G2(0x6b89,Cp); G1(Ofs_ESP,Cp);
		break;

	case O_PUSHI: {
		static char pseq16[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// leal -2(%%ebp),%%ebp
			0x8d,0x6d,0xfe,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// leal (%%esi,%%ebp,1),%%eax
			0x8d,0x04,0x2e,
			// movw $immed,(%%eax)
/*11*/			0x66,0xc7,0x00,0x00,0x00,
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// leal -4(%%ebp),%%ebp
			0x8d,0x6d,0xfc,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// leal (%%esi,%%ebp,1),%%eax
			0x8d,0x04,0x2e,
			// movl $immed,(%%eax)
/*11*/			0xc7,0x00,0x00,0x00,0x00,0x00,
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
		};
		register char *p; int sz;
		if (mode&DATA16) {
			p = pseq16,sz=sizeof(pseq16);
			*((short *)(p+0x14)) = va_arg(ap,short);
		}
		else {
			p = pseq32,sz=sizeof(pseq32);
			*((long *)(p+0x13)) = va_arg(ap,long);
		}
		GNX(Cp, p, sz);
		} break;

	case O_PUSHA: {
		/* push order: eax ecx edx ebx esp ebp esi edi */
		static char pseq16[] = {	// wrong if SP wraps!
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// leal -16(%%ebp),%%ebp
			0x8d,0x6d,0xf0,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// pushl %%esi; leal (%%esi,%%ebp,1),%%esi
			0x56,0x8d,0x34,0x2e,
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
			// popl %%esi; movl %%ebp,Ofs_ESP(%%ebx)
			0x5e,0x89,0x6b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// leal -32(%%ebp),%%ebp
			0x8d,0x6d,0xe0,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// pushl %%esi; leal (%%esi,%%ebp,1),%%esi
			0x56,0x8d,0x34,0x2e,
			// movl Ofs_EDI(%%ebx),%%eax
			// movl Ofs_ESI(%%ebx),%%edx
			0x8b,0x43,Ofs_EDI,0x8b,0x53,Ofs_ESI,
			// movl %%eax,0(%%esi)
			// movl %%edx,4(%%esi)
			0x89,0x46,0x00,0x89,0x56,0x04,
			// movl Ofs_EBP(%%ebx),%%eax
			// movl Ofs_ESP(%%ebx),%%edx
			0x8b,0x43,Ofs_EBP,0x8b,0x53,Ofs_ESP,
			// movl %%eax,8(%%esi)
			// movl %%edx,12(%%esi)
			0x89,0x46,0x08,0x89,0x56,0x0c,
			// movl Ofs_EBX(%%ebx),%%eax
			// movl Ofs_EDX(%%ebx),%%edx
			0x8b,0x43,Ofs_EBX,0x8b,0x53,Ofs_EDX,
			// movl %%eax,16(%%esi)
			// movl %%edx,20(%%esi)
			0x89,0x46,0x10,0x89,0x56,0x14,
			// movl Ofs_ECX(%%ebx),%%eax
			// movl Ofs_EAX(%%ebx),%%edx
			0x8b,0x43,Ofs_ECX,0x8b,0x53,Ofs_EAX,
			// movl %%eax,24(%%esi)
			// movl %%edx,28(%%esi)
			0x89,0x46,0x18,0x89,0x56,0x1c,
			// popl %%esi; movl %%ebp,Ofs_ESP(%%ebx)
			0x5e,0x89,0x6b,Ofs_ESP
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
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// movw (%%esi,%%ebp,1),%%ax
			0x66,0x8b,0x04,0x2e,
			// movw %%ax,offs(%%ebx)
/*0f*/			0x66,0x89,0x43,0x00,
			// leal 2(%%ebp),%%ebp
			0x8d,0x6d,0x02,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
#endif
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// movl (%%esi,%%ebp,1),%%eax
			0x90,0x8b,0x04,0x2e,
			// movl %%eax,offs(%%ebx)
/*0f*/			0x90,0x89,0x43,0x00,
			// leal 4(%%ebp),%%ebp
			0x8d,0x6d,0x04,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			// pushfl; movl StackMask(%%ebx),%%edx
			0x9c,0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ebp; popfl
			0x09,0xd5,
			0x9d,
#endif
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
		};
		register char *p; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		// for popping into memory the sequence is:
		//	first pop, then adjust stack, then
		//	do address calculation and last store data
		if (mode & MEMADR) {
			*((long *)(p+0x0f)) = 0x90909090;
		}
		else {
			*((short *)(p+0x10)) = 0x4389;
			p[0x12] = va_arg(ap,char);
		}
		GNX(Cp, p, sz);
		} break;

/* POP derived (sub-)sequences: */
	case O_POP1: {
		static char pseq[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d
		};
		GNX(Cp, pseq, sizeof(pseq));
		} break;

	case O_POP2: {
		static char pseq16[] = {
			// movw (%%esi,%%ebp,1),%%ax
			0x66,0x8b,0x04,0x2e,
			// movw %%ax,offs(%%ebx)
/*04*/			0x66,0x89,0x43,0x00,
			// leal 2(%%ebp),%%ebp
			0x8d,0x6d,0x02,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d
#endif
		};
		static char pseq32[] = {
			// movl (%%esi,%%ebp,1),%%eax
			0x90,0x8b,0x04,0x2e,
			// movl %%eax,offs(%%ebx)
/*04*/			0x90,0x89,0x43,0x00,
			// leal 4(%%ebp),%%ebp
			0x8d,0x6d,0x04,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			// pushfl; movl StackMask(%%ebx),%%edx
			0x9c,0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ebp; popfl
			0x09,0xd5,
			0x9d,
#endif
		};
		register char *p; int sz;
		if (mode&DATA16) p=pseq16,sz=sizeof(pseq16);
			else p=pseq32,sz=sizeof(pseq32);
		// for popping into memory the sequence is:
		//	first pop, then adjust stack, then
		//	do address calculation and last store data
		if (mode & MEMADR) {
			*((long *)(p+0x04)) = 0x90909090;
		}
		else {
			*((short *)(p+0x05)) = 0x4389;
			p[0x07] = va_arg(ap,char);
		}
		GNX(Cp, p, sz);
		} break;

	case O_POP3:
		G2(0x6b89,Cp); G1(Ofs_ESP,Cp);
		break;

	case O_POPA: {
		static char pseq16[] = {	// wrong if SP wraps!
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// pushl %%esi; leal (%%esi,%%ebp,1),%%esi
			0x56,0x8d,0x34,0x2e,
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
			// popl %%esi; leal 16(%%ebp),%%ebp
			0x5e,0x8d,0x6d,0x10,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
#endif
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
		};
		static char pseq32[] = {
			// movl Ofs_XSS(%%ebx),%%esi
			0x8b,0x73,Ofs_XSS,
			// movl Ofs_ESP(%%ebx),%%ebp
			0x8b,0x6b,Ofs_ESP,
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
			// pushl %%esi; leal (%%esi,%%ebp,1),%%esi
			0x56,0x8d,0x34,0x2e,
			// movl 0(%%esi),%%eax
			// movl 4(%%esi),%%edx
			0x8b,0x46,0x00,0x8b,0x56,0x04,
			// movl %%eax,Ofs_EDI(%%ebx)
			// movl %%edx,Ofs_ESI(%%ebx)
			0x89,0x43,Ofs_EDI,0x89,0x53,Ofs_ESI,
			// movl 8(%%esi),%%eax
			0x8b,0x46,0x08,
			// movl %%eax,Ofs_EBP(%%ebx)
			0x89,0x43,Ofs_EBP,
			// movl 16(%%esi),%%eax
			// movl 20(%%esi),%%edx
			0x8b,0x46,0x10,0x8b,0x56,0x14,
			// movl %%eax,Ofs_EBX(%%ebx)
			// movl %%edx,Ofs_EDX(%%ebx)
			0x89,0x43,Ofs_EBX,0x89,0x53,Ofs_EDX,
			// movl 24(%%esi),%%eax
			// movl 28(%%esi),%%edx
			0x8b,0x46,0x18,0x8b,0x56,0x1c,
			// movl %%eax,Ofs_ECX(%%ebx)
			// movl %%edx,Ofs_EAX(%%ebx)
			0x89,0x43,Ofs_ECX,0x89,0x53,Ofs_EAX,
			// popl %%esi; leal 32(%%ebp),%%ebp
			0x5e,0x8d,0x6d,0x20,
#ifdef STACK_WRAP_MP	/* mask after incrementing */
			// pushfl; andl StackMask(%%ebx),%%ebp; popfl
			0x9c,0x23,0x6b,Ofs_STACKM,0x9d,
#endif
#ifdef KEEP_ESP	/* keep high 16-bits of ESP in small-stack mode */
			// pushfl; movl StackMask(%%ebx),%%edx
			0x9c,0x8b,0x53,Ofs_STACKM,
			// notl %%edx
			0xf7,0xd2,
			// andl Ofs_ESP(%%ebx),%%edx
			0x23,0x53,Ofs_ESP,
			// orl %%edx,%%ebp; popfl
			0x09,0xd5,
			0x9d,
#endif
			// movl %%ebp,Ofs_ESP(%%ebx)
			0x89,0x6b,Ofs_ESP
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

	case O_MOVS_SetA: {
		static char pseq16[] = {
			// movl OVERR_DS(%%ebx),%%esi
/*00*/			0x8b,0x73,0x00,
			// movzwl Ofs_SI(%%ebx),%%edx
			0x0f,0xb7,0x53,Ofs_SI,
			// leal (%%edx,%%esi,1),%%esi
			0x8d,0x34,0x32,
			// movl Ofs_XES(%%ebx),%%edi
			0x8b,0x7b,Ofs_XES,
			// movzwl Ofs_DI(%%ebx),%%ecx
			0x0f,0xb7,0x4b,Ofs_DI,
			// leal (%%ecx,%%edi,1),%%edi
			0x8d,0x3c,0x39,
			// movzwl Ofs_CX(%%ebx),%%ecx (unused if not REP)
			0x0f,0xb7,0x4b,Ofs_CX,
			// push ds; pop es	(dangerous?)
			//0x1e,0x07
		};
		static char pseq32[] = {
			// movl OVERR_DS(%%ebx),%%esi
/*00*/			0x8b,0x73,0x00,
			// movl Ofs_ESI(%%ebx),%%edx
			0x8b,0x53,Ofs_ESI,
			// leal (%%edx,%%esi,1),%%esi
			0x8d,0x34,0x32,
			// movl Ofs_XES(%%ebx),%%edi
			0x8b,0x7b,Ofs_XES,
			// movl Ofs_EDI(%%ebx),%%ecx
			0x8b,0x4b,Ofs_EDI,
			// leal (%%ecx,%%edi,1),%%edi
			0x8d,0x3c,0x39,
			// movl Ofs_ECX(%%ebx),%%ecx (unused if not REP)
			0x8b,0x4b,Ofs_ECX,
			// push ds; pop es	(dangerous?)
			//0x1e,0x07
		};
		register char *p; int sz;
		if (mode&ADDR16) {
			p = pseq16,sz=sizeof(pseq16);
		}
		else {
			p = pseq32,sz=sizeof(pseq32);
		}
		p[2] = OVERR_DS;
		GNX(Cp, p, sz);
		} break;

	case O_MOVS_MovD:
		if (mode&(MREP|MREPNE))	{ G1(REP,Cp); }
		if (mode&MBYTE)	{ G1(MOVSb,Cp); }
		else {
			if (mode&DATA16) G1(OPERoverride,Cp);
			G1(MOVSw,Cp);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_LodD:
		if (mode&(MREP|MREPNE))	{ G1(REP,Cp); }
		if (mode&MBYTE)	{ G1(LODSb,Cp); }
		else {
			if (mode&DATA16) G1(OPERoverride,Cp);
			G1(LODSw,Cp);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_StoD:
		if (mode&(MREP|MREPNE))	{ G1(REP,Cp); }
		if (mode&MBYTE)	{ G1(STOSb,Cp); }
		else {
			if (mode&DATA16) G1(OPERoverride,Cp);
			G1(STOSw,Cp);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_ScaD:
		if (mode&MREP) { G1(REP,Cp); }
			else if	(mode&MREPNE) {	G1(REPNE,Cp); }
		if (mode&MBYTE)	{ G1(SCASb,Cp); }
		else {
			if (mode&DATA16) G1(OPERoverride,Cp);
			G1(SCASw,Cp);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_CmpD:
		if (mode&MREP) { G1(REP,Cp); }
			else if	(mode&MREPNE) {	G1(REPNE,Cp); }
		if (mode&MBYTE)	{ G1(CMPSb,Cp); }
		else {
			if (mode&DATA16) G1(OPERoverride,Cp);
			G1(CMPSw,Cp);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;

	case O_MOVS_SavA: {
		static char pseq16[] = {
			// movw %%cx,Ofs_CX(%%ebx) (redundant if not REP)
			0x66,0x89,0x4b,Ofs_CX,
			// esi = base1 + CPU_(e)SI +- n
			// edi = base2 + CPU_(e)DI +- n
			// pushfl; subl OVERR_DS(%%ebx),%%esi
/*04*/			0x9c,0x2b,0x73,0x00,
			// subl Ofs_XES(%%ebx),%%edi; popfl
			0x2b,0x7b,Ofs_XES,0x9d,
			// movw %%si,Ofs_SI(%%ebx)
			0x66,0x89,0x73,Ofs_SI,
			// movw %%di,Ofs_DI(%%ebx)
			0x66,0x89,0x7b,Ofs_DI
		};
		static char pseq32[] = {
			// movl %%ecx,Ofs_ECX(%%ebx) (redundant if not REP)
			0x89,0x4b,Ofs_ECX,
			// esi = base1 + CPU_(e)SI +- n
			// edi = base2 + CPU_(e)DI +- n
			// pushfl; subl OVERR_DS(%%ebx),%%esi
/*03*/			0x9c,0x2b,0x73,0x00,
			// subl Ofs_XES(%%ebx),%%edi; popfl
			0x2b,0x7b,Ofs_XES,0x9d,
			// movl %%esi,Ofs_ESI(%%ebx)
			0x89,0x73,Ofs_ESI,
			// movl %%edi,Ofs_EDI(%%ebx)
			0x89,0x7b,Ofs_EDI
		};
		register char *p; int sz;
		if (mode&ADDR16) {
			p = pseq16,sz=sizeof(pseq16);
			p[7] = OVERR_DS;
		}
		else {
			p = pseq32,sz=sizeof(pseq32);
			p[6] = OVERR_DS;
		}
		GNX(Cp, p, sz);
		} break;

	case O_CJMP:
		rcod = va_arg(ap,int);	// cond
		if (rcod < 16) {
			G1(0x0f,Cp); G1(rcod+0x80,Cp);
		}
		else
			G1(0xe9,Cp);
		rcod = va_arg(ap,int);	// target
		G4(rcod-(long)Cp-4,Cp);
		break;

	case O_SLAHF:
		rcod = va_arg(ap,int)&1;	// 0=LAHF 1=SAHF
		// pushfl; popl %%eax
		G2M(0x9c,POPax,Cp);
		if (rcod==0) {		/* LAHF */
			// movb %%al,Ofs_AH(%%ebx)
			G2M(0x88,0x43,Cp); G1(Ofs_AH,Cp);
		}
		else {			/* SAHF */
			// movb Ofs_AH(%%ebx),%%al
			G2M(0x8a,0x43,Cp); G1(Ofs_AH,Cp);
			// pushl %%eax; popfl
			G2M(PUSHax,0x9d,Cp);
		}
		break;
	case O_SETFL: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		switch(o1) {	// these are direct on x86
		case CMC: G1(CMC,Cp); break;
		case CLC: G1(CLC,Cp); break;
		case STC: G1(STC,Cp); break;
		case CLD: G1(CLD,Cp); break;
		case STD: G1(STD,Cp); break;
		} }
		break;
	case O_BSWAP: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		// movl offs(%%ebx),%%eax
		G2M(0x8b,0x43,Cp); G1(o1,Cp);
		// bswap %%eax
		G2M(0x0f,0xc8,Cp);
		// movl %%eax,offs(%%ebx)
		G2M(0x89,0x43,Cp); G1(o1,Cp);
		}
		break;
	case O_SETCC: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		// setcc (%%edi)
		G3M(0x0f,(0x90|(o1&15)),0x07,Cp);
		}
		break;
	case O_BITOP: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		switch (o1) {
		case 0x03: /* BT */
		case 0x0b: /* BTS */
		case 0x13: /* BTR */
		case 0x1b: /* BTC */
		case 0x1c: /* BSF */
		case 0x1d: /* BSR */
			// mov{wl} offs(%%ebx),%%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x8b,0x43,Cp); Offs_From_Arg(Cp);
			// OP{wl} %%{e}ax,(%%edi)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G3M(0x0f,(o1+0xa0),0x07,Cp);
			break;
		case 0x20: /* BT  imm8 */
		case 0x28: /* BTS imm8 */
		case 0x30: /* BTR imm8 */
		case 0x38: /* BTC imm8 */
			// OP{wl} $immed,(%%edi)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G3M(0x0f,0xba,(o1|0x07),Cp); Offs_From_Arg(Cp);
			break;
		}
		}
		break;
	case O_SHFD: {
		unsigned char l_r = (unsigned char)va_arg(ap,int)&8;
		if (mode & IMMED) {
			unsigned char shc = (unsigned char)va_arg(ap,int)&0x1f;
			// mov{wl} offs(%%ebx),%%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x8b,0x43,Cp); Offs_From_Arg(Cp);
			// sh{lr}d $immed,%%{e}ax,(%%edi)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G4M(0x0f,(0xa4|l_r),0x07,shc,Cp);
		}
		else {
			// mov{wl} offs(%%ebx),%%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2M(0x8b,0x43,Cp); Offs_From_Arg(Cp);
			// movl Ofs_ECX(%%ebx),%%ecx
			G2M(0x8b,0x4b,Cp); G1(Ofs_ECX,Cp);
			// sh{lr}d %%cl,%%{e}ax,(%%edi)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G3M(0x0f,(0xa5|l_r),0x07,Cp);
		}
		}
		break;

	case O_RDTSC: {
		long a = (long)&(TheCPU.EMUtime);
		// movl TheCPU.EMUtime,%%eax
		// movl %%eax,Ofs_EAX(%%ebx)
		G1(0xa1,Cp); G4(a,Cp); G2M(0x89,0x43,Cp); G1(Ofs_EAX,Cp);
		// movl TheCPU.EMUtime+4,%%eax
		// movl %%eax,Ofs_EDX(%%ebx)
		G1(0xa1,Cp); G4(a+4,Cp); G2M(0x89,0x43,Cp); G1(Ofs_EDX,Cp);
		}
		break;

#ifdef CPUEMU_DIRECT_IO
	case O_INPDX:
		// movl Ofs_EDX(%%ebx),%%edx
		G2(0x538b,Cp); G1(Ofs_EDX,Cp);
		if (mode&MBYTE) {
			// inb (%%dx),%%al; movb %%al,Ofs_AL(%%ebx)
			G3(0x4388ec,Cp); G1(Ofs_AL,Cp);
		}
		else {
			// in{wl} (%%dx),%%{e}ax
			if (mode&DATA16) G1(OPERoverride,Cp);
			G1(0xed,Cp);
			// mov{wl} %%{e}ax,Ofs_EAX(%%ebx)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G2(0x4389,Cp); G1(Ofs_EAX,Cp);
		}
		break;
	case O_OUTPDX:
		// movl Ofs_EDX(%%ebx),%%edx
		G2(0x538b,Cp); G1(Ofs_EDX,Cp);
		if (mode&MBYTE) {
			// movb Ofs_AL(%%ebx),%%al; outb %%al,(%%dx)
			G2(0x438a,Cp); G1(Ofs_AL,Cp); G1(0xee,Cp);
		}
		else {
			// movl Ofs_EAX(%%ebx),%%eax
			G2(0x438b,Cp); G1(Ofs_EAX,Cp);
			// out{wl} %%{e}ax,(%%dx)
			if (mode&DATA16) G1(OPERoverride,Cp);
			G1(0xef,Cp);
		}
		break;
#endif

#ifdef OPTIMIZE_BACK_JUMPS
	case JB_LOCAL: {	// cond, tgt_addr_in_buf, PC_here
		unsigned char cond = (unsigned char)va_arg(ap,int);
		/* target address */
		long p = va_arg(ap,int);
		/* check for signals, to avoid infinite loops */
		// movl e_signal_pending,%%ecx
		G2(0x0d8b,Cp); G4((long)&e_signal_pending,Cp);
		// jecxz +8
		G2(0x08e3,Cp);
		/* standard termination code */
		// pushfl; pop %%edx
		// movl $arg,%%eax
		// ret
		G3(0xb85a9c,Cp); G4(va_arg(ap,int),Cp); G1(0xc3,Cp);
		if (cond < 0x10) {
			p -= ((long)Cp+6); /* here plus instruction len */
			// jcond target-*+6
			G2M(0x0f,(0x80|cond),Cp); G4((int)p,Cp);
		}
		else {
			p -= ((long)Cp+5); /* here plus instruction len */
			// jmp target-*+5
			G1(0xe9,Cp); G4((int)p,Cp);
		}
		}
		break;
	case JCXZ_LOCAL: {	// tgt_addr_in_buf
		long p = va_arg(ap,int);
		// movl Ofs_ECX(%%ebx),%%ecx
		G2(0x4b8b,Cp); G1(Ofs_ECX,Cp);
		// j{e}cxz +2; jmp +5	(jmp around jmp)
		if (mode&ADDR16) G1(ADDRoverride,Cp);
		G4(0x05eb02e3,Cp);
		p -= ((long)Cp+5); /* here plus instruction len */
		// jmp target-*+5
		G1(0xe9,Cp); G4((int)p,Cp);
		}
		break;
	case JLOOP_LOCAL: {	// cond, tgt_addr_in_buf, PC_here
		// cond is 00(loop),04(z),05(nz)
		unsigned char cond = (unsigned char)va_arg(ap,int);
		/* target address */
		long p = va_arg(ap,int);
		// pushfl; dec{wl} Ofs_ECX(%%ebx); popfl
		G1(0x9c,Cp);
		if (mode&ADDR16) G1(OPERoverride,Cp);
		G2(0x4bff,Cp); G1(Ofs_ECX,Cp);
		G1(0x9d,Cp);
		// movl Ofs_ECX(%%ebx),%%ecx
		G2(0x4b8b,Cp); G1(Ofs_ECX,Cp);
		// j{e}cxz +25 or +26
		if (mode&ADDR16) G1(ADDRoverride,Cp);
		if (cond) {G2(0x1ae3,Cp);} else {G2(0x19e3,Cp);}
		/* check for signals, to avoid infinite loops */
		// movl e_signal_pending,%%ecx
		G2(0x0d8b,Cp); G4((long)&e_signal_pending,Cp);
		// jecxz +12
		G2(0x0ce3,Cp);
		/* modified termination code */
		// pushfl; pop %%edx
		G2(0x5a9c,Cp);
		/* oops.. restore (e)cx */
		// inc{wl} Ofs_ECX(%%ebx)
		if (mode&ADDR16) G1(OPERoverride,Cp); else G1(NOP,Cp);
		G2(0x43ff,Cp); G1(Ofs_ECX,Cp);
		// movl $arg,%%eax
		// ret
		G1(0xb8,Cp); G4(va_arg(ap,int),Cp); G1(0xc3,Cp);
		if (cond) {
			p -= ((long)Cp+6); /* here plus instruction len */
			// jcond target-*+6
			G2M(0x0f,(0x80|cond),Cp); G4((int)p,Cp);
		}
		else {
			p -= ((long)Cp+5); /* here plus instruction len */
			// jmp target-*+5
			G1(0xe9,Cp); G4((int)p,Cp);
		}
		}
		break;
#endif
#ifdef OPTIMIZE_FW_JUMPS
	case JF_LOCAL: {	// cond, PC_there
		unsigned char cond = (unsigned char)va_arg(ap,int);
		// j_not_cc +8
		G2M(0x70|(cond^1),8,Cp);	// reverse cond
		/* standard termination code */
		// pushfl; pop %%edx
		// movl $arg,%%eax
		// ret
		G3(0xb85a9c,Cp); G4(va_arg(ap,int),Cp); G1(0xc3,Cp);
		}
		break;
#endif
	}

	CodePtr = Cp;
	va_end(ap);
}


/////////////////////////////////////////////////////////////////////////////


#ifdef OPTIMIZE_FW_JUMPS
static void AdjustFwRefs(unsigned char *PC)
{
	IMeta *F = ForwIRef;
	while (F) {
	    // this is a forward jump, so start from next instruction
	    int n = (F - InstrMeta) + 1;
	    // if the jump is out of our sequence, do nothing
	    if (F->jtgt < PC) {
		IMeta *G = &InstrMeta[n];
		// look for the instruction, note that it could also
		// not be found if common seq optimization is on
		while (G->npc < F->jtgt) G++;
		if (G->npc == F->jtgt) {
		    // translated code begins with 7x 08
		    int dp = G->addr - F->addr - 7;
		    char *p = F->addr + 2;
		    e_printf("Fwj: %03d %08lx %08lx: e9 %08x\n",n,
			(long)F->addr,(long)G->addr,dp);
		    // replace tail code with a jump
		    // now the code becomes:
		    // 7x 08		(7x=inverse condition jump)
		    // e9 dddddddd	(the actual jump)
		    // xx xx c3		(junk, see above)
		    *p++ = 0xe9;
		    *((long *)p) = dp;
		}
	    }
	    F = F->fwref;
	}
	ForwIRef = NULL;
}
#endif


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
 *	The input PC is the address in the original code of the next
 *	instruction following the end of the code block. It is stored
 *	into the TailCode of the block.
 * 2) if XECFND is not 0 we are executing a sequence found in the collecting
 *	tree. The input PC is (not counting tricks) the starting address
 *	of the block in the original code.
 *	Currently a checksum of the block is calculated every time, and
 *	we go back to the parser if the code was modified. This is heavy;
 *	the real solution for detecting self-modifying stuff will be to
 *	implement write faults on code segments.
 *
 * If the block is executed until its normal end, it returns the address
 * stored in its TailCode, which is the point where the interpreter can
 * look for another precompiled block and/or resume parsing.
 * The returned PC can be different if:
 * - a forward jump out of the block is taken. See AdjustFwRefs above.
 * - a loop is suspended because of a pending signal.
 * - an instruction in the block generates a fault.
 */

unsigned char *CloseAndExec(unsigned char *PC, int mode)
{
	static unsigned long flg, ecpu;
	static long mem_ref;
	static hitimer_u t0,t1;
	static unsigned char *ePC;
	static unsigned short seqflg, fpuc, ofpuc;
	unsigned char *SeqStart;
	int ifl;

	if (mode & XECFND) {
		/* sorry for the reuse of parameter PC this way */
		TNode *G = (TNode *)PC;
		PC = G->addr;
		SeqStart = PC;
		seqflg = mode >> 16;
	}
	else if (IsCodeInBuf()) {
		unsigned char *p = CodePtr;
		/* we only know here the total length of the original
		 * code block */
		InstrMeta[0].cklen = PC - InstrMeta[0].npc;
		if (d.emu>3) e_printf("Seq len %d\n",InstrMeta[0].cklen);
		// tail	instructions
		memcpy(p, TailCode, sizeof(TailCode));
		*((long	*)(p+3)) = (long)PC;
		SeqStart = CodeBuf;
		seqflg = InstrMeta[0].flags;
		if (d.emu>2) {
			e_printf("============ Closing sequence at %08lx\n** Adding tail code at %08lx\n",
				(long)PC,(long)p);
		}
#ifdef OPTIMIZE_FW_JUMPS
		if (ForwIRef) AdjustFwRefs(PC);
#endif
	}
	else {
		return PC;
	}

	ecpu = (long)CPUOFFS(0);
	if (d.emu>2) {
		e_printf("** Executing code at %08lx flg=%04x\n",
			(long)SeqStart,seqflg);
		if (e_signal_pending) e_printf("** SIGNAL is pending\n");
	}
	ifl = EFLAGS & 0x3f7300;	// save	reserved bits
	if (seqflg & F_FPOP) {
		fpuc = (TheCPU.fpuc & 0x1f00) | 0xff;
		__asm__ __volatile__ ("
			fstcw	%0\n
			fldcw	%1"
			: "=m"(ofpuc) : "m"(fpuc): "memory" );
	}
	/* get the protected mode flags. Note that RF and VM are cleared
	 * by pushfd (but not by ints and traps) */
	__asm__ __volatile__ ("
		pushfl
		popl	%0"
		: "=m"(flg)
		:
		: "memory");

	/* pass TF=0, IF=1 */
	flg = (flg & ~0xfd5) | (EFLAGS & 0xcd5) | 0x200;

	/* This is for exception processing */
	InCompiledCode = mode | MCEXEC;	/* to make it != 0 */

	__asm__	__volatile__ ("
		pushfl\n
		pushl	%%ebp\n		/* looks like saving these four */
		pushl	%%ebx\n		/* registers is enough; if not, */
		pushl	%%edi\n		/* you'll discover soon         */
		pushl	%%esi\n
		rdtsc\n
		movl	%7,%%ebx\n	/* address of TheCPU (+0x80 !)  */
		movl	%%eax,%0\n	/* save time before execution   */
		movl	%%edx,%1\n
		pushw	%8\n		/* push and get TheCPU flags    */
		popfw\n
		call	*%9\n		/* call SeqStart                */
		movl	%%edx,%2\n	/* save flags                   */
		movl	%%eax,%3\n	/* save PC at block exit        */
		rdtsc\n
		movl	%%edi,%6\n	/* save last calculated address */
		movl	%%eax,%4\n	/* save time after execution    */
		movl	%%edx,%5\n
		popl	%%esi\n		/* restore regs                 */
		popl	%%edi\n
		popl	%%ebx\n
		popl	%%ebp\n
		popfl"
		: "=m"(t0.t.tl),"=m"(t0.t.th),"=m"(flg),"=m"(ePC),"=m"(t1.t.tl),"=m"(t1.t.th),"=m"(mem_ref)
		: "m"(ecpu),"2"(flg),"c"(SeqStart)
		: "%eax","%edx","memory" );
	InCompiledCode = 0;

	EFLAGS = (flg &	0xcff) | ifl;
	TheCPU.mem_ref = mem_ref;

	/* was there at least one FP op in the sequence? */
	if (seqflg & F_FPOP) {
		int exs = TheCPU.fpus & 0x7f;
		__asm__ __volatile__ ("
			fnclex\n
			fldcw	%0"
			: : "m"(ofpuc): "memory" );
		if (exs) {
			e_printf("FPU: error status %02x\n",exs);
			if ((exs & ~TheCPU.fpuc) & 0x3f) {
				e_printf("FPU exception\n");
				/* TheCPU.err = EXCP10_COPR; */
			}
		}
	}

	/* adjust total execution time */
	t1.td -= t0.td;
	TheCPU.EMUtime += t1.td;
	ExecTime += t1.td;
	e_signal_pending = 0;
	if (d.emu>2) {
		e_printf("** End code, PC=%08lx\n",(long)ePC);
		e_printf("\n%s",e_print_regs());
		if (seqflg & F_FPOP) {
			e_printf("  %s\n", e_trace_fp());
		}
		if ((d.emu>4) && (mem_ref >= 0) && (mem_ref < 0x110000))
			e_printf("*mem_ref [%08lx] = %08lx\n",mem_ref,
				*((unsigned long *)mem_ref));
	}
	if (!(mode & XECFND)) {
		Move2ITree();
	    if ((d.emu>2) && (ePC != PC)) {
		e_printf("## Return %08lx instead of %08lx\n",(long)ePC,
			(long)PC);
	    }
	}
	return ePC;
}

/////////////////////////////////////////////////////////////////////////////

