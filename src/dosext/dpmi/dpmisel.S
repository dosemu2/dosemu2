/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "emm.h"
#include "macros86.h"

	.section .text

#ifdef __x86_64__
	.globl DPMI_iret
	.type DPMI_iret,@function
DPMI_iret:	
	iretl
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
	.globl	DPMI_return_from_dosint
DPMI_return_from_dosint:
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
	.globl DPMI_sel_end
DPMI_sel_end:

	.globl MSDOS_spm_start
MSDOS_spm_start:
	.globl MSDOS_XMS_call
MSDOS_XMS_call:
	hlt
	.globl MSDOS_spm_end
MSDOS_spm_end:

	.globl MSDOS_epm_start
MSDOS_epm_start:
	.globl MSDOS_XMS_ret
MSDOS_XMS_ret:
	hlt
	.globl MSDOS_epm_end
MSDOS_epm_end:

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
	.globl MSDOS_LDT_call
MSDOS_LDT_call:
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

	.globl DPMI_sel_code_end
DPMI_sel_code_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
