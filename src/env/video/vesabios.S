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
 * support.
 *
 * Note that some parts of the video BIOS are added by vbe_init()
 * in env/video/vesa.c.
 *
 * /REMARK
 * DANG_END_NODULE
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
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
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
#ifndef NEW_X_CODE
#include "vgaemu.h"
#endif /* NEW_X_CODE */
#ifdef __linux__
#include <linux/linkage.h>
#endif


		.text
#ifndef NEW_X_CODE
.globl CISH(vgaemu_VESA_BIOS_label_start)
#endif /* NEW_X_CODE */

#ifndef NEW_X_CODE
CISH(vgaemu_VESA_BIOS_label_start):

		.org VGAEMU_BIOS_START
		.byte	0x55, 0xaa	/* BIOS signature, required for */
					/* BIOS extentions */
		.byte	0x01		/* BIOS extention length in 
					 * units of 512 bytes 
					 */
#else /* NEW_X_CODE */
		.globl CISH(vgaemu_bios_start)
		.globl CISH(vgaemu_bios_prod_name)
		.globl CISH(vgaemu_bios_win_func)
		.globl CISH(vgaemu_bios_end)

		.org 0

CISH(vgaemu_bios_start):
		.byte	0x55, 0xaa	/* BIOS signature, required for BIOS extensions */
		.byte	0x00		/* BIOS extension length in units of 512 bytes */
#endif /* NEW_X_CODE */
		retf			/* BIOS init function */
		nop
		
#ifndef NEW_X_CODE
		.org VGAEMU_BIOS_VERSIONSTRING
#else /* NEW_X_CODE */
CISH(vgaemu_bios_prod_name):
		.ascii	"DOSEmu VGA/VBE 2.0 Emulator"
		.byte	0x00

#endif /* NEW_X_CODE */
		.ascii	"DOSEMU VGAemu. (C) 1995 1996, "
		.ascii	"I.A. Filius and J.A.K. Mouw"
		.byte	0x00
#ifndef NEW_X_CODE
		
		.org VGAEMU_BIOS_VESASTRING
		.ascii	"DOSEMU VGAemu"
#else /* NEW_X_CODE */

		.ascii	"VESA BIOS Extensions V2.0 for DOSEmu."
		.byte	0x0d, 0x0a
		.ascii	"Copyright (c) 1997 Steffen Winterfeldt"
#endif /* NEW_X_CODE */
		.byte	0x00
#ifndef NEW_X_CODE
		
		.org VGAEMU_BIOS_WINDOWFUNCTION
		mov	ax,#0x4f05	/* This function is the same as
					 * int 0x10, ax=0x4f05, so we just
					 * call it.
					 */
		int	#0x10
#else /* NEW_X_CODE */

CISH(vgaemu_bios_win_func):
		push	bx
		/* 0x40 is our own special code, (cf. vesa.c),
		 * make it 0x00 if you think it causes problems
		 */
		or	bh,#0x40
		mov	ax,#0x4f05
		int 	#0x10
		pop	bx
#endif /* NEW_X_CODE */
		retf
#ifndef NEW_X_CODE
		nop
#endif /* not NEW_X_CODE */

#ifndef NEW_X_CODE
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
#else /* NEW_X_CODE */
CISH(vgaemu_bios_end):

#endif /* NEW_X_CODE */
