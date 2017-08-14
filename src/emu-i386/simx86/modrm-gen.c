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
#include "codegen.h"

static char R1Tab_b[8] =
	{ Ofs_AL, Ofs_CL, Ofs_DL, Ofs_BL, Ofs_AH, Ofs_CH, Ofs_DH, Ofs_BH };
static char R1Tab_w[8] =
	{ Ofs_AX, Ofs_CX, Ofs_DX, Ofs_BX, Ofs_SP, Ofs_BP, Ofs_SI, Ofs_DI };
static char R1Tab_seg[8] =
	{ Ofs_ES, Ofs_CS, Ofs_SS, Ofs_DS, Ofs_FS, Ofs_GS, 0xff, 0xff };
static char R1Tab_xseg[8] =
	{ Ofs_XES, Ofs_XCS, Ofs_XSS, Ofs_XDS, Ofs_XFS, Ofs_XGS, 0xff, 0xff };
static char R1Tab_l[8] =
	{ Ofs_EAX, Ofs_ECX, Ofs_EDX, Ofs_EBX, Ofs_ESP, Ofs_EBP, Ofs_ESI, Ofs_EDI };
static char R1Tab_wb[8] =
	{ Ofs_BX, Ofs_BX, Ofs_BP, Ofs_BP, Ofs_SI, Ofs_DI, Ofs_BP, Ofs_BX };
static char R1Tab_wi[8] =
	{ Ofs_SI, Ofs_DI, Ofs_SI, Ofs_DI, Ofs_SP, Ofs_SP, Ofs_SP, Ofs_SP };

/////////////////////////////////////////////////////////////////////////////

static void modrm_sib(unsigned char mod, unsigned char sib, unsigned int base)
{
	if (mod == 0 && D_LO(sib) == 5) {
		int v = FetchL(base);
		e_printf("ModRM sibd sib=%02x base=%08x v=%08x\n",sib,base,v);
	}
	else {
		e_printf("ModRM sib  sib=%02x\n",sib);
	}
}

/////////////////////////////////////////////////////////////////////////////

int _ModRM(unsigned char opc, unsigned int PC, int mode)
{
	unsigned char mod,cab=Fetch(PC+1);
	int l=2;
	int dsp, base, index, shift;

	if (!(mode&NOFLDR)) {
		mod = D_MO(cab);
		if (mode & MBYTE)		// not for movx
			REG1 = R1Tab_b[mod];
		else if (mode & SEGREG) {
			REG1 = R1Tab_seg[mod];
			SBASE = R1Tab_xseg[mod];
		}
		else
			REG1 = (mode&ADDR16? R1Tab_w[mod]:R1Tab_l[mod]);
	}
	mod = D_HO(cab);
	if (mod == 3) {
		TheCPU.mode |= RM_REG;
		if (mode & (MBYTE|MBYTX))
			REG3 = R1Tab_b[cab&7];
		else if (mode & ADDR16)
			REG3 = R1Tab_w[cab&7];
		else
			REG3 = R1Tab_l[cab&7];
		if (mode & MLOAD)
			Gen(L_REG, mode, REG3);	// mov al,[ebx+reg]
		else if (mode & MSTORE)
			Gen(S_REG, mode, REG3);	// mov [ebx+reg],al
		return l;
	}
	dsp = shift = 0;
	if (mode & ADDR16) {
		base = R1Tab_wb[D_LO(cab)];
		index = R1Tab_wi[D_LO(cab)];
		if (mod == 2 || (mod == 0 && D_LO(cab) == 6)) {
			dsp=(unsigned short)FetchW(PC+l); l+=2;
		}
	}
	else {
		base = R1Tab_l[D_LO(cab)];
		index = Ofs_ESP;
		if (base == Ofs_ESP) {
			unsigned char sib = Fetch(PC+l); l++;
			base = R1Tab_l[D_LO(sib)];
			index = R1Tab_l[D_MO(sib)];
			shift = D_HO(sib);
			if (debug_level('e')>5)
				modrm_sib(mod, sib, PC+l);
		}
		if (mod == 2 || (mod == 0 && base == Ofs_EBP)) {
			dsp=FetchL(PC+l); l+=4;
		}
	}
	if (mod == 0 && l > 3) {
		if (index == Ofs_ESP)
			AddrGen(A_DI_0, mode|IMMED, OVERR_DS, dsp);
		else
			/* no SS: override on index */
			AddrGen(A_DI_2D, mode, dsp, index, shift);
	}
	else {
		int overr = (base == Ofs_ESP || base == Ofs_EBP) ?
			OVERR_SS : OVERR_DS;
		if (mod==1) {
			dsp=(signed char)Fetch(PC+l); l++;
		}
		if (index == Ofs_ESP)
			AddrGen(A_DI_1, mode|IMMED, overr, dsp, base);
		else
			AddrGen(A_DI_2, mode|IMMED, overr, dsp, base,
				index, shift);
	}
	if (mode & MLOAD)
		Gen(L_DI_R1, mode);		// mov al,[edi]
	else if (mode & MSTORE)
		Gen(S_DI, mode);		// mov [edi],al
	return l;
}


int ModGetReg1(unsigned int PC, int mode)
{
	unsigned char mod,cab=Fetch(PC+1);

	mod = D_MO(cab);
	if (mode & MBYTE)		// not for movx
		REG1 = R1Tab_b[mod];
	else if (mode & SEGREG) {
		REG1 = R1Tab_seg[mod];
		SBASE = R1Tab_xseg[mod];
	}
	else
		REG1 = (mode&ADDR16? R1Tab_w[mod]:R1Tab_l[mod]);
	mod = D_HO(cab);
	if (mod==3) {
		TheCPU.mode |= RM_REG;
		if (mode & MBYTE)
			REG3 = R1Tab_b[cab&7];
		else if (mode & ADDR16)
			REG3 = R1Tab_w[cab&7];
		else
			REG3 = R1Tab_l[cab&7];
	}
	return mod;
}

