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

/*
 * Why this one? It is only a duplicate of modrm-gen but it interpretes
 * addresses instead of compiling them. After some headaches I decided
 * that it was better to have a duplicate than to parse,compile,execute,
 * find the node,delete it etc.
 *
 */
unsigned long rods, ross;

/////////////////////////////////////////////////////////////////////////////

static void modrm_sibd(unsigned char sib, int mode, unsigned char *base)
{
	long v = FetchL(base) + rods;
	if (debug_level('e')>5)
		e_printf("ModRM sibd sib=%02x base=%08lx v=%08lx\n",sib,(long)base,v);
	switch(sib) {
		/* 0x	DS: d32 + (index<<0) */
		case 0x05:
		/* 4x	DS: d32 + (index<<1) */
		case 0x45:
		/* 8x	DS: d32 + (index<<2) */
		case 0x85:
		/* cx	DS: d32 + (index<<3) */
		case 0xc5:
			MEMREF = v + (rEAX << ((sib>>6)&3)); break;
		case 0x0d: case 0x4d: case 0x8d: case 0xcd:
			MEMREF = v + (rECX << ((sib>>6)&3)); break;
		case 0x15: case 0x55: case 0x95: case 0xd5:
			MEMREF = v + (rEDX << ((sib>>6)&3)); break;
		case 0x1d: case 0x5d: case 0x9d: case 0xdd:
			MEMREF = v + (rEBX << ((sib>>6)&3)); break;
		case 0x25:
			MEMREF = v; break;
		/* no SS: override on index */
		case 0x2d: case 0x6d: case 0xad: case 0xed:
			MEMREF = v + (rEBP << ((sib>>6)&3)); break;
		case 0x35: case 0x75: case 0xb5: case 0xf5: 
			MEMREF = v + (rESI << ((sib>>6)&3)); break;
		case 0x3d: case 0x7d: case 0xbd: case 0xfd:
			MEMREF = v + (rEDI << ((sib>>6)&3)); break;
		default:   break;	/* ERROR */
	}
}

static void modrm_sib(int sib, int mode)
{
	unsigned long oR2;
	if (debug_level('e')>5)
		e_printf("ModRM sib  sib=%02x\n",sib);
	if ((sib&0x38)==0x20) {
		switch (sib) {
			case 0x20: MEMREF = rods + rEAX; return;
			case 0x21: MEMREF = rods + rECX; return;
			case 0x22: MEMREF = rods + rEDX; return;
			case 0x23: MEMREF = rods + rEBX; return;
			case 0x24: MEMREF = ross + rESP; return;
			case 0x25: MEMREF = ross + rEBP; return;
			case 0x26: MEMREF = rods + rESI; return;
			case 0x27: MEMREF = rods + rEDI; return;
			default: dbug_printf("ModRM sib: invalid %02x\n",sib);
				leavedos(-95);
		}
	}
	switch(D_MO(sib)) {
		case 0: oR2 = rEAX; break;
		case 1: oR2 = rECX; break;
		case 2: oR2 = rEDX; break;
		case 3: oR2 = rEBX; break;
		// case 4: special, see above
		case 5: oR2 = rEBP; break;
		case 6: oR2 = rESI; break;
		case 7: default: oR2 = rEDI; break;
	}
	switch(D_LO(sib)) {
		case 0x00: MEMREF = rods + rEAX + (oR2 << ((sib>>6)&3)); break;
		case 0x01: MEMREF = rods + rECX + (oR2 << ((sib>>6)&3)); break;
		case 0x02: MEMREF = rods + rEDX + (oR2 << ((sib>>6)&3)); break;
		case 0x03: MEMREF = rods + rEBX + (oR2 << ((sib>>6)&3)); break;
		case 0x04: MEMREF = ross + rESP + (oR2 << ((sib>>6)&3)); break;
		case 0x05: MEMREF = ross + rEBP + (oR2 << ((sib>>6)&3)); break;
		case 0x06: MEMREF = rods + rESI + (oR2 << ((sib>>6)&3)); break;
		case 0x07: MEMREF = rods + rEDI + (oR2 << ((sib>>6)&3)); break;
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

int ModRMSim(unsigned char *PC, int mode)
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
	rods = (mode&MLEA? 0 : ((DTR *)CPUOFFS(OVERR_DS))->Base);
	ross = (mode&MLEA? 0 : ((DTR *)CPUOFFS(OVERR_SS))->Base);
	switch (mod) {
	    case 0:
			if (mode & ADDR16) switch (D_LO(cab)) {
				case 0: MEMREF = rods + ((rBX+rSI)&0xffff); break;
				case 1: MEMREF = rods + ((rBX+rDI)&0xffff); break;
				case 2: MEMREF = ross + ((rBP+rSI)&0xffff); break;
				case 3: MEMREF = ross + ((rBP+rDI)&0xffff); break;
				case 4: MEMREF = rods + rSI; break;
				case 5: MEMREF = rods + rDI; break;
				case 6: MEMREF = rods + (unsigned short)FetchW(PC+2);
					l=4; break;
				case 7: MEMREF = rods + rBX; break;
			}
			else switch (D_LO(cab)) {
				case 0: MEMREF = rods + rEAX; break;
				case 1: MEMREF = rods + rECX; break;
				case 2: MEMREF = rods + rEDX; break;
				case 3: MEMREF = rods + rEBX; break;
				case 4: { 
					unsigned char sib = Fetch(PC+2);
					if (D_LO(sib)==5) {
						modrm_sibd(sib,mode&MLEA,(PC+3)); l=7;
					}
					else {
						modrm_sib(sib,mode&MLEA); l=3;
					} }
					break;
				case 5: MEMREF = rods + FetchL(PC+2);
					l=6; break;
				case 6: MEMREF = rods + rESI; break;
				case 7: MEMREF = rods + rEDI; break;
			}
			break;
	    case 1:
	    case 2: {
			int dsp=0;
			if (mode & ADDR16) {
				if (mod==1) { dsp=(signed char)Fetch(PC+2); l=3; }
					else { dsp=(short)FetchW(PC+2); l=4; }
				switch (D_LO(cab)) {
					case 0: MEMREF = rods + ((rBX+rSI+dsp)&0xffff); break;
					case 1: MEMREF = rods + ((rBX+rDI+dsp)&0xffff); break;
					case 2: MEMREF = ross + ((rBP+rSI+dsp)&0xffff); break;
					case 3: MEMREF = ross + ((rBP+rDI+dsp)&0xffff); break;
					case 4: MEMREF = rods + ((rSI+dsp)&0xffff); break;
					case 5: MEMREF = rods + ((rDI+dsp)&0xffff); break;
					case 6: MEMREF = ross + ((rBP+dsp)&0xffff); break;
					case 7: MEMREF = rods + ((rBX+dsp)&0xffff); break;
				}
			}
			else {
				if (D_LO(cab)!=4) {
					if (mod==1) { dsp=(signed char)Fetch(PC+2); l=3; }
						else { dsp=FetchL(PC+2); l=6; }
				}
				switch (D_LO(cab)) {
					case 0: MEMREF = rods + rEAX + dsp; break;
					case 1: MEMREF = rods + rECX + dsp; break;
					case 2: MEMREF = rods + rEDX + dsp; break;
					case 3: MEMREF = rods + rEBX + dsp; break;
					case 4: { 
						unsigned char sib = Fetch(PC+2);
						modrm_sib(sib, mode&MLEA);
						if (mod==1) { dsp=(signed char)Fetch(PC+3); l=4; }
							else { dsp=FetchL(PC+3); l=7; }
						}
						MEMREF += dsp;
						break;
					case 5: MEMREF = ross + rEBP + dsp; break;
					case 6: MEMREF = rods + rESI + dsp; break;
					case 7: MEMREF = rods + rEDI + dsp; break;
				}
			}
		}
	    break;
	    case 3:
			if (mode & (MBYTE|MBYTX))
				MEMREF = (unsigned long)CPUOFFS(R1Tab_b[cab&7]);
			else if (mode & ADDR16)
				MEMREF = (unsigned long)CPUOFFS(R1Tab_w[cab&7]);
			else
				MEMREF = (unsigned long)CPUOFFS(R1Tab_l[cab&7]);
			break;
	}
	return l;
}

