#ifndef MSDOSHLP_H
#define MSDOSHLP_H

#ifdef DOSEMU
#include "sig.h"
#endif
#include "cpu.h"
#include "emudpmi.h"

enum MsdOpIds { MSDOS_FAULT, MSDOS_PAGEFAULT, API_CALL, API_WINOS2_CALL,
	MSDOS_LDT_CALL16, MSDOS_LDT_CALL32, MSDOS_EXT_CALL };

enum { MSDOS_NONE, MSDOS_RMINT, MSDOS_RM, MSDOS_PM, MSDOS_DONE };
enum { POSTEXT_NONE, POSTEXT_PUSH };

void msdos_pm_call(sigcontext_t *scp);

struct pmaddr_s get_pmcb_handler(void (*handler)(sigcontext_t *,
	const struct RealModeCallStructure *, int, void *),
	void *arg,
	void (*ret_handler)(sigcontext_t *,
	struct RealModeCallStructure *, int),
	int num);
struct pmaddr_s get_pm_handler(enum MsdOpIds id,
	void (*handler)(sigcontext_t *, void *), void *arg);
struct pmrm_ret {
    int ret;
    far_t faddr;
    int inum;
    DPMI_INTDESC prev;
};
struct pext_ret {
    int ret;
    unsigned arg;
};
struct pmaddr_s get_pmrm_handler_m(enum MsdOpIds id,
	struct pmrm_ret (*handler)(
	sigcontext_t *, struct RealModeCallStructure *,
	unsigned short, void *(*)(int), int),
	void *(*arg)(int),
	struct pext_ret (*ret_handler)(
	sigcontext_t *, const struct RealModeCallStructure *,
	unsigned short, int),
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg, int len, int r_offs[]);
far_t get_exec_helper(void);
far_t get_term_helper(void);

void msdoshlp_init(int (*is_32)(void), int len);

struct dos_helper_s {
    int tid;
    unsigned entry;
    unsigned short (*rm_seg)(sigcontext_t *, int, void *);
    void *rm_arg;
    int e_offs[256];
};
void doshlp_setup_retf(struct dos_helper_s *h,
	const char *name, void (*thr)(void *),
	unsigned short (*rm_seg)(sigcontext_t *, int, void *),
	void *rm_arg);

struct pmaddr_s doshlp_get_entry(struct dos_helper_s *h);

void doshlp_quit_dpmi(sigcontext_t *scp);
int doshlp_idle(void);

#endif
