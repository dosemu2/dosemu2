/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
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

#ifndef __KERNEL__
/* This comes from the kernel, include/asm/string.h */
static inline void * __memcpy(void * to, const void * from, size_t n)
{
int d0, d1, d2;
__asm__ __volatile__(
	"cld\n\t"
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

/////////////////////////////////////////////////////////////////////////////

extern unsigned e_VgaRead(unsigned offs, int mode);
extern void e_VgaWrite(unsigned offs, unsigned u, int mode);
extern int TrapVgaOn;

#define GTRACE0(s)
#define GTRACE1(s,a)
#define GTRACE2(s,a,b)
#define GTRACE3(s,a,b,c)
#define GTRACE4(s,a,b,c,d)
#define GTRACE5(s,a,b,c,d,e)

/////////////////////////////////////////////////////////////////////////////

#define STD_WRITE_B	G2(0x0788,Cp);G3(0x909090,Cp)
#define STD_WRITE_WL(m)	G5((m)&DATA16?0x90078966:0x90900789,0x90,Cp)

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
	v = e_munprotect(addr, 0);
	if (m&DATA16)
		*((short *)addr) = *((short *)w);
	else
		*((int *)addr) = *((int *)w);
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

#define GetStackDF(Cp)	G8(0x24ba0ffc,0x01730a24,(Cp));G1(0xfd,(Cp))

/////////////////////////////////////////////////////////////////////////////
//

// 'no-jump' version, straight
#define Gen66(mode, Cp) \
	*(Cp)=OPERoverride; Cp+=BTA(BitDATA16, mode)

// 'no-jump' version, tricky (depends on bit position)
#define G2_4(mode, val, Cp) \
	*((long *)(Cp))=(val); Cp+=BT24(BitDATA16, mode)


/////////////////////////////////////////////////////////////////////////////

#endif
