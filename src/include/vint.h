#ifndef VINT_H
#define VINT_H

void vint_init(void);
void vint_setup(void);
int vint_is_masked(int vi_num, uint8_t *imr);
void vint_post_irq_dpmi(int vi_num, int masked);
int vint_register(void (*handler)(void), int irq, int orig_irq, int inum);
void vint_set_tweaked(int vi_num, int on, unsigned flags);

#endif
