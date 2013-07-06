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

 /* initialize the librouter library. slirpexec is a string with the         *
  * path/filename of the slirp binary to use, or NULL if librouter have to   *
  * try to figure it out by itself. Returns a socket that can be monitored   *
  * for new incoming packets, or -1 on error.                                */
  int librouter_init(char *slirpexec);

 /* receive a frame from librouter. *buff is the place where the frame have  *
  * to be copied. This function is non-blocking. It returns 0 if nothing     *
  * awaited, a negative value on error, and a positive value with the length *
  * of the frame that has been copied into *buff. Make sure that buff can    *
  * contain at least 2048 bytes.                                             */
  int librouter_recvframe(int sock, uint8_t *buff);

  /* send a frame to librouter. *buff is the content you want to send. len   *
   * is the total length of the frame.                                       *
   * returns 0 on success, a negative value on error, or a positive value if *
   * *buff have been filled with a frame you should read. IMPORTANT: *buff   *
   * must be at least 1024 bytes long!.                                      */
  int librouter_sendframe(int sock, uint8_t *buff, int len);

  /* closes the librouter communication channel.                             */
  void librouter_close(int sock);

#endif
