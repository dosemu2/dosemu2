/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#define DPMI_SEL_OFF(x) (x-DPMI_sel_code_start)

#ifdef __x86_64__
extern int		DPMI_direct_transfer(void);
extern void		DPMI_iret(void);
#else
extern unsigned char	DPMI_direct_transfer[];
extern int		DPMI_indirect_transfer(void);
#endif
extern unsigned char	DPMI_direct_transfer_end[];

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
extern unsigned char	MSDOS_rrm_start[];
extern unsigned char	MSDOS_return_from_pm[];
extern unsigned char	MSDOS_rrm_end[];

extern unsigned char	DPMI_sel_code_end[];
