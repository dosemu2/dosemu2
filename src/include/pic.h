#ifndef PIC_H
#define PIC_H

#include "types.h"

Bit8u pic0_get_base(void);
Bit8u pic1_get_base(void);
unsigned pic_get_isr(void);
                                       /* set function and interrupt vector */
void pic_request(int inum);                            /* interrupt trigger */
void pic_untrigger(int inum);                          /* interrupt untrigger */
int pic_pending(void);			/* inform caller if interrupt is pending */
int pic_irq_active(int num);
int pic_get_inum(void);

extern void pic_reset(void);
extern void pic_init(void);

#endif	/* PIC_H */
