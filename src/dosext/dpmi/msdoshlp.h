#ifndef MSDOSHLP_H
#define MSDOSHLP_H

enum MsdOpIds { NONE, API_CALL, XMS_CALL, MOUSE_CB, PS2MOUSE_CB, RMCB };

extern int msdos_pre_rm(struct sigcontext *scp,
	const struct RealModeCallStructure *rmreg);
extern void msdos_post_rm(struct sigcontext *scp,
	struct RealModeCallStructure *rmreg);
extern int msdos_pre_pm(struct sigcontext *scp,
	struct RealModeCallStructure *rmreg);
extern void msdos_post_pm(struct sigcontext *scp,
	const struct RealModeCallStructure *rmreg);
extern void msdos_pm_call(struct sigcontext *scp, int is_32);

extern far_t allocate_realmode_callback(void (*handler)(
	struct RealModeCallStructure *));
extern int free_realmode_callback(u_short seg, u_short off);
extern struct pmaddr_s get_pm_handler(enum MsdOpIds id,
	void (*handler)(struct sigcontext *));
extern struct pmaddr_s get_pmrm_handler(enum MsdOpIds id,
	void (*handler)(struct RealModeCallStructure *));
extern far_t get_rm_handler(enum MsdOpIds id,
	int (*handler)(struct sigcontext *,
	const struct RealModeCallStructure *));
extern far_t get_lr_helper(far_t rmcb);
extern far_t get_lw_helper(far_t rmcb);
extern far_t get_exec_helper(void);

#endif
