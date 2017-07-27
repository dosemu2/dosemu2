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
 * Purpose: VDE networking support.
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
#include <libvdeplug.h>
#include "emu.h"
#include "init.h"
#include "utilities.h"
#include "pktdrvr.h"
#include "sig.h"

static VDECONN *vde;
static struct popen2 vdesw, slirp;

static void vde_exit(void)
{
    error("vde failed, exiting\n");
    leavedos(35);
}

static char *start_vde(void)
{
    char cmd[256];
    int err, n, started, fd, pmin, pmax, q;
    fd_set fds;
    struct timeval tv;
    char *nam = tmpnam(NULL);
    fd = -1;
    q = (pkt_get_flags() & PKT_FLG_QUIET);
    if (q) {
	fd = dup(STDERR_FILENO);
	close(STDERR_FILENO);
	open("/dev/null", O_WRONLY);
    }
    snprintf(cmd, sizeof(cmd), "vde_switch -s %s", nam);
    err = popen2(cmd, &vdesw);
    if (fd != -1) {
	close(STDERR_FILENO);
	dup(fd);
	close(fd);
    }
    if (err) {
	error("failed to start %s\n", cmd);
	goto fail2;
    }
    FD_ZERO(&fds);
    FD_SET(vdesw.from_child, &fds);
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    err = select(vdesw.from_child + 1, &fds, NULL, NULL, &tv);
    switch(err) {
    case -1:
	error("select failed: %s\n", strerror(errno));
	goto fail1;
    case 0:
	error("you appear to have unpatched vde\n");
	break;
    default:
	n = read(vdesw.from_child, cmd, sizeof(cmd) - 1);
	switch (n) {
	case -1:
	    error("read failed: %s\n", strerror(errno));
	    goto fail1;
	case 0:
	    if (!q)
		error("failed to start %s\n", cmd);
	    goto fail1;
	}
	cmd[n] = 0;
	if (!strstr(cmd, " started")) {
	    error("vde_switch failed: %s\n", cmd);
	    goto fail1;
	}

	/* check for userspace pings */
	fd = open("/proc/sys/net/ipv4/ping_group_range", O_RDONLY);
	if (fd == -1) {
	    pd_printf("PKT: no /proc/sys/net/ipv4/ping_group_range??\n");
	    goto out;
	}
	n = read(fd, cmd, sizeof(cmd) - 1);
	if (n <= 0)
	    goto out2;
	cmd[n] = 0;
	sscanf(cmd, "%i %i", &pmin, &pmax);
	/* this is not a strict check, but default value is 1 0 */
	if (pmin > pmax)
	    error("userspace pings are disabled.\n"
		    "\tTo enable them, do as root:\n"
		    "\techo 0 65535 > /proc/sys/net/ipv4/ping_group_range\n");
	pd_printf("PKT: ping_group_range is %s", cmd);
out2:
	close(fd);
out:
	break;
    }
    snprintf(cmd, sizeof(cmd), "slirpvde -s %s %s 2>&1", nam,
	     config.slirp_args ? : "");
    err = popen2(cmd, &slirp);
    if (err) {
	error("failed to start %s\n", cmd);
	goto fail1;
    }
    started = 0;
    while (1) {
	n = read(slirp.from_child, cmd, sizeof(cmd) - 1);
	if (n <= 0)
	    break;
	cmd[n] = 0;
	pd_printf("slirp: %s", cmd);
	if (strstr(cmd, "vde switch")) {
	    started = 1;
	    break;
	}
    }
    if (!started) {
	error("failed to start slirpvde\n");
	goto fail0;
    }

    sigchld_register_handler(vdesw.child_pid, vde_exit);
    sigchld_register_handler(slirp.child_pid, vde_exit);
    pd_printf("PKT: started VDE at %s\n", nam);
    return nam;

  fail0:
    pclose2(&slirp);
  fail1:
    pclose2(&vdesw);
  fail2:
    return NULL;
}

static int OpenNetworkLinkVde(char *name)
{
    if (!name[0]) {
	name = start_vde();
	if (!name)
	    return -1;
    }
    vde = vde_open(name, "dosemu", NULL);
    if (!vde)
	return -1;
    receive_mode = 6;
    return vde_datafd(vde);
}

static void CloseNetworkLinkVde(int pkt_fd)
{
    remove_from_io_select(pkt_fd);
    vde_close(vde);
    sigchld_enable_handler(slirp.child_pid, 0);
    pclose2(&slirp);
    sigchld_enable_handler(vdesw.child_pid, 0);
    pclose2(&vdesw);
}

static int GetDeviceMTUVde(void)
{
    return 1500;
}

static int GetDeviceHardwareAddressVde(unsigned char *addr)
{
    pkt_get_fake_mac(addr);
    return 0;
}

static ssize_t pkt_read_vde(int pkt_fd, void *buf, size_t count)
{
    ssize_t len;
    fd_set set;
    struct timeval tv = { };
    int selrt;
    FD_ZERO(&set);
    FD_SET(pkt_fd, &set);
    /* vde_recv() ignores flags... so we use select() */
    selrt = select(pkt_fd + 1, &set, NULL, NULL, &tv);
    if (selrt <= 0)
	return 0;
    len = vde_recv(vde, buf, count, MSG_DONTWAIT);
    if (len == 0) {
	error("VDE unexpectedly terminated\n");
	leavedos(3);
    }
    if (len < 0)
	error("recv() returned %zi, %s\n", len, strerror(errno));
    return len;
}

static ssize_t pkt_write_vde(int pkt_fd, const void *buf, size_t count)
{
    ssize_t ret;
    ret = vde_send(vde, buf, count, 0);
    if (ret == -1) {
	int err = errno;
	error("vde_send(): %s\n", strerror(err));
	if (err == ENOTCONN || err == ECONNREFUSED) {
	    error("VDE unexpectedly terminated\n");
	    leavedos(3);
	}
    }
    return ret;
}

static struct pkt_ops vde_ops = {
    .id = VNET_TYPE_VDE,
    .open = OpenNetworkLinkVde,
    .close = CloseNetworkLinkVde,
    .get_hw_addr = GetDeviceHardwareAddressVde,
    .get_MTU = GetDeviceMTUVde,
    .pkt_read = pkt_read_vde,
    .pkt_write = pkt_write_vde,
};

CONSTRUCTOR(static void vde_init(void))
{
    pkt_register_backend(&vde_ops);
}
