/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * attremu.c
 *
 * Attribute controller emulator for VGAemu
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
 * This code emulates the Atrribute Controller for VGAemu.
 * The Attribute Controller is part of the VGA (Video Graphics Array,
 * a video adapter for IBM compatible PC's).
 *
 * For an excellent reference to programming SVGA cards see Finn Thøgersen's
 * VGADOC4, available at http://www.datashopper.dk/~finth
 *
 * VGADOC says about the attribute controller:
 * Port 3C0h is special in that it is both address and data-write register.
 * Data reads happen from port 3C1h. An internal flip-flop remembers whether 
 * it is currently acting as an address or data register.
 * Reading port 3DAh will reset the flip-flop to address mode.
 *
 * What the DOC does not say, but my card does:
 * - index reg, bit 5 affects only the palette regs (index < 16) (Not even the
 *   overscan color reg!)
 * - a read from an inaccessible reg (or a nonexistent reg) returns the index reg
 * - all unspecified bits are zero and cannot be set
 * -- sw
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The Attribute Controller emulator for VGAemu.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1996/05/06:
 *  - Changed Attr_get_input_status_1() to get it _slower_.
 *    Idea from Adam D. Moss (aspirin@tigerden.com).
 * 1996/05/09:
 *  - Added horizontal retrace too (--adm)
 *
 * 1998/09/20: Added proper init values for all graphics modes.
 * -- sw
 *
 * 1998/10/25: Restructured code, removed unnecessary parts, added some code.
 * The emulation is now (as far as I could test) identical to my VGA chip's
 * controller (S3 968).
 * -- sw
 *
 * DANG_END_CHANGELOG
 */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* define to debug the Attribute Controller */
#undef DEBUG_ATTR


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define ATTR_INDEX_FLIPFLOP	0
#define ATTR_DATA_FLIPFLOP	1

#define ATTR_MODE_CTL	0x10
#define ATTR_OVERSCAN	0x11
#define ATTR_COL_PLANE	0x12
#define ATTR_HOR_PAN	0x13
#define ATTR_COL_SELECT	0x14

#if !defined True
#define False 0
#define True 1
#endif

#ifdef DEBUG_ATTR
#define attr_deb(x...) v_printf(x)
#else
#define attr_deb(x...)
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "timers.h"
#include "vgaemu.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned char attr_ival[9][ATTR_MAX_INDEX + 1] = {
  {	/* 0  TEXT, 4 bits */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x0c, 0x00, 0x0f, 0x08, 0x00
  },
  {	/* 1  CGA, 2 bits */
    0x00, 0x13, 0x15, 0x17, 0x02, 0x04, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x01, 0x00, 0x03, 0x00, 0x00
  },
  {	/* 2  CGA: PL1, 1 bit (mode 0x06) */
    0x00, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
    0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,
    0x01, 0x00, 0x01, 0x00, 0x00
  },
  {	/* 3  TEXT, 1 bit */
    0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08,
    0x10, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,
    0x0e, 0x00, 0x0f, 0x08, 0x00
  },
  {	/* 4  EGA: PL4, 4 bits (modes 0x0d, 0x0e) */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x01, 0x00, 0x0f, 0x00, 0x00
  },
  {	/* 5  EGA: PL1, 1 bit (mode 0x0f) */
    0x00, 0x08, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
    0x0b, 0x00, 0x05, 0x00, 0x00
  },
  {	/* 6  VGA: PL4, 4 bit */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
    0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,
    0x01, 0x00, 0x0f, 0x00, 0x00
  },
  {	/* 7  VGA: PL1, 1 bit */
    0x00, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,
    0x01, 0x00, 0x01, 0x00, 0x00
  },
  {	/* 8  P8, 8 bit */
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x41, 0x00, 0x0f, 0x00, 0x00
  }
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Set unspecified bits to zero.
 */
static unsigned char clear_undef_bits(unsigned char i, unsigned char v)
{
  unsigned char m;

  if(i > ATTR_MAX_INDEX) return v;

  switch(i) {
    case ATTR_MODE_CTL  : m = ~0x10; break;
    case ATTR_HOR_PAN   :
    case ATTR_COL_SELECT: m = 0x0f; break;
    default             : m = 0x3f;
  }

  return v & m;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION Attr_init
 *
 * Initializes the attribute controller.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void Attr_init()
{
  int i = 0, j;
  vga_mode_info *vmi = vga.mode_info;

  if(vmi == NULL) {
    v_printf("VGAEmu: Attr_init failed\n");
  }
  else {
    switch(vmi->mode) {
      case 0x06: i = 2; break;
      case 0x0d:
      case 0x0e: i = 4; break;
      case 0x0f: i = 5; break;
      default:
        switch(vmi->color_bits) {
          case  1: if(vmi->type == TEXT) i = 3;
                   if(vmi->type == PL1)  i = 7; break;
          case  2: if(vmi->type == CGA)  i = 1; break;
          case  4: if(vmi->type == TEXT) i = 0;
                   if(vmi->type == PL4)  i = 6; break;
          case  8:
          case 15:
          case 16:
          case 24:
          case 32: i = 8;
        }
    }
  }

  for(j = 0; j <= ATTR_MAX_INDEX; j++) {
    vga.attr.data[j] = attr_ival[i][j];
    vga.attr.dirty[j] = True;
  }

  vga.color_modified = True;
  vga.attr.index = 0;
  vga.attr.cpu_video = 0x20;
  vga.attr.flipflop = ATTR_INDEX_FLIPFLOP;

  v_printf("VGAEmu: Attr_init done\n");
}


/*
 * DANG_BEGIN_FUNCTION Attr_get_entry
 *
 * Directly reads the Attribute Controller's registers.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Attr_get_entry(unsigned char index)
{
  unsigned char u;

  u = index <= ATTR_MAX_INDEX ? vga.attr.data[index] : 0xff;

  attr_deb("VGAEmu: Attr_get_entry: data[0x%02x] = 0x%02x\n", (unsigned) index, (unsigned) u);

  return u;
}


/*
 * DANG_BEGIN_FUNCTION Attr_set_entry
 *
 * Directly sets the Attribute Controller's registers.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void Attr_set_entry(unsigned char index, unsigned char value)
{
  unsigned i;

  attr_deb("VGAEmu: Attr_set_entry: data[0x%02x] = 0x%02x\n", (unsigned) index, (unsigned) value);

  if(index > ATTR_MAX_INDEX) return;

  value = clear_undef_bits(index, value);

  if(vga.attr.data[index] != value) {
    vga.attr.data[index] = value;
    vga.attr.dirty[index] = True;
    vga.color_modified = True;
    if(index == ATTR_MODE_CTL || index == ATTR_COL_SELECT) {
      for(i = 0; i < 16; i++) vga.attr.dirty[i] = True;
    }
  }
}


/*
 * DANG_BEGIN_FUNCTION Attr_read_value
 *
 * Emulates reads from the attribute controller.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Attr_read_value()
{
  unsigned i = vga.attr.index;
  unsigned char uc = i | vga.attr.cpu_video;

  if(i <= ATTR_MAX_INDEX && (vga.attr.cpu_video == 0 ||i > 15)) {
    uc = vga.attr.data[i];

    attr_deb("VGAEmu: Attr_read_value: attr[0x%02x] = 0x%02x\n", i, (unsigned) uc);
  }
  else {
    attr_deb("VGAEmu: Attr_read_value: data reg inaccessible, index = 0x%02x\n", (unsigned) uc);
  }

  return uc;
}


/*
 * DANG_BEGIN_FUNCTION Attr_write_value
 *
 * Emulates writes to attribute controller combined index and data
 * register. Read VGADOC for details.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void Attr_write_value(unsigned char data)
{
  unsigned i, j;

  if(vga.attr.flipflop == ATTR_INDEX_FLIPFLOP) {
    vga.attr.flipflop = ATTR_DATA_FLIPFLOP;

    vga.attr.index = data & 0x1f;
    vga.attr.cpu_video = data & 0x20;

    attr_deb(
      "VGAEmu: Attr_write_value: index = 0x%02x\n",
      (unsigned) (vga.attr.index | vga.attr.cpu_video)
    );
  }
  else {	/* Attr_flipflop == ATTR_DATA_FLIPFLOP */
    vga.attr.flipflop = ATTR_INDEX_FLIPFLOP;

    i = vga.attr.index;

    if(i <= ATTR_MAX_INDEX && (vga.attr.cpu_video == 0 || i > 15)) {
      vga.attr.data[i] = clear_undef_bits(i, data);
      vga.attr.dirty[i] = True;
      vga.color_modified = True;
      if(i == ATTR_MODE_CTL || i == ATTR_COL_SELECT) {
        for(j = 0; j < 16; j++) vga.attr.dirty[j] = True;
      }

      attr_deb("VGAEmu: Attr_write_value: attr[0x%02x] = 0x%02x\n", i, (unsigned) data);
    }
    else {
      attr_deb("VGAEmu: Attr_write_value: data ignored\n");
    }
  }
}


/*
 * DANG_BEGIN_FUNCTION Attr_get_index
 *
 * Returns the current index of the attribute controller.
 * This is a hardware emulation function, though in fact this function
 * is undefined in a real attribute controller.
 * Well, it is exactly what my VGA board (S3) does. -- sw
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Attr_get_index()
{
  unsigned char uc = vga.attr.index | vga.attr.cpu_video;

  attr_deb("VGAEmu: Attr_get_index: index = 0x%02x\n", (unsigned) uc);

  return uc;
}


/*
 * DANG_BEGIN_FUNCTION Attr_get_input_status_1
 *
 * Emulate input status #1 register. The essential part is to
 * simulate the retrace signals.
 * Clears the Attribute Controller's flip-flop.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Attr_get_input_status_1()
{
  /* 
   * Graphic status - many programs will use this port to sync with
   * the vert & horz retrace so as not to cause CGA snow. On VGAs this
   * register is used to get full (read: fast) access to the video memory 
   * during the vertical retrace.
   *
   * Idea from Adam Moss:
   * Wait 20 milliseconds before we tell the DOS program that the VGA is
   * in a vertical retrace. This is to avoid that some programs run too
   * _fast_ in Dosemu (yes, I know, this sounds odd, but such programs
   * really exist!). This option works only if the system has
   * gettimeofday().
   *
   * Now simpler and more 'realtime', for better or for worse.  Implements
   * horizontal retrace too.  (--adm)
   *
   */
#ifdef HAVE_GETTIMEOFDAY
  static unsigned char hretrace=0, vretrace=0, first=1;
  static hitimer_t t_vretrace = 0;
  /* Timings are 'ballpark' guesses and may vary from mode to mode, but
     such accuracy is probably not important... I hope. (--adm) */
  static int vvfreq = 14250;	/* 70 Hz */
  hitimer_t t;
  long tdiff;
#else
  static unsigned int cga_r=0;
#endif
  unsigned char retval;

  vga.attr.flipflop = ATTR_INDEX_FLIPFLOP;

#ifdef HAVE_GETTIMEOFDAY
  t=GETusTIME(0);

  if (first) {
    t_vretrace=t; first=0;
  }
  tdiff = t-t_vretrace;

  switch (vretrace) {
    /* set retrace on timeout */
    case 0: if (tdiff > vvfreq) {
	  /* We're in vertical retrace?  If so, set VR and DE flags */
    		vretrace=0x09; t_vretrace=t;
    		v_printf("V_RETRACE\n");
    	    }
    	    else
	  /* We're in horizontal retrace?  If so, just set DE flag, 0 in VR */
    	    	hretrace = ((tdiff%49) > 35);
    	    break;
    /* Otherwise, we're in 'display' mode and should return 0 in DE and VR */
    /* set display after 1ms from retrace start */
    case 9: if (tdiff > 1000) vretrace=hretrace=0;
    	    break;
  }
  retval = 0xc4 | hretrace | vretrace;
#else
  retval=((cga_r ^= 1) ? 0xcf : 0xc6);
#endif

  attr_deb("VGAEmu: Attr_get_input_status_1: status = 0x%02x\n", retval);

  return retval;
}

