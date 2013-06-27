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


#include "arp.h"  /* include self for control */



/* returns 0 if both MAC addresses are equal, non-zero otherwise. */
int maccmp(uint8_t *maca, uint8_t *macb) {
  int x;
  for (x = 0; x < 6; x++) {
    if (maca[x] != macb[x]) return(1);
  }
  return(0);
}


/* whenever we receive a packet, we call arp_learn to make it possibly learn a new mac/ip pair. */
struct arptabletype *arp_learn(struct arptabletype *arptable, uint8_t *macaddr, uint32_t ipaddr) {
  struct arptabletype *arpentry;
  int x;
  for (arpentry = arptable; arpentry != NULL; arpentry = arpentry->next) {
    if (arpentry->ipaddr == ipaddr) { /* found entry - update it and quit */
      for (x = 0; x < 6; x++) arpentry->macaddr[x] = macaddr[x];
      return(arptable);
    }
  }
  /* if we are here, it means we haven't found any existing entry that match */
  arpentry = malloc(sizeof(struct arptabletype));
  if (arpentry == NULL) return(arptable); /* out of memory */
  /* fill the new entry with data */
  arpentry->ipaddr = ipaddr;
  for (x = 0; x < 6; x++) arpentry->macaddr[x] = macaddr[x];
  arpentry->next = arptable;
  return(arpentry);
}


/* looks into the arp table and returns a pointer to the mac address of the given IP. returns NULL if no record found. */
uint8_t *arp_getmac(struct arptabletype *arptable, uint32_t ipaddr) {
  struct arptabletype *arpentry;
  for (arpentry = arptable; arpentry != NULL; arpentry = arpentry->next) {
    if (arpentry->ipaddr == ipaddr) break; /* found entry */
  }
  return(arpentry->macaddr);
}


/* looks into the arp table and returns the first IP address of a given mac. returns 0 if no record found. */
uint32_t arp_getip(struct arptabletype *arptable, uint8_t *macaddr) {
  struct arptabletype *arpentry;
  for (arpentry = arptable; arpentry != NULL; arpentry = arpentry->next) {
    if (maccmp(arpentry->macaddr, macaddr) == 0) return(arpentry->ipaddr);
  }
  return(0);
}
