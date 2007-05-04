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
#include "dosnet.h"
#include "pktdrvr.h"

static int tun_alloc(char *dev);

static unsigned short int DosnetID = 0xffff;
char local_eth_addr[6] = {0,0,0,0,0,0};

/* Should return a unique ID corresponding to this invocation of
   dosemu not clashing with other dosemus. We use a random value and
   hope for the best.
   */

int GetDosnetID(void)
{  
    return DosnetID;
}

void GenerateDosnetID(void)
{  
    DosnetID = DOSNET_TYPE_BASE + (rand() & 0xff);
    pd_printf("Assigned DosnetID=%x\n", DosnetID);
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

int OpenNetworkLink(char *name, unsigned short netid)
{
	PRIV_SAVE_AREA
	int s, proto, ret;
	struct ifreq req;
	struct sockaddr_ll addr;

	if (config.vnet == VNET_TYPE_TAP) {
		receive_mode = 6;
		return tun_alloc(name);
	}

	proto = htons(netid);

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
	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
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

	return s;
}

/*
 *	Close a file handle to a raw packet type.
 */

void 
CloseNetworkLink(int sock)
{
	close(sock);
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

int 
GetDeviceHardwareAddress(char *device, char *addr)
{  
	if (config.vnet == VNET_TYPE_DSN || config.vnet == VNET_TYPE_TAP) {
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
       	}
	else {
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
	}
	return 0;
}

/*
 *	Obtain the maximum packet size on an interface.
 *
 *	Return:
 *	>0	Return is the mtu of the interface
 *	-1	Error.
 */

int 
GetDeviceMTU(char *device)
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


static int tun_alloc(char *dev)
{
      struct ifreq ifr;
      int fd, err;

      if( (fd = open("/dev/net/tun", O_RDWR)) < 0 )
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

      if( (err = ioctl(fd, TUNSETIFF, (void *) &ifr)) < 0 ){
         close(fd);
         return err;
      }
      strcpy(dev, ifr.ifr_name);

      return fd;
}
