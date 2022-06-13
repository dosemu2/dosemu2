#ifndef VTMR_H
#define VTMR_H

enum { VTMR_PIT, VTMR_RTC, VTMR_MAX };

void vtmr_init(void);
void vtmr_done(void);
void vtmr_reset(void);
void vtmr_raise(int vtmr_num);
void vtmr_register(int timer, int (*handler)(int));
void vtmr_set_tweaked(int timer, int on, unsigned flags);

int vtmr_pre_irq_dpmi(uint8_t *imr);
void vtmr_post_irq_dpmi(int masked);
int vrtc_pre_irq_dpmi(uint8_t *imr);
void vrtc_post_irq_dpmi(int masked);

#define VTMR_IRQ 0xb
#define VTMR_INTERRUPT 0x73

#define VRTC_IRQ 0xe
#define VRTC_INTERRUPT 0x76

#endif
