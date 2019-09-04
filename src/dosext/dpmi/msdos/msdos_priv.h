#ifndef MSDOS_PRIV_H
#define MSDOS_PRIV_H

unsigned short ConvertSegmentToDescriptor_lim(unsigned short segment,
    unsigned int limit);

void msdos_pre_xms(const sigcontext_t *scp,
	struct RealModeCallStructure *rmreg, int *r_mask);
void msdos_post_xms(sigcontext_t *scp,
	const struct RealModeCallStructure *rmreg, int *r_mask);

unsigned int msdos_malloc(unsigned long size);
int msdos_free(unsigned int addr);

#endif
