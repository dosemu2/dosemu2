#ifndef MSDOSHLP_H
#define MSDOSHLP_H

struct msdos_ops {
    void (*api_call)(struct sigcontext *scp);
    void (*xms_call)(struct RealModeCallStructure *rmreg);
    int (*mouse_callback)(struct sigcontext *scp,
	const struct RealModeCallStructure *rmreg);
    int (*ps2_mouse_callback)(struct sigcontext *scp,
	const struct RealModeCallStructure *rmreg);
    void (*rmcb_handler)(struct RealModeCallStructure *rmreg);
};

void doshlp_init(const struct msdos_ops *ops);

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
extern struct pmaddr_s get_pm_handler(void (*handler)(struct sigcontext *));
extern far_t get_rm_handler(int (*handler)(struct sigcontext *,
	const struct RealModeCallStructure *));
extern far_t get_lr_helper(far_t rmcb);
extern far_t get_lw_helper(far_t rmcb);
extern far_t get_exec_helper(void);
extern struct pmaddr_s get_pmrm_handler(void (*handler)(
	struct RealModeCallStructure *));

#endif
