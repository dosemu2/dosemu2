/*
 *	SOCK_PACKET support.
 *	Placed under the GNU LGPL.
 *
 *	First cut at a library of handy support routines. Comments, additions
 *	and bug fixes greatfully received.
 *
 *	(c) 1994 Alan Cox	iiitac@pyr.swan.ac.uk	GW4PTS@GB7SWN
 */
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <net/if.h>
#include <netinet/in.h>
#include "Linux/if_tun.h"
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>
#include <assert.h>

#include "emu.h"
#include "utilities.h"
#include "priv.h"
#include "pktdrvr.h"
#include "libpacket.h"

#define TAP_DEVICE  "dosemu_tap%d"

static int tun_alloc(char *dev);
static int pkt_is_registered_type(int type);

static uint8_t local_eth_addr[6] = {0,0,0,0,0,0};
#define DOSNET_FAKED_ETH_ADDRESS   "fbx\x90xx"

static int num_backends;
static struct pkt_ops *ops[VNET_TYPE_MAX];

static int pkt_flags;
static int early_fd;
static int rcv_mode;

/* Should return a unique ID corresponding to this invocation of
   dosemu not clashing with other dosemus. We use a random value and
   hope for the best.
   */

static void GenerateDosnetID(void)
{
	pid_t pid = getpid();
	memcpy(local_eth_addr, DOSNET_FAKED_ETH_ADDRESS, 6);
	assert((local_eth_addr[0] & 3) == 2);
	memcpy(local_eth_addr + 3, &pid, 2);
	local_eth_addr[5] = rand();
}

static struct pkt_ops *find_ops(int id)
{
	int i;
	for (i = 0; i < num_backends; i++) {
		if (ops[i]->id == id)
			return ops[i];
	}
	return NULL;
}

/*
 *	Obtain a file handle on a raw ethernet type. In actual fact
 *	you can also request the dummy types for AX.25 or 802.3 also
 *
 *	-1 indicates an error
 *	0  or higher is a file descriptor which we have set non blocking
 *
 *	WARNING: It is ok to listen to a service the system is using (eg arp)
 *	but don't try and run a user mode stack on the same service or all
 *	hell will break loose - unless you use virtual TCP/IP (dosnet).
 */

static int OpenNetworkLinkEth(char *name, void (*cbk)(int, int))
{
	PRIV_SAVE_AREA
	int s, proto, ret;
	struct ifreq req;
	struct sockaddr_ll addr;
	unsigned short receive_mode;

	proto = htons(ETH_P_ALL);
	enter_priv_on();
	s = socket(PF_PACKET, SOCK_RAW, proto);
	leave_priv_setting();
	if (s < 0) {
		if (errno == EPERM) error("Must be root for direct NIC access\n");
		return -1;
	}
	fcntl(s, F_SETFL, O_NDELAY);
	strcpy(req.ifr_name, name);
	if (ioctl(s, SIOCGIFINDEX, &req) < 0) {
		close(s);
		return -1;
	}
	addr.sll_family = AF_PACKET;
	addr.sll_protocol = proto;
	addr.sll_ifindex = req.ifr_ifindex;
	if (bind(s, (void *)&addr, sizeof(addr)) < 0) {
		pd_printf("OpenNetwork: could not bind socket: %s\n",
			strerror(errno));
		close(s);
		return -1;
	}

	enter_priv_on();
	ret = ioctl(s, SIOCGIFFLAGS, &req);
	leave_priv_setting();
	if (ret < 0) {
		close(s);
		return -1;
	}

	receive_mode = (req.ifr_flags & IFF_PROMISC) ? 6 :
		((req.ifr_flags & IFF_BROADCAST) ? 3 : 2);

	cbk(s, receive_mode);
	return 0;
}

static int OpenNetworkLinkTap(char *name, void (*cbk)(int, int))
{
	int pkt_fd = tun_alloc(name);
	if (pkt_fd < 0)
		return pkt_fd;
	cbk(pkt_fd, 6);
	return 0;
}

static void set_fd(int fd, int mode)
{
	early_fd = fd;
	rcv_mode = mode;
}

static int Open_sockets(char *name, int vnet)
{
	struct pkt_ops *o = find_ops(vnet);
	if (!o)
		return -1;
	return o->open(name, set_fd);
}

int OpenNetworkLink(void (*cbk)(int, int))
{
	int ret = -1;
	struct pkt_ops *o;

	assert(early_fd != 0);
	if (early_fd != -1) {
		cbk(early_fd, rcv_mode);
		return 0;
	}
	/* try non-priv setups like vde */
	switch (config.vnet) {
	case VNET_TYPE_AUTO:
		pkt_set_flags(PKT_FLG_QUIET);
		/* no break */
	case VNET_TYPE_VDE: {
		const char *pr_dev = config.vdeswitch[0] ? config.vdeswitch : "(auto)";
		if (!pkt_is_registered_type(VNET_TYPE_VDE)) {
			if (config.vnet != VNET_TYPE_AUTO)
				error("vde support is not compiled in\n");
			break;
		}
		o = find_ops(VNET_TYPE_VDE);
		if (!o)
			ret = -1;
		else
			ret = o->open(config.vdeswitch, cbk);
		if (ret < 0) {
			if (config.vnet == VNET_TYPE_AUTO)
				warn("PKT: Cannot run VDE %s\n", pr_dev);
			else
				error("Unable to run VDE %s\n", pr_dev);
		} else {
			if (config.vnet == VNET_TYPE_AUTO)
				config.vnet = VNET_TYPE_VDE;
			pd_printf("PKT: Using device %s\n", pr_dev);
		}
		break;
	}
	}
	return ret;
}

/*
 *	Close a file handle to a raw packet type.
 */

static void CloseNetworkLinkEth(int pkt_fd)
{
	close(pkt_fd);
}

void CloseNetworkLink(int pkt_fd)
{
	find_ops(config.vnet)->close(pkt_fd);
}

/*
 *	Handy support routines.
 */

/*
 *	NET2 or NET3 - work for both.
 *      (NET3 is valid for all kernels > 1.3.38)
 */
#define NET3

/*
 *	Obtain the hardware address of an interface.
 *	addr should be a buffer of 8 bytes or more.
 *
 *	Return:
 *	0	Success, buffer holds data.
 *	-1	Error.
 */

static int GetDeviceHardwareAddressEth(unsigned char *addr)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq req;
	int err;

	strcpy(req.ifr_name, config.ethdev);

	err = ioctl(s, SIOCGIFHWADDR, &req);
	close(s);
	if (err == -1)
		return err;
#ifdef NET3
	memcpy(addr, req.ifr_hwaddr.sa_data,8);
#else
	memcpy(addr, req.ifr_hwaddr, 8);
#endif

	return 0;
}

void pkt_get_fake_mac(unsigned char *addr)
{
	memcpy(addr, local_eth_addr, 6);
}

static int GetDeviceHardwareAddressTap(unsigned char *addr)
{
	/* This routine is totally local; doesn't make
	   request to actual device. */
	pkt_get_fake_mac(addr);
	return 0;
}

int GetDeviceHardwareAddress(unsigned char *addr)
{
	int i;
	int ret = find_ops(config.vnet)->get_hw_addr(addr);
	pd_printf("Assigned Ethernet Address = ");
	for (i=0; i < 6; i++)
		pd_printf("%02x:", local_eth_addr[i] & 0xff);
	pd_printf("\n");
	return ret;
}

/*
 *	Obtain the maximum packet size on an interface.
 *
 *	Return:
 *	>0	Return is the mtu of the interface
 *	-1	Error.
 */

static int GetDeviceMTUEth(void)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq req;
	int err;

	strcpy(req.ifr_name, config.ethdev);

	err = ioctl(s, SIOCGIFMTU, &req);
	close(s);
	if (err < 0)
		return -1;
	return req.ifr_mtu;
}

int GetDeviceMTU(void)
{
	return find_ops(config.vnet)->get_MTU();
}

int tun_alloc(char *dev)
{
      PRIV_SAVE_AREA
      struct ifreq ifr;
      int fd, err;

      enter_priv_on();
      fd = open("/dev/net/tun", O_RDWR);
      leave_priv_setting();
      if (fd < 0)
         return -1;

      memset(&ifr, 0, sizeof(ifr));

      /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
       *        IFF_TAP   - TAP device
       *
       *        IFF_NO_PI - Do not provide packet information
       */
      ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
      if( *dev )
         snprintf(ifr.ifr_name, IFNAMSIZ, "%s", dev);

      enter_priv_on();
      err = ioctl(fd, TUNSETIFF, (void *) &ifr);
      leave_priv_setting();
      if (err < 0 ) {
         close(fd);
         return err;
      }
      strcpy(dev, ifr.ifr_name);

      return fd;
}

static ssize_t pkt_read_eth(int pkt_fd, void *buf, size_t count)
{
    struct timeval tv;
    fd_set readset;

    tv.tv_sec = 0;				/* set a (small) timeout */
    tv.tv_usec = 0;

    /* anything ready? */
    FD_ZERO(&readset);
    FD_SET(pkt_fd, &readset);
    /* anything ready? */
    if (select(pkt_fd + 1, &readset, NULL, NULL, &tv) <= 0)
        return 0;

    if(!FD_ISSET(pkt_fd, &readset))
        return 0;

    return read(pkt_fd, buf, count);
}

ssize_t pkt_read(int fd, void *buf, size_t count)
{
    return find_ops(config.vnet)->pkt_read(fd, buf, count);
}

static ssize_t pkt_write_eth(int pkt_fd, const void *buf, size_t count)
{
    return write(pkt_fd, buf, count);
}

ssize_t pkt_write(int fd, const void *buf, size_t count)
{
    return find_ops(config.vnet)->pkt_write(fd, buf, count);
}

int pkt_register_backend(struct pkt_ops *o)
{
    int idx = num_backends++;
    assert(idx < ARRAY_SIZE(ops));
    ops[idx] = o;
    return idx;
}

static struct pkt_ops eth_ops = {
	.id = VNET_TYPE_ETH,
	.open = OpenNetworkLinkEth,
	.close = CloseNetworkLinkEth,
	.get_hw_addr = GetDeviceHardwareAddressEth,
	.get_MTU = GetDeviceMTUEth,
	.pkt_read = pkt_read_eth,
	.pkt_write = pkt_write_eth,
};

static struct pkt_ops tap_ops = {
	.id = VNET_TYPE_TAP,
	.open = OpenNetworkLinkTap,
	.close = CloseNetworkLinkEth,
	.get_hw_addr = GetDeviceHardwareAddressTap,
	.get_MTU = GetDeviceMTUEth,
	.pkt_read = pkt_read_eth,
	.pkt_write = pkt_write_eth,
};

void LibpacketInit(void)
{
	int ret;

	GenerateDosnetID();

	pkt_register_backend(&eth_ops);
	pkt_register_backend(&tap_ops);

#ifdef USE_DL_PLUGINS
#ifdef USE_VDE
	load_plugin("vde");
#endif
#endif
	early_fd = -1;
	/* Open sockets only for priv configs */
	switch (config.vnet) {
	case VNET_TYPE_ETH:
		pd_printf("PKT: Using ETH device %s\n", config.ethdev);
		ret = Open_sockets(config.ethdev, VNET_TYPE_ETH);
		if (ret < 0)
			error("PKT: Cannot open %s: %s\n", config.ethdev, strerror(errno));
		break;
	case VNET_TYPE_AUTO:
	case VNET_TYPE_TAP: {
		char devname[256];
		if (!config.tapdev || !config.tapdev[0]) {
			pd_printf("PKT: Using dynamic TAP device\n");
			strcpy(devname, TAP_DEVICE);
		} else {
			pd_printf("PKT: trying to bind to TAP device %s\n", config.tapdev);
			strcpy(devname, config.tapdev);
		}
		ret = Open_sockets(devname, VNET_TYPE_TAP);
		if (ret < 0) {
			if (config.vnet != VNET_TYPE_AUTO) {
				error("PKT: Cannot open %s: %s\n", devname, strerror(errno));
			} else {
				pd_printf("PKT: Cannot open %s: %s\n", devname, strerror(errno));
			}
		} else {
			if (config.vnet == VNET_TYPE_AUTO)
				config.vnet = VNET_TYPE_TAP;
			pd_printf("PKT: Using device %s\n", devname);
		}
		break;
	}
	}
}

void pkt_set_flags(int flags)
{
	pkt_flags |= flags;
}

void pkt_clear_flags(int flags)
{
	pkt_flags &= ~flags;
}

int pkt_get_flags(void)
{
	return pkt_flags;
}

static int pkt_is_registered_type(int type)
{
	return !!find_ops(type);
}
