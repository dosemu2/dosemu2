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
#include "arp.h"
#include "forgehdr.h"
#include "dhcp.h"  /* include self for control */



/* gets an IP for a given mac */
static uint32_t librouter_compute_client_ip(struct arptabletype **arptable, uint8_t *dhcpclientmac, uint32_t mynet) {
  uint32_t result;
  /* first let's check if the mac is already present */
  result = librouter_arp_getip(*arptable, dhcpclientmac);
  if (result != 0) return(result);
  /* if this mac is unknown, then find the next free ip */
  for (result = mynet + 1;; result += 1) {
    if (librouter_arp_getmac(*arptable, result) == 0) {
      *arptable = librouter_arp_learn(*arptable, dhcpclientmac, result);
      return(result);
    }
  }
}


/* analyzes a DHCP request, and computes an answer */
int librouter_dhcpserver(uint8_t *udppayload, int bufflen, struct arptabletype **arptable, uint32_t myip, uint32_t mynetmask, uint8_t *mymac, uint8_t **buffanswerptr) {
  uint8_t bootphdr[] = {0x01, 0x01, 0x06};
  uint8_t dhcpcookie[] = {0x63, 0x82, 0x53, 0x63};
  uint8_t dhcpclientmac[6];
  uint32_t transactionid;
  int x, dhcpmsg;
  uint8_t *buffanswer = *buffanswerptr;
  /* first validate (more or less) the correctness of the bootp strucure */
  if (bufflen < 240) return(0); /* a BOOTP request is at least 240 bytes long */
  for (x = 0; x < (int)sizeof(bootphdr); x++) {
    if (udppayload[x] != bootphdr[x]) return(0);
  }
  for (x = 0; x < 4; x++) if (udppayload[236 + x] != dhcpcookie[x]) return(0); /* check the DHCP cookie */
  /* validation is ok - fetch the transaction id and client's mac now */
  transactionid = udppayload[4];
  transactionid <<= 8;
  transactionid |= udppayload[5];
  transactionid <<= 8;
  transactionid |= udppayload[6];
  transactionid <<= 8;
  transactionid |= udppayload[7];
  dhcpclientmac[0] = udppayload[28];
  dhcpclientmac[1] = udppayload[29];
  dhcpclientmac[2] = udppayload[30];
  dhcpclientmac[3] = udppayload[31];
  dhcpclientmac[4] = udppayload[32];
  dhcpclientmac[5] = udppayload[33];
  /* printf("Got a DHCP message with transactionid = 0x%4X from %02X:%02X:%02X:%02X:%02X:%02X\n", transactionid, dhcpclientmac[0], dhcpclientmac[1], dhcpclientmac[2], dhcpclientmac[3], dhcpclientmac[4], dhcpclientmac[5]); */
  /* iterate through options until we find the DHCP msg type (option 53) */
  dhcpmsg = -1;
  for (x = 240; x < bufflen;) {
    int optionid = udppayload[x++];
    int optionlen;
    if (optionid == 0xFF) break; /* option 255 is 'END' */
    optionlen = udppayload[x++];
    if ((optionid == 53) && (optionlen == 1)) {
        dhcpmsg = udppayload[x++];
      } else {
        x += optionlen;
        /* printf("Got unknown option %d with %d bytes of payload. Skipping.\n", optionid, optionlen); */
    }
  }
  if (dhcpmsg < 0) {
    /* puts("Got a DHCP request without any DHCP message... Ignored."); */
    return(0);
  }
  if ((dhcpmsg == 1) || (dhcpmsg == 3)) { /* DISCOVER or REQUEST */
      uint32_t clientip;
      int y;
      /* printf("Got a DHCP request from %02X:%02X:%02X:%02X:%02X:%02X\n", dhcpclientmac[0], dhcpclientmac[1], dhcpclientmac[2], dhcpclientmac[3], dhcpclientmac[4], dhcpclientmac[5]); */
      /* compute an 'OFFER' answer */
      clientip = librouter_compute_client_ip(arptable, dhcpclientmac, myip & mynetmask); /* find the IP to assign to the client */
      x = 0;
      buffanswer[x++] = 0x02; /* msg type = BOOT REPLY */
      buffanswer[x++] = 0x01; /* hw type = ethernet */
      buffanswer[x++] = 0x06; /* hw addr len = 6 bytes */
      buffanswer[x++] = 0x00; /* hops = 0 */
      buffanswer[x++] = (transactionid >> 24) & 0xFF; /* transaction id */
      buffanswer[x++] = (transactionid >> 16) & 0xFF; /* transaction id */
      buffanswer[x++] = (transactionid >> 8) & 0xFF;  /* transaction id */
      buffanswer[x++] = transactionid & 0xFF;         /* transaction id */
      buffanswer[x++] = 0;    /* seconds elapsed */
      buffanswer[x++] = 0;    /* seconds elapsed */
      buffanswer[x++] = 0;    /* boot flags */
      buffanswer[x++] = 0;    /* boot flags */
      buffanswer[x++] = 0;    /* client IP addr */
      buffanswer[x++] = 0;    /* client IP addr */
      buffanswer[x++] = 0;    /* client IP addr */
      buffanswer[x++] = 0;    /* client IP addr */
      buffanswer[x++] = (clientip >> 24) & 0xFF; /* assigned addr */
      buffanswer[x++] = (clientip >> 16) & 0xFF; /* assigned addr */
      buffanswer[x++] = (clientip >> 8) & 0xFF;  /* assigned addr */
      buffanswer[x++] = clientip & 0xFF;         /* assigned addr */
      for (y = 0; y < 4; y++) buffanswer[x++] = 0;  /* nextserver IP addr */
      for (y = 0; y < 4; y++) buffanswer[x++] = 0;  /* relayagent IP addr */
      for (y = 0; y < 6; y++) buffanswer[x++] = dhcpclientmac[y]; /* client mac addr */
      for (y = 0; y < 10; y++) buffanswer[x++] = 0;  /* client hw padding */
      for (y = 0; y < 64; y++) buffanswer[x++] = 0;  /* server host name */
      for (y = 0; y < 128; y++) buffanswer[x++] = 0; /* boot file name */
      for (y = 0; y < 4; y++) buffanswer[x++] = dhcpcookie[y]; /* dhcp cookie */
      buffanswer[x++] = 53;   /* option 53 "DHCP Message type" */
      buffanswer[x++] = 0x01; /* option len = 1 byte */
      if (dhcpmsg == 1) { /* if it was a 'DISCOVER' request... */
          buffanswer[x++] = 0x02; /* DHCP OFFER */
        } else { /* if the request was a 'REQUEST', change the 'OFFER' to be an 'ACK' */
          buffanswer[x++] = 0x05; /* DHCP ACK */
      }
      buffanswer[x++] = 58;  /* option 58 is 'renewal time value' */
      buffanswer[x++] = 4;   /* option len = 4 bytes */
      buffanswer[x++] = 0;   /* the renewal time is 24h */
      buffanswer[x++] = 0x01;
      buffanswer[x++] = 0x51;
      buffanswer[x++] = 0x80;
      buffanswer[x++] = 59;  /* option 59 is 'rebinding time value' */
      buffanswer[x++] = 4;   /* option len = 4 bytes */
      buffanswer[x++] = 0;   /* the rebinding time is 46h */
      buffanswer[x++] = 0x02;
      buffanswer[x++] = 0x86;
      buffanswer[x++] = 0xE0;
      buffanswer[x++] = 51;  /* option 51 is 'lease time' */
      buffanswer[x++] = 4;   /* option len = 4 bytes */
      buffanswer[x++] = 0;   /* the lease time is 48h */
      buffanswer[x++] = 0x02;
      buffanswer[x++] = 0xA3;
      buffanswer[x++] = 0x00;
      buffanswer[x++] = 54;  /* option 54 is 'DHCP server identifier' */
      buffanswer[x++] = 4;   /* option len = 4 bytes */
      buffanswer[x++] = (myip >> 24) & 0xFF;
      buffanswer[x++] = (myip >> 16) & 0xFF;
      buffanswer[x++] = (myip >> 8) & 0xFF;
      buffanswer[x++] = myip & 0xFF;
      buffanswer[x++] = 1;  /* option 1 is 'subnet mask' */
      buffanswer[x++] = 4;  /* option len = 4 bytes */
      buffanswer[x++] = (mynetmask >> 24) & 0xFF;
      buffanswer[x++] = (mynetmask >> 16) & 0xFF;
      buffanswer[x++] = (mynetmask >> 8) & 0xFF;
      buffanswer[x++] = mynetmask & 0xFF;
      buffanswer[x++] = 6;   /* option 6 is 'Domain name server' */
      buffanswer[x++] = 4;   /* option len = 4 bytes */
      buffanswer[x++] = (myip >> 24) & 0xFF;
      buffanswer[x++] = (myip >> 16) & 0xFF;
      buffanswer[x++] = (myip >> 8) & 0xFF;
      buffanswer[x++] = 3 /*myip & 0xFF*/;   /* this is a special thing related to SLIRP - it provides a DNS redirector under its .3 IP address */
      buffanswer[x++] = 3;   /* option 3 is 'Routers' */
      buffanswer[x++] = 4;   /* option len = 4 bytes (could be more, if there are more routers) */
      buffanswer[x++] = (myip >> 24) & 0xFF;
      buffanswer[x++] = (myip >> 16) & 0xFF;
      buffanswer[x++] = (myip >> 8) & 0xFF;
      buffanswer[x++] = myip & 0xFF;
      buffanswer[x++] = 255; /* option '255' is not a real option, it's a END marker */
      for (y = 0; y < 16; y++) buffanswer[x++] = 0; /* add some "optional padding".. I don't know what's the point is, but I've seen other DHCP server doing this, so they might be some obscure reasons */
      /* encapsulate into UDP */
      librouter_forge_udp(67, 68, &buffanswer, &x);
      librouter_forge_ip(myip, clientip, 17, &buffanswer, &x);
      librouter_forge_eth(&buffanswer, &x, mymac, dhcpclientmac);
      *buffanswerptr = buffanswer;
      /* printf("client will receive IP %u.%u.%u.%u\n", (clientip >> 24) & 0xFF, (clientip >> 16) & 0xFF, (clientip >> 8) & 0xFF, clientip & 0xFF); */
      return(x);
    } else {
      /* printf("Got a DHCP request of unknown type (%d)\n", dhcpmsg); */
      return(0);
  }
}
