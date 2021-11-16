#ifndef MSDOSHLP_H
#define MSDOSHLP_H

enum MsdOpIds { MSDOS_FAULT, MSDOS_PAGEFAULT, API_CALL, API_WINOS2_CALL,
	MSDOS_LDT_CALL16, MSDOS_LDT_CALL32, XMS_CALL };

int msdos_pre_pm(int offs, const sigcontext_t *scp,
	struct RealModeCallStructure *rmreg);
void msdos_post_pm(int offs, sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg);
void msdos_pm_call(sigcontext_t *scp, int is_32);

struct pmaddr_s get_pmcb_handler(void (*handler)(sigcontext_t *,
	const struct RealModeCallStructure *, int, void *),
	void *(*arg)(int),
	void (*ret_handler)(sigcontext_t *,
	struct RealModeCallStructure *, int),
	int num);
struct pmaddr_s get_pm_handler(enum MsdOpIds id,
	void (*handler)(sigcontext_t *, void *), void *(*arg)(void));
struct pmaddr_s get_pmrm_handler(enum MsdOpIds id, void (*handler)(
	const sigcontext_t *, struct RealModeCallStructure *, void *),
	void *(*arg)(void),
	void (*ret_handler)(
	sigcontext_t *, const struct RealModeCallStructure *));
far_t get_lr_helper(far_t rmcb);
far_t get_lw_helper(far_t rmcb);
far_t get_exec_helper(void);

void msdoshlp_init(int (*is_32)(void));

#endif
