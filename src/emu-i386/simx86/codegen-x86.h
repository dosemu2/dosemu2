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

#ifndef _EMU86_CODEGEN_X86_H
#define _EMU86_CODEGEN_X86_H

#include "codegen.h"
#include "vgaemu.h"

#define HOST_ARCH_X86

#define TAILSIZE	7
#define TAILFIX		1

/* If you undefine this, in 16-bit stack mode the high 16 bits of ESP
 * will be zeroed after every push/pop operation. There's a small
 * possibility of breaking some code, you can easily figure out how.
 * For 32-bit stacks, keeping ESP is also a waste of time. */
#undef KEEP_ESP

/////////////////////////////////////////////////////////////////////////////

extern int TrapVgaOn;

#define GTRACE0(s)
#define GTRACE1(s,a)
#define GTRACE2(s,a,b)
#define GTRACE3(s,a,b,c)
#define GTRACE4(s,a,b,c,d)
#define GTRACE5(s,a,b,c,d,e)

/////////////////////////////////////////////////////////////////////////////

#define STD_WRITE_B	G3M(0x88,0x07,0x90,Cp);
#define STD_WRITE_WL(m)	G3((m)&DATA16?0x078966:0x900789,Cp)

#define GenAddECX(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0xc183,Cp); G1((o),Cp); } else {\
			G2(0xc181,Cp); G4((o),Cp); }

#define GenLeaECX(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0x498d,Cp); G1((o),Cp); } else {\
			G2(0x898d,Cp); G4((o),Cp); }

#define GenLeaEDI(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0x7f8d,Cp); G1((o),Cp); } else {\
			G2(0xbf8d,Cp); G4((o),Cp); }

#define StackMaskEBP	{G2(0x6b23,Cp); G1(Ofs_STACKM,Cp); }

/////////////////////////////////////////////////////////////////////////////

// returns 1(16 bit), 0(32 bit)
#define BTA(bpos, mode) ({ register int temp; \
	__asm__ ("bt	%1,%2\n \
		rcrl	$1,%0\n \
		shrl	$31,%0" \
		: "=&r"(temp) \
		: "i"(bpos), "g"(mode) ); temp; })

// returns 2(16 bit), 4(32 bit)	
#define BT24(bpos, mode) ({ register int temp; \
	__asm__ ("movb	$4,%b0\n \
		bt	%1,%2\n \
		sbbb	$0,%b0\n \
		andl	$6,%0" \
		: "=&q"(temp) \
		: "i"(bpos), "g"(mode) ); temp; })

static __inline__ int FastLog2(register int v)
{
	register int temp;
	__asm__ ("bsr	%1,%0\n \
		jnz	1f\n \
		xor	%0,%0\n \
1: 		" \
		: "=a"(temp) : "g"(v) );
	return temp;
}

/////////////////////////////////////////////////////////////////////////////

static __inline__ void PUSH(int m, void *w)
{
	unsigned long sp;
	caddr_t addr;
	int v;
	sp = (TheCPU.esp-BT24(BitDATA16, m)) & TheCPU.StackMask;
	addr = (caddr_t)(LONG_SS + sp);
	v = e_check_munprotect(addr);
	if (m&DATA16)
		WRITE_WORD(addr, *(short *)w);
	else
		WRITE_DWORD(addr, *(int *)w);
	if (v) e_mprotect(addr, 0);
#ifdef KEEP_ESP
	TheCPU.esp = (sp&TheCPU.StackMask) | (TheCPU.esp&~TheCPU.StackMask);
#else
	TheCPU.esp = sp;
#endif
}

/////////////////////////////////////////////////////////////////////////////

#define FWJ_OFFS	5
// get all flags and let them on the stack
#define PopPushF(Cp)	if (((Cp)==BaseGenBuf)||((Cp)[-1]!=PUSHF)) \
				G2(0x9c9d,(Cp))

// cld; btl $0xa,EFLAGS(%ebx); jnc 1f; std; 1f:
#define GetDF(Cp)		\
  G4M(CLD,TwoByteESC,0xba,0x63,Cp);	\
  G1(Ofs_EFLAGS,Cp);		\
  G4M(0x0a,JNB_JAE,0x01,STD,Cp);

/////////////////////////////////////////////////////////////////////////////
//

// 'no-jump' version, straight
#define Gen66(mode, Cp) \
	*(Cp)=OPERoverride; Cp+=BTA(BitDATA16, mode)

// 'no-jump' version, tricky (depends on bit position)
#define G2_4(mode, val, Cp) \
	*((int *)(Cp))=(val); Cp+=BT24(BitDATA16, mode)


/////////////////////////////////////////////////////////////////////////////

#endif
