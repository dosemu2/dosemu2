/*
 *	SOCK_PACKET support.
 *	Placed under the GNU LGPL.
 *
 *	First cut at a library of handy support routines. Comments, additions
 *	and bug fixes greatfully received.
 *
 *	(c) 1994 Alan Cox	iiitac@pyr.swan.ac.uk	GW4PTS@GB7SWN
 */

#include "kversion.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <netinet/in.h>

#include "libpacket.h"

/*
 *	Obtain a file handle on a raw ethernet type. In actual fact
 *	you can also request the dummy types for AX.25 or 802.3 also
 *
 *	-1 indicates an error
 *	0  or higher is a file descriptor which we have set non blocking
 *
 *	WARNING: It is ok to listen to a service the system is using (eg arp)
 *	but don't try and run a user mode stack on the same service or all
 *	hell will break loose.
 */

int 
OpenNetworkType(unsigned short netid)
{
  int s = socket(AF_INET, SOCK_PACKET, htons(netid));

  if (s == -1)
    return -1;
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
 *	ethernet card). Please don't assume eth0, make it configurable
 *	- plip is ethernet like but not eth0, ditto for the de600's.
 *
 *	Return: -1 is an error
 *		otherwise bytes written.
 */

int 
WriteToNetwork(int sock, const char *device, const char *data, int len)
{
  struct sockaddr sa;

  sa.sa_family = AF_INET;
  strcpy(sa.sa_data, device);

  return (sendto(sock, data, len, 0, &sa, sizeof(sa)));
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

  error = recvfrom(sock, data, len, 0, &sa, &sz);

  if (error == -1)
    return -1;

  strcpy(device, sa.sa_data);
  return error;			/* Actually size of received packet */
}

/*
 *	Handy support routines.
 */

/*
 *	Obtain the hardware address of an interface.
 *	addr should be a buffer of 8 bytes or more.
 *
 *	Return:
 *	0	Success, buffer holds data.
 *	-1	Error.
 */

/*
 *	NET2 or NET3 - work for both.
 */
#if defined(OLD_SIOCGIFHWADDR) || (KERNEL_VERSION >= 1003038)
#define NET3
#endif

int 
GetDeviceHardwareAddress(char *device, char *addr)
{
  int s = socket(AF_INET, SOCK_DGRAM, 0);
  struct ifreq req;
  int err;

  strcpy(req.ifr_name, device);

  err = ioctl(s, SIOCGIFHWADDR, &req);  
  close(s);	/* Thanks Rob. for noticing this */
  if (err == -1)
    return err;
#ifdef NET3    
  memcpy(addr, req.ifr_hwaddr.sa_data,8);
#else
  memcpy(addr, req.ifr_hwaddr, 8);
#endif  
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
  close(s);	/* So I'll add this one as well.  Ok Alan? - Rob */
  if (err == -1)
    return err;
  return req.ifr_mtu;
}
