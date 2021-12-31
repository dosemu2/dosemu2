#ifndef LIO_H
#define LIO_H

void msdos_lr_helper(sigcontext_t *scp, int is_32,
	unsigned short rm_seg, void (*post)(sigcontext_t *));
void msdos_lw_helper(sigcontext_t *scp, int is_32,
	unsigned short rm_seg, void (*post)(sigcontext_t *));
void lio_init(void);

#endif
