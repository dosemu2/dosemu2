#ifndef QEMU_IRQ_H
#define QEMU_IRQ_H

/* Generic IRQ/GPIO pin infrastructure.  */

#define TYPE_IRQ "irq"

typedef struct IRQState *qemu_irq;
typedef void (*qemu_irq_handler)(void *opaque, int n, int level);
struct IRQState {
    qemu_irq_handler handler;
    void *opaque;
    int n;
};

void qemu_set_irq(qemu_irq irq, int level);

static inline void qemu_irq_raise(qemu_irq irq)
{
    qemu_set_irq(irq, 1);
}

static inline void qemu_irq_lower(qemu_irq irq)
{
    qemu_set_irq(irq, 0);
}

static inline void qemu_irq_pulse(qemu_irq irq)
{
    qemu_set_irq(irq, 1);
    qemu_set_irq(irq, 0);
}

#endif
