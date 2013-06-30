/*
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 *	SOCK_PACKET support.
 *	Placed under the GNU LGPL.
 *
 *	First cut at a library of handy support routines. Comments, additions
 *	and bug fixes greatfully received.
 *
 *	(c) 1994 Alan Cox	iiitac@pyr.swan.ac.uk	GW4PTS@GB7SWN
 */
#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <netinet/in.h>
#include "Linux/if_tun.h"
#include <netinet/if_ether.h>
#include <netpacket/packet.h>
#include <net/ethernet.h>

#include "emu.h"
#include "priv.h"
#include "libpacket.h"
#include "pktdrvr.h"

static int tun_alloc(char *dev);

static unsigned short int DosnetID = 0xffff;
char local_eth_addr[6] = {0,0,0,0,0,0};
#define DOSNET_TYPE_BASE        0x9000
#define DOSNET_FAKED_ETH_ADDRESS   "dbx\x90xx"

static int pkt_fd = -1;

/* Should return a unique ID corresponding to this invocation of
   dosemu not clashing with other dosemus. We use a random value and
   hope for the best.
   */

static void GenerateDosnetID(void)
{
	DosnetID = DOSNET_TYPE_BASE + (rand() & 0xff);
	pd_printf("Assigned DosnetID=%x\n", DosnetID);
}

void LibpacketInit(void)
{

	GenerateDosnetID();
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

static int OpenNetworkLinkEth(char *name)
{
	PRIV_SAVE_AREA
	int s, proto, ret;
	struct ifreq req;
	struct sockaddr_ll addr;

	proto = htons(DosnetID);

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

	pkt_fd = s;
	return 0;
}

static int OpenNetworkLinkTap(char *name)
{
	receive_mode = 6;
	pkt_fd = tun_alloc(name);
	return 0;
}

int OpenNetworkLink(char *name)
{

	switch (config.vnet) {
	case VNET_TYPE_TAP:
	    return OpenNetworkLinkTap(name);
	case VNET_TYPE_ETH:
	    return OpenNetworkLinkEth(name);
	}
	return -1;
}

/*
 *	Close a file handle to a raw packet type.
 */

static void CloseNetworkLinkEth(void)
{
	close(pkt_fd);
}

void CloseNetworkLink(int sock)
{
	switch (config.vnet) {
	case VNET_TYPE_TAP:
	case VNET_TYPE_ETH:
		CloseNetworkLinkEth();
	}
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

static int GetDeviceHardwareAddressEth(char *device, unsigned char *addr)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq req;
	int err;

	strcpy(req.ifr_name, device);

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

static int GetDeviceHardwareAddressTap(char *device, unsigned char *addr)
{
	/* This routine is totally local; doesn't make
	   request to actual device. */
	int i;
	memcpy(local_eth_addr, DOSNET_FAKED_ETH_ADDRESS, 6);
	*(unsigned short int *)&(local_eth_addr[2]) = DosnetID;

	memcpy(addr, local_eth_addr, 6);

	pd_printf("Assigned Ethernet Address = ");
	for (i=0; i < 6; i++)
		pd_printf("%02x:", local_eth_addr[i] & 0xff);
	pd_printf("\n");

	return 0;
}

int GetDeviceHardwareAddress(char *device, unsigned char *addr)
{
	switch (config.vnet) {
	case VNET_TYPE_TAP:
		return GetDeviceHardwareAddressTap(device, addr);
	case VNET_TYPE_ETH:
		return GetDeviceHardwareAddressEth(device, addr);
	}
	return -1;
}

/*
 *	Obtain the maximum packet size on an interface.
 *
 *	Return:
 *	>0	Return is the mtu of the interface
 *	-1	Error.
 */

static int GetDeviceMTUEth(char *device)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	struct ifreq req;
	int err;

	strcpy(req.ifr_name, device);

	err = ioctl(s, SIOCGIFMTU, &req);
	close(s);
	if (err < 0)
		return -1;
	return req.ifr_mtu;
}

int GetDeviceMTU(char *device)
{
	switch (config.vnet) {
	case VNET_TYPE_TAP:
	case VNET_TYPE_ETH:
		return GetDeviceMTUEth(device);
	}
	return -1;
}

static int tun_alloc(char *dev)
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
         strncpy(ifr.ifr_name, dev, IFNAMSIZ);

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

static void pkt_io_select_eth(void(*callback)(void *), void *arg)
{
    add_to_io_select(pkt_fd, callback, arg);
}

void pkt_io_select(void(*callback)(void *), void *arg)
{
    switch (config.vnet) {
    case VNET_TYPE_TAP:
    case VNET_TYPE_ETH:
	pkt_io_select_eth(callback, arg);
    }
}

static ssize_t pkt_read_eth(void *buf, size_t count)
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

ssize_t pkt_read(void *buf, size_t count)
{
    switch (config.vnet) {
    case VNET_TYPE_TAP:
    case VNET_TYPE_ETH:
	return pkt_read_eth(buf, count);
    }
    return -1;
}

static ssize_t pkt_write_eth(const void *buf, size_t count)
{
    return write(pkt_fd, buf, count);
}

ssize_t pkt_write(const void *buf, size_t count)
{
    switch (config.vnet) {
    case VNET_TYPE_TAP:
    case VNET_TYPE_ETH:
	return pkt_write_eth(buf, count);
    }
    return -1;
}
