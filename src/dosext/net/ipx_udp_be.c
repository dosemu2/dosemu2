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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include "dosemu_debug.h"
#include "doshelpers.h"
#include "utilities.h"
#include "ioselect.h"
#include "emu.h"
#include "ipx.h"
#include "ipx_be.h"

static int udp_sock = -1;
static int udp_enabled;
static char HOST[128] = "localhost";
static uint16_t PORT = 1024;

struct ipx_sock {
    uint16_t sock;
    int pipe[2];
};
#define SOCKTABLESIZE	150 // DOS IPX driver was limited to 150 open sockets
static struct ipx_sock opensockets[SOCKTABLESIZE];
static int socketCount;
static pthread_rwlock_t sock_lck = PTHREAD_RWLOCK_INITIALIZER;

static unsigned char MyAddress[10] =
{0x01, 0x01, 0x00, 0xe0,
 0x00, 0x00, 0x1b, 0x33, 0x2b, 0x13};
static int inited;

static in_addr_t get_ip(const char *host)
{
    struct addrinfo hints = {};
    struct addrinfo *res;
    struct sockaddr_in *sin;
    in_addr_t ret;
    int err;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    err = getaddrinfo(host, "ipx", &hints, &res);
    if (err) {
        error("getaddrinfo(): %s\n", strerror(errno));
        return 0;
    }
    sin = (struct sockaddr_in *)res->ai_addr;
    ret = sin->sin_addr.s_addr;
    freeaddrinfo(res);
    return ret;
}

static void udp_async_callback(int fd, void *arg)
{
#define MAX_PACKET_DATA 1500
    char buf[MAX_PACKET_DATA];
    uint16_t sock;
    IPXPacket_t *IPXHeader;
    int i;
    int size = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
    if (size <= 0) {
        error("UDP recv error (%i): %s\n", size, strerror(errno));
        return;
    }
    IPXHeader = (IPXPacket_t *)buf;
    memcpy(&sock, IPXHeader->Destination.Socket, 2);
    pthread_rwlock_rdlock(&sock_lck);
    for (i = 0; i < socketCount; i++) {
        if (opensockets[i].sock == sock)
            break;
    }
    if (i == socketCount)
        error("IPX socket %x not found\n", sock);
    else
        write(opensockets[i].pipe[1], buf, size);
    pthread_rwlock_unlock(&sock_lck);
//    ioselect_complete(fd);
}

static enum CbkRet recv_cb(int fd, void *buf, int len, int *r_err)
{
    int rc = recv(fd, buf, len, MSG_DONTWAIT);
    *r_err = rc;
    if (rc == -1 && errno != EAGAIN) {
        error("recv(): %s\n", strerror(errno));
        return CBK_ERR;
    }
    if (rc > 0)
        return CBK_DONE;
    return CBK_CONT;
}

static int __GetMyAddress(const char *host, uint16_t port)
{
    struct sockaddr_in sin;
    struct sockaddr_in sout;
    IPXPacket_t IPXHeader;
    int i, rc, len;
    uint16_t sock = htons(2);

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_sock == -1) {
        n_printf("IPX: could not open socket in GetMyAddress: %s\n", strerror(errno));
        return -1;
    }

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = 0;
    if (bind(udp_sock, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
        n_printf("IPX: could not bind UDP socket: %s\n", strerror(errno));
        goto err;
    }

    sout.sin_family = AF_INET;
    sout.sin_addr.s_addr = get_ip(host);
    sout.sin_port = htons(port);
    if (connect(udp_sock, (struct sockaddr*)&sout, sizeof(sout)) == -1) {
        n_printf("IPX: could not connect UDP socket: %s\n", strerror(errno));
        goto err;
    }
    IPXHeader.Checksum = 0xFFFF;
    IPXHeader.Length = htons(sizeof(IPXPacket_t));
    IPXHeader.TransportControl = 0;
    IPXHeader.PacketType = 0;
    memset(IPXHeader.Destination.Network, 0, 4);
    memset(IPXHeader.Destination.Node, 0, 6);
    memcpy(IPXHeader.Destination.Socket, &sock, 2);
    memset(IPXHeader.Source.Network, 0, 4);
    memset(IPXHeader.Source.Node, 0, 6);
    memcpy(IPXHeader.Source.Socket, &sock, 2);
    rc = send(udp_sock, &IPXHeader, sizeof(IPXHeader), 0);
    if (rc <= 0) {
        n_printf("IPX: could not send UDP packet: %s\n", strerror(errno));
        goto err;
    }
    rc = handle_timeout(30, recv_cb, udp_sock, &IPXHeader, sizeof(IPXHeader),
            &len);
    if (rc != 0) {
        n_printf("IPX: could not recv UDP packet: %s\n", strerror(errno));
        goto err;
    }
    if (len != sizeof(IPXHeader)) {
        n_printf("IPX: bad recv ren, need %zi got %i\n", sizeof(IPXHeader),
                 len);
        goto err;
    }

    add_to_io_select_threaded(udp_sock, udp_async_callback, NULL);
    memcpy(MyAddress, IPXHeader.Destination.Network, 4);
    for (i = 0; i < 6; i++) {
        MyAddress[4+i] = IPXHeader.Destination.Node[i];
    }
    n_printf("IPX: using network address %02X%02X%02X%02X\n",
        MyAddress[0], MyAddress[1], MyAddress[2], MyAddress[3] );
    n_printf("IPX: using node address of %02X%02X%02X%02X%02X%02X\n",
        MyAddress[4], MyAddress[5], MyAddress[6], MyAddress[7],
        MyAddress[8], MyAddress[9] );
    return 0;

err:
    close(udp_sock);
    udp_sock = -1;
    return -1;
}

static void _GetMyAddress(void)
{
  int err;

  if (inited)
    return;
  err = __GetMyAddress(HOST, PORT);
  if (err)
    return;
  inited = 1;
}

static const unsigned char *GetMyAddress(void)
{
  _GetMyAddress();
  return MyAddress;
}

static int sockInUse(uint16_t sockNum)
{
    int i;
    for (i = 0; i < socketCount; i++) {
        if (opensockets[i].sock == sockNum)
            return 1;
    }
    return 0;
}

static int _do_open(u_short port, u_short *newPort, int *err)
{
    int i, rc;
    struct ipx_sock *s;

    if (port == 0x0000) {
        // Dynamic socket allocation
        uint16_t sockAlloc = 0x4002;
        while (sockInUse(sockAlloc) && (sockAlloc < 0x7fff))
            sockAlloc++;
        if (sockAlloc > 0x7fff) {
            // I have no idea how this could happen if the IPX driver
            // is limited to 150 open sockets at a time
            error("IPX: Out of dynamic sockets\n");
            *err = RCODE_SOCKET_TABLE_FULL;
            return -1;
        }
        port = sockAlloc;
    } else if (sockInUse(port)) {
        *err = RCODE_SOCKET_ALREADY_OPEN;
        return -1;
    }

    for (i = 0; i < socketCount; i++) {
        if (!opensockets[i].sock)
            break;
    }
    if (i == socketCount) {
        if (socketCount >= SOCKTABLESIZE) {
            error("IPX: socket tabler full\n");
            *err = RCODE_SOCKET_TABLE_FULL;
            return -1;
        }
        socketCount++;
    }
    s = &opensockets[i];
    s->sock = port;
    rc = pipe(s->pipe);
    assert(!rc);
    fcntl(s->pipe[0], F_SETFL, O_NONBLOCK);
    *newPort = port;
    return s->pipe[0];
}

static int do_open(u_short port, u_short *newPort, int *err)
{
    int ret;

    _GetMyAddress();

    pthread_rwlock_wrlock(&sock_lck);
    ret = _do_open(port, newPort, err);
    pthread_rwlock_unlock(&sock_lck);
    return ret;
}

static int _do_close(int fd)
{
    int i;

    for (i = 0; i < socketCount; i++) {
        if (opensockets[i].pipe[0] == fd)
            break;
    }
    if (i == socketCount)
        return -1;
    close(opensockets[i].pipe[1]);
    close(opensockets[i].pipe[0]);
    opensockets[i].sock = 0;
    while (socketCount && !opensockets[socketCount - 1].sock)
        socketCount--;
    return 0;
}

static int do_close(int fd)
{
    int ret;

    pthread_rwlock_wrlock(&sock_lck);
    ret = _do_close(fd);
    pthread_rwlock_unlock(&sock_lck);
    return ret;
}

static int do_recv(int fd, u_char *buffer, int bufLen, u_short sock)
{
    return read(fd, buffer, bufLen);
}

static int do_send(int fd, u_char *data, int dataLen)
{
    return send(udp_sock, data, dataLen, 0);
}

static const struct ipx_ops iops = {
    GetMyAddress,
    do_open,
    do_close,
    do_recv,
    do_send,
};

void ipx_helper(struct vm86_regs *regs)
{
    switch (HI_BYTE_d(regs->eax)) {
        case DOS_SUBHELPER_IPX_CONFIG:
            regs->ebx = 0;
            if (config.ipxsup)
                regs->ebx |= 1;
            if (udp_enabled)
                regs->ebx |= 2;
            if (inited)
                regs->ebx |= 4;
            break;
        case DOS_SUBHELPER_IPX_CONNECT:
            if (regs->ebx)
                PORT = regs->ebx;
            if (regs->ecx)
                memcpy(HOST, LINEAR2UNIX(SEGOFF2LINEAR(_ES, _DI)),
                        regs->ecx + 1);
            _GetMyAddress();
            if (inited) {
                int err = ipx_register_ops(&iops);
                if (!err)
                    udp_enabled = 1;
            } else {
                CARRY;
            }
            break;
        case DOS_SUBHELPER_IPX_DISCONNECT:
            if (inited) {
                close(udp_sock);
                udp_sock = -1;
                inited = 0;
            } else {
                CARRY;
            }
            break;
    }
}
