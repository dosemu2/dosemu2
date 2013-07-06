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


#ifndef librouter_forgehdr_h_sentinel
#define librouter_forgehdr_h_sentinel
void librouter_forge_udp(int portsrc, int portdst, uint8_t **buffptr, int *bufflen);
void librouter_forge_ip(uint32_t ipsrc, uint32_t ipdst, int ipproto, uint8_t **buffptr, int *bufflen);
void librouter_forge_eth(uint8_t **buffptr, int *bufflen, uint8_t *srcmac, uint8_t *dstmac);
int librouter_cksum(uint8_t *buff, int count);
#endif
