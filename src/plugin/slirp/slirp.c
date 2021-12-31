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
 * Purpose: slirp networking support.
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include "libvdeslirp.h"
#include "emu.h"
#include "init.h"
#include "utilities.h"
#include "pktdrvr.h"

static SlirpConfig slirpcfg;
static struct vdeslirp *myslirp;

static int OpenNetworkLinkSlirp(const char *name, void (*cbk)(int, int))
{
    int fd;

    vdeslirp_init(&slirpcfg, VDE_INIT_DEFAULT);
    myslirp = vdeslirp_open(&slirpcfg);
    if (!myslirp)
	return -1;
    /* check for userspace pings */
    fd = open("/proc/sys/net/ipv4/ping_group_range", O_RDONLY);
    if (fd != -1) {
	char cmd[256];
	int pmin, pmax;
	int n = read(fd, cmd, sizeof(cmd) - 1);
	if (n > 0) {
	    cmd[n] = 0;
	    sscanf(cmd, "%i %i", &pmin, &pmax);
	    /* this is not a strict check, but default value is 1 0 */
	    if (pmin > pmax)
		error("userspace pings are disabled.\n"
		    "\tTo enable them, do as root:\n"
		    "\techo 0 65535 > /proc/sys/net/ipv4/ping_group_range\n");
	    pd_printf("PKT: ping_group_range is %s", cmd);
	}
	close(fd);
    } else {
	pd_printf("PKT: no /proc/sys/net/ipv4/ping_group_range??\n");
    }
    cbk(vdeslirp_fd(myslirp), 6);
    return 0;
}

static void CloseNetworkLinkSlirp(int pkt_fd)
{
    vdeslirp_close(myslirp);
}

static int GetDeviceMTUSlirp(void)
{
    return slirpcfg.if_mtu;
}

static int GetDeviceHardwareAddressSlirp(unsigned char *addr)
{
    pkt_get_fake_mac(addr);
    return 0;
}

static ssize_t pkt_read_slirp(int pkt_fd, void *buf, size_t count)
{
    ssize_t len;
    fd_set set;
    struct timeval tv = { };
    int selrt;
    FD_ZERO(&set);
    FD_SET(pkt_fd, &set);
    selrt = select(pkt_fd + 1, &set, NULL, NULL, &tv);
    if (selrt <= 0)
	return 0;
    len = vdeslirp_recv(myslirp, buf, count);
    if (len == 0) {
	error("slirp unexpectedly terminated\n");
	leavedos(3);
    }
    if (len < 0)
	error("recv() returned %zi, %s\n", len, strerror(errno));
    return len;
}

static ssize_t pkt_write_slirp(int pkt_fd, const void *buf, size_t count)
{
    ssize_t ret;
    ret = vdeslirp_send(myslirp, buf, count);
    if (ret == -1) {
	int err = errno;
	error("slirp_send(): %s\n", strerror(err));
	if (err == ENOTCONN || err == ECONNREFUSED) {
	    error("slirp unexpectedly terminated\n");
	    leavedos(3);
	}
    }
    return ret;
}

static struct pkt_ops slirp_ops = {
    .id = VNET_TYPE_SLIRP,
    .open = OpenNetworkLinkSlirp,
    .close = CloseNetworkLinkSlirp,
    .get_hw_addr = GetDeviceHardwareAddressSlirp,
    .get_MTU = GetDeviceMTUSlirp,
    .pkt_read = pkt_read_slirp,
    .pkt_write = pkt_write_slirp,
};

CONSTRUCTOR(static void slirpnet_init(void))
{
    pkt_register_backend(&slirp_ops);
}
