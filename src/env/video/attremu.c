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
 * Lots of VGA information comes from Finn Thoergersen's VGADOC3, available
 * at every Simtel mirror in vga/vgadoc3.zip, and in the dosemu directory at 
 * tsx-11.mit.edu.
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
 * DANG_END_CHANGELOG
 */

/* Define to debug the Attribute controller */
#undef DEBUG_ATTR

#if !defined True
#define False 0
#define True 1
#endif




/* **************** include files **************** */
#include "config.h"
#include "emu.h"
#include "timers.h"
#include "vgaemu.h"


/* **************** Attribute Controller data **************** */

/*
 * vgadoc3 says about the attribute controller:
 *  Port 3C0h is special in that it is both address and data-write register.
 *  Data reads happen from port 3C1h. An internal flip-flop remembers whether 
 *  it is currently acting as an address or data register.
 *  Reading port 3dAh will reset the flip-flop to address mode.
 */

#define ATTR_INDEX_FLIPFLOP 0
#define ATTR_DATA_FLIPFLOP 1
#define ATTR_MAX_INDEX 0x14

unsigned char attr_ival[9][ATTR_MAX_INDEX + 2] = {
  /* 0  TEXT, 4 bits */
  {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,  0x0c, 0x00, 0x0f, 0x08, 0x00,  0x00},
  /* 1  CGA, 2 bits */
  {0x00, 0x13, 0x15, 0x17, 0x02, 0x04, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  0x01, 0x00, 0x03, 0x00, 0x00,  0x00},
  /* 2  CGA: PL1, 1 bit (mode 0x06) */
  {0x00, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17, 0x17,  0x01, 0x00, 0x01, 0x00, 0x00,  0x00},
  /* 3  TEXT, 1 bit */
  {0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x10, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18,  0x0e, 0x00, 0x0f, 0x08, 0x00,  0x00},
  /* 4  EGA: PL4, 4 bits (modes 0x0d, 0x0e) */
  {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,  0x01, 0x00, 0x0f, 0x00, 0x00,  0x00},
  /* 5  EGA: PL1, 1 bit (mode 0x0f) */
  {0x00, 0x08, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,  0x0b, 0x00, 0x05, 0x00, 0x00,  0x00},
  /* 6  VGA: PL4, 4 bit */
  {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f,  0x01, 0x00, 0x0f, 0x00, 0x00,  0x00},
  /* 7  VGA: PL1, 1 bit */
  {0x00, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f, 0x3f,  0x01, 0x00, 0x01, 0x00, 0x00,  0x00},
  /* 8  P8, 8 bit */
  {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,  0x41, 0x00, 0x0f, 0x00, 0x00,  0x00}
};


static unsigned char Attr_index = 0;
static int Attr_flipflop = ATTR_INDEX_FLIPFLOP;
static indexed_register Attr_data[ATTR_MAX_INDEX + 2] =
{
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x00 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x01 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x02 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x03 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x04 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x05 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x06 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x07 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x08 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x09 */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x0a */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x0b */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x0c */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x0d */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x0e */
  {0x00, 0x00, reg_read_write, False},        /* Palette 0x0f */
  {0x00, 0x00, reg_read_write, False},        /* Mode ctrl */
  {0x00, 0x00, reg_read_write, False},        /* Overscan color */
  {0x00, 0x00, reg_read_write, False},        /* Colorplane enable */
  {0x00, 0x00, reg_read_write, False},        /* Horizontal PEL panning */
  {0x00, 0x00, reg_read_write, False},        /* Color select */
  {0x00, 0x00, reg_read_write, False}         /* Dummy register for wrong indices */
};


/* *************** Attribute controller emulation functions *************** */

/*
 * DANG_BEGIN_FUNCTION Attr_init
 *
 * Initializes the attribute controller.
 *
 * DANG_END_FUNCTION
 */
void Attr_init(void)
{
  int i = 0, j;
  vga_mode_info *vmi = vga.mode_info;

  if(vmi == NULL) {
#ifdef DEBUG_ATTR
    v_printf("VGAEmu: Attr_init failed\n");
#endif
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

  for(j = 0; j < ATTR_MAX_INDEX + 2; j++) {
    Attr_data[j].read = Attr_data[j].write = attr_ival[i][j];
    Attr_data[j].dirty = True;
  }

  Attr_index = 0;
  Attr_flipflop = ATTR_INDEX_FLIPFLOP;

  v_printf("VGAEmu: Attr_init done\n");
}




/*
 * DANG_BEGIN_FUNCTION Attr_write_value
 *
 * Emulates writes to attribute controller combined index and data
 * register. Read vgadoc3 for details.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 */
void Attr_write_value(unsigned char data)
{
  if(Attr_flipflop==ATTR_INDEX_FLIPFLOP)
    {
      Attr_flipflop=ATTR_DATA_FLIPFLOP;
      
      if(data>ATTR_MAX_INDEX)
        {
          Attr_index=ATTR_MAX_INDEX+1;

#ifdef DEBUG_ATTR
          v_printf("VGAemu: Attr_write_value(0x%02x): ERROR index too big. "
                   "Attr_index set to 0x%02x\n", data, ATTR_MAX_INDEX+1);
#endif
        }
      else
        {
          Attr_index=data;

#ifdef DEBUG_ATTR
          v_printf("VGAemu: Attr_write_value(0x%02x): Attr_index set\n", data);
#endif
        }
    }
  else /* Attr_flipflop==ATTR_DATA_FLIPFLOP */
    {
      Attr_flipflop=ATTR_INDEX_FLIPFLOP;
      
#ifdef DEBUG_ATTR
      v_printf("VGAemu: Attr_write_value(0x%02x): data written in port "
               "0x%02x\n", data, Attr_index);
#endif
      Attr_data[Attr_index].write=data;
      Attr_data[Attr_index].read=data;
      Attr_data[Attr_index].dirty=True;
    }
}





/*
 * DANG_BEGIN_FUNCTION Attr_read_value
 *
 * Emulates reads from the attribute controller.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 */
unsigned char Attr_read_value(void)
{
#ifdef DEBUG_ATTR
  v_printf("VGAemu: Attr_read_value() returns 0x%02x\n", 
           Attr_data[Attr_index].read);
#endif

/*
 * I don't know if the next line is OK. It seems reasonable that the
 * flipflop for port 0x3c0 is set to index if you read the data from
 * 0x3c1, but vgadoc3 says nothing about it and I don't have any other
 * documentation here at the moment :-( -- EM
 */
  Attr_flipflop=ATTR_INDEX_FLIPFLOP;
  
  return(Attr_data[Attr_index].read);
}




/*
 * DANG_BEGIN_FUNCTION Attr_get_index
 *
 * Returns the current index of the attribute controller.
 * This is a hardware emulation function, though in fact this function
 * is undefined in a real attribute controller.
 *
 * DANG_END_FUNCTION
 *
 */
inline unsigned char Attr_get_index(void)
{
#ifdef DEBUG_ATTR
  v_printf("VGAemu: Attr_get_index() returns 0x%02x\n", Attr_index);
#endif

  return(Attr_index);
}




/*
 * DANG_BEGIN_FUNCTION Attr_get_input_status_1
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Attr_get_input_status_1(void)
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

#ifdef DEBUG_ATTR
  v_printf("VGAemu: Attr_get_input_status_1() resets flipflop to index\n");
#endif

  Attr_flipflop=ATTR_INDEX_FLIPFLOP;

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

#ifdef DEBUG_ATTR
  v_printf("VGAemu: Attr_get_input_status_1() returns 0x%02x\n", retval);
#endif

  return(retval);
}




/*
 * Attribute controller interface function should come here.
 */


unsigned char Attr_get_entry(unsigned char index)
{
  unsigned char u;

  u = index < ATTR_MAX_INDEX + 2 ? Attr_data[index].read : 0xff;

#ifdef DEBUG_ATTR
  v_printf("VGAEmu: Attr_get_entry: data[0x%02x] = 0x%02x\n", (unsigned) index, (unsigned) u);
#endif

  return u;
}

void Attr_set_entry(unsigned char index, unsigned char value)
{
#ifdef DEBUG_ATTR
  v_printf("VGAEmu: Attr_set_entry: data[0x%02x] = 0x%02x\n", (unsigned) index, (unsigned) value);
#endif

  if(index >= ATTR_MAX_INDEX + 2) return;

  if(Attr_data[index].read != value || Attr_data[index].write != value) {
    Attr_data[index].read = value;
    Attr_data[index].write = value;
    Attr_data[index].dirty = True;
  }
}

int Attr_is_dirty(unsigned char index)
{
  int u;

  if(index < ATTR_MAX_INDEX + 2) {
    u = Attr_data[index].dirty;
    Attr_data[index].dirty = False;
  }
  else {
    u = False;
  }

#ifdef DEBUG_ATTR
  v_printf("VGAEmu: Attr_is_dirty: data[0x%02x] = 0x%02x\n", (unsigned) index, (unsigned) u);
#endif

  return u;
}


