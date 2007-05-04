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

#define TAILSIZE	7
#define TAILFIX		1

/* If you undefine this, in 16-bit stack mode the high 16 bits of ESP
 * will be zeroed after every push/pop operation. There's a small
 * possibility of breaking some code, you can easily figure out how.
 * For 32-bit stacks, keeping ESP is also a waste of time. */
#undef KEEP_ESP

/////////////////////////////////////////////////////////////////////////////

typedef union {
	unsigned int d;
	signed int ds;
	signed char *ps;
	unsigned char *pu;
	signed short *pws;
	unsigned short *pwu;
	signed int *pds;
	unsigned int *pdu;
	float *pff;
	double *pfd;
	struct { unsigned short l,h; } w;
	struct { signed short l,h; } ws;
	struct { unsigned char bl,bh,b2,b3; } b;
	struct { signed char bl,bh,b2,b3; } bs;
} wkreg;

typedef struct {
	int valid, mode;
	int S1,S2;
	wkreg RES;
} flgtmp;

#define V_INVALID	0
#define V_GEN		1	// general/add case
#define V_SUB		2
#define V_SBB		3
#define V_ADC		4

extern wkreg DR1;	// "eax"
extern wkreg DR2;	// "edx"
extern wkreg AR1;	// "edi"
extern wkreg AR2;	// "esi"
extern wkreg SR1;	// "ebp"
extern wkreg TR1;	// "ecx"
extern flgtmp RFL;

extern int TrapVgaOn;

#define GTRACE0(s)		if (debug_level('e')>2) e_printf("(G) %-12s [%s]\n",(s),showmode(mode))
#define GTRACE1(s,r)		if (debug_level('e')>2) e_printf("(G) %-12s %s [%s]\n",(s),\
					showreg(r),showmode(mode))
#define GTRACE2(s,r1,r2)	if (debug_level('e')>2) e_printf("(G) %-12s %s %s [%s]\n",(s),\
					showreg(r1),showreg(r2),showmode(mode))
#define GTRACE3(s,r1,r2,a)	if (debug_level('e')>2) e_printf("(G) %-12s %s %s %08x [%s]\n",(s),\
					showreg(r1),showreg(r2),(int)(a),showmode(mode))
#define GTRACE4(s,r1,r2,a,b)	if (debug_level('e')>2) e_printf("(G) %-12s %s %s %08x %08x [%s]\n",\
					(s),showreg(r1),showreg(r2),(int)(a),(int)(b),showmode(mode))
#define GTRACE5(s,r1,r2,a,b,c)	if (debug_level('e')>2) e_printf("(G) %-12s %s %s %08x %08x %08x [%s]\n",\
					(s),showreg(r1),showreg(r2),(int)(a),(int)(b),(int)(c),showmode(mode))
extern void FlagSync_AP (void);
extern void FlagSync_O (void);
extern void FlagSync_All (void);

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
	unsigned int sp;
	caddr_t addr;
	int v;
	sp = (TheCPU.esp-BT24(BitDATA16, m)) & TheCPU.StackMask;
	addr = (caddr_t)(uintptr_t)(LONG_SS + sp);
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

/////////////////////////////////////////////////////////////////////////////

#endif
