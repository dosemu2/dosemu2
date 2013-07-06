
librouter - a virtual router library
Copyright (C) Mateusz Viste 2013


*** What it is ***

librouter is a 'virtual router as a library' software. It provides the application with a small set of functions to exchange frames (as in ethernet-layer communication). librouter emulates a small LAN with a DHCP server, a default gateway and a DNS server:
 virtual network: 10.0.2.0/24
 default gateway: 10.0.2.1
 dns server.....: 10.0.2.3

For the 'pseudo WAN' interface of the virtual router, librouter uses SLiRP. Whenever librouter receives an ethernet frame targeting an IP that is not part of the 10.0.2.0/24 network, it hands the IP packet to SLiRP, via a SLIP tunnel.


*** How to use it ***

librouter provides a set of following functions:

 int librouter_init(char *slirpexec);
 int librouter_sendframe(int sock, uint8_t *buff, int len);
 int librouter_recvframe(int sock, uint8_t *buff);
 int librouter_close(int sock);

Read the librouter header (librouter.h) file for detailed information. The 'high level' idea is that you start librouter using librouter_init(), then you communicate using librouter_recvframe() and librouter_sendframe(), and when you are done, you close librouter using librouter_close().


*** Legal mumbo jumbo ***

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,  either version 3 of the License,  or (at your option)  any later version.

This program is  distributed in the hope that it will be useful,  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
