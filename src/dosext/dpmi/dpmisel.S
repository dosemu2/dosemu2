/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "emm.h"
#include "macros86.h"

	.section .text
.code32

	.globl DPMI_sel_code_start
DPMI_sel_code_start:

/* ======================= Addr = dpmi_sel():0000 */
	.globl DPMI_raw_mode_switch_pm
DPMI_raw_mode_switch_pm:
	hlt
/* ======================= Addr = dpmi_sel():0001 */
	.globl DPMI_save_restore_pm
DPMI_save_restore_pm:
	hlt
	lretl
/* ======================= Addr = dpmi_sel():0003 */
	.globl DPMI_API_extension
DPMI_API_extension:
	hlt
	lretl
/* ======================= Addr = dpmi_sel():0005 */
	.globl DPMI_return_from_pm
DPMI_return_from_pm:
	hlt
/* ======================= Addr = dpmi_sel():0006 */
	.globl DPMI_return_from_exception
DPMI_return_from_exception:
	hlt
/* ======================= Addr = dpmi_sel():0007 */
	.globl DPMI_return_from_rm_callback
DPMI_return_from_rm_callback:
	hlt
/* ======================= Addr = dpmi_sel():0008 */
	.globl DPMI_exception
DPMI_exception:
	FILL_OPCODE 32,hlt
/* ======================= Addr = dpmi_sel():0028 */
	.globl DPMI_interrupt
DPMI_interrupt:
	FILL_OPCODE 256,hlt
/* ======================= Addr = dpmi_sel():0128 */
/*	.globl	reserved */
reserved:
	hlt
/* ======================= Addr = dpmi_sel():0129 */
	.globl DPMI_return_from_ext_exception
DPMI_return_from_ext_exception:
	hlt
/* ======================= Addr = dpmi_sel():012A */
	.globl DPMI_return_from_int_1c
DPMI_return_from_int_1c:
	hlt
/* ======================= Addr = dpmi_sel():012B */
	.globl DPMI_return_from_int_23
DPMI_return_from_int_23:
	hlt
/* ======================= Addr = dpmi_sel():012C */
	.globl DPMI_return_from_int_24
DPMI_return_from_int_24:
	hlt
/* ======================= Addr = dpmi_sel():012D */
	.globl DPMI_return_from_RSPcall
DPMI_return_from_RSPcall:
	hlt
/* ======================= Addr = dpmi_sel():012E */
	.globl DPMI_return_from_rm_ext_exception
DPMI_return_from_rm_ext_exception:
	hlt
/* ======================= Addr = dpmi_sel():012F */
	.globl DPMI_return_from_rm_exception
DPMI_return_from_rm_exception:
	hlt
/* ======================= Addr = dpmi_sel():0130 */
	.globl DPMI_ext_exception
DPMI_ext_exception:
	FILL_OPCODE 32,hlt
/* ======================= Addr = dpmi_sel():0150 */
	.globl DPMI_rm_exception
DPMI_rm_exception:
	FILL_OPCODE 32,hlt
/* ======================= Addr = dpmi_sel():0170 */
	.globl DPMI_return_from_LDTcall
DPMI_return_from_LDTcall:
	hlt
/* ======================= Addr = dpmi_sel():0171 */
	.globl DPMI_return_from_LDTExitCall
DPMI_return_from_LDTExitCall:
	hlt
/* ======================= Addr = dpmi_sel():0172 */
	.globl DPMI_reinit
DPMI_reinit:
	hlt
	.globl DPMI_sel_end
/* ======================= Addr = dpmi_sel():0173 */
	.globl DPMI_shlock
DPMI_shlock:
	hlt
	jc 1f  // canceled
	jz 1f  // success
	movl $0x1680, %eax
	int $0x2f
	jmp DPMI_shlock
1:
	lret
	.globl DPMI_sel_end
DPMI_sel_end:

	.globl MSDOS_pmc_start
MSDOS_pmc_start:
	.globl MSDOS_fault
MSDOS_fault:
	hlt
	lretl
	.globl MSDOS_pagefault
MSDOS_pagefault:
	hlt
	lretl
	.globl MSDOS_API_call
MSDOS_API_call:
	hlt
	lretl
	.globl MSDOS_API_WINOS2_call
MSDOS_API_WINOS2_call:
	hlt
	lretl
	.globl MSDOS_LDT_call16
MSDOS_LDT_call16:
	hlt
	lretl
	.globl MSDOS_LDT_call32
MSDOS_LDT_call32:
	hlt
	lretl
	.globl MSDOS_RSP_call16
MSDOS_RSP_call16:
	hlt
	lretl
	.globl MSDOS_RSP_call32
MSDOS_RSP_call32:
	hlt
	lretl

	.globl MSDOS_rmcb_call_start
MSDOS_rmcb_call_start:
	.globl MSDOS_rmcb_call0
MSDOS_rmcb_call0:
	hlt
	.globl MSDOS_rmcb_ret0
MSDOS_rmcb_ret0:
	hlt
	iretl
	.globl MSDOS_rmcb_call1
MSDOS_rmcb_call1:
	hlt
	.globl MSDOS_rmcb_ret1
MSDOS_rmcb_ret1:
	hlt
	iretl
	.globl MSDOS_rmcb_call2
MSDOS_rmcb_call2:
	hlt
	.globl MSDOS_rmcb_ret2
MSDOS_rmcb_ret2:
	hlt
	iretl
	.globl MSDOS_rmcb_call_end
MSDOS_rmcb_call_end:
	.globl MSDOS_hlt_start
MSDOS_hlt_start:
	FILL_OPCODE 64,hlt
	.globl MSDOS_hlt_end
MSDOS_hlt_end:
	.globl MSDOS_pmc_end
MSDOS_pmc_end:

	.globl	DPMI_VXD_start
DPMI_VXD_start:
	.globl	DPMI_VXD_VMM
DPMI_VXD_VMM:
	hlt
	lretl
	.globl	DPMI_VXD_PageFile
DPMI_VXD_PageFile:
	hlt
	lretl
	.globl	DPMI_VXD_Reboot
DPMI_VXD_Reboot:
	hlt
	lretl
	.globl	DPMI_VXD_VDD
DPMI_VXD_VDD:
	hlt
	lretl
	.globl	DPMI_VXD_VMD
DPMI_VXD_VMD:
	hlt
	lretl
	.globl	DPMI_VXD_VXDLDR
DPMI_VXD_VXDLDR:
	hlt
	lretl
	.globl	DPMI_VXD_SHELL
DPMI_VXD_SHELL:
	hlt
	lretl
	.globl	DPMI_VXD_VCD
DPMI_VXD_VCD:
	hlt
	lretl
	.globl	DPMI_VXD_VTD
DPMI_VXD_VTD:
	hlt
	lretl
	.globl	DPMI_VXD_CONFIGMG
DPMI_VXD_CONFIGMG:
	hlt
	lretl
	.globl	DPMI_VXD_ENABLE
DPMI_VXD_ENABLE:
	hlt
	lretl
	.globl	DPMI_VXD_APM
DPMI_VXD_APM:
	hlt
	lretl
	.globl	DPMI_VXD_VTDAPI
DPMI_VXD_VTDAPI:
	hlt
	lretl
	.globl	DPMI_VXD_end
DPMI_VXD_end:

	.globl DPMI_call
DPMI_call:
	int $0x31
	lretl
	.globl DPMI_call_args
DPMI_call_args:
	pushl %ebp
	movl %esp, %ebp
	pushl %ecx
	pushl %edx
	movl %ecx, %edx
	jcxz 2f
1:
	pushw 12-2(%ebp,%ecx,2)
	loop 1b
	movl %edx, %ecx
2:
	int $0x31
	movl -4(%ebp), %ecx
	shll $1, %ecx
	addl %ecx, %esp
	popl %edx
	popl %ecx
	popl %ebp
	lretl
	.globl DPMI_call_args16
DPMI_call_args16:
.code16
	pushw %bp
	movw %sp, %bp
	pushw %cx
	pushw %dx
	pushw %si
	movw %cx, %dx
	jcxz 2f
1:
	movw %cx, %si
	shlw $1, %si
	pushw 6-2(%bp,%si)
	loop 1b
	movw %dx, %cx
2:
	int $0x31
	movw -2(%bp), %cx
	shlw $1, %cx
	addw %cx, %sp
	popw %si
	popw %dx
	popw %cx
	popw %bp
	lretw
.code32
	.globl DPMI_msdos
DPMI_msdos:
	int $0x21
	lretl

.macro vint name inum
	.globl DPMI_\name\()_irq
DPMI_\name\()_irq:
	hlt
	jc 1f
	int $\inum
	# IRQ0 is not requested at that point (its not used at all).
	# We disable interrupts and then allow timer to tick.
	cli
	clc
1:
	.globl DPMI_\name\()_post_irq
DPMI_\name\()_post_irq:
	hlt
	/* DPMI spec suggests STI before IRET.
	 * But if the handler was called by some other handler,
	 * then STI should not be done, as the caller's handler would
	 * then be interrupted.
	 * dosemu2 does not require STI before IRET.
	 * (STI is only required before POPF in dosemu2) */
	iretl
.endm

	vint vtmr 8
	vint vrtc 0x70

	.globl DPMI_sel_code_end
DPMI_sel_code_end:
	.global _start
_start: // dummy

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
