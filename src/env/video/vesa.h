/*
 * vesa.h
 *
 * Header file for VESA BIOS enhancements for the Linux dosemu VGA emulator
 *
 * Copyright (C) 1995, Erik Mouw and Arjan Filius
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 * email: J.A.K.Mouw@et.tudelft.nl, I.A.Filius@et.tudelft.nl
 *
 *
 * Read vesa.c for details
 *
 */




#if !defined __VESA_H
#define __VESA_H




/* VESA 1.1 is easier to implement */
#define VESA_VERSION	0x0101




void vesa_init(void);
void do_vesa_int(void);
int vesa_emu_fault(struct sigcontext_struct *scp);




#endif
