/*
 * vgaemu_inside.h    
 * (This sounds nice! Should write a book with a title like this :-)
 *
 * Header file for the VGA emulator for dosemu.
 * This file is for vgaemu internal use only. It defines structures and
 * functions for the VGA emulator and the VESA BIOS extentions. Other
 * parts of dosemu should interface to the VGA emulator through the
 * functions described in vgaemu.h. PLEASE don't use the functions in
 * this file!
 *
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
 * Read vgaemu.c for details
 *
 */

#if !defined __VGAEMU_INSIDE_H
#define __VGAEMU_INSIDE_H



/* 
 * Definition of video memory models.
 * NOTE: Don's change these defines, they're standard VESA! 
 */
#define TEXT		0x00 	/* definition of graphics or textmode */
#define GRAPH		0x01

#define CGA		0x01	/* definition of memory models */
#define HERCULES	0x02
#define EGA16		0x03
#define PACKEDPIXEL	0x04
#define NONCHAIN4	0x05
#define DIRECT		0x06
#define YUV		0x07

/* abbreveations of some memory models */
#define HERC		HERCULES
#define PL4		EGA16
#define P8		NONCHAIN4


/*
 * VGA BIOS offsets 
 */
#define VGAEMU_BIOS_START		0x0000
#define VGAEMU_BIOS_VERSIONSTRING	0x0010
#define VGAEMU_BIOS_VESASTRING		0x0050
#define VGAEMU_BIOS_WINDOWFUNCTION	0x0060
#define VGAEMU_BIOS_VESA_MODELIST	0x0080


#if !defined __ASM__

/*
 * Mode info structure
 */

typedef struct
{
  int mode;			/* video mode number */
  int type;			/* mode type (TEXT/GRAPH) */
  int x_res, y_res;		/* resolution in pixels */
  int x_box, y_box;		/* size of the character box */
  int x_char, y_char;		/* resolution in characters */
  int colors;			/* number of colors */
  unsigned long bufferstart;	/* start of the screen buffer */
  unsigned long bufferlen;	/* length of the screen buffer */
  int memorymodel;		/* used memory model */
} vga_mode_info;


vga_mode_info* get_vgaemu_mode_info(int mode);

#endif /* !defined __ASM__ */

#endif
