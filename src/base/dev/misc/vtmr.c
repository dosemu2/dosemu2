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
#include "emu.h"
#include "timers.h"
#include "chipset.h"
#include "vint.h"
#include "vtmr.h"

#define VTMR_FIRST_PORT 0x550
#define VTMR_VPEND_PORT VTMR_FIRST_PORT
#define VTMR_IRR_PORT (VTMR_FIRST_PORT + 2)
#define VTMR_ACK_PORT (VTMR_FIRST_PORT + 3)
#define VTMR_TOTAL_PORTS 4

static uint16_t vtmr_irr;
static uint8_t last_acked;

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
    return timer_get_vpend(last_acked);
}

static void vtmr_ack_write(ioport_t port, Bit8u value)
{
    int masked = (value >> 7) & 1;
    int timer = value & 0x7f;

    if (timer >= VTMR_MAX)
        return;
    vtmr_lower(timer);
    if (vth[timer].handler)
        vth[timer].handler(masked);
    h_printf("vtmr: ACK on %i, irr=%x\n", timer, vtmr_irr);
    last_acked = timer;
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

int vtmr_pre_irq_dpmi(uint8_t *imr)
{
    int masked = vint_is_masked(vth[VTMR_PIT].vint, imr);
    do_ack(VTMR_PIT, masked);
    return masked;
}

void vtmr_post_irq_dpmi(int masked)
{
    vint_post_irq_dpmi(vth[VTMR_PIT].vint, masked);
}

int vrtc_pre_irq_dpmi(uint8_t *imr)
{
    int masked = vint_is_masked(vth[VTMR_RTC].vint, imr);
    do_ack(VTMR_RTC, masked);
    return masked;
}

void vrtc_post_irq_dpmi(int masked)
{
    vint_post_irq_dpmi(vth[VTMR_RTC].vint, masked);
}

void vtmr_init(void)
{
    emu_iodev_t io_dev = {};

    io_dev.write_portb = vtmr_ack_write;
    io_dev.read_portb = vtmr_irr_read;
    io_dev.read_portw = vtmr_vpend_read;
    io_dev.start_addr = VTMR_FIRST_PORT;
    io_dev.end_addr = VTMR_FIRST_PORT + VTMR_TOTAL_PORTS - 1;
    io_dev.handler_name = "virtual timer";
    port_register_handler(io_dev, 0);
}

void vtmr_reset(void)
{
    int i;

    vtmr_irr = 0;
    for (i = 0; i < VTMR_MAX; i++)
      pic_untrigger(pic_irq_list[vip[i].irq]);
}

void vtmr_raise(int timer)
{
    if (timer >= VTMR_MAX)
        return;
    assert(!(vtmr_irr & (1 << timer)));
    vtmr_irr |= (1 << timer);
    pic_request(pic_irq_list[vip[timer].irq]);
}

static void vtmr_lower(int timer)
{
    if (timer >= VTMR_MAX || !(vtmr_irr & (1 << timer)))
        return;
    vtmr_irr &= ~(1 << timer);
    pic_untrigger(pic_irq_list[vip[timer].irq]);
}

void vtmr_register(int timer, void (*handler)(int))
{
    struct vthandler *vt = &vth[timer];
    struct vint_presets *vp = &vip[timer];
    assert(timer < VTMR_MAX);
    vt->handler = handler;
    vt->vint = vint_register(ack_handler, vp->irq, vp->orig_irq,
                             vp->interrupt);
}

void vtmr_set_tweaked(int timer, int on, unsigned flags)
{
    vint_set_tweaked(vth[timer].vint, on, flags);
}
