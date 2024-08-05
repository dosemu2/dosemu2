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
#include <pthread.h>
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
#define VIRQ_RST_PORT (VIRQ_IRR_PORT + 3)
#define VIRQ_TOTAL_PORTS 4
#define VIRQ_IRQ_NUM 0xf
#define VIRQ_INTERRUPT (VIRQ_IRQ_NUM - 8 + 0x70)

static uint16_t virq_irr;
static pthread_mutex_t irr_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t hndl_mtx = PTHREAD_MUTEX_INITIALIZER;
static uint16_t virq_hlt;
static int cur_bh;

struct vhandler_s {
  enum VirqHwRet (*hw_handler)(void *);
  enum VirqSwRet (*sw_handler)(void *);
  void *arg;
};
struct vhandler_s vhandlers[VIRQ_MAX];

static void virq_lower(int virq_num);

static Bit16u virq_irr_read(ioport_t port, void *arg)
{
    uint16_t irr;

    pthread_mutex_lock(&irr_mtx);
    irr = virq_irr;
    pthread_mutex_unlock(&irr_mtx);
    return irr;
}

static void virq_hwc_write(ioport_t port, Bit8u value, void *arg)
{
    struct vhandler_s *vh;
    enum VirqHwRet rc = VIRQ_HWRET_DONE;

    switch (port) {
    case VIRQ_RST_PORT:
        switch (value) {
        case 1: {
            uint16_t irr;
            /* re-assert irqs */
            pthread_mutex_lock(&irr_mtx);
            irr = virq_irr;
            pthread_mutex_unlock(&irr_mtx);
            if (irr)
                pic_request(VIRQ_IRQ_NUM);
            break;
        }
        }
        break;

    case VIRQ_HWC_PORT:
        assert(value < VIRQ_MAX);
        vh = &vhandlers[value];
        pthread_mutex_lock(&hndl_mtx);
        if (vh->hw_handler)
            rc = vh->hw_handler(vh->arg);
        if (rc == VIRQ_HWRET_DONE)
            virq_lower(value);
        pthread_mutex_unlock(&hndl_mtx);
        break;
    }
}

static void virq_handler(uint16_t idx, HLT_ARG(arg))
{
    uint16_t irr;
    struct vhandler_s *vh;
    enum VirqSwRet rc;

    /* finish bh first */
    if (cur_bh != -1) {
        vh = &vhandlers[cur_bh];
        rc = vh->sw_handler(vh->arg);
        if (rc == VIRQ_SWRET_BH) {
            assert(_IP != virq_hlt);
            return;
        }
        cur_bh = -1;
    }

    while ((irr = port_inw(VIRQ_IRR_PORT))) {
        int inum = find_bit(irr);

        assert(inum < VIRQ_MAX);
        port_outb(VIRQ_HWC_PORT, inum);
        vh = &vhandlers[inum];
        if (vh->sw_handler) {
            rc = vh->sw_handler(vh->arg);
            if (rc == VIRQ_SWRET_BH) {
                assert(_IP != virq_hlt);
                cur_bh = inum;
                /* If BH is scheduled, we just return and switch back later. */
                set_IF();
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
    pic_untrigger(VIRQ_IRQ_NUM);
    cur_bh = -1;
}

void virq_setup(void)
{
    SETIVEC(VIRQ_INTERRUPT, BIOS_HLT_BLK_SEG, virq_hlt);
    port_outb(VIRQ_RST_PORT, 1);
}

static int virq_is_masked(void)
{
    uint8_t imr[2] = { [0] = port_inb(0x21), [1] = port_inb(0xa1) };
    uint16_t real_imr = (imr[1] << 8) | imr[0];
    return ((imr[0] & 4) || (real_imr & (1 << VIRQ_IRQ_NUM)));
}

void virq_raise(int virq_num)
{
    uint16_t irr;
    uint16_t mask = 1 << virq_num;

    assert(virq_num < VIRQ_MAX);
    pthread_mutex_lock(&hndl_mtx);
    pthread_mutex_lock(&irr_mtx);
    /* __sync_fetch_and_or() */
    irr = virq_irr;
    virq_irr |= mask;
    if (!irr) {
        pic_request(VIRQ_IRQ_NUM);
        if (virq_is_masked())
            error("VIRQ masked\n");
    }
    pthread_mutex_unlock(&irr_mtx);
    pthread_mutex_unlock(&hndl_mtx);
}

static void virq_lower(int virq_num)
{
    uint16_t irr;
    uint16_t mask = 1 << virq_num;

    assert(virq_num < VIRQ_MAX);
    pthread_mutex_lock(&irr_mtx);
    /* __sync_and_and_fetch() */
    virq_irr &= ~mask;
    irr = virq_irr;
    if (!irr)
        pic_untrigger(VIRQ_IRQ_NUM);
    pthread_mutex_unlock(&irr_mtx);
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
