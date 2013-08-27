/*
 * librouter is a virtual router software that moves packets back and forth to
 * SLIRP, taking care of virtual ARP resolution and DHCP management in the
 * process.
 *
 * I created it specifically for making DOSemu networking easier, but it might
 * have other interesting use cases, too.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation,  either version 3 of the License,  or (at your option)  any later
 * version.
 *
 * This program is  distributed in the hope that it will be useful,  but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) Mateusz Viste 2013. All rights reserved.
 * Contact: <mateusz$viste-family.net> (replace the $ sign by a @)
 */

#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/select.h>
#include "slirp.h"
#include "netparse.h"
#include "arp.h"
#include "forgehdr.h"
#include "processpkt.h"
#include "librouter.h"  /* include self for control */


static pid_t slirp_pid = -1;

static uint8_t librouter_mymac[] = {0xF0,0xDE,0xF1,0x29,0x07,0x85};
static uint32_t librouter_myip[] = {(10 << 24) | (0 << 16) | (2 << 8) | 1,  /* me as the gateway */
                                    (10 << 24) | (0 << 16) | (2 << 8) | 2,  /* me as the host */
                                    (10 << 24) | (0 << 16) | (2 << 8) | 3,  /* me as the dns server */
                                    0};                                     /* last entry is marked as '0' */
static uint32_t librouter_mynetmask = 0xFFFFFF00;
static struct arptabletype *librouter_arptable = NULL;


/* close librouter */
void librouter_close(int sock) {
  librouter_slirp_close();
}


/* send a frame to librouter. *buff is the content you want to send. len is the total length of the frame.
   returns 0 on success, a negative value on error, or a positive value if *buff have been filled with a frame you should read. IMPORTANT: *buff must be at least 1024 bytes long!. */
int librouter_sendframe(int sock, uint8_t *buff, int len) {
  return(librouter_processpkt(buff, len, librouter_myip, librouter_mynetmask, librouter_mymac, sock, &librouter_arptable));
}


/* receive a frame from librouter. *buff is the place where the frame have to be copied. This function is non-blocking. It returns 0 if nothing awaited, a negative value on error, and a positive value with the length of the frame that has been copied into *buff. Make sure that buff can contain at least 2048 bytes. */
int librouter_recvframe(int sock, uint8_t *buff) {
  uint8_t *dstmac;
  uint8_t *ipsrcarr, *ipdstarr;
  uint32_t ipdst;
  int ipprotocol, dscp, ttl, id, fragoffset, morefragsflag;
  uint8_t ethbroadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t *buffptr;
  int bufflen, x;
  struct timeval tv;
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);
  /* Wait no time */
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  /* wait for something to happen */
  if (select(sock+1, &rfds, NULL, NULL, &tv) == 0) return(0); /* if nothing awaits, return immediately */
  /* otherwise something is going on on the slirp socket */
  buffptr = buff + 14; /* leaving some place before data, for the eth header */
  bufflen = librouter_slirp_read(buffptr, sock);
  /* printf("Got %d bytes on the slirp iface.\n", bufflen); */
  if (bufflen <= 0)
    return(-1);
  /* else bufflen > 0, so there is some real data awaiting */
  /* analyze the IP header */
  librouter_parse_ipv4(buffptr, bufflen, &ipsrcarr, &ipdstarr, &ipprotocol, &dscp, &ttl, &id, &fragoffset, &morefragsflag);
  ipdst = 0;
  for (x = 0; x < 4; x++) {
    ipdst <<= 8;
    ipdst |= ipdstarr[x];
  }
  /* check if the destination belongs to my network - if not, ignore the packet and return 0 */
  if ((ipdst & librouter_mynetmask) != (librouter_myip[0] & librouter_mynetmask)) return(0);
  /* otherwise the packet is destinated to our network indeed */
  dstmac = librouter_arp_getmac(librouter_arptable, ipdst); /* compute the dst mac */
  if (dstmac == NULL) {
    /* printf("Could not find the mac for %u.%u.%u.%u in the ARP cache. Broadcasting then!\n", ipdstarr[0], ipdstarr[1], ipdstarr[2], ipdstarr[3]); */
    dstmac = ethbroadcast;
  }
  /* encapsulate the data into an eth frame and relay it */
  librouter_forge_eth(&buffptr, &bufflen, librouter_mymac, dstmac);
  return(bufflen);
}


/* initialize the librouter library. slirpexec is a string with the path/filename of the slirp binary to use, or NULL if librouter have to try to figure it out by itself. Returns a socket that can be monitored for new incoming packets, or -1 on error. */
int librouter_init(char *slirpexec) {
  int x;
  int slirpfd;
  pid_t pid;

  /* open the communication channel with SLIRP */
  pid = librouter_slirp_open(slirpexec, &slirpfd);
  if (pid == -1) {
    /* printf("Error: failed to invoke '%s'.\n", slirpexec); */
    return(-1);
  }
  slirp_pid = pid;
  /* printf("open SLIRP channel (%s): success.\n", slirpexec); */

  /* add all my IP addresses to the ARP cache, to avoid distributing them later to clients via DHCP */
  for (x = 0; librouter_myip[x] != 0; x++) {
    librouter_arptable = librouter_arp_learn(librouter_arptable, librouter_mymac, librouter_myip[x]);
  }
  /* return the slirp socket to the application for monitoring needs */
  return(slirpfd);
}

pid_t librouter_get_slirp_pid(void)
{
  return slirp_pid;
}
