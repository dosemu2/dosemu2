#ifndef MSDOS_PRIV_H
#define MSDOS_PRIV_H

#include "emudpmi.h"

unsigned short ConvertSegmentToDescriptor_lim(unsigned short segment,
    unsigned int limit);

void msdos_pre_xms(const sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, unsigned short rm_seg,
	int *r_mask);
void msdos_post_xms(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg, int *r_mask);

unsigned int msdos_malloc(unsigned long size);
int msdos_free(unsigned int addr);

int msdos_pre_extender(sigcontext_t *scp,
			struct RealModeCallStructure *rmreg,
			int intr, unsigned short rm_seg,
			int *r_mask, far_t *r_rma);
int msdos_post_extender(sigcontext_t *scp,
			const struct RealModeCallStructure *rmreg,
			int intr, unsigned short rm_seg, int *rmask,
			unsigned *arg);

int msdos_get_int_num(int off);
unsigned short get_scratch_seg(void);

#endif
