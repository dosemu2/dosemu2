/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Purpose: virtual interrupt router (another one)
 *
 * Author: Stas Sergeev.
 *
 */
#include <stdint.h>
#include <assert.h>
#include "cpu.h"
#include "int.h"
#include "hlt.h"
#include "memory.h"
#include "port.h"
#include "chipset.h"
#include "emu.h"
#include "vint.h"

#define VINT_MAX 2
static int vi_used;
static uint16_t vint_hlt;

/* TODO: use PIC's poll mode and remove this masking hack */
#define ON_PIC1(n) (vih[n].orig_irq >= 8)
#define IMR1_MASK(n) (1 << (vih[n].irq - 8))

struct vihandler {
    void (*handler)(int, int);
    void (*mask)(int, int);
    uint8_t irq;
    uint8_t orig_irq;
    uint8_t interrupt;
    int tweaked;
    unsigned tw_flags;
};
struct vihandler vih[VINT_MAX];

static void half_eoi(void)
{
    port_outb(0xa0, 0x20);
}

static void full_eoi(void)
{
    port_outb(0xa0, 0x20);
    port_outb(0x20, 0x20);
}

static void do_ret(int vi_num)
{
    clear_IF();
    vih[vi_num].mask(vi_num, 0);
    do_iret();
}

int vint_is_masked(int vi_num, uint8_t *imr)
{
    uint16_t real_imr = (imr[1] << 8) | imr[0];
    return !!(real_imr & (1 << vih[vi_num].orig_irq));
}

static void vint_handler(uint16_t idx, HLT_ARG(arg))
{
    uint8_t imr[2];
    int masked;
    int vi_num = idx >> 1;

    if (idx & 1) {
        do_ret(vi_num);
        return;
    }

    imr[0] = port_inb(0x21);
    imr[1] = port_inb(0xa1);
    masked = vint_is_masked(vi_num, imr);
    if (masked) {
        do_eoi2_iret();
    } else {
        uint8_t irq = vih[vi_num].orig_irq;
        uint16_t port = (irq >= 8 ? PIC1_VECBASE_PORT : PIC0_VECBASE_PORT);
        uint8_t inum = port_inb(port) + (irq & 7);
        if (!ON_PIC1(vi_num))
            half_eoi();
        if (vih[vi_num].tweaked) {
            _IP++;  // skip hlt
            real_run_int(inum);
            vih[vi_num].mask(vi_num, 1);
        } else {
            jmp_to(ISEG(inum), IOFF(inum));
        }
    }

    if (vih[vi_num].handler)
        vih[vi_num].handler(vi_num, masked);
}

void vint_post_irq_dpmi(int vi_num, int masked)
{
    if (masked)
        full_eoi();
    else if (!ON_PIC1(vi_num))
        half_eoi();
}

void vint_init(void)
{
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;

    hlt_hdlr.name = "vint";
    hlt_hdlr.func = vint_handler;
    hlt_hdlr.len = VINT_MAX * 2;
    vint_hlt = hlt_register_handler_vm86(hlt_hdlr);
}

void vint_setup(void)
{
    int i;

    for (i = 0; i < VINT_MAX; i++) {
        if (vih[i].interrupt)
            SETIVEC(vih[i].interrupt, BIOS_HLT_BLK_SEG, vint_hlt + 2 * i);
    }
}

int vint_register(void (*ack_handler)(int, int),
                  void (*mask_handler)(int, int),
                  int irq, int orig_irq, int inum)
{
    struct vihandler *vi = &vih[vi_used];
    assert(vi_used < VINT_MAX);
    vi->handler = ack_handler;
    vi->mask = mask_handler;
    vi->irq = irq;
    vi->orig_irq = orig_irq;
    vi->interrupt = inum;
    return vi_used++;
}

void vint_set_tweaked(int vi_num, int on, unsigned flags)
{
    struct vihandler *vi = &vih[vi_num];
    assert(vi_num < VINT_MAX);
    vi->tweaked = on;
    vi->tw_flags = flags;
}
