#ifndef VTMR_H
#define VTMR_H

enum { VTMR_PIT, VTMR_MAX };

void vtmr_init(void);
void vtmr_reset(void);
void vtmr_setup(void);
void vtmr_raise(int vtmr_num);
int vtmr_pre_irq_dpmi(uint8_t *imr);
void vtmr_post_irq_dpmi(int masked);
void vtmr_register(int timer, void (*handler)(void),
        int (*is_masked)(uint8_t *), unsigned flags);

#define VTMR_IRQ 0xb
#define VTMR_INTERRUPT 0x73

#define VTMR_FLG_TWEAKED 1

#endif
