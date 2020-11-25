#ifndef MSDOSHLP_H
#define MSDOSHLP_H

enum MsdOpIds { MSDOS_FAULT, API_CALL, API_WINOS2_CALL, XMS_CALL };

extern int msdos_pre_pm(int offs, const sigcontext_t *scp,
	struct RealModeCallStructure *rmreg);
extern void msdos_post_pm(int offs, sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg);
extern void msdos_pm_call(sigcontext_t *scp, int is_32);

extern struct pmaddr_s get_pmcb_handler(void (*handler)(sigcontext_t *,
	const struct RealModeCallStructure *, int, void *),
	void *arg,
	void (*ret_handler)(sigcontext_t *,
	struct RealModeCallStructure *, int),
	int num);
extern struct pmaddr_s get_pm_handler(enum MsdOpIds id,
	void (*handler)(sigcontext_t *, void *), void *arg);
extern struct pmaddr_s get_pmrm_handler(enum MsdOpIds id, void (*handler)(
	const sigcontext_t *, struct RealModeCallStructure *, void *),
	void *arg,
	void (*ret_handler)(
	sigcontext_t *, const struct RealModeCallStructure *));
extern far_t get_lr_helper(far_t rmcb);
extern far_t get_lw_helper(far_t rmcb);
extern far_t get_exec_helper(void);

#endif
