#ifndef HW_I8259_H
#define HW_I8259_H

#include <stdint.h>

typedef uint32_t hwaddr;

/* i8259.c */

typedef struct PICCommonState PICCommonState;
int pic_get_output(PICCommonState *s);
int pic_read_irq(PICCommonState *s);

void qemu_pic_reset(PICCommonState *s);
void pic_set_irq(PICCommonState *s, int irq, int level);
void pic_ioport_write(PICCommonState *s, hwaddr addr64,
                      uint64_t val64, unsigned size);
uint64_t pic_ioport_read(PICCommonState *s, hwaddr addr, unsigned size);
void elcr_ioport_write(PICCommonState *s, hwaddr addr,
                       uint64_t val, unsigned size);
uint64_t elcr_ioport_read(PICCommonState *s, hwaddr addr, unsigned size);

#endif
