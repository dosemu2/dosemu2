/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "emm.h"
#include "macros86.h"

	.section .text

	.globl DPMI_direct_transfer
DPMI_direct_transfer:
	/* Now put ESP to new context and pop it up */
#ifdef __x86_64__
	.type DPMI_direct_transfer,@function
	mov	%rdx,%rsp	/* rsp=scp */
	add	$8*8,%rsp       /* skip r8-r15 */
	pop	%rdi
	pop	%rsi
	pop	%rbp
	pop	%rbx
	pop	%rdx
	pop	%rax
	pop	%rcx
	pop	%rsp		/* => iret_frame */

	.globl DPMI_iret
	.type DPMI_iret,@function
DPMI_iret:	  
	iretl

	.code32
#else
	movl	%edx,%esp	/* esp=scp */
	pop	%gs
	pop	%fs
	pop	%es
	pop	%ds
	popa
	addl	$4*4,%esp
	popfl
	lss	(%esp),%esp		/* this is: pop ss; pop esp */
	ljmp	*%cs:0
	.globl DPMI_direct_transfer_end
DPMI_direct_transfer_end:

	.globl DPMI_indirect_transfer
	.type DPMI_indirect_transfer,@function
DPMI_indirect_transfer:
	hlt
#endif

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
	lret
/* ======================= Addr = dpmi_sel():0003 */
	.globl DPMI_API_extension
DPMI_API_extension:
	hlt
	lret
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
	.globl DPMI_interrupt
/* ======================= Addr = dpmi_sel():0028 */
DPMI_interrupt:
	FILL_OPCODE 256,hlt
	.globl DPMI_return_from_ext_exception
/* ======================= Addr = dpmi_sel():0128 */
DPMI_return_from_ext_exception:
	hlt

/* ======================= Addr = dpmi_sel():0129 */
	.globl DPMI_return_from_int_1c
DPMI_return_from_int_1c:
	hlt
/* ======================= Addr = dpmi_sel():012A */
	.globl DPMI_return_from_int_23
DPMI_return_from_int_23:
	hlt
/* ======================= Addr = dpmi_sel():012B */
	.globl DPMI_return_from_int_24
DPMI_return_from_int_24:
	hlt
/* ======================= Addr = dpmi_sel():012C */
	.globl DPMI_return_from_RSPcall
DPMI_return_from_RSPcall:
	hlt

/* ======================= Addr = dpmi_sel():012D */
	.globl MSDOS_rrm_start
MSDOS_rrm_start:
	.globl MSDOS_return_from_pm
MSDOS_return_from_pm:
	hlt
/* ======================= Addr = dpmi_sel():012E */
	.globl MSDOS_rrm_end
MSDOS_rrm_end:

	.globl MSDOS_spm_start
MSDOS_spm_start:
/* ======================= Addr = dpmi_sel():012E */
	.globl MSDOS_XMS_call
MSDOS_XMS_call:
	hlt
	lret
	.globl MSDOS_spm_end
MSDOS_spm_end:

/* ======================= Addr = dpmi_sel():0130 */
	.globl	DPMI_VXD_start
DPMI_VXD_start:
	.globl	DPMI_VXD_VMM
DPMI_VXD_VMM:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0132 */
	.globl	DPMI_VXD_PageFile
DPMI_VXD_PageFile:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0134 */
	.globl	DPMI_VXD_Reboot
DPMI_VXD_Reboot:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0136 */
	.globl	DPMI_VXD_VDD
DPMI_VXD_VDD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0138 */
	.globl	DPMI_VXD_VMD
DPMI_VXD_VMD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():013A */
	.globl	DPMI_VXD_VXDLDR
DPMI_VXD_VXDLDR:
	hlt
	lret
/* ======================= Addr = dpmi_sel():013C */
	.globl	DPMI_VXD_SHELL
DPMI_VXD_SHELL:
	hlt
	lret
/* ======================= Addr = dpmi_sel():013E */
	.globl	DPMI_VXD_VCD
DPMI_VXD_VCD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0140 */
	.globl	DPMI_VXD_VTD
DPMI_VXD_VTD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0142 */
	.globl	DPMI_VXD_CONFIGMG
DPMI_VXD_CONFIGMG:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0144 */
	.globl	DPMI_VXD_ENABLE
DPMI_VXD_ENABLE:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0146 */
	.globl	DPMI_VXD_APM
DPMI_VXD_APM:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0148 */
	.globl	DPMI_VXD_VTDAPI
DPMI_VXD_VTDAPI:
	hlt
	lret
/* ======================= Addr = dpmi_sel():014A */
	.globl	DPMI_VXD_end
DPMI_VXD_end:

	.globl DPMI_sel_code_end
DPMI_sel_code_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
