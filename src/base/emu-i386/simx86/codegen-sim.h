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

#ifndef _EMU86_CODEGEN_SIM_H
#define _EMU86_CODEGEN_SIM_H

#include "vgaemu.h"

/////////////////////////////////////////////////////////////////////////////

typedef union {
	unsigned int d;
	signed int ds;
	struct { unsigned short l,h; } w;
	struct { signed short l,h; } ws;
	struct { unsigned char bl,bh,b2,b3; } b;
	struct { signed char bl,bh,b2,b3; } bs;
} wkreg;

typedef struct {
	uint32_t cout, res;
} flgtmp;

/* same constants as used in Bochs and QEMU */
#define LF_BIT_PD	2	/* lazy Parity Delta, same bit as PF */
#define LF_BIT_AF	3	/* lazy Adjust flag */
#define LF_BIT_SD	7	/* lazy Sign Flag Delta, same bit as SF */
#define LF_BIT_CF	31	/* lazy Carry Flag */
#define LF_BIT_PO	30	/* lazy Partial Overflow = CF ^ OF */
#define LF_BIT_RES_SF	31	/* lazy Partial Sign Flag in result, SF ^ SD */

#define LF_MASK_SD	(1U << LF_BIT_SD)
#define LF_MASK_AF	(1U << LF_BIT_AF)
#define LF_MASK_PD	(1U << LF_BIT_PD)
#define LF_MASK_CF	(1U << LF_BIT_CF)
#define LF_MASK_PO	(1U << LF_BIT_PO)

extern wkreg AR1;	// "edi"

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
extern dosaddr_t AddrGen_sim(const IGen *IG);
extern void InitGen_sim(void);

/////////////////////////////////////////////////////////////////////////////

#endif
