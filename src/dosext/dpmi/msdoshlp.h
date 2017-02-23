#ifndef MSDOSHLP_H
#define MSDOSHLP_H

enum MsdOpIds { NONE, API_CALL, API_WINOS2_CALL, XMS_CALL };

extern int msdos_pre_pm(struct sigcontext *scp,
	struct RealModeCallStructure *rmreg);
extern void msdos_pm_call(struct sigcontext *scp, int is_32);

int allocate_realmode_callbacks(void (*handler[])(struct sigcontext *,
	const struct RealModeCallStructure *, int, void *),
	void *(*args)(int),
	void (*ret_handler[])(struct sigcontext *,
	struct RealModeCallStructure *, int),
	int num, unsigned short rmcb_sel, far_t *r_cbks);
extern void free_realmode_callbacks(far_t *r_cbks, int num);
extern struct pmaddr_s get_pm_handler(enum MsdOpIds id,
	void (*handler)(struct sigcontext *, void *), void *arg);
extern struct pmaddr_s get_pmrm_handler(enum MsdOpIds id,
	void (*handler)(struct RealModeCallStructure *, void *), void *arg);
extern far_t get_lr_helper(far_t rmcb);
extern far_t get_lw_helper(far_t rmcb);
extern far_t get_exec_helper(void);

#endif
