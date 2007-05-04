/* 
 * Copyright (c) 1997 Steffen Winterfeldt (original code)
 * email: Steffen Winterfeldt <wfeldt@suse.de>
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The protected mode code for the VESA BIOS Extensions.
 *
 * We define some functions here that are required to support the
 * VBE 2.0 protected mode interface.
 *
 * The whole code _must_ be fully relocatable. It will be copied into
 * the video BIOS during VBE initialization (cf. vbe_init()).
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1998/12/14: Another attempt to fix the pm version of the
 * VBE set palette function. Removed some old code blocks.
 * -- sw
 *
 * DANG_END_CHANGELOG
 *
 *
 */


	.code32

/*
 * We simply call our own BIOS via int 0x10, ax = 0x4fxx but use
 * subfunction codes (typically in BH or BL) of 0x40 instead of 0x00 to
 * allow some nice DEBUG messages (cf. env/video/vesa.c). If you think
 * this causes problems, use 0x00 instead.
 */

		.globl vgaemu_bios_pm_interface
		.globl vgaemu_bios_pm_interface_end


vgaemu_bios_pm_interface:
		.short vbpi_win_func - vgaemu_bios_pm_interface
		.short vbpi_display_start - vgaemu_bios_pm_interface
		.short vbpi_set_palette - vgaemu_bios_pm_interface
		.short 0	/* list of needed memory areas & ports; 0 -> none */
		/* Note that some applications will ignore the PM interface
		 * if you actually put in a nonempty list!
		 */

vbpi_win_func:
		pushl %ebx
		movl $0x4000,%ebx
		movl $0x4f05,%eax
		int $0x10
		popl %ebx
		ret

vbpi_display_start:
		pushl %ebx
		movl $0x40,%ebx
		movl $0x4f07,%eax
		int $0x10
		popl %ebx
		ret

vbpi_set_palette:
		pushl %ebp
		pushl %esi
		pushl %ebx
		movb $0x40,%bl
		movl $0x4f09,%eax
		movl %edi,%esi
		shrl $0x10,%esi
		movw %es,%bp
		int $0x10
		popl %ebx
		popl %esi
		popl %ebp
		ret

vgaemu_bios_pm_interface_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
