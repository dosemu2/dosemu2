#ifndef LIO_H
#define LIO_H

void msdos_lr_helper(cpuctx_t *scp, int is_32,
	unsigned short rm_seg, void (*post)(cpuctx_t *));
void msdos_lw_helper(cpuctx_t *scp, int is_32,
	unsigned short rm_seg, void (*post)(cpuctx_t *));
void lio_init(void);

#endif
