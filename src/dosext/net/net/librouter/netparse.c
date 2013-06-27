/*
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

#include "netparse.h"  /* include self for control */

/* Parse an Ethernet frame, retrieve src mac, dst mac and VLAN id (vlan = -1 means 'no 802.1Q'). Returns the address of the ethernet payload (or NULL in case of an error) */
void *parse_ethernet(uint8_t *ethframe, int ethframe_len, uint8_t **srcmac, uint8_t **dstmac, int *vlanid, int *ethertype) {
  if (ethframe_len < 15) return(NULL); /* an ethernet frame is at least 14 bytes long (that's the minimal header's length) */
  *dstmac = ethframe;  /* fetch dst mac address */
  *srcmac = ethframe + 6; /* fetch src mac address */
  if ((ethframe[12] == 0x81) && (ethframe[13] == 0)) { /* we found a 802.1Q tag */
      if (ethframe_len < 19) return(NULL); /* an ethernet frame with a 802.1Q tag is at least 18 bytes long (that's the minimal header's length) */
      *vlanid = ethframe[14];
      *vlanid <<= 8;
      *vlanid |= ethframe[15];
      *vlanid &= 0xFFF; /* reset all bits that are not part of the VLAN id */
      *ethertype = ethframe[16];
      *ethertype <<= 8;
      *ethertype |= ethframe[17];
      if (*ethertype < 1536) *ethertype = 0; /* values below 1536 are not ethertype, but (legacy) length indication */
      return(ethframe + 18);
    } else { /* no 802.1Q tag found */
      *vlanid = 0;
      *ethertype = ethframe[12];
      *ethertype <<= 8;
      *ethertype |= ethframe[13];
      if (*ethertype < 1536) *ethertype = 0; /* values below 1536 are not ethertype, but (legacy) length indication */
      return(ethframe + 14);
  }
}


void *parse_ipv4(uint8_t *packet, int len, uint8_t **ipsrc, uint8_t **ipdst, int *ipprotocol, int *dscp, int *ttl, int *id, int *fragoffset, int *morefragsflag) {
  int hdrlen;
  if (len < 21) return(NULL); /* an IP packet is at least 20 bytes long (header's length) */
  if ((packet[0] & 0xF0) != 0x40) return(NULL); /* version should be alway 4 for IPv4! */
  hdrlen = (packet[0] & 0x0F);
  hdrlen <<= 2; /* the IHL is a number of 32bits words, not a byte length */
  if (hdrlen < 20) return(NULL);  /* the internet header length can't be less than 20 bytes */
  *dscp = (packet[1] >> 2);
  *id = packet[4];
  *id <<= 8;
  *id |= packet[5];
  *morefragsflag = (packet[6] >> 5) & 1;
  *fragoffset = (packet[6] & 31);
  *fragoffset <<= 8;
  *fragoffset |= packet[7];
  *ttl = packet[8];
  *ipprotocol = packet[9];
  *ipsrc = packet + 12;
  *ipdst = packet + 16;
  return(packet + hdrlen);
}


void *parse_udp(uint8_t *packet, int len, int *portsrc, int *portdst) {
  if (len < 9) return(NULL); /* a UDP header is 8 bytes long */
  *portsrc = packet[0];
  *portsrc <<= 8;
  *portsrc |= packet[1];
  *portdst = packet[2];
  *portdst <<= 8;
  *portdst |= packet[3];
  return(packet + 8);
}


void *parse_tcp(uint8_t *packet, int len, int *portsrc, int *portdst) {
  int hdrlen;
  if (len < 21) return(NULL); /* a TCP header is at least 20 bytes long */
  *portsrc = packet[0];
  *portsrc <<= 8;
  *portsrc |= packet[1];
  *portdst = packet[2];
  *portdst <<= 8;
  *portdst |= packet[3];
  hdrlen = (packet[12] & 0xF0) >> 2; /* hdrlen is stored in the higher 4 bits, as a number of 32 bits words. here we compute a byte length */
  if (hdrlen < 20) return(NULL);
  return(packet + hdrlen);
}
