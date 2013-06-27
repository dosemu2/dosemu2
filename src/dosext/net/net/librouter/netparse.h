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

#ifndef netparse_h_sentinel
#define netparse_h_sentinel
void *parse_ethernet(uint8_t *ethframe, int ethframe_len, uint8_t **srcmac, uint8_t **dstmac, int *vlanid, int *ethertype);
void *parse_ipv4(uint8_t *packet, int len, uint8_t **ipsrc, uint8_t **ipdst, int *ipprotocol, int *dscp, int *ttl, int *id, int *fragoffset, int *morefragsflag);
void *parse_udp(uint8_t *packet, int len, int *portsrc, int *portdst);
void *parse_tcp(uint8_t *packet, int len, int *portsrc, int *portdst);
#endif
