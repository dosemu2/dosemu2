#ifndef VINT_H
#define VINT_H

void vint_init(void);
void vint_setup(void);
int vint_is_masked(int vi_num, uint8_t *imr);
void vint_post_irq_dpmi(int vi_num, int masked);
int vint_register(void (*handler)(int), int irq, int orig_irq, int inum);
void vint_set_tweaked(int vi_num, int on, unsigned flags);

#define VTMR_IRQ 0xb
#define VTMR_INTERRUPT 0x73

#define VRTC_IRQ 0xe
#define VRTC_INTERRUPT 0x76

int vrtc_pre_irq_dpmi(uint8_t *imr);
void vrtc_post_irq_dpmi(int masked);

#endif
