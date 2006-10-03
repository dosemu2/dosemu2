/*
 * (C) Copyright 1992, ..., 2006 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include "emm.h"
#include "macros86.h"

	.code32
	.section .text

	.globl DPMI_sel_code_start
DPMI_sel_code_start:

/* ======================= Addr = dpmi_sel():0000 */
        .globl DPMI_direct_transfer
DPMI_direct_transfer:
	lss	(%esp),%esp		/* this is: pop ss; pop esp */
	ljmp	*%cs:0
	.globl DPMI_direct_transfer_end
DPMI_direct_transfer_end:

/* ======================= Addr = dpmi_sel():000b */
	.globl DPMI_raw_mode_switch_pm
DPMI_raw_mode_switch_pm:
	hlt
/* ======================= Addr = dpmi_sel():000c */
	.globl DPMI_save_restore_pm
DPMI_save_restore_pm:
	hlt
	lret
/* ======================= Addr = dpmi_sel():000e */
	.globl DPMI_API_extension
DPMI_API_extension:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0010 */
	.globl DPMI_return_from_pm
DPMI_return_from_pm:
	hlt
/* ======================= Addr = dpmi_sel():0011 */
	.globl DPMI_return_from_exception
DPMI_return_from_exception:
	hlt
/* ======================= Addr = dpmi_sel():0012 */
	.globl DPMI_return_from_rm_callback
DPMI_return_from_rm_callback:
	hlt
/* ======================= Addr = dpmi_sel():0013 */
	.globl DPMI_exception
DPMI_exception:
	FILL_OPCODE 32,hlt
	.globl DPMI_interrupt
/* ======================= Addr = dpmi_sel():0033 */
DPMI_interrupt:
	FILL_OPCODE 256,hlt
	.globl DPMI_return_from_ext_exception
/* ======================= Addr = dpmi_sel():0133 */
DPMI_return_from_ext_exception:
	hlt

/* ======================= Addr = dpmi_sel():0134 */
	.globl DPMI_return_from_int_1c
DPMI_return_from_int_1c:
	hlt
/* ======================= Addr = dpmi_sel():0135 */
	.globl DPMI_return_from_int_23
DPMI_return_from_int_23:
	hlt
/* ======================= Addr = dpmi_sel():0136 */
	.globl DPMI_return_from_int_24
DPMI_return_from_int_24:
	hlt
/* ======================= Addr = dpmi_sel():0137 */
	.globl DPMI_return_from_RSPcall
DPMI_return_from_RSPcall:
	hlt

/* ======================= Addr = dpmi_sel():0138 */
	.globl MSDOS_rrm_start
MSDOS_rrm_start:
	.globl MSDOS_return_from_pm
MSDOS_return_from_pm:
	hlt
/* ======================= Addr = dpmi_sel():0139 */
	.globl MSDOS_rrm_end
MSDOS_rrm_end:

	.globl MSDOS_spm_start
MSDOS_spm_start:
/* ======================= Addr = dpmi_sel():0139 */
	.globl MSDOS_XMS_call
MSDOS_XMS_call:
	hlt
	lret
	.globl MSDOS_spm_end
MSDOS_spm_end:

/* ======================= Addr = dpmi_sel():013b */
	.globl	DPMI_VXD_start
DPMI_VXD_start:
	.globl	DPMI_VXD_VMM
DPMI_VXD_VMM:
	hlt
	lret
/* ======================= Addr = dpmi_sel():013d */
	.globl	DPMI_VXD_PageFile
DPMI_VXD_PageFile:
	hlt
	lret
/* ======================= Addr = dpmi_sel():013f */
	.globl	DPMI_VXD_Reboot
DPMI_VXD_Reboot:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0141 */
	.globl	DPMI_VXD_VDD
DPMI_VXD_VDD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0143 */
	.globl	DPMI_VXD_VMD
DPMI_VXD_VMD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0145 */
	.globl	DPMI_VXD_VXDLDR
DPMI_VXD_VXDLDR:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0147 */
	.globl	DPMI_VXD_SHELL
DPMI_VXD_SHELL:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0149 */
	.globl	DPMI_VXD_VCD
DPMI_VXD_VCD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():014b */
	.globl	DPMI_VXD_VTD
DPMI_VXD_VTD:
	hlt
	lret
/* ======================= Addr = dpmi_sel():014d */
	.globl	DPMI_VXD_CONFIGMG
DPMI_VXD_CONFIGMG:
	hlt
	lret
/* ======================= Addr = dpmi_sel():014f */
	.globl	DPMI_VXD_ENABLE
DPMI_VXD_ENABLE:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0151 */
	.globl	DPMI_VXD_APM
DPMI_VXD_APM:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0153 */
	.globl	DPMI_VXD_VTDAPI
DPMI_VXD_VTDAPI:
	hlt
	lret
/* ======================= Addr = dpmi_sel():0155 */
	.globl	DPMI_VXD_end
DPMI_VXD_end:

	.globl DPMI_sel_code_end
DPMI_sel_code_end:
