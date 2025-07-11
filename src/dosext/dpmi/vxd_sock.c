/*
 *  Copyright (C) 2025  @stsp
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
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "cpu.h"
#include "dosemu_debug.h"
#include "utilities.h"
#include "emudpmi.h"
#include "msdoshlp.h"
#include "emu.h"
#include "vxd.h"

enum { SOCK_VER, SOCK_OPEN, SOCK_CLOSE, SOCK_BIND, SOCK_SENDTO,
       SOCK_RECVFROM, SOCK_SELECT, SOCK_CONNECT, SOCK_SEND, SOCK_RECV,
       SOCK_LISTEN, SOCK_ACONNECT, SOCK_ACCEPT, SOCK_GETSOCKNAME,
       SOCK_GETPEERNAME, SOCK_NBM, SOCK_GETFDS };

struct sock_s {
    int fd;
    unsigned int used:1;
};
#define SOCK_MAX 32
static struct sock_s socks[SOCK_MAX];
static int num_socks;

static struct dos_helper_s sock_hlp;

static struct sock_s *sock_alloc(void)
{
    struct sock_s *ret;
    int i;

    for (i = 0; i < num_socks; i++) {
        if (!socks[i].used)
            break;
    }
    if (i == num_socks) {
        if (num_socks >= SOCK_MAX) {
            error("socket table overflow\n");
            return NULL;
        }
        ret = &socks[num_socks++];
        assert(!ret->used);
        ret->used = 1;
        return ret;
    }
    ret = &socks[i];
    ret->used = 1;
    return ret;
}

static void sock_free(struct sock_s *sock)
{
    sock->used = 0;
    while (num_socks && !socks[num_socks - 1].used)
        num_socks--;
}

struct sel_arg {
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
};

static enum CbkRet select_cb(int arg0, void *arg, int nfds, int *r_err)
{
    struct sel_arg *sel = arg;
    struct timeval to = {};
    int rc = select(nfds, &sel->rfds, &sel->wfds, &sel->efds, &to);
    *r_err = rc;
    if (rc == -1 && errno != EAGAIN) {
        error("select(): %s\n", strerror(errno));
        return CBK_ERR;
    }
    if (rc > 0)
        return CBK_DONE;
    return CBK_CONT;
}

#define SOCK_IDX(s) (s - socks)

#define CSOCK_ERR_INVALID_PARAM       0x00000100
#define CSOCK_ERR_NO_MEMORY           0x00000200
#define CSOCK_ERR_INVALID_SOCKET      0x00000300
#define CSOCK_ERR_ALREADY_BOUND       0x00000400
#define CSOCK_ERR_NOT_BOUND           0x00000500
#define CSOCK_ERR_ACCESS              0x00000600
#define CSOCK_ERR_INTERNAL            0x00000700
#define CSOCK_ERR_FD_INUSE            0x00000800
#define CSOCK_ERR_INFINITE_WAIT       0x00000900
#define CSOCK_ERR_NOT_CONNECTED       0x00000a00
#define CSOCK_ERR_WOULD_BLOCK         0x00000b00
#define CSOCK_ERR_NOT_LISTENING       0x00000c00

static void sock_thr(void *arg)
{
    cpuctx_t *scp = arg;
    struct sock_s *sock;
    int rc;

    _eflags &= ~CF;
    switch(_eax) {
        case SOCK_VER:
            if (_edi && _edi != 0xffff) {
                rc = tcp_get_driver_info(SEL_ADR(_ds, _esi), _edi);
                if (rc) {
                    _eflags |= CF;
                    break;
                }
            }
            _eax = 2;
            break;

        case SOCK_OPEN:
            sock = sock_alloc();
            if (!sock) {
                _eax = CSOCK_ERR_FD_INUSE;
                _eflags |= CF;
                break;
            }
            if (_ebx == IPPROTO_TCP)
                sock->fd = socket(AF_INET, SOCK_STREAM, 0);
            else
                sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock->fd == -1) {
                perror("socket()");
                sock_free(sock);
                _eax = CSOCK_ERR_INTERNAL;
                _eflags |= CF;
                break;
            }
            _edi = SOCK_IDX(sock);
            _eax = 0;
            break;

#define TCP_PROLOG0 \
            if (_edi >= num_socks || !socks[_edi].used) { \
                error("socket %i not allocated\n", _edi); \
                _eax = CSOCK_ERR_INVALID_SOCKET; \
                _eflags |= CF; \
                break; \
            } \
            sock = &socks[_edi]

        case SOCK_CLOSE:
            TCP_PROLOG0;
            close(sock->fd);
            sock_free(sock);
            _eax = 0;
            break;

#define TCP_PROLOG \
            struct sockaddr_in sa; \
            TCP_PROLOG0; \
            sa.sin_addr.s_addr = _ebx; \
            sa.sin_port = _edx; \
            sa.sin_family = AF_INET

#define TCP_IO(n, op, c) \
            rc = op; \
            switch (rc) { \
            case 0: \
                error(n": EOF\n"); \
                _eax = 0; \
                _ecx = 0; \
                break; \
            case -1: \
                error(n": %s\n", strerror(errno)); \
                _eax = CSOCK_ERR_WOULD_BLOCK; \
                _eflags |= CF; \
                break; \
            default: \
                _eax = 0; \
                _ecx = rc; \
                c \
                break; \
            }

        case SOCK_BIND: {
            TCP_PROLOG;

            rc = bind(sock->fd, (struct sockaddr *)&sa, sizeof(sa));
            if (rc) {
                error("TCP bind to port %i: %s\n", _edx, strerror(errno));
                _eax = CSOCK_ERR_ALREADY_BOUND;
                _eflags |= CF;
                break;
            }
            _eax = 0;
            break;
        }

        case SOCK_SENDTO: {
            TCP_PROLOG;
            TCP_IO("UDP sendto", sendto(sock->fd, SEL_ADR(_ds, _esi), _ecx, 0,
                    (struct sockaddr *)&sa, sizeof(sa)),);
            break;
        }

        case SOCK_RECVFROM: {
            struct sockaddr_in sa;
            socklen_t len = sizeof(sa);
            TCP_PROLOG0;
            TCP_IO("UDP recvfrom", recvfrom(sock->fd, SEL_ADR(_ds, _esi), _ecx, 0,
                    (struct sockaddr *)&sa, &len),
                _ebx = sa.sin_addr.s_addr;
                _edx = sa.sin_port;
            );
            break;
        }

        case SOCK_SELECT: {
            struct sel_arg sel = {};
            int i, err = 0, nfds = -1;

            for (i = 0; i < num_socks; i++) {
                sock = &socks[i];
                int used = 0;
                if (!sock->used)
                    continue;
                if ((1 << i) & _ebx) {
                    FD_SET(sock->fd, &sel.rfds);
                    used++;
                }
                if ((1 << i) & _ecx) {
                    FD_SET(sock->fd, &sel.wfds);
                    used++;
                }
                if ((1 << i) & _edx) {
                    FD_SET(sock->fd, &sel.efds);
                    used++;
                }
                if (used && nfds < sock->fd)
                    nfds = sock->fd;
            }
            nfds++;
            switch (handle_timeout_us(_edi, select_cb,
                    -1, &sel, nfds, &err)) {
                case 0:
                    _ebx = _ecx = _edx = 0;
                    _eax = err;
                    for (i = 0; i < num_socks; i++) {
                        sock = &socks[i];
                        if (FD_ISSET(sock->fd, &sel.rfds))
                            _ebx |= 1 << i;
                        if (FD_ISSET(sock->fd, &sel.wfds))
                            _ecx |= 1 << i;
                        if (FD_ISSET(sock->fd, &sel.efds))
                            _edx |= 1 << i;
                    }
                    break;
                case 1:
                    _eax = CSOCK_ERR_INTERNAL;
                    _eflags |= CF;
                    break;
            }
            break;
        }

        case SOCK_CONNECT: {
            TCP_PROLOG;

            rc = connect(sock->fd, (struct sockaddr *)&sa, sizeof(sa));
            if (rc) {
                error("TCP connect: %s\n", strerror(errno));
                _eax = CSOCK_ERR_WOULD_BLOCK;
                _eflags |= CF;
                break;
            }
            _eax = 0;
            break;
        }

        case SOCK_SEND:
            TCP_PROLOG0;
            TCP_IO("TCP send", send(sock->fd, SEL_ADR(_ds, _esi), _ecx, 0),);
            break;

        case SOCK_RECV:
            TCP_PROLOG0;
            TCP_IO("TCP recv", recv(sock->fd, SEL_ADR(_ds, _esi), _ecx, 0),);
            break;

        case SOCK_LISTEN:
            TCP_PROLOG0;

            rc = listen(sock->fd, _ecx);
            if (rc) {
                error("TCP listen: %s\n", strerror(errno));
                _eax = CSOCK_ERR_INTERNAL;
                _eflags |= CF;
                break;
            }
            _eax = 0;
            break;

        case SOCK_ACONNECT:
            // NIY
            break;

        case SOCK_ACCEPT: {
            TCP_PROLOG0;
            struct sock_s *s;
            struct sockaddr_in sa;
            socklen_t len = sizeof(sa);

            rc = accept(sock->fd, (struct sockaddr *)&sa, &len);
            if (rc <= 0) {
                error("TCP accept: %s\n", strerror(errno));
                _eax = CSOCK_ERR_WOULD_BLOCK;
                _eflags |= CF;
                break;
            }
            s = sock_alloc();
            if (!s) {
                close(rc);
                _eax = CSOCK_ERR_INTERNAL;
                _eflags |= CF;
                break;
            }
            s->fd = rc;
            _edx = SOCK_IDX(s);
            _eax = 0;
            _ebx = sa.sin_port;
            _ecx = sa.sin_addr.s_addr;
            break;
        }

        case SOCK_GETSOCKNAME: {
            TCP_PROLOG0;
            struct sockaddr_in sa;
            socklen_t len = sizeof(sa);

            rc = getsockname(sock->fd, (struct sockaddr *)&sa, &len);
            if (rc) {
                error("TCP getsockname: %s\n", strerror(errno));
                _eax = CSOCK_ERR_INTERNAL;
                _eflags |= CF;
                break;
            }
            _eax = 0;
            _ebx = sa.sin_port;
            _ecx = sa.sin_addr.s_addr;
            break;
        }

        case SOCK_GETPEERNAME: {
            TCP_PROLOG0;
            struct sockaddr_in sa;
            socklen_t len = sizeof(sa);

            rc = getpeername(sock->fd, (struct sockaddr *)&sa, &len);
            if (rc) {
                error("TCP getpeername: %s\n", strerror(errno));
                _eax = CSOCK_ERR_INTERNAL;
                _eflags |= CF;
                break;
            }
            _eax = 0;
            _ebx = sa.sin_port;
            _ecx = sa.sin_addr.s_addr;
            break;
        }

        case SOCK_NBM:
            TCP_PROLOG0;

            if (!_ecx)
                _edx = !!(fcntl(sock->fd, F_GETFL) & O_NONBLOCK);
            else
                fcntl(sock->fd, F_SETFL, _edx ? O_NONBLOCK : 0);
            _eax = 0;
            break;

        case SOCK_GETFDS: {
            int i;

            _eax = 0;
            for (i = 0; i < num_socks; i++) {
                sock = &socks[i];
                if (!sock->used)
                    continue;
                _eax |= 1 << i;
            }
            break;
        }
    }
}

void VXD_Sock(cpuctx_t *scp)
{
    if (!sock_hlp.tid)
        doshlp_setup(&sock_hlp, "sock vxd", sock_thr, dpmi_retf);
    _eip = sock_hlp.entry;
}
