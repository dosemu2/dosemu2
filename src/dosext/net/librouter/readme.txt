
librouter - a virtual router library
Copyright (C) Mateusz Viste 2013


*** What it is ***

librouter is a 'virtual router as a library' software. It provides the application with a standard dgram socket that is used for (virtual) ethernet-layer communication. On the other side of the socket, librouter emulates a small LAN with a DHCP server, a default gateway and a DNS server:
 virtual network: 10.0.2.0/24
 default gateway: 10.0.2.1
 dns server.....: 10.0.2.3

For the 'pseudo WAN' interface of the virtual router, librouter uses SLIRP. Whenever librouter receives an ethernet frame targeting an IP that is not part of the 10.0.2.0/24 network, it hands the IP packet to SLIRP, via a SLIP tunnel.


*** How to use it ***

librouter provides one entry function:

  int librouter_init(char *slirpexec);

This function must be used by the application to initialize librouter. *slirpexec is a pointer to a string containing the path to the slirp executable that should be used. If *slirpexec is NULL, or an empty string, then librouter will try to find the executable in $PATH, trying 'slirp-fullbot' first, and if not found, then 'slirp'.

librouter returns a socket that the application can write() ethernet frames to, read() ethernet frams from, or select() to await for activity. On error, librouter returns a negative value.


*** Legal mumbo jumbo ***

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation,  either version 3 of the License,  or (at your option)  any later version.

This program is  distributed in the hope that it will be useful,  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
