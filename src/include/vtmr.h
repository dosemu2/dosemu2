#ifndef VTMR_H
#define VTMR_H

void vtmr_init(void);
void vtmr_reset(void);
void vtmr_setup(void);
void vtmr_raise(int vtmr_num);
void vtmr_pre_irq_dpmi(void);
void vtmr_post_irq_dpmi(int masked);

#define VTMR_IRQ 0xb
#define VTMR_INTERRUPT 0x73

#endif
