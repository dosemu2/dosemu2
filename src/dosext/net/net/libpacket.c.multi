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
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/sockios.h>
#include <linux/if.h>
#include <netinet/in.h>
#include <linux/if_ether.h>

#include "emu.h"
#include "libpacket.h"

#include  "dosnet.h"

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


/* Should return an unique ID (< 255) corresponding to this invocation 
   of DOSEMU not clashing with other DOSEMU's. 
   We use tty number.  If someone invokes dosemu in background, we 
   are in trouble ... ;-(. 
*/


static unsigned short int DosnetID=0xffff;
int 
GetDosnetID()
{  
  struct stat chkbuf;
  int major, minor;

  if (DosnetID != 0xffff) return (DosnetID) ;

  fstat(STDOUT_FILENO, &chkbuf);
  major = chkbuf.st_rdev >> 8;
  minor = chkbuf.st_rdev & 0xff;

  if (major != 4) {
      /* Not running on tty/ttyp/ttyS. I really don't know what to do. 
         For now, let us exit ... */
      pd_printf( "DosNetID: Can't work without a tty/pty/ttyS, assigning an odd one ... ");
      minor=241;
  }
  
  DosnetID = (unsigned short int)DOSNET_TYPE_BASE + minor;
  pd_printf("Assigned DosnetID=%x\n", DosnetID );
  return(DosnetID);
}


int 
DosnetOpenNetworkType(unsigned short netid)
{
  /* Here, netid is ignored, and instead, we send our own netid. 
     assigned to this dosnet session. 
   */
  int s = socket(AF_INET, SOCK_PACKET, htons(GetDosnetID()));  
  if (s == -1)
    return -1;
  fcntl(s, F_SETFL, O_NDELAY);
  return s;
}

/* Return a socket opened in broadcast mode ... */
int
DosnetOpenBroadcastNetworkType()
{
  int s = socket(AF_INET, SOCK_PACKET, htons(DOSNET_BROADCAST_TYPE));  
           /* this is special value !*/
  if (s == -1) {
    return -1;
    pd_printf("DosnetOpenBroadcast.. couldnot open socket.\n");
  }
  fcntl(s, F_SETFL, O_NDELAY);
  return s;
}

/*
 *	Close a file handle to a raw packet type.
 */

void 
DosnetCloseNetworkLink(int sock)
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
DosnetWriteToNetwork(int sock, const char *device, const char *data, int len)
{
  struct sockaddr sa;

  sa.sa_family = AF_INET;
  strcpy(sa.sa_data, DOSNET_DEVICE);

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
DosnetReadFromNetwork(int sock, char *device, char *data, int len)
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
#ifdef OLD_SIOCGIFHWADDR
#define NET3
#endif



/* This routine is totally local; doesn't make request to actual device. */

char local_eth_addr[6]={0,0,0,0,0,0};
int 
DosnetGetDeviceHardwareAddress(char *device, char *addr)
{  
   int i;
   memcpy(local_eth_addr,DOSNET_FAKED_ETH_ADDRESS, 6);
   (void)GetDosnetID();
   *(unsigned short int *)&(local_eth_addr[2])= DosnetID ;

   memcpy(addr, local_eth_addr, 6);

   pd_printf("Assigned Ethernet Address= " );
   for ( i=0; i< 6 ; i++)  pd_printf("%x:", local_eth_addr[i]);
   pd_printf( "\n");

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
DosnetGetDeviceMTU(char *device)
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

void
printbuf(char *mesg, struct ethhdr *buf)
{ int i;
  pd_printf( "%s :\n Dest.=", mesg);
  for (i=0;i<6;i++) pd_printf("%x:",buf->h_dest[i]);
  pd_printf( " Source=") ;
  for (i=0;i<6;i++) pd_printf("%x:",buf->h_source[i]);
  pd_printf( " Type= %x \n", buf->h_proto) ;
}
