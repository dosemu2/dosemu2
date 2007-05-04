/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
 * 2001/01/17: Converted to gas. Bart Oldeman <bart.oldeman@bristol.ac.uk>.
 *
 * DANG_END_CHANGELOG
 *
 *
 * We must compile this with gas/ld as follows:
 *   gcc -E  vesabios.S >vesabios.s
 *   as -o vesabios.o vesabios.s
 *
 */

#define __ASM__
#include "memory.h"

.code16
.text
	.globl	_start16
_start16:

		.globl CISH(vgaemu_bios_start)
		.globl CISH(vgaemu_bios_prod_name)
		.globl CISH(vgaemu_bios_win_func)
		.globl CISH(vgaemu_bios_end)

CISH(vgaemu_bios_start):
		.byte	0x55, 0xaa	/* BIOS signature, required for BIOS extensions */
		.byte	0x00		/* BIOS extension length in units of 512 bytes */
		lret			/* BIOS init function */
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
		pushw	%bx
		/* 0x40 is our own special code, (cf. vesa.c),
		 * make it 0x00 if you think it causes problems
		 */
		orb	$0x40,%bh
		mov	$0x4f05,%ax
		int 	$0x10
		popw	%bx
		lret

CISH(vgaemu_bios_end):

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
