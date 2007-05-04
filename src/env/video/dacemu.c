/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * dacemu.c
 *
 * DAC emulator for VGAemu
 *
 * Copyright (C) 1995 1996, Erik Mouw and Arjan Filius
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
 * This code emulates the DAC (Digital to Analog Converter) on a VGA
 * (Video Graphics Array, a video adapter for IBM PC's) for VGAemu.
 *
 * For an excellent reference to programming SVGA cards see Finn Thøgersen's
 * VGADOC4, available at http://www.datashopper.dk/~finth
 *
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The DAC emulator for DOSEMU.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1998/09/20: Added proper DAC init values for all graphics modes.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * 1998/10/25: Cleaned up the interface, removed unnecessary parts.
 * Working PEL mask support.
 * -- sw
 *
 * 1998/11/01: Reworked DAC init code.
 * -- sw
 *
 * 1998/12/12: Added RGB to gray scale conversion.
 * -- sw
 *
 * DANG_END_CHANGELOG
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the DAC.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define	DEBUG_DAC	0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define DAC_READ_MODE 3
#define DAC_WRITE_MODE 0

#if !defined True
#define False 0
#define True 1
#endif

#define dac_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_DAC >= 1
#define dac_deb(x...) v_printf("VGAEmu: " x)
#else
#define dac_deb(x...)
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "vgaemu.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
typedef struct { unsigned char r, g, b; } _DAC_entry;


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static _DAC_entry dac_vga[256] = {
  {0x00, 0x00, 0x00}, {0x00, 0x00, 0x2a}, {0x00, 0x2a, 0x00}, {0x00, 0x2a, 0x2a},
  {0x2a, 0x00, 0x00}, {0x2a, 0x00, 0x2a}, {0x2a, 0x15, 0x00}, {0x2a, 0x2a, 0x2a},
  {0x15, 0x15, 0x15}, {0x15, 0x15, 0x3f}, {0x15, 0x3f, 0x15}, {0x15, 0x3f, 0x3f},
  {0x3f, 0x15, 0x15}, {0x3f, 0x15, 0x3f}, {0x3f, 0x3f, 0x15}, {0x3f, 0x3f, 0x3f},
  {0x00, 0x00, 0x00}, {0x05, 0x05, 0x05}, {0x08, 0x08, 0x08}, {0x0b, 0x0b, 0x0b},
  {0x0e, 0x0e, 0x0e}, {0x11, 0x11, 0x11}, {0x14, 0x14, 0x14}, {0x18, 0x18, 0x18},
  {0x1c, 0x1c, 0x1c}, {0x20, 0x20, 0x20}, {0x24, 0x24, 0x24}, {0x28, 0x28, 0x28},
  {0x2d, 0x2d, 0x2d}, {0x32, 0x32, 0x32}, {0x38, 0x38, 0x38}, {0x3f, 0x3f, 0x3f},
  {0x00, 0x00, 0x3f}, {0x10, 0x00, 0x3f}, {0x1f, 0x00, 0x3f}, {0x2f, 0x00, 0x3f},
  {0x3f, 0x00, 0x3f}, {0x3f, 0x00, 0x2f}, {0x3f, 0x00, 0x1f}, {0x3f, 0x00, 0x10},
  {0x3f, 0x00, 0x00}, {0x3f, 0x10, 0x00}, {0x3f, 0x1f, 0x00}, {0x3f, 0x2f, 0x00},
  {0x3f, 0x3f, 0x00}, {0x2f, 0x3f, 0x00}, {0x1f, 0x3f, 0x00}, {0x10, 0x3f, 0x00},
  {0x00, 0x3f, 0x00}, {0x00, 0x3f, 0x10}, {0x00, 0x3f, 0x1f}, {0x00, 0x3f, 0x2f},
  {0x00, 0x3f, 0x3f}, {0x00, 0x2f, 0x3f}, {0x00, 0x1f, 0x3f}, {0x00, 0x10, 0x3f},
  {0x1f, 0x1f, 0x3f}, {0x27, 0x1f, 0x3f}, {0x2f, 0x1f, 0x3f}, {0x37, 0x1f, 0x3f},
  {0x3f, 0x1f, 0x3f}, {0x3f, 0x1f, 0x37}, {0x3f, 0x1f, 0x2f}, {0x3f, 0x1f, 0x27},

  {0x3f, 0x1f, 0x1f}, {0x3f, 0x27, 0x1f}, {0x3f, 0x2f, 0x1f}, {0x3f, 0x37, 0x1f},
  {0x3f, 0x3f, 0x1f}, {0x37, 0x3f, 0x1f}, {0x2f, 0x3f, 0x1f}, {0x27, 0x3f, 0x1f},
  {0x1f, 0x3f, 0x1f}, {0x1f, 0x3f, 0x27}, {0x1f, 0x3f, 0x2f}, {0x1f, 0x3f, 0x37},
  {0x1f, 0x3f, 0x3f}, {0x1f, 0x37, 0x3f}, {0x1f, 0x2f, 0x3f}, {0x1f, 0x27, 0x3f},
  {0x2d, 0x2d, 0x3f}, {0x31, 0x2d, 0x3f}, {0x36, 0x2d, 0x3f}, {0x3a, 0x2d, 0x3f},
  {0x3f, 0x2d, 0x3f}, {0x3f, 0x2d, 0x3a}, {0x3f, 0x2d, 0x36}, {0x3f, 0x2d, 0x31},
  {0x3f, 0x2d, 0x2d}, {0x3f, 0x31, 0x2d}, {0x3f, 0x36, 0x2d}, {0x3f, 0x3a, 0x2d},
  {0x3f, 0x3f, 0x2d}, {0x3a, 0x3f, 0x2d}, {0x36, 0x3f, 0x2d}, {0x31, 0x3f, 0x2d},
  {0x2d, 0x3f, 0x2d}, {0x2d, 0x3f, 0x31}, {0x2d, 0x3f, 0x36}, {0x2d, 0x3f, 0x3a},
  {0x2d, 0x3f, 0x3f}, {0x2d, 0x3a, 0x3f}, {0x2d, 0x36, 0x3f}, {0x2d, 0x31, 0x3f},
  {0x00, 0x00, 0x1c}, {0x07, 0x00, 0x1c}, {0x0e, 0x00, 0x1c}, {0x15, 0x00, 0x1c},
  {0x1c, 0x00, 0x1c}, {0x1c, 0x00, 0x15}, {0x1c, 0x00, 0x0e}, {0x1c, 0x00, 0x07},
  {0x1c, 0x00, 0x00}, {0x1c, 0x07, 0x00}, {0x1c, 0x0e, 0x00}, {0x1c, 0x15, 0x00},
  {0x1c, 0x1c, 0x00}, {0x15, 0x1c, 0x00}, {0x0e, 0x1c, 0x00}, {0x07, 0x1c, 0x00},
  {0x00, 0x1c, 0x00}, {0x00, 0x1c, 0x07}, {0x00, 0x1c, 0x0e}, {0x00, 0x1c, 0x15},
  {0x00, 0x1c, 0x1c}, {0x00, 0x15, 0x1c}, {0x00, 0x0e, 0x1c}, {0x00, 0x07, 0x1c},

  {0x0e, 0x0e, 0x1c}, {0x11, 0x0e, 0x1c}, {0x15, 0x0e, 0x1c}, {0x18, 0x0e, 0x1c},
  {0x1c, 0x0e, 0x1c}, {0x1c, 0x0e, 0x18}, {0x1c, 0x0e, 0x15}, {0x1c, 0x0e, 0x11},
  {0x1c, 0x0e, 0x0e}, {0x1c, 0x11, 0x0e}, {0x1c, 0x15, 0x0e}, {0x1c, 0x18, 0x0e},
  {0x1c, 0x1c, 0x0e}, {0x18, 0x1c, 0x0e}, {0x15, 0x1c, 0x0e}, {0x11, 0x1c, 0x0e},
  {0x0e, 0x1c, 0x0e}, {0x0e, 0x1c, 0x11}, {0x0e, 0x1c, 0x15}, {0x0e, 0x1c, 0x18},
  {0x0e, 0x1c, 0x1c}, {0x0e, 0x18, 0x1c}, {0x0e, 0x15, 0x1c}, {0x0e, 0x11, 0x1c},
  {0x14, 0x14, 0x1c}, {0x16, 0x14, 0x1c}, {0x18, 0x14, 0x1c}, {0x1a, 0x14, 0x1c},
  {0x1c, 0x14, 0x1c}, {0x1c, 0x14, 0x1a}, {0x1c, 0x14, 0x18}, {0x1c, 0x14, 0x16},
  {0x1c, 0x14, 0x14}, {0x1c, 0x16, 0x14}, {0x1c, 0x18, 0x14}, {0x1c, 0x1a, 0x14},
  {0x1c, 0x1c, 0x14}, {0x1a, 0x1c, 0x14}, {0x18, 0x1c, 0x14}, {0x16, 0x1c, 0x14},
  {0x14, 0x1c, 0x14}, {0x14, 0x1c, 0x16}, {0x14, 0x1c, 0x18}, {0x14, 0x1c, 0x1a},
  {0x14, 0x1c, 0x1c}, {0x14, 0x1a, 0x1c}, {0x14, 0x18, 0x1c}, {0x14, 0x16, 0x1c},
  {0x00, 0x00, 0x10}, {0x04, 0x00, 0x10}, {0x08, 0x00, 0x10}, {0x0c, 0x00, 0x10},
  {0x10, 0x00, 0x10}, {0x10, 0x00, 0x0c}, {0x10, 0x00, 0x08}, {0x10, 0x00, 0x04},
  {0x10, 0x00, 0x00}, {0x10, 0x04, 0x00}, {0x10, 0x08, 0x00}, {0x10, 0x0c, 0x00},
  {0x10, 0x10, 0x00}, {0x0c, 0x10, 0x00}, {0x08, 0x10, 0x00}, {0x04, 0x10, 0x00},

  {0x00, 0x10, 0x00}, {0x00, 0x10, 0x04}, {0x00, 0x10, 0x08}, {0x00, 0x10, 0x0c},
  {0x00, 0x10, 0x10}, {0x00, 0x0c, 0x10}, {0x00, 0x08, 0x10}, {0x00, 0x04, 0x10},
  {0x08, 0x08, 0x10}, {0x0a, 0x08, 0x10}, {0x0c, 0x08, 0x10}, {0x0e, 0x08, 0x10},
  {0x10, 0x08, 0x10}, {0x10, 0x08, 0x0e}, {0x10, 0x08, 0x0c}, {0x10, 0x08, 0x0a},
  {0x10, 0x08, 0x08}, {0x10, 0x0a, 0x08}, {0x10, 0x0c, 0x08}, {0x10, 0x0e, 0x08},
  {0x10, 0x10, 0x08}, {0x0e, 0x10, 0x08}, {0x0c, 0x10, 0x08}, {0x0a, 0x10, 0x08},
  {0x08, 0x10, 0x08}, {0x08, 0x10, 0x0a}, {0x08, 0x10, 0x0c}, {0x08, 0x10, 0x0e},
  {0x08, 0x10, 0x10}, {0x08, 0x0e, 0x10}, {0x08, 0x0c, 0x10}, {0x08, 0x0a, 0x10},
  {0x0b, 0x0b, 0x10}, {0x0c, 0x0b, 0x10}, {0x0d, 0x0b, 0x10}, {0x0f, 0x0b, 0x10},
  {0x10, 0x0b, 0x10}, {0x10, 0x0b, 0x0f}, {0x10, 0x0b, 0x0d}, {0x10, 0x0b, 0x0c},
  {0x10, 0x0b, 0x0b}, {0x10, 0x0c, 0x0b}, {0x10, 0x0d, 0x0b}, {0x10, 0x0f, 0x0b},
  {0x10, 0x10, 0x0b}, {0x0f, 0x10, 0x0b}, {0x0d, 0x10, 0x0b}, {0x0c, 0x10, 0x0b},
  {0x0b, 0x10, 0x0b}, {0x0b, 0x10, 0x0c}, {0x0b, 0x10, 0x0d}, {0x0b, 0x10, 0x0f},
  {0x0b, 0x10, 0x10}, {0x0b, 0x0f, 0x10}, {0x0b, 0x0d, 0x10}, {0x0b, 0x0c, 0x10},
  {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}, {0x00, 0x00, 0x00}
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION DAC_init
 *
 * Initializes the DAC.
 * It depends on a correct value in vga.pixel_size. This function should be
 * called during VGA mode initialization.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_init()
{
  DAC_entry dac_zero = {True, 0, 0, 0}, de = dac_zero;
  int i;

  if(vga.pixel_size <= 4) {
    if(vga.VGA_mode == 7 || vga.VGA_mode == 15) {	/* mono modes */
      for(i = 0; i < 64; i++) {
        switch((i >> 3) & 3) {
          case 0: de.r = de.g = de.b = 0x00; break;
          case 1:
          case 2: de.r = de.g = de.b = 0x2a; break;
          case 3: de.r = de.g = de.b = 0x3f; break;
        }
        vga.dac.rgb[i] = de;
      }
    }
    else if(
            vga.VGA_mode ==  4 || vga.VGA_mode ==  5 || vga.VGA_mode ==  6 ||
            vga.VGA_mode == 13 || vga.VGA_mode == 14
           ) {
      for(i = 0; i < 64; i++) {
        de.r = (((i & 4) >> 1) + ((i & 0x10) >> 4)) * 0x15;
        de.g = (((i & 2) >> 0) + ((i & 0x10) >> 4)) * 0x15;
        de.b = (((i & 1) << 1) + ((i & 0x10) >> 4)) * 0x15;
        if((i & 0x17) == 6) de.g = 0x15;
        vga.dac.rgb[i] = de;
      }
    }
    else {
      for(i = 0; i < 64; i++) {
        de.r = (((i & 4) >> 1) + ((i & 0x20) >> 5)) * 0x15;
        de.g = (((i & 2) >> 0) + ((i & 0x10) >> 4)) * 0x15;
        de.b = (((i & 1) << 1) + ((i & 0x08) >> 3)) * 0x15;
        vga.dac.rgb[i] = de;
      }
    }
    while(i < 256) vga.dac.rgb[i++] = dac_zero;
  }
  else {
    for(i = 0; i < 256; i++) {
      de.r = dac_vga[i].r;
      de.g = dac_vga[i].g;
      de.b = dac_vga[i].b;
      vga.dac.rgb[i] = de;
    }
  }

  vga.dac.bits = 6;
  vga.color_modified = True;
  vga.dac.pel_index = 'r';
  vga.dac.pel_mask = 0xff;
  vga.dac.state = DAC_READ_MODE;
  vga.dac.read_index = 0;
  vga.dac.write_index = 0;

  dac_msg("DAC_init done\n");
}


/*
 * DANG_BEGIN_FUNCTION DAC_set_width
 *
 * Sets the DAC width. Typical values are 6 or 8 bits.
 * In theory, we support other values as well (untested).
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_set_width(unsigned bits)
{
  int i;

  if(bits > 8) bits = 8;
  if(bits < 4) bits = 4;	/* it's no use to allow other values than 6 or 8, but anyway... */

  dac_deb(
    "DAC_set_width: width = %u bits%s\n",
    bits, vga.dac.bits == bits ? " (unchanged)" : ""
  );

  if(vga.dac.bits != bits) {
    vga.reconfig.dac = 1;
    vga.dac.bits = bits;
    vga.color_modified = True;
    for(i = 0; i < 256; i++) vga.dac.rgb[i].index = True;	/* index = dirty flag ! */
  }
}


/*
 * DANG_BEGIN_FUNCTION DAC_get_entry
 *
 * Returns a complete DAC entry (r, g, b).
 * Don't forget to set DAC_entry.index first!
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_get_entry(DAC_entry *entry)
{
  unsigned char u;

  *entry = vga.dac.rgb[u = entry->index]; entry->index = u;

  dac_deb(
    "DAC_get_entry: dac.rgb[0x%02x] = 0x%02x 0x%02x 0x%02x\n",
    entry->index, entry->r, entry->g, entry->b
  );
}


/*
 * DANG_BEGIN_FUNCTION DAC_set_entry
 *
 * Sets a complete DAC entry (r,g,b).
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_set_entry(unsigned char index, unsigned char r, unsigned char g, unsigned char b)
{
  unsigned mask = (1 << vga.dac.bits ) - 1;

  r &= mask; g &= mask; b &= mask;

  dac_deb(
    "DAC_set_entry: dac.rgb[0x%02x] = 0x%02x 0x%02x 0x%02x\n",
    (unsigned) index, (unsigned) r, (unsigned) g, (unsigned) b);

  if(
    vga.dac.rgb[index].r != r ||
    vga.dac.rgb[index].g != g ||
    vga.dac.rgb[index].b != b
  ) {
    vga.color_modified = True;
    vga.dac.rgb[index].index = True;
    vga.dac.rgb[index].r = r;
    vga.dac.rgb[index].g = g;
    vga.dac.rgb[index].b = b;
  }
}


/*
 * DANG_BEGIN_FUNCTION DAC_rgb2gray
 *
 * Converts a DAC register's RGB values to gray scale.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_rgb2gray(unsigned char index)
{
  unsigned i, m = (vga.dac.bits << 1) - 1;

  i =  77 * vga.dac.rgb[index].r +
      151 * vga.dac.rgb[index].g +
       28 * vga.dac.rgb[index].b;

  i = (i + 0x80) >> 8;
  if(i > m) i = m;

  vga.dac.rgb[index].index = True;
  vga.dac.rgb[index].r = vga.dac.rgb[index].g = vga.dac.rgb[index].b = i;
}


/*
 * DANG_BEGIN_FUNCTION DAC_set_read_index
 *
 * Specifies which palette entry is read.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_set_read_index(unsigned char index)
{
  dac_deb("DAC_set_read_index: index = 0x%02x\n", (unsigned) index);

  vga.dac.read_index = index;
  vga.dac.pel_index = 'r';
  vga.dac.state = DAC_READ_MODE;

  /* undefined behaviour by Starcon2 (write to 3C9 after 3C7) - clarence */
  vga.dac.write_index = index + 1;
}


/*
 * DANG_BEGIN_FUNCTION DAC_set_write_index
 *
 * Specifies which palette entry is written.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_set_write_index(unsigned char index)
{
  dac_deb("DAC_set_write_index: index = 0x%02x\n", (unsigned) index);

  vga.dac.write_index = index;
  vga.dac.pel_index = 'r';
  vga.dac.state = DAC_WRITE_MODE;
}


/*
 * DANG_BEGIN_FUNCTION DAC_read_value
 *
 * Read a value from the DAC. Each read will cycle through the registers for
 * red, green and blue. After a ``blue read'' the read index will be 
 * incremented. Read VGADOC4 if you want to know more about the DAC.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char DAC_read_value()
{
  unsigned char rv;

#if DEBUG_DAC >= 1
  char c = vga.dac.pel_index;
  unsigned char ri = vga.dac.read_index;
#endif

  switch(vga.dac.pel_index) {
    case 'r':
      rv = vga.dac.rgb[vga.dac.read_index].r;
      vga.dac.pel_index = 'g';
      break;

    case 'g':
      rv = vga.dac.rgb[vga.dac.read_index].g;
      vga.dac.pel_index = 'b';
      break;

    case 'b':
      rv = vga.dac.rgb[vga.dac.read_index].b;
      vga.dac.pel_index = 'r';
      vga.dac.read_index++;

      /* observed behaviour on a real system - clarence */
      vga.dac.write_index = vga.dac.read_index + 1;
      break;

    default:
      dac_msg("DAC_read_value: ERROR: pel_index out of range\n");
      vga.dac.pel_index = 'r';
      rv = 0;
      break;
    }

  dac_deb(
    "DAC_read_value: dac.rgb[0x%02x].%c = 0x%02x\n",
    (unsigned) ri, c, (unsigned) rv
  );

  return rv;
}


/*
 * DANG_BEGIN_FUNCTION DAC_write_value
 *
 * Write a value to the DAC. Each write will cycle through the registers for
 * red, green and blue. After a ``blue write'' the write index will be
 * incremented.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_write_value(unsigned char value)
{
  value &= (1 << vga.dac.bits) - 1;

  dac_deb(
    "DAC_write_value: dac.rgb[0x%02x].%c = 0x%02x\n",
    (unsigned) vga.dac.write_index, vga.dac.pel_index, (unsigned) value
  );

  vga.dac.rgb[vga.dac.write_index].index = True;	/* index = dirty flag ! */
  vga.color_modified = True;

  switch(vga.dac.pel_index) {
    case 'r':
      vga.dac.rgb[vga.dac.write_index].r = value;
      vga.dac.pel_index = 'g';
      break;

    case 'g':
      vga.dac.rgb[vga.dac.write_index].g = value;
      vga.dac.pel_index = 'b';
      break;

    case 'b':
      vga.dac.rgb[vga.dac.write_index].b = value;
      vga.dac.pel_index = 'r';
      vga.dac.write_index++;
      
      /* observed behaviour on a real system - clarence */
      vga.dac.read_index = vga.dac.write_index - 1;
      break;

    default:
      dac_msg("DAC_write_value: ERROR: pel_index out of range\n");
      vga.dac.pel_index = 'r';
      break;
  }
}


/*
 * DANG_BEGIN_FUNCTION DAC_get_pel_mask
 *
 * Returns the current PEL mask. Note that changed_vga_colors() already
 * applies the PEL mask; so applications should not worry too much about it.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char DAC_get_pel_mask()
{
  dac_deb("DAC_get_pel_mask: mask = 0x%02x\n", (unsigned) vga.dac.pel_mask);

  return vga.dac.pel_mask;
}


/*
 * DANG_BEGIN_FUNCTION DAC_set_pel_mask
 *
 * Sets the PEL mask and marks all DAC entries as dirty.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_set_pel_mask(unsigned char mask)
{
  int i;

  dac_deb("DAC_set_pel_mask: mask = 0x%02x\n", (unsigned) mask);

  if(vga.dac.pel_mask != mask) {
    vga.dac.pel_mask = mask;
    vga.color_modified = True;
    for(i = 0; i < 256; i++) { vga.dac.rgb[i].index = True; }	/* index = dirty flag ! */
  }
}


/*
 * DANG_BEGIN_FUNCTION DAC_get_state
 *
 * Returns the current state of the DAC.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char DAC_get_state()
{
  dac_deb("DAC_get_state: state = 0x%02x\n", vga.dac.state);

  return vga.dac.state;
}

