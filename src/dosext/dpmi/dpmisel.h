/*
 * (C) Copyright 1992, ..., 2005 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#define DPMI_SEL_OFF(x) ((char *)(x)-(char *)DPMI_sel_code_start)

void		DPMI_sel_code_start(void);

void		DPMI_direct_transfer(void);
void		DPMI_direct_transfer_end(void);
void		DPMI_save_restore_pm(void);
void		DPMI_raw_mode_switch_pm(void);
void		DPMI_return_from_rm_callback(void);
void		DPMI_VXD_start(void);
void		DPMI_VXD_end(void);
void		DPMI_return_from_RSPcall(void);
void		DPMI_return_from_int_1c(void);
void		DPMI_return_from_int_23(void);
void		DPMI_return_from_int_24(void);
void		DPMI_return_from_rm_callback(void);
void		DPMI_return_from_ext_exception(void);
void		DPMI_return_from_exception(void);
void		DPMI_return_from_pm(void);
void		DPMI_API_extension(void);
void		DPMI_exception(void);
void		DPMI_interrupt(void);

void		DPMI_VXD_start(void);
void		DPMI_VXD_VMM(void);
void		DPMI_VXD_PageFile(void);
void		DPMI_VXD_Reboot(void);
void		DPMI_VXD_VDD(void);
void		DPMI_VXD_VMD(void);
void		DPMI_VXD_VXDLDR(void);
void		DPMI_VXD_SHELL(void);
void		DPMI_VXD_VCD(void);
void		DPMI_VXD_VTD(void);
void		DPMI_VXD_CONFIGMG(void);
void		DPMI_VXD_ENABLE(void);
void		DPMI_VXD_APM(void);
void		DPMI_VXD_VTDAPI(void);
void		DPMI_VXD_end(void);

void		MSDOS_spm_start(void);
void		MSDOS_XMS_call(void);
void		MSDOS_spm_end(void);
void		MSDOS_rrm_start(void);
void		MSDOS_return_from_pm(void);
void		MSDOS_rrm_end(void);

void		DPMI_sel_code_end(void);
