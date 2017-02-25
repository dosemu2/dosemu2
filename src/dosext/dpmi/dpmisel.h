/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef DPMISEL_H
#define DPMISEL_H

#define DPMI_SEL_OFF(x) (x-DPMI_sel_code_start)

#ifdef __x86_64__
extern void		DPMI_iret(void);
#endif

extern unsigned char	DPMI_sel_code_start[];

extern unsigned char	DPMI_save_restore_pm[];
extern unsigned char	DPMI_raw_mode_switch_pm[];
extern unsigned char	DPMI_return_from_rm_callback[];
extern unsigned char	DPMI_VXD_start[];
extern unsigned char	DPMI_VXD_end[];
extern unsigned char	DPMI_return_from_RSPcall[];
extern unsigned char	DPMI_return_from_int_1c[];
extern unsigned char	DPMI_return_from_int_23[];
extern unsigned char	DPMI_return_from_int_24[];
extern unsigned char	DPMI_return_from_rm_callback[];
extern unsigned char	DPMI_return_from_ext_exception[];
extern unsigned char	DPMI_return_from_exception[];
extern unsigned char	DPMI_return_from_pm[];
extern unsigned char	DPMI_API_extension[];
extern unsigned char	DPMI_exception[];
extern unsigned char	DPMI_interrupt[];
extern unsigned char	DPMI_return_from_dosint[];

extern unsigned char	DPMI_VXD_start[];
extern unsigned char	DPMI_VXD_VMM[];
extern unsigned char	DPMI_VXD_PageFile[];
extern unsigned char	DPMI_VXD_Reboot[];
extern unsigned char	DPMI_VXD_VDD[];
extern unsigned char	DPMI_VXD_VMD[];
extern unsigned char	DPMI_VXD_VXDLDR[];
extern unsigned char	DPMI_VXD_SHELL[];
extern unsigned char	DPMI_VXD_VCD[];
extern unsigned char	DPMI_VXD_VTD[];
extern unsigned char	DPMI_VXD_CONFIGMG[];
extern unsigned char	DPMI_VXD_ENABLE[];
extern unsigned char	DPMI_VXD_APM[];
extern unsigned char	DPMI_VXD_VTDAPI[];
extern unsigned char	DPMI_VXD_end[];

extern unsigned char	MSDOS_spm_start[];
extern unsigned char	MSDOS_XMS_call[];
extern unsigned char	MSDOS_spm_end[];
extern unsigned char	MSDOS_epm_start[];
extern unsigned char	MSDOS_XMS_ret[];
extern unsigned char	MSDOS_epm_end[];
extern unsigned char	MSDOS_pmc_start[];
extern unsigned char	MSDOS_API_call[];
extern unsigned char	MSDOS_API_WINOS2_call[];
extern unsigned char	MSDOS_rmcb_call_start[];
extern unsigned char	MSDOS_rmcb_call0[];
extern unsigned char	MSDOS_rmcb_call1[];
extern unsigned char	MSDOS_rmcb_call2[];
extern unsigned char	MSDOS_rmcb_ret0[];
extern unsigned char	MSDOS_rmcb_ret1[];
extern unsigned char	MSDOS_rmcb_ret2[];
extern unsigned char	MSDOS_rmcb_call_end[];
extern unsigned char	MSDOS_pmc_end[];

extern unsigned char	DPMI_sel_code_end[];

#endif
