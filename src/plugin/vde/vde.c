#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <libvdeplug.h>
#include "emu.h"
#include "init.h"
#include "utilities.h"
#include "pktdrvr.h"

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
    int err, n;
    char *nam = tmpnam(NULL);
    snprintf(cmd, sizeof(cmd), "vde_switch -s %s", nam);
    err = popen2(cmd, &vdesw);
    if (err) {
	error("failed to start %s\n", cmd);
	goto fail2;
    }
    sigchld_register_handler(vdesw.child_pid, vde_exit);
    n = read(vdesw.from_child, cmd, sizeof(cmd));
    if (n <= 0) {
	error("read failed: %s\n", strerror(errno));
	goto fail1;
    }
    cmd[n] = 0;
    if (!strstr(cmd, " started")) {
	error("vde_switch failed: %s\n", cmd);
	goto fail1;
    }
    snprintf(cmd, sizeof(cmd), "slirpvde -s %s %s 2>&1", nam,
	     config.slirp_args ? : "");
    err = popen2(cmd, &slirp);
    if (err) {
	error("failed to start %s\n", cmd);
	goto fail1;
    }
    sigchld_register_handler(slirp.child_pid, vde_exit);
    while (1) {
	n = read(slirp.from_child, cmd, sizeof(cmd));
	if (n <= 0)
	    break;
	cmd[n] = 0;
	pd_printf("slirp: %s", cmd);
	if (strstr(cmd, "vde switch"))
	    break;
    }
    pd_printf("PKT: started VDE at %s\n", nam);
    return nam;

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

static int GetDeviceMTUVde(char *device)
{
    return 1500;
}

static int GetDeviceHardwareAddressVde(char *device, unsigned char *addr)
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

CONSTRUCTOR(static void vde_init(void))
{
    struct pkt_ops o;
    o.id = VNET_TYPE_VDE;
    o.open = OpenNetworkLinkVde;
    o.close = CloseNetworkLinkVde;
    o.get_hw_addr = GetDeviceHardwareAddressVde;
    o.get_MTU = GetDeviceMTUVde;
    o.pkt_read = pkt_read_vde;
    o.pkt_write = pkt_write_vde;
    pkt_register_backend(&o);
}
