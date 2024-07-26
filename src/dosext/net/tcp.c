/*
 *  Copyright (C) 2024  @stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include "emu.h"
#include "hlt.h"
#include "int.h"
#include "iodev.h"
#include "coopth.h"

static uint16_t tcp_hlt_off;
static int tcp_tid;

static void tcp_thr(void *arg)
{
    error("TCP call %x\n", _AX);
}

static void tcp_hlt(Bit16u idx, HLT_ARG(arg))
{
    fake_iret();
    if (!config.tcpdrv)
	return;
    coopth_start(tcp_tid, NULL);
}

void tcp_reset(void)
{
    if (!config.tcpdrv)
      return;
    WRITE_WORD(SEGOFF2LINEAR(PKTDRV_SEG, TCPDRV_driver_entry_ip), tcp_hlt_off);
    WRITE_WORD(SEGOFF2LINEAR(PKTDRV_SEG, TCPDRV_driver_entry_cs), BIOS_HLT_BLK_SEG);
}

void tcp_init(void)
{
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    if (!config.tcpdrv)
      return;

    hlt_hdlr.name       = "tcp callout";
    hlt_hdlr.func       = tcp_hlt;
    tcp_hlt_off = hlt_register_handler_vm86(hlt_hdlr);
    tcp_tid = coopth_create("TCP_call", tcp_thr);
}

void tcp_done(void)
{
}
