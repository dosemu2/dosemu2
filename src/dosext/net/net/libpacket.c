/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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

#include "emu.h"
#include "priv.h"
#include "libpacket.h"
#include "dosnet.h"
#include "pktdrvr.h"

static int GetDosnetID(void);

static unsigned short int DosnetID = 0xffff;
char local_eth_addr[6] = {0,0,0,0,0,0};

/* Should return a unique ID corresponding to this invocation of
   dosemu not clashing with other dosemus. We use a random value and
   hope for the best.
   */

static int 
GetDosnetID(void)
{  
    DosnetID = DOSNET_TYPE_BASE + (rand() & 0xff);
    pd_printf("Assigned DosnetID=%x\n", DosnetID);
    return DosnetID;
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

int 
OpenNetworkType(unsigned short netid)
{
	PRIV_SAVE_AREA
	int s, proto;

	if (config.vnet)
		/* netid is ignored, and we use a special netid for the 
		   dosnet session */
		proto = htons(GetDosnetID());
	else
		proto = htons(netid);

	enter_priv_on();

#ifdef AF_PACKET
	if (running_kversion >= 2001000)
		s = socket(AF_PACKET, SOCK_PACKET, proto);
	else
		s = socket(AF_INET, SOCK_PACKET, proto);
#else
	s = socket(AF_INET, SOCK_PACKET, proto);
#endif
	leave_priv_setting();
	if (s < 0) {
		if (errno == EPERM) warn("Must be root for virtual TCP/IP\n");
		return -1;
	}
	fcntl(s, F_SETFL, O_NDELAY);
	return s;
}

/* Return a socket opened in broadcast mode (only used for virtual net) */
int
OpenBroadcastNetworkType(void)
{
	PRIV_SAVE_AREA
	int s;
	enter_priv_on();
#ifdef AF_PACKET
	if (running_kversion >= 2001000)
		s = socket(AF_PACKET, SOCK_PACKET, 
			   htons(DOSNET_BROADCAST_TYPE)); 
	else
		s = socket(AF_INET, SOCK_PACKET, htons(DOSNET_BROADCAST_TYPE));
#else
	s = socket(AF_INET, SOCK_PACKET, htons(DOSNET_BROADCAST_TYPE));  
#endif
	leave_priv_setting();
	if (s < 0) {
		pd_printf("OpenBroadcast: could not open socket\n");
		if (errno == EPERM) warn("Must be root for virtual TCP/IP\n");
		return -1;
	}
	fcntl(s, F_SETFL, O_NDELAY);
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
 *	Write a packet to the network. You have to give a device to
 *	this function. This is a device name (eg 'eth0' for the first
 *	ethernet card).
 *
 *      For virtual TCP/IP, the device name is ignored and we write
 *      to the dosnet device instead (usually 'dsn0')
 *
 *	Return: -1 is an error
 *		otherwise bytes written.
 */

int 
WriteToNetwork(int sock, const char *device, const char *data, int len)
{
	struct sockaddr sa;

	if (config.vnet == VNET_TYPE_TAP) {
	  return write(sock, data, len);
	}

#ifdef AF_PACKET
	if (running_kversion >= 2001000)
		sa.sa_family = AF_PACKET;
        else	
		sa.sa_family = AF_INET;
#else
	sa.sa_family = AF_INET;
#endif
	if (config.vnet)
		strcpy(sa.sa_data, DOSNET_DEVICE);
	else
		strcpy(sa.sa_data, device);
	
	return sendto(sock, data, len, 0, &sa, sizeof(sa));
}

/*
 *	Read a packet from the network. The device parameter will
 *	be filled in by this routine (make it 32 bytes or more).
 *	If you wish to work with one interface only you must filter
 *	yourself. Remember to make your buffer big enough for your
 *	data. Oversized packets will be truncated.
 *
 *	Return:
 *	 	-1	   Error
 *		otherwise  Size of packet received.
 */

int 
ReadFromNetwork(int sock, char *device, char *data, int len)
{
	struct sockaddr sa;
	int sz = sizeof(sa);
	int error;

	if (config.vnet == VNET_TYPE_TAP) {
		return read(sock, data, len);
	}

	error = recvfrom(sock, data, len, 0, &sa, &sz);

	if (error == -1)
		return -1;

	strcpy(device, sa.sa_data);
	return error;		/* Actually size of received packet */
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
	if (config.vnet) {
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


int tun_alloc(char *dev)
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

      DosnetID = GetDosnetID();

      return fd;
}
