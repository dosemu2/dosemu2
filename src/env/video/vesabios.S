/*
 * vesabios.S
 *
 * This is vgaemu's VESA BIOS (C000:0 .. C800:0)
 *
 * Copyright (C) 1995, Erik Mouw and Arjan Filius
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
 * The VESA information comes from Finn Thoergersen's VGADOC3, available
 * at every Simtel mirror in vga/vgadoc3.zip, and in the dosemu directory at 
 * tsx-11.mit.edu.
 *
 * DANG_BEGIN_MODULE
 *
 * The VESA BIOS for vgaemu.
 *
 * DANG_END_NODULE
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
#include "vgaemu_inside.h"
#ifdef __linux__
#include <linux/linkage.h>
#endif


.text


.globl CISH(vgaemu_VESA_BIOS_label_start)


CISH(vgaemu_VESA_BIOS_label_start):

		.org VGAEMU_BIOS_START
		.byte	0x55, 0xaa	/* BIOS signature, required for */
					/* BIOS extentions */
		.byte	0x01		/* BIOS extention length in 
					 * units of 512 bytes 
					 */
		retf			/* BIOS init function */
		nop
		
		.org VGAEMU_BIOS_VERSIONSTRING
		.ascii	"DOSEMU vgaemu. (C) 1995, "
		.ascii	"I.A. Filius and J.A.K. Mouw"
		.byte	0x00
		
		.org VGAEMU_BIOS_VESASTRING
		.ascii	"DOSEMU vgaemu"
		.byte	0x00
		
		.org VGAEMU_BIOS_WINDOWFUNCTION
		mov	ax,0x4f05	/* This function is the same as
					 * int 0x10, ax=0x4f05, so we just
					 * call it.
					 */
		int	0x10
		retf
		nop

		.org VGAEMU_BIOS_VESA_MODELIST
		.word 0x0100	/*  640x400x256 */
		.word 0x0101	/*  640x480x256 */
		.word 0x0103	/*  800x600x256 */
		.word 0x0105	/* 1024x768x256 */
		.word 0x0108	/*        80x60 */
		.word 0x0109	/*       132x25 */
		.word 0x010a	/*       132x43 */
		.word 0x010c	/*       132x60 */
		.word 0xffff	/*  end of list */

		.org 0x1ff
.globl CISH(vgaemu_VESA_BIOS_label_end)
CISH(vgaemu_VESA_BIOS_label_end):
		.byte	0x00
