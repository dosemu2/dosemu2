/*
 *  Copyright (C) 2006 Stas Sergeev <stsp@users.sourceforge.net>
 *
 * The below copyright strings have to be distributed unchanged together
 * with this file. This prefix can not be modified or separated.
 */

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
 * DMA controller emulation.
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 */

#include "emu.h"
#include "init.h"
#include "utilities.h"
#include "port.h"
#include "timers.h"
#include "dma.h"
#include <string.h>

typedef union {
    Bit8u byte[2];
    Bit16u value;
} dma_reg16;

struct dma_channel {
    dma_reg16 base_addr;
    dma_reg16 base_count;
    dma_reg16 cur_addr;
    dma_reg16 cur_count;
    Bit8u page;
    Bit8u mode;
};

struct dma_controller {
    struct dma_channel chans[4];
#if 0
    /* looks like we don't need those */
    Bit16u tmp_addr;
    Bit16u tmp_count;
#endif
    Bit8u tmp_reg;
    Bit8u status;
    Bit8u command;
    Bit8u mask;
    Bit8u request;
    Bit8u ff;
} dma[2];

static Bit8u dma_data_bus[2];

#define DMA1  0
#define DMA2  1

/* need this as soon as dosemu is threaded */
#define DMA_LOCK()
#define DMA_UNLOCK()

#define DI(c) (((c) & 4) >> 2)
#define CI(c) ((c) & 3)

#define HAVE_DRQ(contr, chan) (dma[contr].status & (1 << ((chan) + 4)))
#define REACHED_TC(contr, chan) (dma[contr].status & (1 << (chan)))
#define MASKED(contr, chan) (dma[contr].mask & (1 << (chan)))
#define HAVE_SRQ(contr, chan) (dma[contr].request & (1 << (chan)))
#define SW_ACTIVE(contr, chan) \
  (HAVE_SRQ(contr, chan) && \
  (dma[contr].chans[chan].mode & 0x30) == 0x20)


static void dma_soft_reset(int dma_idx)
{
    dma[dma_idx].command = 0;
    dma[dma_idx].status = 0;
    dma[dma_idx].request = 0;
    dma[dma_idx].tmp_reg = 0;
    dma[dma_idx].ff = 0;
    dma[dma_idx].mask = 0x0f;
    q_printf("DMA: Soft Reset on controller %i\n", dma_idx);
}

static void dma_poll_DRQ(int dma_idx, int chan_idx)
{
    /* Since our model is simplified (API has only pulse_DRQ() instead
     * of assert_DRQ()/release_DRQ()), poll never sees the DRQ asserted. */
    dma[dma_idx].status &= ~(1 << (chan_idx + 4));
}

static void dma_update_DRQ(int dma_idx, int chan_idx)
{
    switch (dma[dma_idx].chans[chan_idx].mode & 0x30) {
    case 0x00:			// demand
	dma_poll_DRQ(dma_idx, chan_idx);
	break;
    case 0x10:			// single
	dma[dma_idx].status &= ~(1 << (chan_idx + 4));
	break;
    case 0x20:			// block
	if (REACHED_TC(dma_idx, chan_idx))
	    dma_poll_DRQ(dma_idx, chan_idx);
	break;
    case 0x30:			// cascade
	dma_poll_DRQ(dma_idx, chan_idx);
	break;
    }
}

static void dma_process_channel(int dma_idx, int chan_idx)
{
    struct dma_channel *chan = &dma[dma_idx].chans[chan_idx];
    Bit32u addr = (chan->page << 16) | (chan->cur_addr.value << dma_idx);
    Bit8u mode = chan->mode;

    /* first, do the transfer */
    switch (mode & 3) {
    case 0:			/* verify */
	q_printf("DMA: verify mode does nothing\n");
	break;
    case 1:			/* write */
	MEMCPY_2DOS(addr, dma_data_bus, 1 << dma_idx);
	break;
    case 2:			/* read */
	MEMCPY_2UNIX(dma_data_bus, addr, 1 << dma_idx);
	break;
    case 3:			/* invalid */
	q_printf("DMA: invalid mode does nothing\n");
	break;
    }

    /* now advance the address */
    if (!(dma[dma_idx].command & 2))
	chan->cur_addr.value += (mode & 8) ? -1 : 1;

    /* and the counter */
    chan->cur_count.value--;
    if (chan->cur_count.value == 0xffff) {	/* overflow */
	if (mode & 4) {		/* auto-init */
	    q_printf("DMA: controller %i, channel %i reinitialized\n",
		     dma_idx, chan_idx);
	    chan->cur_addr.value = chan->base_addr.value;
	    chan->cur_count.value = chan->base_count.value;
	} else {		/* eop */
	    q_printf("DMA: controller %i, channel %i EOP\n", dma_idx,
		     chan_idx);
	    dma[dma_idx].status |= 1 << chan_idx;
	    dma[dma_idx].request &= ~(1 << chan_idx);
	    /* the datasheet says it gets automatically masked too */
	    dma[dma_idx].mask |= 1 << chan_idx;
	}
    }
}

static void dma_run_channel(int dma_idx, int chan_idx)
{
    int done = 0;
    long ticks = 0;
    while (!done &&
	   (HAVE_DRQ(dma_idx, chan_idx) || SW_ACTIVE(dma_idx, chan_idx))) {
	if (!MASKED(dma_idx, chan_idx) &&
	    !REACHED_TC(dma_idx, chan_idx) &&
	    !(dma[dma_idx].command & 4) &&
	    ((dma[dma_idx].chans[chan_idx].mode & 0x30) != 0x30)) {
	    dma_process_channel(dma_idx, chan_idx);
	    ticks++;
	} else {
	    done = 1;
	}
	dma_update_DRQ(dma_idx, chan_idx);
    }
    if (ticks > 1)
	q_printf("DMA: processed %lu (left %u) cycles on controller %i channel %i\n",
	     ticks, dma[dma_idx].chans[chan_idx].cur_count.value, dma_idx,
	     chan_idx);
}

static void dma_process(void)
{
    int contr_num, chan_num;

    DMA_LOCK();
    for (contr_num = 0; contr_num < 2; contr_num++) {
	for (chan_num = 0; chan_num < 4; chan_num++)
	    dma_run_channel(contr_num, chan_num);
    }
    DMA_UNLOCK();
}

int dma_pulse_DRQ(int ch, Bit8u * buf)
{
    int ret = DMA_DACK;
    if (MASKED(DI(ch), CI(ch))) {
	q_printf("DMA: channel %i masked, DRQ ignored\n", ch);
	ret = DMA_NO_DACK;
    }
    if ((dma[DI(ch)].status & 0xf0) || dma[DI(ch)].request) {
	error("DMA: channel %i already active! (m=%#x s=%#x r=%#x)\n",
	      dma[DI(ch)].chans[CI(ch)].mode, dma[DI(ch)].status,
	      dma[DI(ch)].request);
	ret = DMA_NO_DACK;
    }
#if 0
    q_printf("DMA: pulse DRQ on channel %d\n", ch);
#endif
    if (ret == DMA_DACK) {
	DMA_LOCK();
	dma[DI(ch)].status |= 1 << (CI(ch) + 4);
	memcpy(dma_data_bus, buf, 1 << DI(ch));
	dma_run_channel(DI(ch), CI(ch));
	memcpy(buf, dma_data_bus, 1 << DI(ch));
	DMA_UNLOCK();
    } else {
	memset(buf, 0xff, 1 << DI(ch));
    }
    return ret;
}


#define d(x) (x-1)
static Bit8u dma_io_read(ioport_t port)
{
    Bit8u r = 0xff;
    switch (port) {

/* lets ride on the cpp ass */
#define HANDLE_CUR_ADDR_READ(d_n, c_n) \
  case DMA##d_n##_ADDR_##c_n: \
    r = dma[d(d_n)].chans[d(c_n)].cur_addr.byte[dma[d(d_n)].ff]; \
    q_printf("DMA%i: cur_addr read: %#x from Channel %d byte %d\n", \
	d_n, r, d(c_n), dma[d(d_n)].ff); \
    dma[d(d_n)].ff ^= 1; \
    break
	HANDLE_CUR_ADDR_READ(1, 1);
	HANDLE_CUR_ADDR_READ(1, 2);
	HANDLE_CUR_ADDR_READ(1, 3);
	HANDLE_CUR_ADDR_READ(1, 4);

	HANDLE_CUR_ADDR_READ(2, 1);
	HANDLE_CUR_ADDR_READ(2, 2);
	HANDLE_CUR_ADDR_READ(2, 3);
	HANDLE_CUR_ADDR_READ(2, 4);
#undef HANDLE_CUR_ADDR_READ

#define HANDLE_CUR_CNT_READ(d_n, c_n) \
  case DMA##d_n##_CNT_##c_n: \
    r = dma[d(d_n)].chans[d(c_n)].cur_count.byte[dma[d(d_n)].ff]; \
    q_printf("DMA%i: cur_cnt read: %#x from Channel %d byte %d\n", \
	d_n, r, d(c_n), dma[d(d_n)].ff); \
    dma[d(d_n)].ff ^= 1; \
    break
	HANDLE_CUR_CNT_READ(1, 1);
	HANDLE_CUR_CNT_READ(1, 2);
	HANDLE_CUR_CNT_READ(1, 3);
	HANDLE_CUR_CNT_READ(1, 4);

	HANDLE_CUR_CNT_READ(2, 1);
	HANDLE_CUR_CNT_READ(2, 2);
	HANDLE_CUR_CNT_READ(2, 3);
	HANDLE_CUR_CNT_READ(2, 4);
#undef HANDLE_CUR_CNT_READ

#define HANDLE_PAGE_READ(d_n, c_n) \
  case DMA##d_n##_PAGE_##c_n: \
    r = dma[d(d_n)].chans[d(c_n)].page; \
    q_printf("DMA%i: page read: %#x from Channel %d\n", \
	d_n, r, d(c_n)); \
    break
	HANDLE_PAGE_READ(1, 1);
	HANDLE_PAGE_READ(1, 2);
	HANDLE_PAGE_READ(1, 3);
	HANDLE_PAGE_READ(1, 4);

	HANDLE_PAGE_READ(2, 1);
	HANDLE_PAGE_READ(2, 2);
	HANDLE_PAGE_READ(2, 3);
	HANDLE_PAGE_READ(2, 4);
#undef HANDLE_PAGE_READ

    case DMA1_STAT_REG:
	r = dma[DMA1].status;
	q_printf("DMA1: Read %u from Status reg\n", r);
	dma[DMA1].status &= 0xf0;	/* clear status bits */
	break;
    case DMA2_STAT_REG:
	r = dma[DMA2].status;
	q_printf("DMA2: Read %u from Status reg\n", r);
	dma[DMA2].status &= 0xf0;	/* clear status bits */
	break;

    case DMA1_TEMP_REG:
	r = dma[DMA1].tmp_reg;
	q_printf("DMA1: Read %u from temporary register unimplemented\n",
		 r);
	break;
    case DMA2_TEMP_REG:
	r = dma[DMA2].tmp_reg;
	q_printf("DMA2: Read %u from temporary register unimplemented\n",
		 r);
	break;

    default:
	q_printf("DMA: Unhandled Read on 0x%04x\n", (Bit16u) port);
    }

    dma_process();		// Not needed in fact

    return r;
}

static void dma_io_write(ioport_t port, Bit8u value)
{
    switch (port) {

#define HANDLE_ADDR_WRITE(d_n, c_n) \
  case DMA##d_n##_ADDR_##c_n: \
    dma[d(d_n)].chans[d(c_n)].base_addr.byte[dma[d(d_n)].ff] = value; \
    dma[d(d_n)].chans[d(c_n)].cur_addr.byte[dma[d(d_n)].ff] = value; \
    q_printf("DMA%i: addr write: %#x to Channel %d byte %d\n", \
	d_n, value, d(c_n), dma[d(d_n)].ff); \
    dma[d(d_n)].ff ^= 1; \
    break
	HANDLE_ADDR_WRITE(1, 1);
	HANDLE_ADDR_WRITE(1, 2);
	HANDLE_ADDR_WRITE(1, 3);
	HANDLE_ADDR_WRITE(1, 4);

	HANDLE_ADDR_WRITE(2, 1);
	HANDLE_ADDR_WRITE(2, 2);
	HANDLE_ADDR_WRITE(2, 3);
	HANDLE_ADDR_WRITE(2, 4);
#undef HANDLE_ADDR_WRITE

#define HANDLE_CNT_WRITE(d_n, c_n) \
  case DMA##d_n##_CNT_##c_n: \
    dma[d(d_n)].chans[d(c_n)].base_count.byte[dma[d(d_n)].ff] = value; \
    dma[d(d_n)].chans[d(c_n)].cur_count.byte[dma[d(d_n)].ff] = value; \
    q_printf("DMA%i: count write: %#x to Channel %d byte %d\n", \
	d_n, value, d(c_n), dma[d(d_n)].ff); \
    dma[d(d_n)].ff ^= 1; \
    break
	HANDLE_CNT_WRITE(1, 1);
	HANDLE_CNT_WRITE(1, 2);
	HANDLE_CNT_WRITE(1, 3);
	HANDLE_CNT_WRITE(1, 4);

	HANDLE_CNT_WRITE(2, 1);
	HANDLE_CNT_WRITE(2, 2);
	HANDLE_CNT_WRITE(2, 3);
	HANDLE_CNT_WRITE(2, 4);
#undef HANDLE_CNT_WRITE

#define HANDLE_PAGE_WRITE(d_n, c_n) \
  case DMA##d_n##_PAGE_##c_n: \
    dma[d(d_n)].chans[d(c_n)].page = value; \
    q_printf("DMA%i: page write: %#x to Channel %d\n", \
	d_n, value, d(c_n)); \
    break
	HANDLE_PAGE_WRITE(1, 1);
	HANDLE_PAGE_WRITE(1, 2);
	HANDLE_PAGE_WRITE(1, 3);
	HANDLE_PAGE_WRITE(1, 4);

	HANDLE_PAGE_WRITE(2, 1);
	HANDLE_PAGE_WRITE(2, 2);
	HANDLE_PAGE_WRITE(2, 3);
	HANDLE_PAGE_WRITE(2, 4);
#undef HANDLE_PAGE_WRITE

    case DMA1_MASK_REG:
	if (value & 4) {
	    q_printf("DMA1: mask channel %i\n", value & 3);
	    dma[DMA1].mask |= 1 << (value & 3);
	} else {
	    q_printf("DMA1: unmask channel %i\n", value & 3);
	    dma[DMA1].mask &= ~(1 << (value & 3));
	    dma[DMA1].status &= ~(1 << (value & 3));
	}
	break;
    case DMA2_MASK_REG:
	if (value & 4) {
	    q_printf("DMA2: mask channel %i\n", value & 3);
	    dma[DMA2].mask |= 1 << (value & 3);
	} else {
	    q_printf("DMA2: unmask channel %i\n", value & 3);
	    dma[DMA2].mask &= ~(1 << (value & 3));
	    dma[DMA2].status &= ~(1 << (value & 3));
	}
	break;

    case DMA1_MODE_REG:
	dma[DMA1].chans[value & 3].mode = value >> 2;
	q_printf("DMA1: Write mode 0x%x to Channel %u\n", value >> 2,
		 value & 3);
	break;
    case DMA2_MODE_REG:
	dma[DMA2].chans[value & 3].mode = value >> 2;
	q_printf("DMA2: Write mode 0x%x to Channel %u\n", value >> 2,
		 value & 3);
	break;

    case DMA1_CMD_REG:
	dma[DMA1].command = value;
	q_printf("DMA1: Write 0x%x to Command reg\n", value);
	break;
    case DMA2_CMD_REG:
	dma[DMA2].command = value;
	q_printf("DMA2: Write 0x%x to Command reg\n", value);
	break;

    case DMA1_CLEAR_FF_REG:
	q_printf("DMA1: Clearing Output Flip-Flop\n");
	dma[DMA1].ff = 0;
	break;
    case DMA2_CLEAR_FF_REG:
	q_printf("DMA2: Clearing Output Flip-Flop\n");
	dma[DMA2].ff = 0;
	break;

    case DMA1_RESET_REG:
	q_printf("DMA1: Reset\n");
	dma_soft_reset(DMA1);
	break;
    case DMA2_RESET_REG:
	q_printf("DMA2: Reset\n");
	dma_soft_reset(DMA2);
	break;

    case DMA1_REQ_REG:
	if (value & 4) {
	    q_printf("DMA1: Setting request state %#x\n", value);
	    dma[DMA1].request |= 1 << (value & 3);
	} else {
	    q_printf("DMA1: Clearing request state %#x\n", value);
	    dma[DMA1].request &= ~(1 << (value & 3));
	}
	break;
    case DMA2_REQ_REG:
	if (value & 4) {
	    q_printf("DMA2: Setting request state %#x\n", value);
	    dma[DMA2].request |= 1 << (value & 3);
	} else {
	    q_printf("DMA2: Clearing request state %#x\n", value);
	    dma[DMA2].request &= ~(1 << (value & 3));
	}
	break;

    case DMA1_CLR_MASK_REG:
	q_printf("DMA1: Clearing masks\n");
	dma[DMA1].mask = 0;
	dma[DMA1].status &= 0xf0;
	break;
    case DMA2_CLR_MASK_REG:
	q_printf("DMA2: Clearing masks\n");
	dma[DMA2].mask = 0;
	dma[DMA2].status &= 0xf0;
	break;

    case DMA1_MASK_ALL_REG:
	q_printf("DMA1: Setting masks %#x\n", value);
	dma[DMA1].mask = value;
	dma[DMA1].status &= 0xf0;
	break;
    case DMA2_MASK_ALL_REG:
	q_printf("DMA2: Setting masks %#x\n", value);
	dma[DMA2].mask = value;
	dma[DMA2].status &= 0xf0;
	break;

    default:
	q_printf("DMA: Unhandled Write on 0x%04x\n", (Bit16u) port);
    }

    dma_process();		// Not needed in fact
}

#undef d


void dma_new_reset(void)
{
    dma_soft_reset(DMA1);
    dma_soft_reset(DMA2);
}

void dma_new_init(void)
{
    emu_iodev_t io_device;

    /* 8237 DMA controller */
    io_device.read_portb = dma_io_read;
    io_device.write_portb = dma_io_write;
    io_device.read_portw = NULL;
    io_device.write_portw = NULL;
    io_device.read_portd = NULL;
    io_device.write_portd = NULL;
    io_device.irq = EMU_NO_IRQ;
    io_device.fd = -1;

    /*
     * XT Controller 
     */
    io_device.start_addr = 0x0000;
    io_device.end_addr = 0x000F;
    io_device.handler_name = "DMA - XT Controller";
    port_register_handler(io_device, 0);

    /*
     * Page Registers (XT)
     */
    io_device.start_addr = 0x0081;
    io_device.end_addr = 0x0087;
    io_device.handler_name = "DMA - XT Pages";
    port_register_handler(io_device, 0);

    /*
     * AT Controller
     */
    io_device.start_addr = 0x00C0;
    io_device.end_addr = 0x00DE;
    io_device.handler_name = "DMA - AT Controller";
    port_register_handler(io_device, 0);

    /*
     * Page Registers (AT)
     */
    io_device.start_addr = 0x0089;
    io_device.end_addr = 0x008F;
    io_device.handler_name = "DMA - AT Pages";
    port_register_handler(io_device, 0);

    q_printf("DMA: DMA Controller initialized - 8 & 16 bit modes\n");
}

CONSTRUCTOR(static void dma_early_init(void))
{
    /* HACK - putting this into dma_init() should work, but doesn't */
    register_debug_class('q', NULL, "DMA");
}
