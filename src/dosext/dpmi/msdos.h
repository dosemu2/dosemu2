/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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

/* used when passing a DTA higher than 1MB */
extern unsigned short USER_DTA_SEL;
extern unsigned short USER_PSP_SEL;
extern unsigned short CURRENT_PSP;
extern unsigned short CURRENT_ENV_SEL;

extern int msdos_pre_extender(struct sigcontext_struct *scp, int intr);
extern void msdos_post_extender(int intr);
extern int msdos_fault(struct sigcontext_struct *scp);
extern void msdos_post_exec(void);


