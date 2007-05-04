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
#include "codegen.h"

/////////////////////////////////////////////////////////////////////////////

static void modrm_sibd(unsigned char sib, int mode, unsigned char *base)
{
	int v = FetchL(base);
	if (debug_level('e')>5)
		e_printf("ModRM sibd sib=%02x base=%08lx v=%08x\n",sib,(long)base,v);
	switch(sib) {
		/* 0x	DS: d32 + (index<<0) */
		case 0x05:
		/* 4x	DS: d32 + (index<<1) */
		case 0x45:
		/* 8x	DS: d32 + (index<<2) */
		case 0x85:
		/* cx	DS: d32 + (index<<3) */
		case 0xc5:
			AddrGen(A_DI_2D, mode, v, Ofs_EAX, (sib>>6)&3); break;
		case 0x0d: case 0x4d: case 0x8d: case 0xcd:
			AddrGen(A_DI_2D, mode, v, Ofs_ECX, (sib>>6)&3); break;
		case 0x15: case 0x55: case 0x95: case 0xd5:
			AddrGen(A_DI_2D, mode, v, Ofs_EDX, (sib>>6)&3); break;
		case 0x1d: case 0x5d: case 0x9d: case 0xdd:
			AddrGen(A_DI_2D, mode, v, Ofs_EBX, (sib>>6)&3); break;
		case 0x25:
			AddrGen(A_DI_2D, mode|IMMED, v); break;
		/* no SS: override on index */
		case 0x2d: case 0x6d: case 0xad: case 0xed:
			AddrGen(A_DI_2D, mode, v, Ofs_EBP, (sib>>6)&3); break;
		case 0x35: case 0x75: case 0xb5: case 0xf5: 
			AddrGen(A_DI_2D, mode, v, Ofs_ESI, (sib>>6)&3); break;
		case 0x3d: case 0x7d: case 0xbd: case 0xfd:
			AddrGen(A_DI_2D, mode, v, Ofs_EDI, (sib>>6)&3); break;
		default:   break;	/* ERROR */
	}
}

static void modrm_sib(int sib, int mode)
{
	int oR2;
	if (debug_level('e')>5)
		e_printf("ModRM sib  sib=%02x\n",sib);
	if ((sib&0x38)==0x20) {
		switch (sib) {
			case 0x20: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EAX); return;
			case 0x21: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_ECX); return;
			case 0x22: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EDX); return;
			case 0x23: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EBX); return;
			case 0x24: AddrGen(A_DI_1, mode, OVERR_SS, Ofs_ESP); return;
			case 0x25: AddrGen(A_DI_1, mode, OVERR_SS, Ofs_EBP); return;
			case 0x26: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_ESI); return;
			case 0x27: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EDI); return;
			default: dbug_printf("ModRM sib: invalid %02x\n",sib);
				leavedos(-95);
		}
	}
	switch(D_MO(sib)) {
		case 0: oR2 = Ofs_EAX; break;
		case 1: oR2 = Ofs_ECX; break;
		case 2: oR2 = Ofs_EDX; break;
		case 3: oR2 = Ofs_EBX; break;
		// case 4: special, see above
		case 5: oR2 = Ofs_EBP; break;
		case 6: oR2 = Ofs_ESI; break;
		case 7: default: oR2 = Ofs_EDI; break;
	}
	switch(D_LO(sib)) {
		case 0x00: AddrGen(A_DI_2, mode|RSHIFT, OVERR_DS, Ofs_EAX, oR2, (sib>>6)&3); break;
		case 0x01: AddrGen(A_DI_2, mode|RSHIFT, OVERR_DS, Ofs_ECX, oR2, (sib>>6)&3); break;
		case 0x02: AddrGen(A_DI_2, mode|RSHIFT, OVERR_DS, Ofs_EDX, oR2, (sib>>6)&3); break;
		case 0x03: AddrGen(A_DI_2, mode|RSHIFT, OVERR_DS, Ofs_EBX, oR2, (sib>>6)&3); break;
		case 0x04: AddrGen(A_DI_2, mode|RSHIFT, OVERR_SS, Ofs_ESP, oR2, (sib>>6)&3); break;
		case 0x05: AddrGen(A_DI_2, mode|RSHIFT, OVERR_SS, Ofs_EBP, oR2, (sib>>6)&3); break;
		case 0x06: AddrGen(A_DI_2, mode|RSHIFT, OVERR_DS, Ofs_ESI, oR2, (sib>>6)&3); break;
		case 0x07: AddrGen(A_DI_2, mode|RSHIFT, OVERR_DS, Ofs_EDI, oR2, (sib>>6)&3); break;
	}
}

/////////////////////////////////////////////////////////////////////////////

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

int ModRM(unsigned char opc, unsigned char *PC, int mode)
{
	unsigned char mod,cab=Fetch(PC+1);
	int l=2;

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
	switch (mod) {
	    case 0:
			if (mode & ADDR16) switch (D_LO(cab)) {
				case 0: AddrGen(A_DI_2, mode, OVERR_DS, Ofs_BX, Ofs_SI); break;
				case 1: AddrGen(A_DI_2, mode, OVERR_DS, Ofs_BX, Ofs_DI); break;
				case 2: AddrGen(A_DI_2, mode, OVERR_SS, Ofs_BP, Ofs_SI); break;
				case 3: AddrGen(A_DI_2, mode, OVERR_SS, Ofs_BP, Ofs_DI); break;
				case 4: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_SI); break;
				case 5: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_DI); break;
				case 6: AddrGen(A_DI_0, mode|IMMED, OVERR_DS, (unsigned short)FetchW(PC+2));
					l=4; break;
				case 7: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_BX); break;
			}
			else switch (D_LO(cab)) {
				case 0: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EAX); break;
				case 1: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_ECX); break;
				case 2: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EDX); break;
				case 3: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EBX); break;
				case 4: { 
					unsigned char sib = Fetch(PC+2);
					if (D_LO(sib)==5) {
						modrm_sibd(sib,mode&MLEA,(PC+3)); l=7;
					}
					else {
						modrm_sib(sib,mode&MLEA); l=3;
					} }
					break;
				case 5: AddrGen(A_DI_0, mode|IMMED, OVERR_DS, FetchL(PC+2));
					l=6; break;
				case 6: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_ESI); break;
				case 7: AddrGen(A_DI_1, mode, OVERR_DS, Ofs_EDI); break;
			}
			break;
	    case 1:
	    case 2: {
			int dsp=0;
			if (mode & ADDR16) {
				if (mod==1) { dsp=(signed char)Fetch(PC+2); l=3; }
					else { dsp=(short)FetchW(PC+2); l=4; }
				switch (D_LO(cab)) {
					case 0: AddrGen(A_DI_2, mode|IMMED, OVERR_DS, dsp, Ofs_BX, Ofs_SI); break;
					case 1: AddrGen(A_DI_2, mode|IMMED, OVERR_DS, dsp, Ofs_BX, Ofs_DI); break;
					case 2: AddrGen(A_DI_2, mode|IMMED, OVERR_SS, dsp, Ofs_BP, Ofs_SI); break;
					case 3: AddrGen(A_DI_2, mode|IMMED, OVERR_SS, dsp, Ofs_BP, Ofs_DI); break;
					case 4: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_SI); break;
					case 5: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_DI); break;
					case 6: AddrGen(A_DI_1, mode|IMMED, OVERR_SS, dsp, Ofs_BP); break;
					case 7: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_BX); break;
				}
			}
			else {
				if (D_LO(cab)!=4) {
					if (mod==1) { dsp=(signed char)Fetch(PC+2); l=3; }
						else { dsp=FetchL(PC+2); l=6; }
				}
				switch (D_LO(cab)) {
					case 0: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_EAX); break;
					case 1: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_ECX); break;
					case 2: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_EDX); break;
					case 3: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_EBX); break;
					case 4: { 
						unsigned char sib = Fetch(PC+2);
						modrm_sib(sib, mode&MLEA);
						if (mod==1) { dsp=(signed char)Fetch(PC+3); l=4; }
							else { dsp=FetchL(PC+3); l=7; }
						}
						AddrGen(LEA_DI_R, IMMED, dsp);
						break;
					case 5: AddrGen(A_DI_1, mode|IMMED, OVERR_SS, dsp, Ofs_EBP); break;
					case 6: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_ESI); break;
					case 7: AddrGen(A_DI_1, mode|IMMED, OVERR_DS, dsp, Ofs_EDI); break;
				}
			}
		}
	    break;
	    case 3:
			TheCPU.mode |= RM_REG;
			if (mode & (MBYTE|MBYTX))
				REG3 = R1Tab_b[cab&7];
			else if (mode & ADDR16)
				REG3 = R1Tab_w[cab&7];
			else
				REG3 = R1Tab_l[cab&7];
			AddrGen(LEA_DI_R, 0, REG3);
			break;
	}
	return l;
}


int ModGetReg1(unsigned char *PC, int mode)
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

