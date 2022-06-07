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
 * Purpose: virtual timer device extension
 *
 * Author: Stas Sergeev.
 *
 */
#include <stdint.h>
#include <assert.h>
#include "port.h"
#include "pic.h"
#include "cpu.h"
#include "int.h"
#include "coopth.h"
#include "bitops.h"
#include "emu.h"
#include "timers.h"
#include "chipset.h"
#include "vint.h"
#include "vtmr.h"

#define VTMR_FIRST_PORT 0x550
#define VTMR_VPEND_PORT VTMR_FIRST_PORT
#define VTMR_IRR_PORT (VTMR_FIRST_PORT + 2)
#define VTMR_ACK_PORT (VTMR_FIRST_PORT + 3)
#define VTMR_REQUEST_PORT (VTMR_FIRST_PORT + 4)
#define VTMR_MASK_PORT (VTMR_FIRST_PORT + 5)
#define VTMR_UNMASK_PORT (VTMR_FIRST_PORT + 6)
#define VTMR_TOTAL_PORTS 7

static uint16_t vtmr_irr;
static uint16_t vtmr_imr;
static uint16_t vtmr_pirr;
static uint8_t last_acked;
static int smi_tid;

struct vthandler {
    void (*handler)(int);
    int vint;
};
struct vthandler vth[VTMR_MAX];

struct vint_presets {
    uint8_t irq;
    uint8_t orig_irq;
    uint8_t interrupt;
};
static struct vint_presets vip[VTMR_MAX] = {
    [VTMR_PIT] = { .irq = VTMR_IRQ, .orig_irq = 0,
            .interrupt = VTMR_INTERRUPT },
    [VTMR_RTC] = { .irq = VRTC_IRQ, .orig_irq = 8,
            .interrupt = VRTC_INTERRUPT },
};

static void vtmr_lower(int timer);

static Bit8u vtmr_irr_read(ioport_t port)
{
    return vtmr_irr;
}

static Bit16u vtmr_vpend_read(ioport_t port)
{
    Bit16u ret = vtmr_pirr;
    vtmr_pirr = 0;
    return ret;
}

static void post_req(void *arg)
{
    int timer = (uintptr_t)arg;

    if (vth[timer].handler)
        vth[timer].handler(0);
    h_printf("vtmr: post-REQ on %i, irr=%x\n", timer, vtmr_irr);
}

static void vtmr_io_write(ioport_t port, Bit8u value)
{
    int masked = (value >> 7) & 1;
    int timer = value & 0x7f;
    uint16_t msk = 1 << timer;

    if (timer >= VTMR_MAX)
        return;
    switch (port) {
    case VTMR_REQUEST_PORT:
        vtmr_pirr &= ~msk;
        if (!masked) {
            vtmr_irr |= msk;
            if (!(vtmr_imr & msk))
                pic_request(vip[timer].irq);
        } else {
            pic_untrigger(vip[timer].orig_irq);
            pic_request(vip[timer].orig_irq);
            coopth_add_post_handler(post_req, (void *)(uintptr_t)timer);
        }
        h_printf("vtmr: REQ on %i, irr=%x, masked=%i\n", timer, vtmr_irr,
                masked);
        break;
    case VTMR_MASK_PORT:
        if (!(vtmr_imr & msk)) {
            vtmr_imr |= msk;
            if (vtmr_irr & msk)
                pic_untrigger(vip[timer].irq);
        }
        break;
    case VTMR_UNMASK_PORT:
        if (vtmr_imr & msk) {
            vtmr_imr &= ~msk;
            if (vtmr_irr & msk)
                pic_request(vip[timer].irq);
        }
        break;
    case VTMR_ACK_PORT:
        vtmr_lower(timer);
        if (vth[timer].handler)
            vth[timer].handler(masked);
        h_printf("vtmr: ACK on %i, irr=%x\n", timer, vtmr_irr);
        last_acked = timer;
        break;
    }

}

static void do_ack(int timer, int masked)
{
    port_outb(VTMR_ACK_PORT, timer | (masked << 7));
}

static void ack_handler(int vint, int masked)
{
    int i;

    for (i = 0; i < VTMR_MAX; i++) {
        if (vth[i].vint == vint) {
            do_ack(i, masked);
            break;
        }
    }
}

static void do_mask(int timer)
{
    port_outb(VTMR_MASK_PORT, timer);
}

static void do_unmask(int timer)
{
    port_outb(VTMR_UNMASK_PORT, timer);
}

static void mask_handler(int vint, int masked)
{
    int i;

    for (i = 0; i < VTMR_MAX; i++) {
        if (vth[i].vint == vint) {
            if (masked)
                do_mask(i);
            else
                do_unmask(i);
            break;
        }
    }
}

int vtmr_pre_irq_dpmi(uint8_t *imr)
{
    int masked = vint_is_masked(vth[VTMR_PIT].vint, imr);
    do_mask(VTMR_PIT);
    do_ack(VTMR_PIT, masked);
    vint_post_irq_dpmi(vth[VTMR_PIT].vint, masked);
    return masked;
}

void vtmr_post_irq_dpmi(int masked)
{
    do_unmask(VTMR_PIT);
}

int vrtc_pre_irq_dpmi(uint8_t *imr)
{
    int masked = vint_is_masked(vth[VTMR_RTC].vint, imr);
    do_mask(VTMR_RTC);
    do_ack(VTMR_RTC, masked);
    vint_post_irq_dpmi(vth[VTMR_RTC].vint, masked);
    return masked;
}

void vrtc_post_irq_dpmi(int masked)
{
    do_unmask(VTMR_RTC);
}

static int vtmr_is_masked(int timer)
{
    uint8_t imr[2] = { [0] = port_inb(0x21), [1] = port_inb(0xa1) };
    uint16_t real_imr = (imr[1] << 8) | imr[0];
    return ((imr[0] & 4) || !!(real_imr & (1 << vip[timer].irq)));
}

static void vtmr_smi(void *arg)
{
    int timer;
    uint16_t pirr = port_inw(VTMR_VPEND_PORT);

    while ((timer = find_bit(pirr)) != -1) {
        int masked = vtmr_is_masked(timer);
        pirr &= ~(1 << timer);
        port_outb(VTMR_REQUEST_PORT, timer | (masked << 7));
        if (!masked)
            port_outb(0x4d2, 1);  // set fake IRR
    }
}

void vtmr_init(void)
{
    emu_iodev_t io_dev = {};

    io_dev.write_portb = vtmr_io_write;
    io_dev.read_portb = vtmr_irr_read;
    io_dev.read_portw = vtmr_vpend_read;
    io_dev.start_addr = VTMR_FIRST_PORT;
    io_dev.end_addr = VTMR_FIRST_PORT + VTMR_TOTAL_PORTS - 1;
    io_dev.handler_name = "virtual timer";
    port_register_handler(io_dev, 0);

    smi_tid = coopth_create("vtmr smi", vtmr_smi);
    coopth_set_ctx_handlers(smi_tid, sig_ctx_prepare, sig_ctx_restore, NULL);
}

void vtmr_reset(void)
{
    int i;

    vtmr_irr = 0;
    vtmr_imr = 0;
    vtmr_pirr = 0;
    for (i = 0; i < VTMR_MAX; i++)
      pic_untrigger(vip[i].irq);
}

void vtmr_raise(int timer)
{
    uint16_t pirr = vtmr_pirr;

    if (timer >= VTMR_MAX || ((vtmr_pirr | vtmr_irr) & (1 << timer)))
        return;
    vtmr_pirr |= (1 << timer);
    if (!pirr)
        coopth_start(smi_tid, NULL);
}

static void vtmr_lower(int timer)
{
    if (timer >= VTMR_MAX || !(vtmr_irr & (1 << timer)))
        return;
    vtmr_irr &= ~(1 << timer);
    pic_untrigger(vip[timer].irq);
}

void vtmr_register(int timer, void (*handler)(int))
{
    struct vthandler *vt = &vth[timer];
    struct vint_presets *vp = &vip[timer];
    assert(timer < VTMR_MAX);
    vt->handler = handler;
    vt->vint = vint_register(ack_handler, mask_handler, vp->irq,
                             vp->orig_irq, vp->interrupt);
}

void vtmr_set_tweaked(int timer, int on, unsigned flags)
{
    vint_set_tweaked(vth[timer].vint, on, flags);
}
