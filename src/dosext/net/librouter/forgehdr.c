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

#include <inttypes.h>
#include "forgehdr.h"  /* include self for control */


/* computes a checksum for n bytes, starting at *buff. */
int librouter_cksum(uint8_t *buff, int n) {
  int x, cksum = 0;
  uint16_t *hdr16 = (uint16_t *) buff;
  for (x = 0; x < (n >> 1); x++) cksum += hdr16[x];
  if ((n & 1) != 0) cksum += buff[n - 1]; /* add the last byte if n is odd */
  x = (cksum >> 16) & 0xF;  /* save the first nibble */
  cksum += x;  /* add the first nibble (carry) to the cksum */
  cksum &= 0xFFFF; /* keep only the first 16 bits */
  cksum ^= 0xFFFF;  /* invert all bits */
  return(cksum);
}


void librouter_forge_udp(int portsrc, int portdst, uint8_t **buffptr, int *bufflen) {
  uint8_t *buffdata;
  *buffptr -= 8;
  *bufflen += 8;
  buffdata = *buffptr;
  buffdata[0] = portsrc >> 8;   /* src port */
  buffdata[1] = portsrc & 0xFF;
  buffdata[2] = portdst >> 8;   /* dst port */
  buffdata[3] = portdst & 0xFF;
  buffdata[4] = *bufflen >> 8;             /* len (including udp hdr) */
  buffdata[5] = *bufflen & 0xFF;
  buffdata[6] = 0;                         /* cksum (not mandatory) */
  buffdata[7] = 0;
}


void librouter_forge_ip(uint32_t ipsrc, uint32_t ipdst, int ipproto, uint8_t **buffptr, int *bufflen) {
  int cksum;
  uint8_t *buffdata;
  *buffptr -= 20;
  *bufflen += 20;
  buffdata = *buffptr;
  buffdata[0] = 0x45;             /* IP version (4) + length (20) */
  buffdata[1] = 0;                /* DSCP */
  buffdata[2] = *bufflen >> 8;    /* length */
  buffdata[3] = *bufflen & 0xFF;
  buffdata[4] = 0;                /* IP identification */
  buffdata[5] = 1;
  buffdata[6] = 0;                /* Flags (DF, more frags..) and frag offset */
  buffdata[7] = 0;
  buffdata[8] = 16;               /* TTL */
  buffdata[9] = ipproto;          /* protocol */
  buffdata[10] = 0;   /* checksum (leave it zero for now, it'll filled last) */
  buffdata[11] = 0;
  buffdata[12] = (ipsrc >> 24) & 0xFF;  /* IP src */
  buffdata[13] = (ipsrc >> 16) & 0xFF;
  buffdata[14] = (ipsrc >> 8) & 0xFF;
  buffdata[15] = ipsrc & 0xFF;
  buffdata[16] = (ipdst >> 24) & 0xFF;  /* IP dst */
  buffdata[17] = (ipdst >> 16) & 0xFF;
  buffdata[18] = (ipdst >> 8) & 0xFF;
  buffdata[19] = ipdst & 0xFF;
  /* compute the IP cksum */
  cksum = librouter_cksum(buffdata, 20);
  buffdata[10] = cksum & 0xFF;    /* update the cksum values in the header */
  buffdata[11] = cksum >> 8;
}


void librouter_forge_eth(uint8_t **buffptr, int *bufflen, uint8_t *srcmac, uint8_t *dstmac) {
  uint8_t *buffdata;
  *buffptr -= 14;
  *bufflen += 14;
  buffdata = *buffptr;
  buffdata[0] = dstmac[0];  /* dst mac */
  buffdata[1] = dstmac[1];  /* dst mac */
  buffdata[2] = dstmac[2];  /* dst mac */
  buffdata[3] = dstmac[3];  /* dst mac */
  buffdata[4] = dstmac[4];  /* dst mac */
  buffdata[5] = dstmac[5];  /* dst mac */
  buffdata[6] = srcmac[0];  /* src mac */
  buffdata[7] = srcmac[1];  /* src mac */
  buffdata[8] = srcmac[2];  /* src mac */
  buffdata[9] = srcmac[3];  /* src mac */
  buffdata[10] = srcmac[4]; /* src mac */
  buffdata[11] = srcmac[5]; /* src mac */
  buffdata[12] = 0x08;  /* ethertype (0x0800 = IPv4) */
  buffdata[13] = 0x00;
}
