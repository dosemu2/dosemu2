/*
 * librouter is a virtual router software that moves packets back and forth
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

#ifndef librouter_h_sentinel
  #define librouter_h_sentinel
  int librouter_init(char *slirpexec);
#endif
