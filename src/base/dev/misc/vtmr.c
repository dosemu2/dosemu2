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
#include "hlt.h"
#include "cpu.h"
#include "int.h"
#include "bitops.h"
#include "emu.h"
#include "timers.h"
#include "chipset.h"
#include "vtmr.h"

#define VTMR_FIRST_PORT 0x550
#define VTMR_VPEND_PORT VTMR_FIRST_PORT
#define VTMR_IRR_PORT (VTMR_FIRST_PORT + 2)
#define VTMR_ACK_PORT (VTMR_FIRST_PORT + 3)
#define VTMR_TOTAL_PORTS 4

static uint16_t vtmr_irr;
static uint8_t last_acked;
static uint16_t vtmr_hlt;

struct vthandler {
    void (*handler)(void);
    unsigned flags;
    uint8_t irq;
    uint8_t orig_irq;
};
struct vthandler vth[VTMR_MAX];

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
    if (value >= VTMR_MAX || !vth[value].handler)
        return;
    vtmr_lower(value);
    vth[value].handler();
    last_acked = value;
}

static void half_eoi(void)
{
    port_outb(0xa0, 0x20);
}

static void full_eoi(void)
{
    port_outb(0xa0, 0x20);
    port_outb(0x20, 0x20);
}

static int do_ack(void)
{
    uint8_t irr = port_inb(VTMR_IRR_PORT);
    int timer = find_bit(irr);

    port_outb(VTMR_ACK_PORT, timer);
    return timer;
}

static void do_ret(void)
{
    clear_IF();
    half_eoi();
    do_iret();
}

static int is_masked(int timer, uint8_t *imr)
{
    uint16_t real_imr = (imr[1] << 8) | imr[0];
    return (real_imr & (1 << vth[timer].orig_irq));
}

static void vtmr_handler(uint16_t idx, HLT_ARG(arg))
{
    uint8_t imr[2];
    int timer;

    if (idx == 1) {
        do_ret();
        return;
    }

    imr[0] = port_inb(0x21);
    imr[1] = port_inb(0xa1);
    timer = do_ack();
    if (is_masked(timer, imr)) {
        do_eoi2_iret();
    } else {
        uint8_t inum = port_inb(PIC0_VECBASE_PORT);
        if (vth[timer].flags & VTMR_FLG_TWEAKED) {
            _IP++;  // skip hlt
            fake_int_to(ISEG(inum), IOFF(inum));
        } else {
            half_eoi();
            jmp_to(ISEG(inum), IOFF(inum));
        }
    }
}

int vtmr_pre_irq_dpmi(uint8_t *imr)
{
    int timer = do_ack();
    return is_masked(timer, imr);
}

void vtmr_post_irq_dpmi(int masked)
{
    if (masked)
        full_eoi();
    else
        half_eoi();
}

void vtmr_init(void)
{
    emu_iodev_t io_dev = {};
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;

    io_dev.write_portb = vtmr_ack_write;
    io_dev.read_portb = vtmr_irr_read;
    io_dev.read_portw = vtmr_vpend_read;
    io_dev.start_addr = VTMR_FIRST_PORT;
    io_dev.end_addr = VTMR_FIRST_PORT + VTMR_TOTAL_PORTS;
    io_dev.handler_name = "virtual timer";
    port_register_handler(io_dev, 0);

    hlt_hdlr.name = "vtmr";
    hlt_hdlr.func = vtmr_handler;
    hlt_hdlr.len = 2;
    vtmr_hlt = hlt_register_handler_vm86(hlt_hdlr);
}

void vtmr_reset(void)
{
    vtmr_irr = 0;
    pic_untrigger(pic_irq_list[VTMR_IRQ]);
}

void vtmr_setup(void)
{
    SETIVEC(VTMR_INTERRUPT, BIOS_HLT_BLK_SEG, vtmr_hlt);
}

void vtmr_raise(int timer)
{
    if (timer >= VTMR_MAX || (vtmr_irr & (1 << timer)))
        return;
    vtmr_irr |= (1 << timer);
    pic_request(pic_irq_list[vth[timer].irq]);
}

static void vtmr_lower(int timer)
{
    if (timer >= VTMR_MAX || !(vtmr_irr & (1 << timer)))
        return;
    vtmr_irr &= ~(1 << timer);
    pic_untrigger(pic_irq_list[vth[timer].irq]);
}

void vtmr_register(int timer, void (*handler)(void), unsigned flags)
{
    struct vthandler *vt = &vth[timer];
    assert(timer < VTMR_MAX);
    vt->handler = handler;
    vt->flags = flags;
    vt->irq = VTMR_IRQ;
    vt->orig_irq = 0;
}
