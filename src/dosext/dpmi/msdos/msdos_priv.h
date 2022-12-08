#ifndef MSDOS_PRIV_H
#define MSDOS_PRIV_H

#include "emudpmi.h"

unsigned short ConvertSegmentToDescriptor_lim(unsigned short segment,
    unsigned int limit);

dosaddr_t msdos_malloc(unsigned long size);
int msdos_free(unsigned int addr);

int msdos_pre_extender(cpuctx_t *scp,
			struct RealModeCallStructure *rmreg,
			int intr, unsigned short rm_seg,
			int *r_mask, far_t *r_rma);
int msdos_post_extender(cpuctx_t *scp,
			const struct RealModeCallStructure *rmreg,
			int intr, unsigned short rm_seg, int *rmask,
			unsigned *arg);

int msdos_get_int_num(int off);
unsigned short get_scratch_seg(void);
unsigned short scratch_seg(cpuctx_t *scp, int off, void *arg);
far_t get_xms_call(void);
int msdos_is_32(void);

#endif
