/*
 * DANG_BEGIN_MODULE
 *
 * The protected mode code for the VESA BIOS Extensions.
 *
 * We define some functions here that are required to support the
 * VBE 2.0 protected mode interface.
 *
 * The whole code _must_ be fully relocatable. It will be copied into
 * the video BIOS during VBE initialization (cf. vbe_init()).
 *
 * DANG_END_NODULE
 *
 *
 * Copyright (c) 1997 Steffen Winterfeldt,
 * email: Steffen.Winterfeldt@itp.uni-leipzig.de
 *
 */

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
#if 1
		pushl %ebx
		movb $0x4000,%ebx
		movl $0x4f05,%eax
		int $0x10
		popl %ebx
		ret
#else
		movb %dl,%ah
		pushl %edx
		movl $0x3d4,%edx
		movb $0xf4,%al
		outw %ax,%dx
		popl %edx
		movl $0x4f,%eax
		ret
#endif

vbpi_display_start:
#if 1
		pushl %ebx
		movl $0x40,%ebx
		movl $0x4f07,%eax
		int $0x10
		popl %ebx
		ret
#else
		pushl %edx
		movl $0x3d4,%edx
		movb $0xf0,%al
		movb %cl,%ah
		outw %ax,%dx
		incb %al
		movb %ch,%ah
		outw %ax,%dx
		incb %al
		popl %ecx
		pushl %ecx
		movb %cl,%ah
		outw %ax,%dx
		incb %al
		movb %ch,%ah
		outw %ax,%dx
		incb %al
		movb %bl,%ah
		outw %ax,%dx
		popl %edx
		movl $0x4f,%eax
		ret
#endif

vbpi_set_palette:
		pushl %ebx
		movb $0,%bl
		movl $0x4f09,%eax
		int $0x10
		popl %ebx
		ret

vgaemu_bios_pm_interface_end:

