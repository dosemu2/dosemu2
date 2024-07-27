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

enum {
    TCP_DRIVER_INFO = 0,
    TCP_DRIVER_UNLOAD,
    TCP_DRIVER_DOIO,
    TCP_DRIVER_CRIT_FLAG,
    TCP_COPY_DRIVER_INFO,

    TCP_OPEN = 0x10,
    TCP_CLOSE,
    TCP_GET,
    TCP_PUT,
    TCP_STATUS,

    UDP_OPEN = 0x20,
    UDP_CLOSE,
    UDP_RECV,
    UDP_SEND,
    UDP_STATUS,

    IP_OPEN = 0x30,
    IP_CLOSE,
    IP_RECV,
    IP_SEND,
    IP_STATUS,

    ATTACH_EVENT_GLOBAL = 0x40,
    DETACH_EVENT_GLOBAL,
    ATTACH_EVENT_LOCAL,
    DETACH_EVENT_LOCAL,
};

enum {
    ERR_NO_ERROR = 0,
    ERR_BAD_CALL,
    ERR_CRITICAL,
    ERR_NOHANDLES,
    ERR_BADHANDLE,
    ERR_TIMEOUT,
    ERR_BADSESSION,
};

struct driver_info_rec {
    uint32_t myip;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dnsserver;
    uint32_t timeserver;
    uint16_t mtu;
    uint8_t def_ttl;
    uint8_t def_tos;
    uint16_t tcp_mss;
    uint16_t tcp_rwin;
    uint16_t debug;
    char domain[255];
};

struct session_info_rec {
    uint32_t ip_srce;
    uint32_t ip_dest;
    uint8_t ip_prot;
    uint8_t active;
};

static void tcp_thr(void *arg)
{
#define __S(x) #x
#define _S(x) __S(x)
#define _D(x) \
  case x: \
    error("TCP call %s\n", _S(x)); \
    break

    switch (HI(ax)) {
        _D(TCP_DRIVER_INFO);
        _D(TCP_DRIVER_UNLOAD);
        _D(TCP_DRIVER_DOIO);
        _D(TCP_DRIVER_CRIT_FLAG);
        _D(TCP_COPY_DRIVER_INFO);

        _D(TCP_OPEN);
        _D(TCP_CLOSE);
        _D(TCP_GET);
        _D(TCP_PUT);
        _D(TCP_STATUS);

        _D(UDP_OPEN);
        _D(UDP_CLOSE);
        _D(UDP_RECV);
        _D(UDP_SEND);
        _D(UDP_STATUS);

        _D(IP_OPEN);
        _D(IP_CLOSE);
        _D(IP_RECV);
        _D(IP_SEND);
        _D(IP_STATUS);
    }
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
