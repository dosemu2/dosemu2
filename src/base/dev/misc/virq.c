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
 * Purpose: virtual IRQ router
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
#include "virq.h"

#define VIRQ_IRR_PORT 0x50a
#define VIRQ_HWC_PORT (VIRQ_IRR_PORT + 2)
#define VIRQ_TOTAL_PORTS 3
#define VIRQ_IRQ_NUM 0xa

static uint16_t virq_irr;
static uint16_t virq_hlt;

struct vhandler_s {
  enum VirqHwRet (*hw_handler)(void *);
  enum VirqSwRet (*sw_handler)(void *);
  void *arg;
};
struct vhandler_s vhandlers[VIRQ_MAX];

static void virq_lower(int virq_num);

static Bit16u virq_irr_read(ioport_t port)
{
    return virq_irr;
}

static void virq_hwc_write(ioport_t port, Bit8u value)
{
    struct vhandler_s *vh;
    enum VirqHwRet rc = VIRQ_HWRET_DONE;

    assert(value < VIRQ_MAX);
    vh = &vhandlers[value];
    if (vh->hw_handler)
        rc = vh->hw_handler(vh->arg);
    if (rc == VIRQ_HWRET_DONE)
        virq_lower(value);
}

static void virq_handler(uint16_t idx, HLT_ARG(arg))
{
    uint16_t irr;

    while ((irr = port_inw(VIRQ_IRR_PORT))) {
        struct vhandler_s *vh;
        int inum = find_bit(irr);

        assert(inum < VIRQ_MAX);
        port_outb(VIRQ_HWC_PORT, inum);
        vh = &vhandlers[inum];
        if (vh->sw_handler) {
            enum VirqSwRet rc = vh->sw_handler(vh->arg);
            if (rc == VIRQ_SWRET_BH) {
                assert(_IP != virq_hlt);
                /* If BH is scheduled, we just return and switch back later. */
                return;
            }
        } else {
            error("virq: no handler for %i\n", inum);
        }
    }
    assert(_IP == virq_hlt);
    do_eoi2_iret();
}

void virq_init(void)
{
    emu_iodev_t io_dev = {};
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;

    io_dev.write_portb = virq_hwc_write;
    io_dev.read_portw = virq_irr_read;
    io_dev.start_addr = VIRQ_IRR_PORT;
    io_dev.end_addr = VIRQ_IRR_PORT + VIRQ_TOTAL_PORTS - 1;
    io_dev.handler_name = "virtual IRQ router";
    port_register_handler(io_dev, 0);

    hlt_hdlr.name = "virq";
    hlt_hdlr.func = virq_handler;
    virq_hlt = hlt_register_handler_vm86(hlt_hdlr);
}

void virq_reset(void)
{
    virq_irr = 0;
    pic_untrigger(pic_irq_list[VIRQ_IRQ_NUM]);
}

void virq_setup(void)
{
    SETIVEC(0x72, BIOS_HLT_BLK_SEG, virq_hlt);
}

void virq_raise(int virq_num)
{
    if (virq_num >= VIRQ_MAX || (virq_irr & (1 << virq_num)))
        return;
    virq_irr |= (1 << virq_num);
    pic_request(pic_irq_list[VIRQ_IRQ_NUM]);
}

static void virq_lower(int virq_num)
{
    if (virq_num >= VIRQ_MAX || !(virq_irr & (1 << virq_num)))
        return;
    virq_irr &= ~(1 << virq_num);
    pic_untrigger(pic_irq_list[VIRQ_IRQ_NUM]);
}

void virq_register(int virq_num, enum VirqHwRet (*hw_handler)(void *),
        enum VirqSwRet (*sw_handler)(void *), void *arg)
{
    if (virq_num >= VIRQ_MAX)
        return;
    vhandlers[virq_num].hw_handler = hw_handler;
    vhandlers[virq_num].sw_handler = sw_handler;
    vhandlers[virq_num].arg = arg;
}

void virq_unregister(int virq_num)
{
    vhandlers[virq_num].hw_handler = NULL;
    vhandlers[virq_num].sw_handler = NULL;
}
