/***************************************************************************
 * 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
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

#ifndef _EMU86_CODEGEN_X86_H
#define _EMU86_CODEGEN_X86_H

#include "codegen.h"

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

/* ok, this is maybe not the standard way to do a checksum... but
 * it doesn't matter as long as it is used consistently.
 */
static inline unsigned char __fchk(const void *src, size_t n)
{
int d0, d1;
__asm__ __volatile__("
	xorl	%1,%1\n
	jecxz	2f\n
1:	addl	(%4),%1\n
	addl	$4,%4\n		/* first add 4-byte blocks to eax */
	loop	1b\n
2:	testb	$2,%b3\n
	je	3f\n
	addw	(%4),%w1\n	/* add a 2-byte block to ax */
	addl	$2,%4\n
3:	testb	$1,%b3\n
	je	4f\n
	addb	(%4),%b1\n	/* add last byte to al */
4:	shld	$16,%1,%0\n	/* add bits 16-31 into 0-15 */
	addl	%0,%1\n
	addb	%h1,%b1"	/* add bits 8-15 into 0-7 */
	: "=&c" (d0), "=&a" (d1)
	: "0" (n/4), "q" (n), "r" (src)
	: "memory");
return (unsigned char)d1;
}

/////////////////////////////////////////////////////////////////////////////

extern unsigned e_VgaRead(unsigned offs, int mode);
extern void e_VgaWrite(unsigned offs, unsigned u, int mode);
extern int TrapVgaOn;

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

/////////////////////////////////////////////////////////////////////////////

#define GenAddECX(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0xc183,Cp); G1((o),Cp); } else {\
			G2(0xc181,Cp); G4((o),Cp); }

#define GenLeaECX(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0x498d,Cp); G1((o),Cp); } else {\
			G2(0x898d,Cp); G4((o),Cp); }

#define GenLeaEDI(o)	if (((o) > -128) && ((o) < 128)) {\
			G2(0x7f8d,Cp); G1((o),Cp); } else {\
			G2(0xbf8d,Cp); G4((o),Cp); }

#define StackMaskEBP	{G3(0x6b239c,Cp); G1(Ofs_STACKM,Cp); G1(0x9d,Cp);}

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
	
/////////////////////////////////////////////////////////////////////////////
//
// Most-used code generator sequences implemented as macros
//

// 'no-jump' version, straight
#define Gen66(mode, Cp) \
	*(Cp)=OPERoverride; Cp+=BTA(BitDATA16, mode)

// 'no-jump' version, tricky (depends on bit position)
#define G2_4(mode, val, Cp) \
	*((long *)(Cp))=(val); Cp+=BT24(BitDATA16, mode)


// movb offs(%%ebx),%%al	read working reg al from CPU struct
#define GenL_REG_byte(mode, ofs) \
({ \
	register unsigned char *Cp = CodePtr; \
	G2(0x438a,Cp); G1(ofs,Cp); \
	CodePtr = Cp; \
})

// movb %%al,offs(%%ebx)	write working reg al into CPU struct
#define GenS_REG_byte(mode, ofs) \
({ \
	register unsigned char *Cp = CodePtr; \
	G2(0x4388,Cp); G1(ofs,Cp); \
	CodePtr = Cp; \
})

// mov{wl} offs(%%ebx),%%{e}ax	read working reg eax from CPU struct
#define GenL_REG_wl(mode, ofs) \
({ \
	register unsigned char *Cp = CodePtr; \
	Gen66(mode,Cp); \
	G2(0x438b,Cp); G1(ofs,Cp); \
	CodePtr = Cp; \
})

// mov{wl} %%{e}ax,offs(%%ebx)	write working reg eax into CPU struct
#define GenS_REG_wl(mode, ofs) \
({ \
	register unsigned char *Cp = CodePtr; \
	Gen66(mode,Cp); \
	G2(0x4389,Cp); G1(ofs,Cp); \
	CodePtr = Cp; \
})

// movb (%%edi),%%al		read memory into al
#define GenL_DI_R1_byte(mode) \
({ \
	register unsigned char *Cp = CodePtr; \
	TRAPVGAR(Cp,(mode)); \
	G2(0x078a,Cp); \
	CodePtr = Cp; \
})

// mov{wl} (%%edi),%%{e}ax	read memory into eax
#define GenL_DI_R1_wl(mode) \
({ \
	register unsigned char *Cp = CodePtr; \
	TRAPVGAR(Cp,(mode)); \
	Gen66(mode,Cp); \
	G2(0x078b,Cp); \
	CodePtr = Cp; \
})

// movb %%al,(%%edi)		write memory from al
#define GenS_DI_byte(mode) \
({ \
	register unsigned char *Cp = CodePtr; \
	TRAPVGAW(Cp,(mode)); \
	G2(0x0788,Cp); \
	CodePtr = Cp; \
})

// mov{wl} %%{e}ax,(%%edi)	write memory from eax
#define GenS_DI_wl(mode) \
({ \
	register unsigned char *Cp = CodePtr; \
	TRAPVGAW(Cp,(mode)); \
	Gen66(mode,Cp); \
	G2(0x0789,Cp); \
	CodePtr = Cp; \
})

// movb $xx,(%%edi)		write memory from immedb
#define GenS_DI_byte_imm(mode, val) \
({ \
	register unsigned char *Cp = CodePtr; \
	G2(0x07c6,Cp); G1(val,Cp); \
	CodePtr = Cp; \
})

// mov{wl} $xx,(%%edi)		write memory from immed{wl}
#define GenS_DI_wl_imm(mode, val) \
({ \
	register unsigned char *Cp = CodePtr; \
	Gen66(mode,Cp); G2(0x07c7,Cp); G2_4(mode,val,Cp); \
	CodePtr = Cp; \
})

// move register to register
#define GenReg2Reg(rs, rd, mode) \
({ \
	register unsigned char *Cp = CodePtr; \
	if ((mode) & MBYTE) { \
		G2(0x438a,Cp); G1((rs),Cp); \
		G2(0x4388,Cp); G1((rd),Cp); \
	} else { \
		Gen66(mode,Cp); \
		G2(0x438b,Cp); G1((rs),Cp); \
		Gen66(mode,Cp); \
		G2(0x4389,Cp); G1((rd),Cp); \
	} CodePtr = Cp; \
})


/////////////////////////////////////////////////////////////////////////////

#endif
