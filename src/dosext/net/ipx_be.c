/*
 *  Copyright (C) 2024  @stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "ipx_wrp.h"
#include "dosemu_debug.h"
#include "init.h"
#include "emu.h"
#include "ipx.h"
#include "ipx_be.h"

static int GetMyAddress(unsigned char *MyAddress)
{
  int sock;
  struct sockaddr_ipx ipxs;
  struct sockaddr_ipx ipxs2;
  socklen_t len;
  int i;

  sock=socket(AF_IPX,SOCK_DGRAM,PF_IPX);
  if(sock==-1)
  {
    n_printf("IPX: could not open socket in GetMyAddress: %s\n", strerror(errno));
    return(-1);
  }
#define DYNAMIC_PORT 1
#if DYNAMIC_PORT
  #define DEF_PORT 0
#else
  #define DEF_PORT 0x5000
#endif
  ipxs.sipx_family=AF_IPX;
  ipxs.sipx_network=htonl(config.ipx_net);
  ipxs.sipx_port=htons(DEF_PORT);

  /* bind this socket to network */
  if(bind(sock,(struct sockaddr*)&ipxs,sizeof(ipxs))==-1)
  {
    n_printf("IPX: could not bind to network %#x in GetMyAddress: %s\n",
      config.ipx_net, strerror(errno));
    close( sock );
    return(-1);
  }

  len = sizeof(ipxs2);
  if (getsockname(sock,(struct sockaddr*)&ipxs2,&len) < 0) {
    n_printf("IPX: could not get socket name in GetMyAddress: %s\n", strerror(errno));
    close( sock );
    return(-1);
  }

  memcpy(MyAddress, &ipxs2.sipx_network, 4);
  for (i = 0; i < 6; i++) {
    MyAddress[4+i] = ipxs2.sipx_node[i];
  }
  n_printf("IPX: using network address %02X%02X%02X%02X\n",
    MyAddress[0], MyAddress[1], MyAddress[2], MyAddress[3] );
  n_printf("IPX: using node address of %02X%02X%02X%02X%02X%02X\n",
    MyAddress[4], MyAddress[5], MyAddress[6], MyAddress[7],
    MyAddress[8], MyAddress[9] );
#if DYNAMIC_PORT
  close( sock );
#endif
  return(0);
}

static int do_open(u_short port, u_char *MyAddress, u_short *newPort, int *err)
{
  int sock;			/* sock here means Linux socket handle */
  int opt;
  struct sockaddr_ipx ipxs;
  socklen_t len;
  struct sockaddr_ipx ipxs2;

  /* DANG_FIXTHIS - kludge to support broken linux IPX stack */
  /* need to convert dynamic socket open into a real socket number */
/*  if (port == 0) {
    n_printf("IPX: using socket %x\n", nextDynamicSocket);
    port = nextDynamicSocket++;
  }
*/
  /* do a socket call, then bind to this port */
  sock = socket(AF_IPX, SOCK_DGRAM, PF_IPX);
  if (sock == -1) {
    n_printf("IPX: could not open IPX socket: %s.\n", strerror(errno));
    /* I can't think of anything else to return */
    *err = RCODE_SOCKET_TABLE_FULL;
    return -1;
  }

  opt = 1;
  /* Permit broadcast output */
  if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
		 &opt, sizeof(opt)) == -1) {
    /* I can't think of anything else to return */
    n_printf("IPX: could not set socket option for broadcast: %s.\n", strerror(errno));
    *err = RCODE_SOCKET_TABLE_FULL;
    return -1;
  }
  /* allow setting the type field in the IPX header */
  opt = 1;
  if (setsockopt(sock, SOL_IPX, IPX_TYPE, &opt, sizeof(opt)) == -1) {
    /* I can't think of anything else to return */
    n_printf("IPX: could not set socket option for type: %s.\n", strerror(errno));
    *err = RCODE_SOCKET_TABLE_FULL;
    return -1;
  }
  ipxs.sipx_family = AF_IPX;
  memcpy(&ipxs.sipx_network, MyAddress, 4);
/*  ipxs.sipx_network = htonl(MyNetwork); */
  memset(ipxs.sipx_node, 0, 6);	/* Please fill in my node name */
  ipxs.sipx_port = port;

  /* now bind to this port */
  if (bind(sock, (struct sockaddr*)&ipxs, sizeof(ipxs)) == -1) {
    /* I can't think of anything else to return */
    n_printf("IPX: could not bind socket to address: %s\n", strerror(errno));
    close( sock );
    *err = RCODE_SOCKET_TABLE_FULL;
    return -1;
  }

  if( port==0 ) {
    len = sizeof(ipxs2);
    if (getsockname(sock,(struct sockaddr*)&ipxs2,&len) < 0) {
      /* I can't think of anything else to return */
      n_printf("IPX: could not get socket name in IPXOpenSocket: %s\n", strerror(errno));
      close( sock );
      *err = RCODE_SOCKET_TABLE_FULL;
      return -1;
    } else {
      port = ipxs2.sipx_port;
      n_printf("IPX: opened dynamic socket %04x\n", port);
    }
  }
  *newPort = port;
  return sock;
}

static int do_close(int sock)
{
  return close(sock);
}

static int do_recv(int fd, u_char *buffer, int bufLen, u_char *MyAddress,
    u_short port)
{
  struct sockaddr_ipx ipxs;
  socklen_t sz;
  int size;

  sz = sizeof(ipxs);
  size = recvfrom(fd, buffer + sizeof(IPXPacket_t),
                  bufLen - sizeof(IPXPacket_t),
                  MSG_DONTWAIT, (struct sockaddr*)&ipxs,
                  &sz);
  if (size > 0) {
    IPXPacket_t *IPXHeader;
    /* use data from sipx to fill out source address */
    IPXHeader = (IPXPacket_t *) buffer;
    IPXHeader->Checksum = 0xFFFF;
    IPXHeader->Length = htons(size + sizeof(IPXPacket_t)); /* in network order */
    /* DANG_FIXTHIS - use real values to fill out IPX header here */
    IPXHeader->TransportControl = 1;
    IPXHeader->PacketType = 0;
    memcpy((u_char *) & IPXHeader->Destination, MyAddress, 10);
    memcpy(IPXHeader->Destination.Socket, &port, 2);
    memcpy(IPXHeader->Source.Network, (char *) &ipxs.sipx_network, 4);
    memcpy(IPXHeader->Source.Node, ipxs.sipx_node, 6);
    memcpy(IPXHeader->Source.Socket, &ipxs.sipx_port, 2);
    printIPXHeader(IPXHeader);
  }
  return size;
}

static int do_send(int fd, u_char *data, int dataLen, u_char *MyAddress)
{
  struct sockaddr_ipx ipxs;
  IPXPacket_t *IPXHeader = (IPXPacket_t *)data;

  ipxs.sipx_family = AF_IPX;
  /* get destination address from IPX packet header */
  memcpy(&ipxs.sipx_network, IPXHeader->Destination.Network, 4);
  /* if destination address is 0, then send to my net */
  if (ipxs.sipx_network == 0) {
    memcpy(&ipxs.sipx_network, MyAddress, 4);
/*  ipxs.sipx_network = htonl(MyNetwork); */
  }
  memcpy(&ipxs.sipx_node, IPXHeader->Destination.Node, 6);
  memcpy(&ipxs.sipx_port, IPXHeader->Destination.Socket, 2);
  ipxs.sipx_type = 1;
  /*	ipxs.sipx_port=htons(0x452); */
  return sendto(fd, data + sizeof(IPXPacket_t),
	    dataLen - sizeof(IPXPacket_t), 0,
	    (struct sockaddr*)&ipxs, sizeof(ipxs));
}

static const struct ipx_ops iops = {
  GetMyAddress,
  do_open,
  do_close,
  do_recv,
  do_send,
};

CONSTRUCTOR(static void init(void))
{
  ipx_register_ops(&iops);
}
