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
 * 
 * 1996/05/06:
 *  - Changed Attr_get_input_status_1() to get it _slower_.
 *    Idea from Adam D. Moss (aspirin@tigerden.com).
 * 1996/05/09:
 *  - Added horizontal retrace too (--adm)
 *
 * DANG_BEGIN_MODULE
 *
 * The Attribute Controller emulator for VGAemu.
 *
 * DANG_END_MODULE
 *
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
#include "vgaemu.h"
#include "vgaemu_inside.h"
#ifdef HAVE_GETTIMEOFDAY
#  ifdef TIME_WITH_SYS_TIME
#    include <sys/time.h>
#    include <time.h>
#  else
#    if HAVE_SYS_TIME
#      include <sys/time.h>
#    else
#      include <time.h>
#    endif
#  endif
#endif


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

static unsigned char Attr_index=0;
static int Attr_flipflop=ATTR_INDEX_FLIPFLOP;
static indexed_register Attr_data[0x16]=
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
  {0x00, 0x00, reg_read_write, False}         /* Dummy register for wrong
                                               * indices
                                               */
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
#ifdef DEBUG_ATTR
  v_printf("VGAemu: Attr_init()\n");
#endif

  Attr_index=0;
  Attr_flipflop=ATTR_INDEX_FLIPFLOP;  
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
  struct timeval currenttime;
  struct timezone tz;
#else
  static unsigned int cga_r=0;
#endif
  unsigned char retval;

#ifdef DEBUG_ATTR
  v_printf("VGAemu: Attr_get_input_status_1() resets flipflop to index\n");
#endif

  Attr_flipflop=ATTR_INDEX_FLIPFLOP;

#ifdef HAVE_GETTIMEOFDAY
  gettimeofday(&currenttime, &tz);

  /* Timings are 'ballpark' guesses and may vary from mode to mode, but
     such accuracy is probably not important... I hope. (--adm) */

  /* We're in vertical retrace?  If so, set VR and DE flags */
  if ((currenttime.tv_usec % 20000) < 7000) retval=0xCF;

  /* We're in horizontal retrace?  If so, just set DE flag, 0 in VR */
  else if ((currenttime.tv_usec % 50) < 25) retval=0xC7;

  /* Otherwise, we're in 'display' mode and should return 0 in DE and VR */
  else retval=0xC6;

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






