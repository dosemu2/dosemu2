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
#include "ringbuf.h"
#include "priv.h"
#include "libpacket.h"
#include "pktdrvr.h"

/* librouter is used for usernet networking (using slirp as a backend) */
#include "librouter/librouter.h"

static int tun_alloc(char *dev);

static unsigned short int DosnetID = 0xffff;
char local_eth_addr[6] = {0,0,0,0,0,0};
#define DOSNET_TYPE_BASE        0x9000
#define DOSNET_FAKED_ETH_ADDRESS   "dbx\x90xx"

static int pkt_fd = -1;

static struct seqbuf sqb;
#define LIBROUTER_BUF_LEN 2048
static unsigned char librouter_buff[LIBROUTER_BUF_LEN];

struct pkt_ops {
    int (*open)(char *name);
    void (*close)(void);
    int (*get_hw_addr)(char *device, unsigned char *addr);
    int (*get_MTU)(char *device);
    ssize_t (*pkt_read)(void *buf, size_t count);
    ssize_t (*pkt_write)(const void *buf, size_t count);
};

static struct pkt_ops ops[VNET_TYPE_MAX];

/* Should return a unique ID corresponding to this invocation of
   dosemu not clashing with other dosemus. We use a random value and
   hope for the best.
   */

static void GenerateDosnetID(void)
{
	DosnetID = DOSNET_TYPE_BASE + (rand() & 0xff);
	pd_printf("Assigned DosnetID=%x\n", DosnetID);
}

static void pkt_receive_req_async(void *arg)
{
  pic_request(PIC_NET);
}

static void pkt_receive_req(void)
{
  pic_request(PIC_NET);
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

	add_to_io_select(pkt_fd, pkt_receive_req_async, NULL);

	return 0;
}

static int OpenNetworkLinkTap(char *name)
{
	receive_mode = 6;
	pkt_fd = tun_alloc(name);
	if (pkt_fd < 0)
		return pkt_fd;
	add_to_io_select(pkt_fd, pkt_receive_req_async, NULL);
	return 0;
}

static void slirp_exit(void)
{
	error("SLIRP unexpectedly terminated\n");
	leavedos(3);
}

static int OpenNetworkLinkSlirp(char *name)
{
	seqbuf_init(&sqb, librouter_buff, LIBROUTER_BUF_LEN);
	receive_mode = 6;
	pkt_fd = librouter_init(name);
	add_to_io_select(pkt_fd, pkt_receive_req_async, NULL);
	sigchld_register_handler(librouter_get_slirp_pid(), slirp_exit);
	return 0;
}

int OpenNetworkLink(char *name)
{

	return ops[config.vnet].open(name);
}

/*
 *	Close a file handle to a raw packet type.
 */

static void CloseNetworkLinkEth(void)
{
	remove_from_io_select(pkt_fd);
	close(pkt_fd);
}

static void CloseNetworkLinkSlirp(void)
{
        remove_from_io_select(pkt_fd);
        librouter_close(pkt_fd);
}


void CloseNetworkLink(void)
{
	ops[config.vnet].close();
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
	return ops[config.vnet].get_hw_addr(device, addr);
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

static int GetDeviceMTUSlirp(char *device)
{
  return(1500);
}

int GetDeviceMTU(char *device)
{
	return ops[config.vnet].get_MTU(device);
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

static ssize_t pkt_read_slirp(void *buf, size_t count)
{
  size_t len = seqbuf_get_read_len(&sqb);
  /* first check the internal single-packet buffer */
  if (len > 0) {
    if (len > count) {
	error("pkt_read_slirp: small read requested, %zi<%zi\n", count, len);
	return -1;
    }
    return seqbuf_read(&sqb, buf, count);
  }
  /* else poll librouter */
  return librouter_recvframe(pkt_fd, buf);
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
    return ops[config.vnet].pkt_read(buf, count);
}

static ssize_t pkt_write_eth(const void *buf, size_t count)
{
    return write(pkt_fd, buf, count);
}

static ssize_t pkt_write_slirp(const void *buf, size_t count)
{
  int buf2_len;
  unsigned char buf2[LIBROUTER_BUF_LEN];
  memcpy(buf2, buf, count); /* we copy buf into our own buffer first, because librouter needs write access to the buffer */
  buf2_len = librouter_sendframe(pkt_fd, buf2, count);
  if (buf2_len > 0) { /* we've got something back already! */
    seqbuf_write(&sqb, buf2, buf2_len);
    pkt_receive_req(); /* this makes the underlying DOS get some kind of a signal 'hey buddy, you've got some data' */
  }
  return(count);
}

ssize_t pkt_write(const void *buf, size_t count)
{
    return ops[config.vnet].pkt_write(buf, count);
}

void LibpacketInit(void)
{
	ops[VNET_TYPE_ETH].open = OpenNetworkLinkEth;
	ops[VNET_TYPE_ETH].close = CloseNetworkLinkEth;
	ops[VNET_TYPE_ETH].get_hw_addr = GetDeviceHardwareAddressEth;
	ops[VNET_TYPE_ETH].get_MTU = GetDeviceMTUEth;
	ops[VNET_TYPE_ETH].pkt_read = pkt_read_eth;
	ops[VNET_TYPE_ETH].pkt_write = pkt_write_eth;

	ops[VNET_TYPE_TAP].open = OpenNetworkLinkTap;
	ops[VNET_TYPE_TAP].close = CloseNetworkLinkEth;
	ops[VNET_TYPE_TAP].get_hw_addr = GetDeviceHardwareAddressTap;
	ops[VNET_TYPE_TAP].get_MTU = GetDeviceMTUEth;
	ops[VNET_TYPE_TAP].pkt_read = pkt_read_eth;
	ops[VNET_TYPE_TAP].pkt_write = pkt_write_eth;

	ops[VNET_TYPE_SLIRP].open = OpenNetworkLinkSlirp;
	ops[VNET_TYPE_SLIRP].close = CloseNetworkLinkSlirp;
	ops[VNET_TYPE_SLIRP].get_hw_addr = GetDeviceHardwareAddressTap;
        ops[VNET_TYPE_SLIRP].get_MTU = GetDeviceMTUSlirp;
	ops[VNET_TYPE_SLIRP].pkt_read = pkt_read_slirp;
	ops[VNET_TYPE_SLIRP].pkt_write = pkt_write_slirp;

	GenerateDosnetID();
}
