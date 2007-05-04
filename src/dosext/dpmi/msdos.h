/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* 	MS-DOS API translator for DOSEMU\'s DPMI Server
 *
 * DANG_BEGIN_MODULE msdos.h
 *
 * REMARK
 * MS-DOS API translator allows DPMI programs to call DOS service directly
 * in protected mode.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * First Attempted by Dong Liu,  dliu@rice.njit.edu
 *
 */

#ifndef __MSDOS_H__
#define __MSDOS_H__

#define MSDOS_MAX_MEM_ALLOCS 1024

struct msdos_struct {
  int is_32;
  struct pmaddr_s mouseCallBack, PS2mouseCallBack; /* user\'s mouse routine */
  far_t XMS_call;
  /* used when passing a DTA higher than 1MB */
  unsigned short user_dta_sel;
  unsigned long user_dta_off;
  unsigned short user_psp_sel;
  unsigned short current_psp;
  unsigned short lowmem_seg;
  char ems_map_buffer[PAGE_MAP_SIZE];
  int ems_frame_mapped;
  dpmi_pm_block mem_map[MSDOS_MAX_MEM_ALLOCS];
};

extern void msdos_init(int is_32, unsigned short mseg,
    unsigned short psp);
extern void msdos_done(void);
extern int msdos_get_lowmem_size(void);
extern int msdos_pre_extender(struct sigcontext_struct *scp, int intr);
extern int msdos_post_extender(struct sigcontext_struct *scp, int intr);
extern int msdos_fault(struct sigcontext_struct *scp);
extern int msdos_pre_rm(struct sigcontext_struct *scp);
extern void msdos_post_rm(struct sigcontext_struct *scp);
extern int msdos_pre_pm(struct sigcontext_struct *scp);
extern void msdos_post_pm(struct sigcontext_struct *scp);

#define MSDOS_DONE 1
#define MSDOS_ALT_ENT 2

#define DTA_Para_ADD 0
#define DTA_Para_SIZE 8
#define EXEC_Para_ADD (DTA_Para_ADD + DTA_Para_SIZE)
#define EXEC_Para_SIZE 30

#endif
