/*
 * QEMU 8259 - common bits of emulated and KVM kernel model
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 * Copyright (c) 2011      Jan Kiszka, Siemens AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "utilities.h"
#include "i8259.h"
#include "i8259_internal.h"

static int irq_level[16];
static uint64_t irq_count[16];

void pic_reset_common(PICCommonState *s)
{
    s->last_irr = 0;
    s->irr &= s->elcr;
    s->imr = 0;
    s->isr = 0;
    s->priority_add = 0;
    s->irq_base = 0;
    s->read_reg_select = 0;
    s->poll = 0;
    s->special_mask = 0;
    s->init_state = 0;
    s->auto_eoi = 0;
    s->rotate_on_auto_eoi = 0;
    s->special_fully_nested_mode = 0;
    s->init4 = 0;
    s->single_mode = 0;
    /* Note: ELCR is not reset */
}

void pic_stat_update_irq(int irq, int level)
{
    if (level != irq_level[irq]) {
        irq_level[irq] = level;
        if (level == 1) {
            irq_count[irq]++;
        }
    }
}

bool pic_get_statistics(PICCommonState *s,
                        uint64_t **irq_counts, unsigned int *nb_irqs)
{
    if (s->master) {
        *irq_counts = irq_count;
        *nb_irqs = ARRAY_SIZE(irq_count);
    } else {
        *irq_counts = NULL;
        *nb_irqs = 0;
    }

    return true;
}
