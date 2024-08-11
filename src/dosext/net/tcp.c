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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <errno.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <limits.h>
#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#endif
#include "emu.h"
#include "hlt.h"
#include "int.h"
#include "iodev.h"
#include "coopth.h"
#include "doshelpers.h"

static const char *DEFAULT_DNS = "8.8.8.8";
static const char *DEFAULT_NTP = "pool.ntp.org";

static uint16_t tcp_hlt_off;
static int tcp_tid;
static in_addr_t myip;

enum {
    TCP_DRIVER_INFO = 0,
    TCP_DRIVER_UNLOAD,
    TCP_DRIVER_DOIO,
    TCP_DRIVER_CRIT_FLAG,
    TCP_COPY_DRIVER_INFO,

    TCP_OPEN = 0x10,
    _TCP_CLOSE,
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
    uint16_t port_src;
    uint32_t ip_dest;
    uint16_t port_dst;
    uint8_t ip_prot;
    uint8_t active;
} __attribute__((packed));

struct ses_wrp {
    struct session_info_rec si;
    int fd;
    int lfd;
    int used;
};

#define MAX_SES 20
static struct ses_wrp ses[MAX_SES];
static int num_ses;

enum { TS_ERROR, TS_LISTEN, TS_SYN_SENT, TS_SYN_RECV, TS_ESTABLISHED,
    TS_FIN_WAIT1, TS_FIN_WAIT2, TS_CLOSE_WAIT, TS_CLOSING,
    TS_LAST_ACK, TS_TIME_WAIT, TS_CLOSE, TS_TOO_HIGH };

static int get_tcp_state(int st)
{
#define TS(s) \
  case TCP_##s: \
    return TS_##s

    switch (st) {
        TS(ESTABLISHED);
        TS(SYN_SENT);
        TS(SYN_RECV);
        TS(FIN_WAIT1);
        TS(FIN_WAIT2);
        TS(TIME_WAIT);
        TS(CLOSE);
        TS(CLOSE_WAIT);
        TS(LAST_ACK);
        TS(LISTEN);
        TS(CLOSING);
    }
    return TS_ERROR;
}

static int alloc_ses(void)
{
    int i;

    for (i = 0; i < num_ses; i++) {
        if (!ses[i].used) {
            memset(&ses[i], 0, sizeof(ses[i]));
            ses[i].used = 1;
            return i;
        }
    }
    if (num_ses >= MAX_SES)
        return -1;
    memset(&ses[num_ses], 0, sizeof(ses[num_ses]));
    ses[num_ses].used = 1;
    return num_ses++;
}

static void free_ses(int idx)
{
    assert(idx < num_ses);
    ses[idx].used = 0;
    while (num_ses && !ses[num_ses - 1].used)
        num_ses--;
}

// https://gist.github.com/javiermon/6272065
static int getgatewayandiface(in_addr_t *addr, char *interface)
{
    long destination, gateway;
    char iface[IF_NAMESIZE];
    char buf[1024];
    FILE *file;

    memset(iface, 0, sizeof(iface));
    memset(buf, 0, sizeof(buf));

    file = fopen("/proc/net/route", "r");
    if (!file)
        return -1;

    while (fgets(buf, sizeof(buf), file)) {
        if (sscanf(buf, "%s %lx %lx", iface, &destination, &gateway) == 3) {
            if (destination == 0) { /* default */
                *addr = gateway;
                strcpy(interface, iface);
                fclose(file);
                return 0;
            }
        }
    }

    /* default route not found */
    if (file)
        fclose(file);
    return -1;
}

void tcp_helper(struct vm86_regs *regs)
{
    switch (HI_BYTE_d(regs->eax)) {
        case DOS_SUBHELPER_TCP_CONFIG:
            regs->ebx = config.tcpdrv;
            if (config.tcpiface) {
                int len = strlen(config.tcpiface);
                if (len > 15)
                    len = 15;
                MEMCPY_2DOS(SEGOFF2LINEAR(PKTDRV_SEG, TCPDRV_iface_name),
                        config.tcpiface, len);
                WRITE_BYTE(SEGOFF2LINEAR(PKTDRV_SEG, TCPDRV_iface_name) + len,
                        '\0');
                regs->ecx = len;
            } else {
                char iface[IF_NAMESIZE];
                in_addr_t gw;
                int len;
                int err = getgatewayandiface(&gw, iface);
                if (err) {
                    error("TCP: can't find default interface\n");
                    return;
                }
                len = strlen(iface);
                if (len > 15)
                    len = 15;
                MEMCPY_2DOS(SEGOFF2LINEAR(PKTDRV_SEG, TCPDRV_iface_name),
                        iface, len);
                WRITE_BYTE(SEGOFF2LINEAR(PKTDRV_SEG, TCPDRV_iface_name) + len,
                        '\0');
                regs->ecx = 0;  // dynamic iface
            }
            regs->es = PKTDRV_SEG;
            regs->edi = TCPDRV_iface_name;
            break;
        case DOS_SUBHELPER_TCP_ENABLE:
            config.tcpdrv = LO_BYTE_d(regs->ebx);
            break;
        case DOS_SUBHELPER_TCP_SETIF:
            free(config.tcpiface);
            config.tcpiface = NULL;
            if (LO_BYTE_d(regs->ecx))
                config.tcpiface = strndup(SEG_ADR((char *), es, di),
                        LO_BYTE_d(regs->ecx));
            break;
        case DOS_SUBHELPER_TCP_GETGW: {
            in_addr_t gw;
            if (config.tcpgw) {
                gw = config.tcpgw;
            } else {
                char iface[IF_NAMESIZE];
                int err = getgatewayandiface(&gw, iface);
                if (err) {
                    error("TCP: can't find default interface\n");
                    return;
                }
            }
            regs->ecx = gw >> 16;
            regs->edx = gw & 0xffff;
            break;
        }
        case DOS_SUBHELPER_TCP_SETGW:
            config.tcpgw = ((unsigned)(regs->ecx & 0xffff) << 16) |
                    (regs->edx & 0xffff);
            break;
        default:
            CARRY;
            break;
    }
}

static in_addr_t get_ntp(const char *host)
{
    struct addrinfo hints = {};
    struct addrinfo *res;
    struct sockaddr_in *sin;
    in_addr_t ret;
    int err;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    err = getaddrinfo(host, "ntp", &hints, &res);
    if (err) {
        error("getaddrinfo(): %s\n", strerror(errno));
        return 0;
    }
    sin = (struct sockaddr_in *)res->ai_addr;
    ret = sin->sin_addr.s_addr;
    freeaddrinfo(res);
    return ret;
}

static int get_driver_info(struct driver_info_rec *di_out)
{
    struct ifaddrs *addrs, *it;
    char iface[IF_NAMESIZE];
    in_addr_t gw = 0;
    int err;
    int ret = -1;

    if (config.tcpiface) {
        strlcpy(iface, config.tcpiface, sizeof(iface));
    } else {
        err = getgatewayandiface(&gw, iface);
        if (err) {
            error("TCP: can't find default interface\n");
            return -1;
        }
    }
    L_printf("TCP: iface %s\n", iface);
    if (config.tcpgw)
        gw = config.tcpgw;
    err = getifaddrs(&addrs);
    if (err) {
        error("getifaddrs(): %s\n", strerror(errno));
        return -1;
    }
    for (it = addrs; it; it = it->ifa_next) {
        if (strcmp(iface, it->ifa_name) == 0 &&
                it->ifa_addr->sa_family == AF_INET)
            break;
    }
    if (it) {
        struct driver_info_rec di;
        struct sockaddr_in *sin = (struct sockaddr_in *)it->ifa_addr;
        struct sockaddr_in *sinm = (struct sockaddr_in *)it->ifa_netmask;

        myip = sin->sin_addr.s_addr;
        di.myip = sin->sin_addr.s_addr;
        di.netmask = sinm->sin_addr.s_addr;
        di.gateway = gw;
        di.dnsserver = inet_addr(DEFAULT_DNS);
        di.timeserver = get_ntp(DEFAULT_NTP);
        di.mtu = 1500;
        di.def_ttl = 64;
        di.def_tos = 0;
        di.tcp_mss = 1500;
        di.tcp_rwin = 4096;
        di.debug = 0;
        /* use strncpy() to 0-pad entire buffer */
        strncpy(di.domain, "localdomain", sizeof(di.domain));
        *di_out = di;
        ret = 0;
    } else {
        error("TCP: interface %s not found\n", iface);
    }
    freeifaddrs(addrs);
    return ret;
}

static hitimer_t get_timeout_us(uint16_t to)
{
    return (to * 65536ULL * 1000000ULL) / PIT_TICK_RATE;
}

enum CbkRet { CBK_DONE, CBK_CONT, CBK_ERR };

static enum CbkRet conn_cb(int fd, void *sa, int len, int *r_err)
{
    int err = connect(fd, sa, len);
    *r_err = err;
    if (err && errno != EINPROGRESS && errno != EALREADY && errno != EISCONN) {
        error("connect(): %s\n", strerror(errno));
        return CBK_ERR;
    }
    if (!err || errno == EISCONN)
        return CBK_DONE;
    return CBK_CONT;
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

static enum CbkRet read_cb(int fd, void *buf, int len, int *r_err)
{
    int rc = read(fd, buf, len);
    *r_err = rc;
    if (rc == -1 && errno != EAGAIN) {
        error("read(): %s\n", strerror(errno));
        return CBK_ERR;
    }
    if (rc > 0)
        return CBK_DONE;
    return CBK_CONT;
}

static int handle_timeout(uint16_t to,
    enum CbkRet (*cbk)(int, void *, int, int *),
    int arg, void *arg2, int arg3, int *r_err)
{
    hitimer_t end = GETusTIME(0) + get_timeout_us(to);
    int first = 1;
    enum CbkRet cbr;

    do {
        if (!first)
            coopth_wait();
        cbr = cbk(arg, arg2, arg3, r_err);
        if (cbr == CBK_ERR)
            return ERR_CRITICAL;
        if (cbr == CBK_DONE)
            break;
        first = 0;
    } while (to && (to == 0xffff || GETusTIME(0) < end));
    if (cbr != CBK_DONE)
        return ERR_TIMEOUT;
    return ERR_NO_ERROR;
}

static int tcp_connect(uint32_t dest, uint16_t port, uint16_t to,
    uint16_t *r_port, uint16_t *r_hand)
{
    struct sockaddr_in sa;
    int fd, tmp, err, rc;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return ERR_NOHANDLES;
    tmp = 1;
    ioctl(fd, FIONBIO, &tmp); /* non-blocking i/o */
    sa.sin_addr.s_addr = dest;
    sa.sin_port = htons(port);
    sa.sin_family = AF_INET;
    err = handle_timeout(to, conn_cb, fd, &sa,
            sizeof(struct sockaddr_in), &rc);
    if (err != ERR_NO_ERROR) {
        close(fd);
    } else {
        struct sockaddr_in msa;
        int sh;
        struct ses_wrp *s;
        socklen_t l = sizeof(msa);
        getsockname(fd, &msa, &l);
        *r_port = ntohs(msa.sin_port);
        sh = alloc_ses();
        if (sh == -1) {
            error("TCP: out of handles\n");
            close(fd);
            return ERR_NOHANDLES;
        }
        *r_hand = sh;
        s = &ses[sh];
        s->fd = fd;
        s->lfd = -1;
        s->si.ip_srce = msa.sin_addr.s_addr;
        s->si.port_src = msa.sin_port;
        s->si.ip_dest = dest;
        s->si.port_dst = htons(port);
        s->si.ip_prot = IPPROTO_TCP;
        s->si.active = 1;
    }
    return err;
}

static int tcp_listen(uint32_t dest, uint16_t port,
    uint16_t *r_port, uint16_t *r_hand)
{
    struct sockaddr_in sa;
    socklen_t l = sizeof(sa);
    int fd, tmp, rc, sh;
    struct ses_wrp *s;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1)
        return ERR_NOHANDLES;
    tmp = 1;
    ioctl(fd, FIONBIO, &tmp); /* non-blocking i/o */
    sa.sin_addr.s_addr = myip;
    sa.sin_port = htons(port);
    sa.sin_family = AF_INET;
    rc = bind(fd, &sa, sizeof(sa));
    if (rc) {
        error("TCP bind to port %i: %s\n", port, strerror(errno));
        close(fd);
        return ERR_CRITICAL;
    }
    rc = listen(fd, 1);
    if (rc) {
        error("TCP listen: %s\n", strerror(errno));
        close(fd);
        return ERR_CRITICAL;
    }

    getsockname(fd, &sa, &l);
    *r_port = ntohs(sa.sin_port);
    sh = alloc_ses();
    if (sh == -1) {
        error("TCP: out of handles\n");
        close(fd);
        return ERR_NOHANDLES;
    }
    *r_hand = sh;
    s = &ses[sh];
    s->lfd = fd;
    s->fd = -1;
    s->si.ip_srce = sa.sin_addr.s_addr;
    s->si.port_src = sa.sin_port;
    s->si.ip_dest = 0;
    s->si.port_dst = 0;
    s->si.ip_prot = IPPROTO_TCP;
    s->si.active = 1;
    return ERR_NO_ERROR;
}

static int udp_connect(uint32_t dest, uint16_t port,
    uint16_t *r_port, uint16_t *r_hand)
{
    struct sockaddr_in sa;
    int fd, tmp, rc, sh;
    struct ses_wrp *s;
    socklen_t l = sizeof(sa);

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == -1)
        return ERR_NOHANDLES;
    tmp = 1;
    ioctl(fd, FIONBIO, &tmp); /* non-blocking i/o */
    sa.sin_addr.s_addr = dest;
    sa.sin_port = htons(port);
    sa.sin_family = AF_INET;
    rc = connect(fd, &sa, sizeof(sa));
    if (rc) {
        error("UDP connect: %s\n", strerror(errno));
        close(fd);
        return ERR_CRITICAL;
    }

    getsockname(fd, &sa, &l);
    *r_port = ntohs(sa.sin_port);
    sh = alloc_ses();
    if (sh == -1) {
        error("TCP: out of handles\n");
        close(fd);
        return ERR_NOHANDLES;
    }
    *r_hand = sh;
    s = &ses[sh];
    s->fd = fd;
    s->lfd = -1;
    s->si.ip_srce = sa.sin_addr.s_addr;
    s->si.port_src = sa.sin_port;
    s->si.ip_dest = dest;
    s->si.port_dst = htons(port);
    s->si.ip_prot = IPPROTO_UDP;
    s->si.active = 1;
    return ERR_NO_ERROR;
}

static int icmp_connect(uint32_t dest, uint16_t *r_hand)
{
    struct sockaddr_in sa;
    int fd, tmp, rc, sh;
    struct ses_wrp *s;
    socklen_t l = sizeof(sa);

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
    if (fd == -1)
        return ERR_NOHANDLES;
    tmp = 1;
    ioctl(fd, FIONBIO, &tmp); /* non-blocking i/o */
    sa.sin_addr.s_addr = dest;
    sa.sin_port = 0;
    sa.sin_family = AF_INET;
    rc = connect(fd, &sa, sizeof(sa));
    if (rc) {
        error("ICMP connect: %s\n", strerror(errno));
        close(fd);
        return ERR_CRITICAL;
    }

    getsockname(fd, &sa, &l);
    sh = alloc_ses();
    if (sh == -1) {
        error("TCP: out of handles\n");
        close(fd);
        return ERR_NOHANDLES;
    }
    *r_hand = sh;
    s = &ses[sh];
    s->fd = fd;
    s->lfd = -1;
    s->si.ip_srce = sa.sin_addr.s_addr;
    s->si.port_src = 0;
    s->si.ip_dest = dest;
    s->si.port_dst = 0;
    s->si.ip_prot = IPPROTO_ICMP;
    s->si.active = 1;
    return ERR_NO_ERROR;
}

static void tcp_thr(void *arg)
{
    int err;

#define __S(x) #x
#define _S(x) __S(x)
#define _D(x) \
  case x: \
    error("TCP call %s\n", _S(x)); \
    CARRY; \
    break

    switch (HI(ax)) {
        case TCP_DRIVER_INFO:
            err = get_driver_info(LINEAR2UNIX(SEGOFF2LINEAR(
                    TCPDRV_SEG, TCPDRV_driver_info)));
            if (err) {
                CARRY;
            } else {
                NOCARRY;
                _AX = 0;
                _DX = (1 << 8);  // support timeouts
                _DS = TCPDRV_SEG;
                _SI = TCPDRVR_OFF;
                _ES = TCPDRV_SEG;
                _DI = TCPDRV_driver_info;
            }
            break;

        _D(TCP_DRIVER_UNLOAD);

        case TCP_DRIVER_DOIO:
            _AX = 10;
            _CX = 10;
            _DX = ERR_NO_ERROR;
            break;

        _D(TCP_DRIVER_CRIT_FLAG);
        _D(TCP_COPY_DRIVER_INFO);

        case TCP_OPEN:
            switch (LO(ax)) {
                case 0: {
                    uint16_t lport, h;
                    err = tcp_connect(((unsigned)_SI << 16) | _DI, _CX, _DX, &lport, &h);
                    if (err) {
                        CARRY;
                    } else {
                        NOCARRY;
                        _AX = lport;
                        _BX = h;
                    }
                    _DX = err;
                    break;
                }
                case 1: {
                    uint16_t lport, h;
                    err = tcp_listen(((unsigned)_SI << 16) | _DI, _BX, &lport, &h);
                    if (err) {
                        CARRY;
                    } else {
                        NOCARRY;
                        _AX = lport;
                        _BX = h;
                    }
                    _DX = err;
                    break;
                }
                default:
                    error("TCP open flag %x unsupported\n", LO(ax));
                    CARRY;
                    break;
            }
            break;

#define TCP_PROLOG \
            struct ses_wrp *s; \
            if (_BX >= num_ses) { \
                CARRY; \
                _DX = ERR_BADHANDLE; \
                break; \
            } \
            s = &ses[_BX]; \
            if (!s->used) { \
                CARRY; \
                _DX = ERR_BADHANDLE; \
                break; \
            } \
            NOCARRY; \
            _DX = ERR_NO_ERROR
#define TCP_PROLOG2 \
            int to = _DX; \
            TCP_PROLOG

        case _TCP_CLOSE: {
            TCP_PROLOG;
            if (s->fd != -1) {
                if (LO(ax) == 0)
                    shutdown(s->fd, SHUT_RDWR);
                close(s->fd);
            }
            if (s->lfd != -1)
                close(s->lfd);
            free_ses(_BX);
            break;
        }

        case TCP_GET: {
            TCP_PROLOG2;
            if (_DX)
                error("TCP get timeout unsupported\n");
            switch (LO(ax)) {
                case 0:
                    error("TCP full get unimplemented\n");
                    /* break; */
                case 1: {
                    int rc;
                    _DX = handle_timeout(to, read_cb, s->fd,
                            SEG_ADR((char *), es, di), _CX, &rc);
                    _AX = (rc < 0 ? 0 : rc);
                    break;
                }
                case 2: {
                    FILE *f = fdopen(dup(s->fd), "r");
                    char *s, buf[4096];

                    setbuf(f, NULL);
                    s = fgets(buf, sizeof(buf), f);
                    fclose(f);
                    _AX = 0;
                    if (s) {
                        struct char_set_state kstate;
                        struct char_set_state dstate;
                        int len = 0;
                        char *p = strpbrk(s, "\r\n");
                        const char *p1 = s;
                        if (p) {
                            *p = '\0';
                            HI(dx) = 2;  // cr/lf skipped
                        }
                        init_charset_state(&kstate, trconfig.keyb_charset);
                        init_charset_state(&dstate, trconfig.dos_charset);
                        while (*p1) {
                            int rc;
#define MAX_LEN (1024+1)
                            t_unicode uni[MAX_LEN];
                            const t_unicode *u = uni;
                            char buf2[MAX_LEN * MB_LEN_MAX];
                            rc = charset_to_unicode_string(&kstate, uni, &p1, strlen(p1),
                                    MAX_LEN);
                            if (rc <= 0)
                                break;
                            rc = unicode_to_charset_string(&dstate, buf2, &u, rc,
                                    sizeof(buf2));
                            if (rc <= 0)
                                break;
                            if (len + rc > _CX)
                                break;
                            MEMCPY_2DOS(SEGOFF2LINEAR(_ES, _DI) + len, buf2, rc);
                            len += rc;
                        }
                        cleanup_charset_state(&kstate);
                        cleanup_charset_state(&dstate);
                        _AX = len;
                    }
                    break;
                }
                default:
                    error("TCP get flag %x unsupported\n", LO(ax));
                    break;
            }
            break;
        }

        case TCP_PUT: {
            int len = 0, rc;
            char buf[4096];
            TCP_PROLOG;
            if (_CX >= sizeof(buf)) {
                error("TCP: too large write, %i\n", _CX);
                _DX = ERR_CRITICAL;
                break;
            }
            if ((LO(ax) & ~4) == 2) {
                struct char_set_state ostate;
                struct char_set_state dstate;
                char *p = strndup(SEG_ADR((char *), es, di), _CX);
                const char *p1;

                init_charset_state(&ostate, trconfig.output_charset);
                init_charset_state(&dstate, trconfig.dos_charset);
                p1 = p;
                while (*p1) {
                    int rc;
                    t_unicode uni[MAX_LEN];
                    const t_unicode *u = uni;
                    char buf2[MAX_LEN * MB_LEN_MAX];
                    rc = charset_to_unicode_string(&dstate, uni, &p1, strlen(p1),
                            MAX_LEN);
                    if (rc <= 0)
                        break;
                    rc = unicode_to_charset_string(&ostate, buf2, &u, rc,
                            sizeof(buf2));
                    if (rc <= 0)
                        break;
                    if (len + rc >= sizeof(buf)) {
                        error("TCP: buffer overflow\n");
                        break;
                    }
                    memcpy(buf + len, buf2, rc);
                    len += rc;
                }
                cleanup_charset_state(&ostate);
                cleanup_charset_state(&dstate);
                free(p);
                if (len + 2 >= sizeof(buf)) {
                    error("TCP: buffer overflow\n");
                    _DX = ERR_CRITICAL;
                    break;
                }
                memcpy(buf + len, "\r\n", 2);
                len += 2;
            } else {
                len = _CX;
                memcpy(buf, SEG_ADR((char *), es, di), len);
            }
            buf[len] = '\0';
            rc = 0;
            switch (LO(ax) & ~4) {
                case 0:
                    error("TCP full put unimplemented\n");
                    /* no break */
                case 1:
                case 2:
                    rc = write(s->fd, buf, len);
                    break;
                default:
                    error("TCP put flag %x unsupported\n", LO(ax));
                    break;
            }
            if (rc == -1) {
                _DX = ERR_CRITICAL;
                break;
            }
            if ((LO(ax) & ~4) == 2) {
                assert(rc >= 2);
                rc -= 2;  // unaccount cr/lf
            }
            _AX = rc;
            break;
        }

        case TCP_STATUS: {
            TCP_PROLOG;
            if (s->fd == -1) {  // listener
                struct sockaddr_in sin;
                socklen_t sil = sizeof(sin);
                s->fd = accept(s->lfd, &sin, &sil);
                if (s->fd != -1) {
                    s->si.ip_dest = sin.sin_addr.s_addr;
                    s->si.port_dst = sin.sin_port;
                }
            }
            if (s->fd == -1) {
                _AX = 0;
                _CX = 0;
                HI(dx) = TS_LISTEN;
            } else {
                struct tcp_info ti = {};
                socklen_t sl = sizeof(ti);
                int nr = 0, nw = 0;
                ioctl(s->fd, FIONREAD, &nr);
                ioctl(s->fd, TIOCOUTQ, &nw);
                getsockopt(s->fd, SOL_TCP, TCP_INFO, &ti, &sl);
                _AX = nr;
                _CX = nw;
                HI(dx) = get_tcp_state(ti.tcpi_state);
            }
            _ES = TCPDRV_SEG;
            _DI = TCPDRV_session_info;
            MEMCPY_2DOS(SEGOFF2LINEAR(_ES, _DI), &s->si, sizeof(s->si));
            break;
        }

        case UDP_OPEN: {
            uint16_t lport, h;
            err = udp_connect(((unsigned)_SI << 16) | _DI, _CX, &lport, &h);
            if (err != ERR_NO_ERROR) {
                CARRY;
            } else {
                NOCARRY;
                _AX = lport;
                _BX = h;
            }
            _DX = err;
            break;
        }

        case UDP_CLOSE: {
            TCP_PROLOG;
            close(s->fd);
            free_ses(_BX);
            break;
        }

        case UDP_RECV: {
            int rc;
            TCP_PROLOG2;
            _DX = handle_timeout(to, recv_cb, s->fd,
                    SEG_ADR((char *), es, di), _CX, &rc);
            _AX = (rc < 0 ? 0 : rc);
            break;
        }

        case UDP_SEND: {
            int rc;
            TCP_PROLOG;
            rc = send(s->fd, SEG_ADR((char *), es, di), _CX, 0);
            if (rc == -1) {
                error("UDP send: %s\n", strerror(errno));
                _DX = ERR_CRITICAL;
                break;
            } else {
                _AX = rc;
            }
            break;
        }

        _D(UDP_STATUS);

        case IP_OPEN: {
            uint16_t h;
            if (LO(bx) != IPPROTO_ICMP) {
                error("TCP: ipproto %i unsupported\n", LO(bx));
                _DX = ERR_CRITICAL;
                break;
            }
            err = icmp_connect(((unsigned)_SI << 16) | _DI, &h);
            if (err != ERR_NO_ERROR) {
                CARRY;
            } else {
                NOCARRY;
                _AX = h;
                _BX = h;  // ??? usually handle returned in BX
            }
            _DX = err;
            break;
        }

        case IP_CLOSE: {
            TCP_PROLOG;
            close(s->fd);
            free_ses(_BX);
            break;
        }

        case IP_RECV: {
            int rc;
            TCP_PROLOG2;
            _DX = handle_timeout(to, recv_cb, s->fd,
                    SEG_ADR((char *), es, di), _CX, &rc);
            _AX = (rc < 0 ? 0 : rc);
            break;
        }

        case IP_SEND: {
            int rc;
            TCP_PROLOG;
            if (_CX < 8) {
                error("ICMP: len too small, %i\n", _CX);
                _DX = ERR_CRITICAL;
                break;
            }
            rc = send(s->fd, SEG_ADR((char *), es, di), _CX, MSG_DONTWAIT);
            if (rc == -1) {
                error("ICMP send: %s\n", strerror(errno));
                _DX = ERR_CRITICAL;
                break;
            } else {
                _AX = rc;
            }
            break;
        }

        case IP_STATUS: {
            int nr;
            char buf[4096];
            TCP_PROLOG;
            nr = recv(s->fd, buf, sizeof(buf), MSG_DONTWAIT | MSG_PEEK);
            if (nr < 0)
                nr = 0;
            _AX = nr;
            _CX = 0;
            _ES = TCPDRV_SEG;
            _DI = TCPDRV_session_info;
            MEMCPY_2DOS(SEGOFF2LINEAR(_ES, _DI), &s->si, sizeof(s->si));
            break;
        }
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
    MEMSET_DOS(SEGOFF2LINEAR(PKTDRV_SEG, TCPDRV_iface_name), '\0', 16);
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
    int i;

    for (i = 0; i < num_ses; i++) {
        if (ses[i].used) {
            struct ses_wrp *s = &ses[i];
            if (s->fd != -1)
                close(s->fd);
            if (s->lfd != -1)
                close(s->lfd);
            free_ses(i);
        }
    }
}
