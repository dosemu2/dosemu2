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
 *	ebp		not modified
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

extern int NextFreeIMeta;

/* Buffer and pointers to store generated code */
unsigned char CodeBuf[CODEBUFSIZE];
unsigned char *CodePtr = NULL;
unsigned char *PrevCodePtr = NULL;
unsigned char *MaxCodePtr = NULL;

/////////////////////////////////////////////////////////////////////////////

#define	Offs_From_Arg	G1(va_arg(ap,int))

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

static void _test_(void)
{
	__asm__ __volatile__ ("
		nop
		" : : "m"(TailCode) : "memory" );
}

/////////////////////////////////////////////////////////////////////////////
/*
 * The actual code generator (x86-on-x86, this makes things a bit easier
 * to test)
 */

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
	va_start(ap, mode);
	switch(op) {
	case LEA_DI_R:
		if (mode&IMMED)	{
			// lea edi, [edi+#imm]:	8d bf ww ww ww ww
			G2M(LEA,0xbf); G4(va_arg(ap,int));
		}
		else {
			G2M(LEA,0x7b); Offs_From_Arg;
		}
		break;
	case A_DI_0:			// base(32), imm
	case A_DI_1:			// base(32), {imm}, reg, {shift}
	case A_DI_2: {			// base(32), {imm}, reg, reg, {shift}
			long idsp=0;
			if (mode & MLEA) {		// discard base	reg
				idsp = va_arg(ap,int);
				// can't use xor because it changes flags
				G1(MOVidi); G4(0);	// mov #0,edi
			}
			else {
				G2M(MOVwtrm,0x7b); Offs_From_Arg;	// mov edi,LONG_seg
			}
			if (mode&IMMED)	{
				idsp = va_arg(ap,int);
				if (op==A_DI_0) {
					if (idsp) {
						// lea edi, [edi+#imm]:	8d bf ww ww ww ww
						G2M(LEA,0xbf); G4(idsp);
					}
					break;
				}
			}
			if (mode & ADDR16) {
				int k = 0;
				// movzx ecx,[ebx+oo]:	0f b7 4b oo
				G3M(TwoByteESC,0xb7,0x4b); Offs_From_Arg;
				if (op==A_DI_2)	{
					// movzx oo(ebx),edx
					G3M(TwoByteESC,0xb7,0x53); Offs_From_Arg;
					// lea (ecx,edx,1),ecx
					G3M(LEA,0x0c,0x11); k=1;
				}
				if (mode&IMMED)	{
					if (idsp) {
						// lea ecx, [ecx+#imm]
						G2M(LEA,0x89); G4(idsp&0xffff);
					}
					k=1;
				}
				if (k) {
					// movzx cx,ecx
					G3M(TwoByteESC,0xb7,0xc9);
				}
				// lea edi, [ecx+edi]: 8d 3c 39
				G3M(LEA,0x3c,0x39);
			}
			else {
				// mov ecx, dword ptr o[ebx]: 8b 4b oo
				G2M(MOVwtrm,0x4b); Offs_From_Arg;
				// lea edi, [ecx+edi]: 8d 3c 39
				G3M(LEA,0x3c,0x39);
				if (op==A_DI_2)	{
					//  mov	ecx, (dword ptr	o[ebx] << sh): 8b 4b oo
					G2M(MOVwtrm,0x4b); Offs_From_Arg;
					if (mode & RSHIFT) {
						unsigned char sh = (unsigned char)(va_arg(ap,int));
						if (sh)	{
							if (sh==1) { G3M(PUSHF,SHIFTw,0xe1); G1(POPF); }
								else { G3M(PUSHF,SHIFTwi,0xe1); G1(sh); G1(POPF); }
						}
					}
					// lea edi, [ecx+edi]: 8d 3c 39
					G3M(LEA,0x3c,0x39);
				}
				if ((mode&IMMED) && idsp) {
					// lea edi, [edi+#imm]:	8d bf ww ww ww ww
					G2M(LEA,0xbf); G4(idsp);
				}
			}
		}
		break;
	case A_DI_2D: {			// modrm_sibd, 32-bit mode
			long idsp;
			if (mode & MLEA) {
				G1(MOVidi); G4(0);	// mov #0,edi
			}
			else {
				G2M(MOVwtrm,0x7b); G1(OVERR_DS);  // mov edi,&LONG_seg
			}
			idsp = va_arg(ap,int);
			if (idsp) {
				// lea edi,[edi+#imm]:	8d bf ww ww ww ww
				G2M(LEA,0xbf); G4(idsp);
			}
			if (!(mode & IMMED)) {
				unsigned char sh;
				// mov ecx,[ebx+oo]:	8b 4b oo
				G2M(MOVwtrm,0x4b); Offs_From_Arg;
				// shl ecx,sh
				sh = (unsigned char)(va_arg(ap,int));
				if (sh)	{
					if (sh==1) { G3M(PUSHF,SHIFTw,0xe1); G1(POPF); }
						else { G3M(PUSHF,SHIFTwi,0xe1); G1(sh); G1(POPF); }
				}
				// lea edi,[ecx+edi]:	8d 3c 39
				G3M(LEA,0x3c,0x39);
			}
		}
		break;
	case A_SR_SH4:	// real mode make base addr from seg
		// mov	 #0,edx
		// movzw sr(ebx),ecx
		G4(0x000000ba); G4(0x4bb70f00); Offs_From_Arg;
		// lea	 (edx,ecx*8),edx
		// lea	 (edx,edx),ecx
		// mov	 ecx,xsr(ebx)
		G4(0x8dca148d); G4(0x4b89120c); Offs_From_Arg;
		break;
	}
	va_end(ap);
}


void Gen(int op, int mode, ...)
{
	int rcod=0;
	va_list	ap;
	va_start(ap, mode);
	switch(op) {
	// This is the x86-on-x86 lazy way to generate code...
	case L_LITERAL:
		rcod = va_arg(ap,int);	// n.bytes
		while (rcod >= 4) {
			G4(va_arg(ap,int)); rcod-=4;
		}
		*((unsigned long *)CodePtr)=va_arg(ap,int); CodePtr+=rcod;
		break;
	// Load Accumulator from CPU register
	case L_XREG1:
		G1(MOViax); G4(va_arg(ap,int));
		break;
	// Special case: CR0&0x3f
	case L_CR0:
		G2(0x438b); G1(Ofs_CR0);
		G4(0x3fe0839c); G1(0x9d);
		break;

	// mov eax, reg
	// mov eax,[esi]	8b 06
	// mov ax,[esi]		66 8b 06
	// mov al,[esi]		8a 06
	case L_DI_R1:				// [edi]->ax
		rcod |= 0x1;
	case L_SI_R1:				// [esi]->ax
		if (mode&MBYTE)	{
			G2M(MOVbtrm,rcod+6);
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G2M(MOVwtrm,rcod+6);
		}
		break;
	// mov [esi],eax	89 06
	// mov [esi],ax		66 89 06
	// mov [esi],al		88 06
	case S_DI:					// al,(e)ax->[edi]
		rcod = 0x1;
	case S_SI:					// al,(e)ax->[esi]
		if (mode&MBYTE)	{
			G2M(MOVbfrm,rcod+6);
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G2M(MOVwfrm,rcod+6);
		}
		break;
	case S_DI_R:
		if (mode&DATA16) G1(OPERoverride);
		G2M(MOVwfrm,0x7b); Offs_From_Arg;
		break;

	case L_IMM:
		if (mode&MBYTE)	{
			G2M(MOVbirm,0x43); Offs_From_Arg;
			G1(va_arg(ap,int));	// immediate code byte
		}
		else if	(mode&DATA16) {
			G3M(OPERoverride,MOVwirm,0x43); Offs_From_Arg;
			G2(va_arg(ap,int));	// immediate code word
		}
		else {
			G2M(MOVwirm,0x43); Offs_From_Arg;
			G4(va_arg(ap,int));	// immediate code long
		}
		break;
	case L_IMM_R1:
		if (mode&MBYTE)	{
			G1(MOVial); G1(va_arg(ap,int)); // immediate code byte
		}
		else if	(mode&DATA16) {
			G2M(OPERoverride,MOViax);
			G2(va_arg(ap,int));	// immediate code word
		}
		else {
			G1(MOViax); G4(va_arg(ap,int)); // immediate code long
		}
		break;
	case L_MOVZS:
		rcod = (va_arg(ap,int)&1)<<3;	// 0=z 8=s
		if (mode & MBYTX) {
			if (mode&DATA16) G1(OPERoverride);
			G3M(0x0f,(0xb6|rcod),0x07);
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G3M(0x0f,(0xb7|rcod),0x07);
		}
		if (mode&DATA16) G1(OPERoverride);
		G2M(0x89,0x43); Offs_From_Arg;
		break;

	case L_LXS1:
		if (mode&DATA16) G1(OPERoverride);
		G2M(0x8b,0x07);			// mov (edi),ax
		if (mode&DATA16) G1(OPERoverride);
		G2M(0x89,0x43); Offs_From_Arg;	// mov ax,reg(ebx)
		G2M(0x8d,0x7f);
		G1(mode&DATA16? 2:4);		// lea n(edi),edi
		break;
	case L_LXS2:
		G4(0x000000ba); G4(0x07b70f00);	// mov #0,edx; movzw (edi),eax
		G4(0x66c2148d); G2(0x4389);	// lea (edx,eax*8),edx
		Offs_From_Arg;			// mov ax,seg(ebx)
		G3(0x12048d); G2(0x4389);	// lea (edx,edx),eax
		Offs_From_Arg;			// mov eax,xseg(ebx)
		break;
	case L_ZXAX:
		G3(0xc0b70f);			// movzxw ax,eax
		break;

	case L_REG:
		rcod = MOVbtrm; goto arith0;
	case S_REG:
		rcod = MOVbfrm; goto arith0;
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
				G1(rcod+2);
				G1(va_arg(ap,int)); // immediate code byte
			}
			else {
				// add al, o[ebx]	02 43 oo
				G2(0x4300|rcod); Offs_From_Arg;
			}
		}
		else {
			if (mode & IMMED) {
				if (mode&DATA16) {
					G1(OPERoverride); G1(rcod+3);
					G2(va_arg(ap,int));	// immediate code word
				}
				else {
					G1(rcod+3);
					G4(va_arg(ap,int));	// immediate code dword
				}
			}
			else {
				if (mode&DATA16) G1(OPERoverride);
				// add (e)ax, o[ebx]	(66) 03	43 oo
				G2(0x4301|rcod); Offs_From_Arg;
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
				// sbb al,#imm	1c xx
				G1(rcod+2);
				G1(va_arg(ap,int)); // immediate code byte
			}
			else {
				// sbb al,[edi]	1a 07
				G2(0x0700|rcod);
			}
		}
		else {
			if (mode & IMMED) {
				if (mode&DATA16) {
					G1(OPERoverride);
					// sbb ax,#imm	1d xx
					G1(rcod+3);
					G2(va_arg(ap,int));	// immediate code word
				}
				else {
					// sbb eax,#imm	1d xx
					G1(rcod+3);
					G4(va_arg(ap,int));	// immediate code dword
				}
			}
			else {
				if (mode&DATA16) G1(OPERoverride);
				// sbb (e)ax,[edi]	(66) 1b	07
				G2(0x0701|rcod);
			}
		}
		break;
	case O_NOT:
		if (mode & MBYTE) {
			G2M(GRP1brm,0x17);
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G2M(GRP1wrm,0x17);
		}
		break;
	case O_NEG:
		if (mode & MBYTE) {
			G2M(GRP1brm,0x1f);
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G2M(GRP1wrm,0x1f);
		}
		break;
	case O_INC:
		if (mode & MBYTE) {
			G2M(GRP2brm,0x07);		// inc [edi]
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G2M(GRP2wrm,0x07);		// inc [edi]
		}
		break;
	case O_DEC:
		if (mode & MBYTE) {
			G2M(GRP2brm,0x0f);		// inc [edi]
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G2M(GRP2wrm,0x0f);		// inc [edi]
		}
		break;
	case O_XCHG: {
			unsigned char o1;
			o1 = (unsigned char)va_arg(ap,int);
			if (mode & MBYTE) {
				// mov al,[ebx+o]
				G2M(MOVbtrm,0x43); G1(o1);
				// mov dl,[edi]; mov [ebx+o],dl
				G4M(MOVbtrm,0x17,MOVbfrm,0x53); G1(o1);
				// mov [edi],al
				G2M(MOVbfrm,0x07);
			}
			else {
				// mov (e)ax,[ebx+o]
				if (mode&DATA16) G1(OPERoverride);
				G2M(MOVwtrm,0x43); G1(o1);
				// mov (e)dx,[edi]
				if (mode&DATA16) G1(OPERoverride);
				G2M(MOVwtrm,0x17);
				// mov [ebx+o],(e)dx
				if (mode&DATA16) G1(OPERoverride);
				G2M(MOVwfrm,0x53); G1(o1);
				// mov [edi],(e)ax
				if (mode&DATA16) G1(OPERoverride);
				G2M(MOVwfrm,0x07);
			}
		}
		break;
	case O_XCHG_R: {
			unsigned char o1,o2;
			o1 = (unsigned char)va_arg(ap,int);
			o2 = (unsigned char)va_arg(ap,int);
			// mov (e)ax,[ebx+o1]
			if (mode&DATA16) G1(OPERoverride);
			G2M(MOVwtrm,0x43); G1(o1);
			// mov (e)dx,[ebx+o2]
			if (mode&DATA16) G1(OPERoverride);
			G2M(MOVwtrm,0x53); G1(o2);
			// mov [ebx+o2],(e)ax
			if (mode&DATA16) G1(OPERoverride);
			G2M(MOVwfrm,0x43); G1(o2);
			// mov [ebx+o1],(e)dx
			if (mode&DATA16) G1(OPERoverride);
			G2M(MOVwfrm,0x53); G1(o1);
		}
		break;
	case O_MUL:
		if (mode & MBYTE) {
			G2M(MOVbtrm,0x43); G1(Ofs_AL);
			G2M(GRP1brm,0x27);
			G3M(OPERoverride,MOVwfrm,0x43); G1(Ofs_AX);
		}
		else {
			if (mode&DATA16) {
				G3M(OPERoverride,MOVwtrm,0x43); G1(Ofs_AX);
				G3M(OPERoverride,GRP1wrm,0x27);
				G3M(OPERoverride,MOVwfrm,0x43); G1(Ofs_AX);
				G3M(OPERoverride,MOVwfrm,0x53); G1(Ofs_DX);
			}
			else {
				G2M(MOVwtrm,0x43); G1(Ofs_EAX);
				G2M(GRP1wrm,0x27);
				G2M(MOVwfrm,0x43); G1(Ofs_EAX);
				G2M(MOVwfrm,0x53); G1(Ofs_EDX);
			}
		}
		break;
	case O_IMUL:
		if (mode & MBYTE) {
			if ((mode&(IMMED|DATA16))==(IMMED|DATA16)) {
				G3M(OPERoverride,0x8b,0x17);
				G3M(OPERoverride,0x6b,0xc2);
				G1(va_arg(ap,int));
				G3M(OPERoverride,0x89,0x43); Offs_From_Arg;  
			}
			else if ((mode&(IMMED|DATA16))==IMMED) {
				G2M(0x8b,0x17);		// mov (edi),(e)dx
				G2M(0x6b,0xc2);		// imul #bb,(e)dx,(e)ax
				G1(va_arg(ap,int));
				G2M(0x89,0x43); Offs_From_Arg;  // movw (e)ax,o(ebx)
			}
			else {
				G2M(MOVbtrm,0x43); G1(Ofs_AL);
				G2M(GRP1brm,0x2f);
				G3M(OPERoverride,MOVwfrm,0x43); G1(Ofs_AX);
			}
		}
		else {
			if (mode&DATA16) {
				if (mode&IMMED) {
					G3M(OPERoverride,0x8b,0x17);
					G3M(OPERoverride,0x69,0xc2);
					G2(va_arg(ap,int));
					G3M(OPERoverride,0x89,0x43); Offs_From_Arg;
				}
				else if (mode&MEMADR) {
					unsigned char o1;
					o1 = (unsigned char)va_arg(ap,int);
					G3M(OPERoverride,0x8b,0x43); G1(o1);
					G4M(OPERoverride,0x0f,0xaf,0x07);
					G3M(OPERoverride,0x89,0x43); G1(o1);
				}
				else {
					G3M(OPERoverride,MOVwtrm,0x43); G1(Ofs_AX);
					G3M(OPERoverride,GRP1wrm,0x2f);
					G3M(OPERoverride,MOVwfrm,0x43); G1(Ofs_AX);
					G3M(OPERoverride,MOVwfrm,0x53); G1(Ofs_DX);
				}
			}
			else {
				if (mode&IMMED) {
					G2M(0x8b,0x17);	// mov (edi),(e)dx
					G2M(0x69,0xc2);	// imul #ww,(e)dx,(e)ax
					G4(va_arg(ap,int));
					G2M(0x89,0x43); Offs_From_Arg;  // movw (e)ax,o(ebx)
				}
				else if (mode&MEMADR) {
					unsigned char o1;
					o1 = (unsigned char)va_arg(ap,int);
					G2M(0x8b,0x43); G1(o1);
					G3M(0x0f,0xaf,0x07);
					G2M(0x89,0x43); G1(o1);
				}
				else {
					G2M(MOVwtrm,0x43); G1(Ofs_EAX);
					G2M(GRP1wrm,0x2f);
					G2M(MOVwfrm,0x43); G1(Ofs_EAX);
					G2M(MOVwfrm,0x53); G1(Ofs_EDX);
				}
			}
		}
		break;
	case O_DIV:
		/* must	trap div by 0 */
		if (mode & MBYTE) {
			G3M(OPERoverride,0x8b,0x43); G1(Ofs_AX);
			G2M(0xf6,0x37);
			G3M(OPERoverride,0x89,0x43); G1(Ofs_AX);
		}
		else {
			if (mode&DATA16) {
				G3M(OPERoverride,0x8b,0x43); G1(Ofs_AX);
				G3M(OPERoverride,0x8b,0x53); G1(Ofs_DX);
				G3M(OPERoverride,0xf7,0x37);
				G3M(OPERoverride,0x89,0x43); G1(Ofs_AX);
				G3M(OPERoverride,0x89,0x53); G1(Ofs_DX);
			}
			else {
				G2M(0x8b,0x43); G1(Ofs_EAX);
				G2M(0x8b,0x53); G1(Ofs_EDX);
				G2M(0xf7,0x37);
				G2M(0x89,0x43); G1(Ofs_EAX);
				G2M(0x89,0x53); G1(Ofs_EDX);
			}
		}
		break;
	case O_IDIV:
		/* must	trap div by 0 */
		if (mode & MBYTE) {
			G3M(OPERoverride,0x8b,0x43); G1(Ofs_AX);
			G2M(0xf6,0x3f);
			G3M(OPERoverride,0x89,0x43); G1(Ofs_AX);
		}
		else {
			if (mode&DATA16) {
				G3M(OPERoverride,0x8b,0x43); G1(Ofs_AX);
				G3M(OPERoverride,0x8b,0x53); G1(Ofs_DX);
				G3M(OPERoverride,0xf7,0x3f);
				G3M(OPERoverride,0x89,0x43); G1(Ofs_AX);
				G3M(OPERoverride,0x89,0x53); G1(Ofs_DX);
			}
			else {
				G2M(0x8b,0x43); G1(Ofs_EAX);
				G2M(0x8b,0x53); G1(Ofs_EDX);
				G2M(0xf7,0x3f);
				G2M(0x89,0x43); G1(Ofs_EAX);
				G2M(0x89,0x53); G1(Ofs_EDX);
			}
		}
		break;
	case O_CBWD:
		if (mode & MBYTE) {		/* 0x98: CBW,CWDE */
			if (mode & DATA16) {
				// mov al,[ebx+AL]
				G2(0x438a); G1(Ofs_AL);
				// cbw
				G2(0x9866);
				// mov [ebx+AH],ah
				G2(0x6388); G1(Ofs_AH);
			}
			else {
				// mov ax,[ebx+AX]
				G3(0x438b66); G1(Ofs_AX);
				// cwde
				G1(0x98);
				// mov [ebx+AXH],dx
				G3(0x538966); G1(Ofs_AXH);
			}
		}
		else if	(mode &	DATA16)	{	/* 0x99	*/
			// mov ax,[ebx+AX]
			G3(0x438b66); G1(Ofs_AX);
			// cwd
			G2(0x9966);
			// mov [ebx+DX],dx
			G3(0x538966); G1(Ofs_DX);
		}
		else {	/* 0x99	*/
			// mov eax,[ebx+EAX]
			G2(0x438b); G1(Ofs_EAX);
			// cdq
			G1(0x99);
			// mov [ebx+EDX],edx
			G2(0x5389); G1(Ofs_EDX);
		}
		break;
	case O_XLAT:
		G2(0x7b8b);	G1(OVERR_DS);
		if (mode & DATA16) {
			G3(0x4bb70f); G1(Ofs_BX);
			// lea edi, [ecx+edi]: 8d 3c 39
		}
		else {
			// mov ecx, dword ptr o[ebx]: 8b 4b oo
			G2(0x4b8b);	G1(Ofs_EBX);
			// lea edi, [ecx+edi]: 8d 3c 39
		}
		G3(0x393c8d);
		G3(0x4bb60f); G1(Ofs_AL);
		G3(0x393c8d);
		G4(0x4388078a);	G1(Ofs_AL);
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
				G1(o1==1? 0xd0:0xc0);
				G1(0x07	| rcod);
				if (o1!=1) G1(o1);
			}
			else {
				G2(0x4b8a); G1(Ofs_CL);
				G1(0xd2); G1(0x07 | rcod);
			}
		}
		else {
			unsigned char o1 = (unsigned char)va_arg(ap,int);
			// op [edi],1:	(66) d1	07+r
			// op [edi],n:	(66) c1	07+r n
			// op [edi],cl:	(66) d3	07+r
			if (mode & IMMED) {
				if (mode & DATA16) G1(OPERoverride);
				G1(o1==1? 0xd1:0xc1);
				G1(0x07	| rcod);
				if (o1!=1) G1(o1);
			}
			else {
				G2(0x4b8a); G1(Ofs_CL);
				if (mode & DATA16) G1(OPERoverride);
				G1(0xd3); G1(0x07 | rcod);
			}
		}
		break;
	case O_OPAX: {
			int n =	va_arg(ap,int);
			G2(0x438b); G1(Ofs_EAX);
			while (n--) Offs_From_Arg;
			G2(0x4389); G1(Ofs_EAX);
		}
		break;

	case O_PUSH:
		// STACK16:	esi = esi + (rSP-size)&0xffff
		// STACK32:	esi = esi + rESP - size
		//
		// mov esi,[ebx+xss]:	8b 73 oo
		G2(0x738b); G1(Ofs_XSS);
		if (mode & STACK16) {
			// movzx sp(ebx),ecx:	0f b7 4b oo
			G3(0x4bb70f); G1(Ofs_SP);
		}
		else {
			// mov esp(ebx),ecx
			G2(0x4b8b); G1(Ofs_ESP);
		}
		//	lea -size(ecx),ecx
		//	movzx cx,ecx (stack16)
		G2(0x498d); G1(mode&DATA16? 0xfe:0xfc);
		if (mode & STACK16) {G3(0xc9b70f);}
		if (mode & DATA16) G1(OPERoverride);
		if (mode & MEMADR) {
			// mov (e)ax,[edi]:	(66) 8b	07
			G2(0x078b);
		}
		else {
			// mov (e)ax,[ebx+oo]:	(66) 8b	43 oo
			G2(0x438b); Offs_From_Arg;
		}
		if (mode & DATA16) G1(OPERoverride);
		// mov [esi+ecx],(e)ax:		89 04 0e
		G3(0x0e0489);
		if (mode & STACK16) {
			// mov [ebx+sp],cx:	66 89 4b oo
			G3(0x4b8966); G1(Ofs_SP);
		}
		else {
			// mov [ebx+esp],ecx:	89 4b oo
			G2(0x4b89); G1(Ofs_ESP);
		}
		break;
/* PUSH derived (sub-)sequences: */
	case O_PUSH1:
		G2(0x738b); G1(Ofs_XSS);
		if (mode & STACK16) {G3(0x4bb70f); G1(Ofs_SP);}
			else {G2(0x4b8b); G1(Ofs_ESP);}
		break;
	case O_PUSH2:
		G2(0x498d); G1(mode&DATA16? 0xfe:0xfc);
		if (mode & STACK16) {G3(0xc9b70f);}
		if (mode & DATA16) G1(OPERoverride);
		if (mode & MEMADR) {G2(0x078b);}
			else {G2(0x438b); Offs_From_Arg;}
		if (mode & DATA16) G1(OPERoverride);
		G3(0x0e0489);
		break;
	case O_PUSH3:
		if (mode & STACK16) {G3(0x4b8966); G1(Ofs_SP);}
			else {G2(0x4b89); G1(Ofs_ESP);}
		break;
	case O_PUSHI:
		G2(0x738b); G1(Ofs_XSS);
		if (mode & STACK16) {
			G3(0x4bb70f); G1(Ofs_SP);
			G2(0x498d); G1(mode&DATA16? 0xfe:0xfc);
			G3(0xc9b70f); G3(0x0e348d);
			if (mode & DATA16) G1(OPERoverride);
			G2(0x06c7);
			if (mode & DATA16) {G2(va_arg(ap,int));}
				else {G4(va_arg(ap,int));}
			G3(0x4b8966); G1(Ofs_SP);
		}
		else {
			G2(0x4b8b); G1(Ofs_ESP);
			G2(0x498d); G1(mode&DATA16? 0xfe:0xfc);
			G3(0x0e348d);
			if (mode & DATA16) G1(OPERoverride);
			G2(0x06c7);
			if (mode & DATA16) {G2(va_arg(ap,int));}
				else {G4(va_arg(ap,int));}
			G2(0x4b89); G1(Ofs_ESP);
		}
		break;
	case O_PUSHA: {
		/* push order: eax ecx edx ebx esp ebp esi edi */
		static char pseq16[] = {	// wrong if SP wraps!
			0x8b,0x43,Ofs_EDI,0x8b,0x53,Ofs_ESI,
			0x66,0x89,0x46,0x00,0x66,0x89,0x56,0x02,
			0x8b,0x43,Ofs_EBP,0x8b,0x53,Ofs_ESP,
			0x66,0x89,0x46,0x04,0x66,0x89,0x56,0x06,
			0x8b,0x43,Ofs_EBX,0x8b,0x53,Ofs_EDX,
			0x66,0x89,0x46,0x08,0x66,0x89,0x56,0x0a,
			0x8b,0x43,Ofs_ECX,0x8b,0x53,Ofs_EAX,
			0x66,0x89,0x46,0x0c,0x66,0x89,0x56,0x0e
		};
		static char pseq32[] = {
			0x8b,0x43,Ofs_EDI,0x8b,0x53,Ofs_ESI,
			0x89,0x46,0x00,0x89,0x56,0x04,
			0x8b,0x43,Ofs_EBP,0x8b,0x53,Ofs_ESP,
			0x89,0x46,0x08,0x89,0x56,0x0c,
			0x8b,0x43,Ofs_EBX,0x8b,0x53,Ofs_EDX,
			0x89,0x46,0x10,0x89,0x56,0x14,
			0x8b,0x43,Ofs_ECX,0x8b,0x53,Ofs_EAX,
			0x89,0x46,0x18,0x89,0x56,0x1c
		};
		// STACK16:	esi = esi + (rSP-size)&0xffff
		// STACK32:	esi = esi + rESP - size
		// mov esi,[ebx+xss]:	8b 73 oo
		G2(0x738b); G1(Ofs_XSS);
		if (mode & STACK16) {
			// movzx sp(ebx),ecx:	0f b7 4b oo
			G3(0x4bb70f); G1(Ofs_SP);
			//	lea -size(ecx),ecx
			//	movzx cx,ecx
			G2(0x498d); G1(mode&DATA16? 0xf0:0xe0);
			G3(0xc9b70f);
		}
		else {
			// mov esp(ebx),ecx:	8b 4b oo
			G2(0x4b8b); G1(Ofs_ESP);
			//	lea -size(ecx),ecx
			G2(0x498d); G1(mode&DATA16? 0xf0:0xe0);
		}
		// 	lea (esi,ecx,1),esi
		G3(0x0e348d);
		if (mode & DATA16) {
			GNX(pseq16);
		}
		else {
			GNX(pseq32);
		}
		if (mode & STACK16) {
			// mov cx,sp(ebx):	66 89 4b oo
			G3(0x4b8966); G1(Ofs_SP);
		}
		else {
			// mov ecx,esp(ebx):	89 4b oo
			G2(0x4b89); G1(Ofs_ESP);
		} }
		break;

	case O_POP:
		// STACK16:	esi = esi + (sp-size)&0xffff
		// STACK32:	esi = esi + esp	- size
		// mov xss(ebx),esi:	8b 73 oo
		G2(0x738b); G1(Ofs_XSS);
		if (mode & STACK16) {
			// movzx sp(ebx),ecx:	0f b7 4b oo
			G3(0x4bb70f); G1(Ofs_SP);
		}
		else {
			// mov esp(ebx),ecx: 8b 4b oo
			G2(0x4b8b); G1(Ofs_ESP);
		}
		// stack address: data to pop
		if (mode & DATA16) G1(OPERoverride);
		// mov (esi+ecx),{e}ax:	(66) 8b	04 0e
		G3(0x0e048b);
		// for popping into memory the sequence is:
		//	first pop, then adjust stack, then
		//	do address calculation and last store data
		if (!(mode & MEMADR)) {
			if (mode & DATA16) G1(OPERoverride);
			// mov {e}ax,oo(ebx):	(66) 89 43 oo
			G2(0x4389); Offs_From_Arg;
		}
		// inc and save	(e)sp
		// lea size(ecx),ecx:	8d 49 sz
		G2(0x498d); G1(mode & DATA16? 2:4);
		if (mode & STACK16) {G3(0xc9b70f);}
		// mov {e}cx,{e}sp(ebx):	(66) 89	4b oo
		if (mode & STACK16) {
			G3(0x4b8966); G1(Ofs_SP);
		}
		else {
			G2(0x4b89); G1(Ofs_ESP);
		}
		break;
/* POP derived (sub-)sequences: */
	case O_POP1:
		G2(0x738b); G1(Ofs_XSS);
		if (mode & STACK16) {G3(0x4bb70f); G1(Ofs_SP);}
			else {G2(0x4b8b); G1(Ofs_ESP);}
		break;
	case O_POP2:
		if (mode & DATA16) G1(OPERoverride);
		G3(0x0e048b);
		if (!(mode & MEMADR)) {
			if (mode & DATA16) G1(OPERoverride);
			G2(0x4389); Offs_From_Arg;
		}
		G2(0x498d); G1(mode & DATA16? 2:4);
		if (mode & STACK16) {G3(0xc9b70f);}
		break;
	case O_POP3:
		if (mode & STACK16) {G3(0x4b8966); G1(Ofs_SP);}
			else {G2(0x4b89); G1(Ofs_ESP);}
		break;
	case O_POPA: {
		// STACK16:	esi = esi + (sp-size)&0xffff
		// STACK32:	esi = esi + esp	- size
		static char pseq16[] = {	// wrong if SP wraps!
			0x66,0x8b,0x46,0x00,0x66,0x8b,0x56,0x02,
			0x66,0x89,0x43,Ofs_DI,0x66,0x89,0x53,Ofs_SI,
			0x66,0x8b,0x46,0x04,
			0x66,0x89,0x43,Ofs_BP,
			0x66,0x8b,0x46,0x08,0x66,0x8b,0x56,0x0a,
			0x66,0x89,0x43,Ofs_BX,0x66,0x89,0x53,Ofs_DX,
			0x66,0x8b,0x46,0x0c,0x66,0x8b,0x56,0x0e,
			0x66,0x89,0x43,Ofs_CX,0x66,0x89,0x53,Ofs_AX
		};
		static char pseq32[] = {
			0x8b,0x46,0x00,0x8b,0x56,0x04,
			0x89,0x43,Ofs_EDI,0x89,0x53,Ofs_ESI,
			0x8b,0x46,0x08,
			0x89,0x43,Ofs_EBP,
			0x8b,0x46,0x10,0x8b,0x56,0x14,
			0x89,0x43,Ofs_EBX,0x89,0x53,Ofs_EDX,
			0x8b,0x46,0x18,0x8b,0x56,0x1c,
			0x89,0x43,Ofs_ECX,0x89,0x53,Ofs_EAX
		};
		// mov esi,[ebx+xss]:	8b 73 oo
		G2(0x738b); G1(Ofs_XSS);
		if (mode & STACK16) {
			// movzx sp(ebx),ecx:	0f b7 4b oo
			G3(0x4bb70f); G1(Ofs_SP);
		}
		else {
			// mov esp(ebx),ecx:	8b 4b oo
			G2(0x4b8b); G1(Ofs_ESP);
		}
		// stack address: data to pop
		// 	lea (esi,ecx,1),esi
		G3(0x0e348d);
		if (mode & DATA16) {
			GNX(pseq16);
		}
		else {
			GNX(pseq32);
		}
		// inc and save (e)sp
		// lea ecx,[ecx+sz]:	8d 49 sz
		G2(0x498d); G1(mode & DATA16? 16:32);
		// mov [ebx+(e)sp],(e)cx:	(66) 89	4b oo
		if (mode & STACK16) {
			G3(0x4b8966); G1(Ofs_SP);
		}
		else {
			G2(0x4b89); G1(Ofs_ESP);
		} }
		break;

	case O_MOVS_SetA:
		// mov esi,OVERR_DS+[ebx+(e)si]
		G2(0x738b);	G1(OVERR_DS);
		if (mode&ADDR16) {
			G3(0x53b70f); G1(Ofs_SI);	// CPU_(e)SI ->	edx
		}
		else {
			G2(0x538b); G1(Ofs_ESI);
		}
		G3(0x32348d);
		// mov edi,LONG_ES+[ebx+(e)di]
		G2(0x7b8b);	G1(Ofs_XES);
		if (mode&ADDR16) {
			G3(0x4bb70f); G1(Ofs_DI);	// CPU_(e)DI ->	ecx
		}
		else {
			G2(0x4b8b); G1(Ofs_EDI);
		}
		G3(0x393c8d);
		if (mode&(MREP|MREPNE))	{
			if (mode&ADDR16) {
				G3(0x4bb70f); G1(Ofs_CX);
			}
			else {
				G2(0x4b8b); G1(Ofs_ECX);
			}
		}
		G2(0x071e);
		break;
	case O_MOVS_MovD:
		if (mode&(MREP|MREPNE))	{ G1(REP); }
		if (mode&MBYTE)	{ G1(MOVSb);	}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G1(MOVSw);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_LodD:
		if (mode&(MREP|MREPNE))	{ G1(REP); }
		if (mode&MBYTE)	{ G1(LODSb);	}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G1(LODSw);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_StoD:
		if (mode&(MREP|MREPNE))	{ G1(REP); }
		if (mode&MBYTE)	{ G1(STOSb);	}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G1(STOSw);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_ScaD:
		if (mode&MREP) { G1(REP); }
			else if	(mode&MREPNE) {	G1(REPNE); }
		if (mode&MBYTE)	{ G1(SCASb);	}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G1(SCASw);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_CmpD:
		if (mode&MREP) { G1(REP); }
			else if	(mode&MREPNE) {	G1(REPNE); }
		if (mode&MBYTE)	{ G1(CMPSb);	}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G1(CMPSw);
		}
		// ! Warning DI,SI wrap	in 16-bit mode
		break;
	case O_MOVS_SavA:
		if (mode&(MREP|MREPNE))	{
			if (mode&ADDR16) {
				G1(OPERoverride);
				G2(0x4b89); G1(Ofs_CX);
			}
			else {
				G2(0x4b89); G1(Ofs_ECX);
			}
		}
		// esi = base1 + CPU_(e)SI +- n
		// edi = base2 + CPU_(e)DI +- n
		// pushf; sub esi,OVERR_DS
		G3(0x732b9c); G1(OVERR_DS);
		// sub edi,LONG_ES; popf
		G2(0x7b2b); G1(Ofs_XES); G1(0x9d);
		if (mode&ADDR16) {
			G3(0x738966); G1(Ofs_SI);
			G3(0x7b8966); G1(Ofs_DI);
		}
		else {
			G2(0x7389); G1(Ofs_ESI);
			G2(0x7b89); G1(Ofs_EDI);
		}
		break;

	case O_CJMP:
		rcod = va_arg(ap,int);	// cond
		if (rcod < 16) {
			G1(0x0f); G1(rcod+0x80);
		}
		else
			G1(0xe9);
		rcod = va_arg(ap,int);	// target
		G4(rcod	- (long)CodePtr - 4);
		break;

	case O_SLAHF:
		rcod = va_arg(ap,int)&1;	// 0=LAHF 1=SAHF
		if (rcod==0) {
			// LAHF:mov al,eflags
			G2M(PUSHF,POPax);
			// LAHF:mov [ebx+AH],al
			G2M(0x88,0x43); G1(Ofs_AH);
		}
		else {
			// SAHF:mov al,[ebx+AH]
			G2M(PUSHF,POPax);
			G2M(0x8a,0x43); G1(Ofs_AH);
			// SAHF:mov eflags,al
			G2M(PUSHax,POPF);
		}
		break;
	case O_SETFL: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		switch(o1) {	// these are direct on x86
		case CMC:	G1(CMC); break;
		case CLC:	G1(CLC); break;
		case STC:	G1(STC); break;
		case CLD:	G1(CLD); break;
		case STD:	G1(STD); break;
		} }
		break;
	case O_BSWAP: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		G2M(0x8b,0x43); G1(o1);
		G2M(0x0f,0xc8);
		G2M(0x89,0x43); G1(o1);
		}
		break;
	case O_SETCC: {
		unsigned char o1 = (unsigned char)va_arg(ap,int);
		G3M(0x0f,(0x90|(o1&15)),0x07);	// setcc (%%edi)
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
			if (mode&DATA16) G1(OPERoverride);
			G2M(0x8b,0x43); Offs_From_Arg;
			if (mode&DATA16) G1(OPERoverride);
			G3M(0x0f,(o1+0xa0),0x07);
			break;
		case 0x20: /* BT  imm8 */
		case 0x28: /* BTS imm8 */
		case 0x30: /* BTR imm8 */
		case 0x38: /* BTC imm8 */
			if (mode&DATA16) G1(OPERoverride);
			G3M(0x0f,0xba,(o1|0x07)); Offs_From_Arg;
			break;
		}
		}
		break;
	case O_SHFD: {
		unsigned char l_r = (unsigned char)va_arg(ap,int)&8;
		if (mode & IMMED) {
			unsigned char shc = (unsigned char)va_arg(ap,int)&0x1f;
			if (mode&DATA16) G1(OPERoverride);	// REG1->(e)ax
			G2M(0x8b,0x43); Offs_From_Arg;
			if (mode&DATA16) G1(OPERoverride);
			G4M(0x0f,(0xa4|l_r),0x07,shc);	// shxd nn,(e)ax,(edi)
		}
		else {
			if (mode&DATA16) G1(OPERoverride);
			G2M(0x8b,0x43); Offs_From_Arg;
			G2M(0x8b,0x4b); G1(Ofs_ECX);
			if (mode&DATA16) G1(OPERoverride);
			G3M(0x0f,(0xa5|l_r),0x07);	// shxd cl,(e)ax,(edi)
		}
		}
		break;

	case O_RDTSC: {
		long a = (long)&(TheCPU.EMUtime);
		G1(0xa1); G4(a); G2M(0x89,0x43); G1(Ofs_EAX);
		G1(0xa1); G4(a+4); G2M(0x89,0x43); G1(Ofs_EDX);
		}
		break;

#ifdef OPTIMIZE_BACK_JUMPS
	case JB_LOCAL: {	// cond, tgt_addr_in_buf, PC_here
		unsigned char cond = (unsigned char)va_arg(ap,int);
		long p = va_arg(ap,int);
		/* check for signals, to avoid infinite loops */
		G2(0x0d8b); G4((long)&e_signal_pending); G2(0x08e3);
		/* termination code */
		G3(0xb85a9c); G4(va_arg(ap,int)); G1(0xc3);
		if (cond < 0x10) {
			p -= ((long)CodePtr + 6);
			G2M(0x0f,(0x80|cond)); G4((int)p);
		}
		else {
			p -= ((long)CodePtr + 5);
			G1(0xe9); G4((int)p);
		}
		}
		break;
	case JCXZ_LOCAL: {	// tgt_addr_in_buf
		long p = va_arg(ap,int);
		// load ecx
		G2(0x4b8b); G1(Ofs_ECX);
		// test (e)cx
		if (mode&ADDR16) G1(ADDRoverride);
		G4(0x05eb02e3);		// jmp around jmp
		p -= ((long)CodePtr + 5);
		G1(0xe9); G4((int)p);
		}
		break;
	case JLOOP_LOCAL: {	// cond, tgt_addr_in_buf, PC_here
		// cond is 00(loop),04(z),05(nz)
		unsigned char cond = (unsigned char)va_arg(ap,int);
		long p = va_arg(ap,int);
		/* dec and check (e)cx */
		G1(0x9c);
		if (mode&ADDR16) G1(OPERoverride); G2(0x4bff); G1(Ofs_ECX);
		G1(0x9d);
		G2(0x4b8b); G1(Ofs_ECX);
		if (mode&ADDR16) G1(ADDRoverride);
		if (cond) {G2(0x1ae3);} else {G2(0x19e3);}
		/* check for signals, to avoid infinite loops */
		G2(0x0d8b); G4((long)&e_signal_pending); G2(0x0ce3);
		/* termination code */
		G2(0x5a9c);
		// oops.. restore (e)cx
		if (mode&ADDR16) G1(OPERoverride); else G1(NOP);
		G2(0x43ff); G1(Ofs_ECX);
		G1(0xb8); G4(va_arg(ap,int)); G1(0xc3);
		if (cond) {
			p -= ((long)CodePtr + 6);
			G2M(0x0f,(0x80|cond)); G4((int)p);
		}
		else {
			p -= ((long)CodePtr + 5);
			G1(0xe9); G4((int)p);
		}
		}
		break;
#endif
#ifdef OPTIMIZE_FW_JUMPS
	case JF_LOCAL: {	// cond, PC_there
		unsigned char cond = (unsigned char)va_arg(ap,int);
		G2M(0x70|(cond^1),8);		// reverse cond
		/* termination code */
		G3(0xb85a9c); G4(va_arg(ap,int)); G1(0xc3);
		}
		break;
#endif
	}

	va_end(ap);
}


/////////////////////////////////////////////////////////////////////////////


#ifdef OPTIMIZE_FW_JUMPS
static void AdjustFwRefs(unsigned char *PC)
{
	/* There's too much self-modifying code around. Do not
	 * jump everywhere if in real/V86 mode. Sigh. */
	IMeta *F = (REALADDR()? NULL:ForwIRef);
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
		SeqStart = PC;
		seqflg = mode >> 16;
	}
	else if (IsCodeInBuf()) {
		unsigned char *p = CodePtr;
		// tail	instructions
		memcpy(p, TailCode, sizeof(TailCode));
		*((long	*)(p+3)) = (long)PC;
		SeqStart = CodeBuf;
		seqflg = InstrMeta[0].flags;
		if (d.emu>3) {
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
	if (d.emu>3) {
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
	__asm__	__volatile__ ("
		pushfl\n
		pushl	%%ebx\n
		pushl	%%edi\n
		pushl	%%esi\n
		rdtsc\n
		movl	%7,%%ebx\n
		movl	%%eax,%0\n
		movl	%%edx,%1\n
		pushw	%8\n
		popfw\n
		call	*%9\n
		movl	%%edx,%2\n
		movl	%%eax,%3\n
		rdtsc\n
		movl	%%edi,%6\n
		movl	%%eax,%4\n
		movl	%%edx,%5\n
		popl	%%esi\n
		popl	%%edi\n
		popl	%%ebx\n
		popfl"
		: "=m"(t0.t.tl),"=m"(t0.t.th),"=m"(flg),"=m"(ePC),"=m"(t1.t.tl),"=m"(t1.t.th),"=m"(mem_ref)
		: "m"(ecpu),"2"(flg),"c"(SeqStart)
		: "%eax","%edx","memory" );
	EFLAGS = (flg &	0xcff) | ifl;
	TheCPU.mem_ref = mem_ref;
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
	t1.td -= t0.td;
	TheCPU.EMUtime += t1.td;
	ExecTime += t1.td;
	e_signal_pending = 0;
	if (d.emu>2) {
		e_printf("** End code, PC=%08lx\n",(long)ePC);
		if (d.emu>3) e_printf("\n%s",e_print_regs());
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

