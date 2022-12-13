#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <assert.h>

void msdos_api_call(cpuctx_t *scp, void *arg);
void msdos_api_winos2_call(cpuctx_t *scp, void *arg);

struct pmrm_ret msdos_ext_call(cpuctx_t *scp,
	struct RealModeCallStructure *rmreg,
	unsigned short rm_seg, void *(*arg)(int), int off);
struct pext_ret msdos_ext_ret(cpuctx_t *scp,
	const struct RealModeCallStructure *rmreg,
	unsigned short rm_seg, int off);

void callbacks_init(unsigned short rmcb_sel, void *(*cbk_args)(int),
	far_t *r_cbks);
void callbacks_done(far_t *r_cbks);

#ifdef DOSEMU
#define RMREG(r) (rmreg->r)
#define X_RMREG(r) (rmreg->r)
#define RMLWORD(r) LO_WORD_(X_RMREG(e##r), const)
#endif
/* pre_extender() is allowed to read only a small set of rmregs, check mask */
#define READ_RMREG(r, m) (assert(m & (1 << r##_INDEX)), RMREG(r))
#define ip_INDEX eip_INDEX
#define sp_INDEX esp_INDEX
#define flags_INDEX eflags_INDEX
#define RM_LO(r) LO_BYTE_(RMLWORD(r), const)

enum { RMCB_MS, RMCB_PS2MS, MAX_RMCBS };

#endif
