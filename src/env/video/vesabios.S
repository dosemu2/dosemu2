/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * vesabios.S
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The VGA/VESA BIOS for VGAEmu.
 *
 * This is what DOS applications see as their video BIOS. It will be
 * mapped at 0xc000:0x0000 and typically be less than one page in size.
 * This video BIOS is write-protected, write accesses to this memory
 * are emulated by vesa_emu_fault() in env/video/vesa.c.
 *
 * It doesn't have much functionality, just stores the VESA mode table
 * and the protected mode interface necessary for VESA BIOS Extension (VBE)
 * support and the VGA fonts.
 *
 * Note that the video BIOS is extended by vbe_init() in env/video/vesa.c.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * This is vgaemu's VESA BIOS (C000:0 .. C800:0)
 *
 * Copyright (C) 1995 1996, Erik Mouw and Arjan Filius
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * email: J.A.K.Mouw@et.tudelft.nl, I.A.Filius@et.tudelft.nl
 *
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1997/07/07: Did some cleaning up and removed lots of #define's. Some
 * parts are now dynamically configurable; they are set up in vbe_init()
 * in env/video/vesa.c.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * DANG_END_CHANGELOG
 *
 *
 * We must compile this with as86/ld86 as follows:
 *   gcc -E  vesabios.S | ../tools/tools86 -E >vesabios.s
 *   as86 -0 -w -g -o vesabios.o vesabios.s
 *   ld86 -0 -r -o vesabios vesabios.o
 *   ../tools/tools86 vesabios
 *   mv -f vesabios vesabios.o
 *
 * We also need to use the macro JMPL (label) if we want have a 16-bit
 * displacement. As86 is not able to do so without the -j switch,
 * and the -j switch also make all conditional jmp's 5 byte long.
 * See file macros86.h for details.
 *
 * NOTE: be careful with overlapping ".org",
 *       or ld86 will then reach your disk quotas !
 *
 */

#define __ASM__
#include "memory.h"
#include "macros86.h"
#include "config.h"
#ifdef __linux__
#include <linux/linkage.h>
#endif


		.text
		.globl CISH(vgaemu_bios_start)
		.globl CISH(vgaemu_bios_prod_name)
		.globl CISH(vgaemu_bios_win_func)
		.globl CISH(vgaemu_bios_end)

		.org 0

CISH(vgaemu_bios_start):
		.byte	0x55, 0xaa	/* BIOS signature, required for BIOS extensions */
		.byte	0x00		/* BIOS extension length in units of 512 bytes */
		retf			/* BIOS init function */
		nop
		
CISH(vgaemu_bios_prod_name):
		.ascii	"DOSEMU VGA/VBE 2.0 Emulator"
		.byte	0x00

		.ascii	"DOSEMU VGAemu. (C) 1995 1996, "
		.ascii	"I.A. Filius and J.A.K. Mouw"
		.byte	0x00

		.ascii	"VESA BIOS Extensions V2.0 for DOSEMU."
		.byte	0x0d, 0x0a
		.ascii	"Copyright (c) 1997 Steffen Winterfeldt"
		.byte	0x00

CISH(vgaemu_bios_win_func):
		push	bx
		/* 0x40 is our own special code, (cf. vesa.c),
		 * make it 0x00 if you think it causes problems
		 */
		or	bh,#0x40
		mov	ax,#0x4f05
		int 	#0x10
		pop	bx
		retf

CISH(vgaemu_bios_end):

