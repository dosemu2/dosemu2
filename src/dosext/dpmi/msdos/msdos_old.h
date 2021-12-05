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

#ifndef MSDOS_H
#define MSDOS_H

int msdos_pre_extender_old(sigcontext_t *scp, int intr,
	struct RealModeCallStructure *rmreg, int *r_mask,
	u_char *stk, int stk_len, int *r_stk_used);
extern void msdos_post_extender_old(sigcontext_t *scp, int intr,
	const struct RealModeCallStructure *rmreg);

#endif
