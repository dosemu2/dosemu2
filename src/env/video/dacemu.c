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
 * Lots of VGA information comes from Finn Thoergersen's VGADOC3, available
 * at every Simtel mirror in vga/vgadoc3.zip, and in the dosemu directory at 
 * tsx-11.mit.edu.
 *
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The DAC emulator for DOSemu.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 */


/* define to debug the DAC */
#undef DEBUG_DAC
#undef DEBUG_DAC_2


#if !defined True
#define False 0
#define True 1
#endif




/* **************** include files **************** */
#include "emu.h"
#include "vgaemu.h"




/* **************** DAC emulator data **************** */

/*
 * The following values are the Trident standard color codes for 256
 * color modes. I think Trident copied them from IBM... ;-)
 * No gamma correction or whatsoever. Gamma correction has to be done
 * in vgaemu clients, not in vgaemu.
 *
 */
const DAC_entry DAC_default_values[256]=
{
  /* 16 standard colors */
  { 0,  0,  0}, { 0,  0, 42}, { 0, 42,  0}, { 0, 42, 42},
  {42,  0,  0}, {42,  0, 42}, {42, 21,  0}, {42, 42, 42},
  {21, 21, 21}, {21, 21, 63}, {21, 63, 21}, {21, 63, 63},
  {63, 21, 21}, {63, 21, 63}, {63, 63, 21}, {63, 63, 63},
  
  /* 16 grey values */
  { 0,  0,  0}, { 5,  5,  5}, { 8,  8,  8}, {11, 11, 11},
  {14, 14, 14}, {17, 17, 17}, {20, 20, 20}, {24, 24, 24},
  {28, 28, 28}, {32, 32, 32}, {36, 36, 36}, {40, 40, 40},
  {45, 45, 45}, {50, 50, 50}, {56, 56, 56}, {63, 63, 63},
  
  /* other colors */
  { 0,  0, 63}, {16,  0, 63}, {31,  0, 63}, {47,  0, 63},
  {63,  0, 63}, {63,  0, 47}, {63,  0, 31}, {63,  0, 16},
  {63,  0,  0}, {63, 16,  0}, {63, 31,  0}, {63, 47,  0},
  {63, 63,  0}, {47, 63,  0}, {31, 63,  0}, {16, 63,  0},
  
  { 0, 63,  0},	{ 0, 63, 16}, { 0, 63, 31}, { 0, 63, 47},
  { 0, 63, 63}, { 0, 47, 63}, { 0, 31, 63}, { 0, 16, 63},
  {31, 31, 63},	{39, 31, 63}, {47, 31, 63}, {55, 31, 63},
  {63, 31, 63}, {63, 31, 55}, {63, 31, 47}, {63, 31, 39},
  
  {63, 31, 31}, {63, 39, 31}, {63, 47, 31}, {63, 55, 31},
  {63, 63, 31}, {55, 63, 31}, {47, 63, 31}, {39, 63, 31},
  {31, 63, 31}, {31, 63, 39}, {31, 63, 47}, {31, 63, 55},
  {31, 63, 63}, {31, 55, 63}, {31, 47, 63}, {31, 39, 63},
  
  {45, 45, 63}, {49, 45, 63}, {54, 45, 63}, {58, 45, 63},
  {63, 45, 63}, {63, 45, 58}, {63, 45, 54}, {63, 45, 49},
  {63, 45, 45}, {63, 49, 45}, {63, 54, 45}, {63, 58, 45},
  {63, 63, 45}, {58, 63, 45}, {54, 63, 45}, {49, 63, 45},
  
  {45, 63, 45}, {45, 63, 49}, {45, 63, 54}, {45, 63, 58},
  {45, 63, 58}, {45, 58, 58}, {45, 54, 58}, {45, 49, 58},
  { 0,  0, 28}, { 7,  0, 28}, {14,  0, 28}, {21,  0, 28},
  {28,  0, 28}, {28,  0, 21}, {28,  0, 14}, {28,  0,  7},
  
  {28,  0,  0}, {28,  7,  0}, {28, 14,  0}, {28, 21,  0},
  {28, 28,  0}, {21, 28,  0}, {14, 28,  0}, { 7, 28,  0},
  { 0, 28,  0}, { 0, 28,  7}, { 0, 28, 14}, { 0, 28, 21},
  { 0, 28, 28}, { 0, 21, 28}, { 0, 14, 28}, { 0,  7, 28},
  
  {14, 14, 28}, {17, 14, 28}, {21, 14, 28}, {24, 14, 28},
  {28, 14, 28}, {28, 14, 24}, {28, 14, 21}, {28, 14, 17},
  {28, 14, 14}, {28, 17, 14}, {28, 21, 14}, {28, 24, 14},
  {28, 28, 14}, {24, 28, 14}, {21, 28, 14}, {17, 28, 14},
  
  {14, 28, 14}, {14, 28, 17}, {14, 28, 21}, {14, 28, 24},
  {14, 28, 28}, {14, 24, 28}, {14, 21, 28}, {14, 17, 28},
  {20, 20, 28}, {22, 20, 28}, {24, 20, 28}, {26, 20, 28},
  {28, 20, 28}, {28, 20, 26}, {28, 20, 24}, {28, 20, 22},
  
  {28, 20, 20}, {28, 22, 20}, {28, 24, 20}, {28, 26, 20},
  {28, 28, 20}, {26, 28, 20}, {24, 28, 20}, {22, 28, 20},
  {20, 28, 20}, {20, 28, 22}, {20, 28, 24}, {20, 28, 26},
  {20, 28, 28}, {20, 26, 28}, {20, 24, 28}, {20, 22, 28},
  
  { 0,  0, 16}, { 4,  0, 16}, { 8,  0, 16}, {12,  0, 16},
  {16,  0, 16}, {16,  0, 12}, {16,  0,  8}, {16,  0,  4},
  {16,  0,  0}, {16,  4,  0}, {16,  8,  0}, {16, 12,  0},
  {16, 16,  0}, {12, 16,  0}, { 8, 16,  0}, { 4, 16,  0},
  
  { 0, 16,  0}, { 0, 16,  4}, { 0, 16,  8}, { 0, 16, 12},
  { 0, 16, 16}, { 0, 12, 16}, { 0,  8, 16}, { 0,  4, 16},
  { 8,  8, 16}, {10,  8, 16}, {12,  8, 16}, {14,  8, 16},
  {16,  8, 16}, {16,  8, 14}, {16,  8, 12}, {16,  8, 10},
  
  {16,  8,  8}, {16, 10,  8}, {16, 12,  8}, {16, 14,  8},
  {16, 16,  8}, {14, 16,  8}, {12, 16,  8}, {10, 16,  8},
  { 8, 16,  8}, { 8, 16, 10}, { 8, 16, 12}, { 8, 16, 14},
  { 8, 16, 16}, { 8, 14, 16}, { 8, 12, 16}, { 8, 10, 16},
  
  {11, 11, 16}, {12, 11, 16}, {13, 11, 16}, {15, 11, 16},
  {16, 11, 16}, {16, 11, 15}, {16, 11, 13}, {16, 11, 12},
  {16, 11, 11}, {16, 12, 11}, {16, 13, 11}, {16, 15, 11},
  {16, 16, 11}, {15, 16, 11}, {13, 16, 11}, {12, 16, 11},
  
  {11, 16, 11}, {11, 16, 12}, {11, 16, 13}, {11, 16, 15},
  {11, 16, 16}, {11, 15, 16}, {11, 13, 16}, {11, 12, 16},
  { 0,  0,  0}, { 0,  0,  0}, { 0,  0,  0}, { 0,  0,  0},
  { 0,  0,  0}, { 0,  0,  0}, { 0,  0,  0}, { 0,  0,  0}
};


#define DAC_READ_MODE 0
#define DAC_WRITE_MODE 3

static DAC_entry DAC[256];
static int DAC_dirty[256];

/*
 * The pelmask is a bit difficult to implement. It has to be stored 
 * here, but the implementation should be in the part that really draws 
 * the screen (a vgaemu client, for example X.c)
 */
static unsigned char DAC_pel_mask=0xff;
static unsigned char DAC_state=DAC_READ_MODE;
static unsigned char DAC_read_index=0;
static unsigned char DAC_write_index=0;
static int DAC_pel_index='r';

#define VGA_DAC_BITS vga.dac.bits


/* **************** DAC emulation functions **************** */

/*
 * DANG_BEGIN_FUNCTION DAC_init
 *
 * Initializes the DAC.
 *
 * DANG_END_FUNCTION
 */
void DAC_init(void)
{
  int i;

#ifdef DEBUG_DAC
  v_printf("VGAEmu: DAC_init()\n");
#endif

  for(i=0; i<256; i++)
    {
#if 0
      DAC[i].r=DAC_default_values[i].r;
      DAC[i].g=DAC_default_values[i].g;
      DAC[i].b=DAC_default_values[i].b;
#else
      DAC[i] = DAC_default_values[i];
#endif
      DAC_dirty[i]=True;
    }

  DAC_pel_mask=0xff;
  DAC_state=DAC_READ_MODE;
  DAC_read_index=0;
  DAC_write_index=0;
  DAC_pel_index='r';
}



/* DANG_BEGIN_FUNCTION DAC_dirty_all
 *
 * Dirty all DAC entries.  Usefull for a mode set.
 *
 * DANG_END_FUNCTION
 */
void DAC_dirty_all(void)
{
	int i;
	for(i = 0; i < 256; i++) {
		DAC_dirty[i]=True;
	}
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
inline void DAC_set_read_index(unsigned char index)
{
#ifdef DEBUG_DAC
  v_printf("VGAEmu: DAC_set_read_index(%i)\n", index);
#endif

  DAC_read_index=index;
  DAC_pel_index='r';
  DAC_state=DAC_READ_MODE;
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
inline void DAC_set_write_index(unsigned char index)
{
#ifdef DEBUG_DAC
  v_printf("VGAEmu: DAC_set_write_index(%i)\n", index);
#endif

  DAC_write_index=index;
  DAC_pel_index='r';
  DAC_state=DAC_WRITE_MODE;
}




/*
 * DANG_BEGIN_FUNCTION DAC_read_value
 *
 * Read a value from the DAC. Each read will cycle through the registers for
 * red, green and blue. After a ``blue read'' the read index will be 
 * incremented. Read vgadoc3 if you want to know more about the DAC.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char DAC_read_value(void)
{
  unsigned char rv;

  DAC_state=DAC_READ_MODE;
  
  switch(DAC_pel_index)
    {
    case 'r':
      rv=DAC[DAC_read_index].r;
      DAC_pel_index='g';
      break;

    case 'g':
      rv=DAC[DAC_read_index].g;
      DAC_pel_index='b';
      break;

    case 'b':
      rv=DAC[DAC_read_index].b;
      DAC_pel_index='r';
      DAC_read_index++;
      break;

    default:
      error("VGAemu: DAC_read_value(): DAC_pel_index out of range\n");
      rv=0;
      DAC_pel_index='r';
      break;
    }

#ifdef DEBUG_DAC
  v_printf("VGAemu: DAC_read_value() returns %i\n", rv);
#endif
  return(rv);
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
  value &= (1 << VGA_DAC_BITS) - 1;

  DAC_state = DAC_WRITE_MODE;

#ifdef DEBUG_DAC_2
  v_printf(
    "VGAEmu: DAC_write_value: DAC[0x%02x].%c = 0x%02x\n",
    (unsigned) DAC_write_index, (char) DAC_pel_index, (unsigned) value
  );
#endif

  DAC_dirty[DAC_write_index]=True;

  switch(DAC_pel_index) {
    case 'r':
      DAC[DAC_write_index].r = value;
      DAC_pel_index='g';
      break;

    case 'g':
      DAC[DAC_write_index].g = value;
      DAC_pel_index='b';
      break;

    case 'b':
      DAC[DAC_write_index].b = value;
      DAC_pel_index='r';
      DAC_write_index++;
      break;

    default:
      error("VGAEmu: DAC_write_value: DAC_pel_index out of range\n");
      DAC_pel_index='r';
      break;
  }
}




/*
 * DANG_BEGIN_FUNCTION DAC_set_pel_mask
 *
 * Sets the pel mask and marks all DAC entries as dirty.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
inline void DAC_set_pel_mask(unsigned char mask)
{
#ifdef DEBUG_DAC
  v_printf("VGAemu: DAC_set_pel_mask(%i)\n", mask);
#endif

  DAC_pel_mask=mask;
}




/* **************** DAC Interface functions **************** */

/*
 * DANG_BEGIN_FUNCTION DAC_get_entry
 *
 * Returns a complete DAC entry (r,g,b). Color values are AND-ed with the
 * pel mask.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_get_entry(DAC_entry *entry, unsigned char index)
{
  entry->r=DAC[index].r & DAC_pel_mask;
  entry->g=DAC[index].g & DAC_pel_mask;
  entry->b=DAC[index].b & DAC_pel_mask;
#if 0 /* This function is too general, to always clear the dirty flag */
  DAC_dirty[index] = False;
#endif

#ifdef DEBUG_DAC_2
  v_printf(
    "VGAEmu: DAC_get_entry: DAC[0x%02x] = 0x%02x 0x%02x 0x%02x (rgb)\n",
    (unsigned) index, (unsigned) entry->r, (unsigned) entry->g, (unsigned) entry->b
  );
#endif
}


/*
 * DANG_BEGIN_FUNCTION DAC_read_entry
 *
 * Returns a complete DAC entry (r,g,b), doesn't un-dirty it.
 * Color values are _not_ maked.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_read_entry(DAC_entry *entry, unsigned char index)
{
  entry->r=DAC[index].r;
  entry->g=DAC[index].g;
  entry->b=DAC[index].b;

#ifdef DEBUG_DAC
  v_printf("VGAemu: DAC_read_entry(0x%02x): (0x%02x, 0x%02x, "
           "0x%02x)\n", index, entry->r, entry->g, entry->b);
#endif
}




/*
 * DANG_BEGIN_FUNCTION DAC_get_dirty_entry
 *
 * Searches the DAC_dirty list for the first dirty entry. Returns the 
 * changed entrynumber and fills in the entry if a dirty entry is found or
 * returns -1 otherwise.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 */
int DAC_get_dirty_entry(DAC_entry *entry)
{
  int i;

  for(i=0; i<256; i++)
    {
      if(DAC_dirty[i]==True)
	{
	  DAC_get_entry(entry, (unsigned char)i);
	  DAC_dirty[i] = False;
#ifdef DEBUG_DAC
	  v_printf("VGAemu: DAC_get_dirty_entry() returns dirty entry "
	           "0x%02x\n", i);
#endif

	  return(i);
	}
    }

#ifdef DEBUG_DAC
  v_printf("VGAemu: DAC_get_dirty_entry() returns -1\n");
#endif

  return(-1);
}




/*
 * DANG_BEGIN_FUNCTION DAC_set_entry
 *
 * Sets a complete DAC entry (r,g,b).
 * This is an interface function for the int 10 handler.
 *
 * DANG_END_FUNCTION
 *
 */
void DAC_set_entry(unsigned char r, unsigned char g, unsigned char b, unsigned char index)
{
  unsigned mask = (1 << VGA_DAC_BITS ) - 1;

  r &= mask; g &= mask; b &= mask;

#ifdef DEBUG_DAC_2
  v_printf(
    "VGAEmu: DAC_set_entry: DAC[0x%02x] = 0x%02x 0x%02x 0x%02x (rgb)\n",
    (unsigned) index, (unsigned) r, (unsigned) g, (unsigned) b);
#endif

  if(DAC[index].r != r || DAC[index].g != g || DAC[index].b != b) DAC_dirty[index] = True;

  DAC[index].r=r;
  DAC[index].g=g;
  DAC[index].b=b;
}


/*
 * DANG_BEGIN_FUNCTION DAC_get_pel_mask
 *
 * Returns the current pel mask. Drawing functions should get the pel
 * mask and AND it with the pixel values to get the correct pixel value.
 * This is *very* slow to implement and fortunately this register is used
 * very rare. Maybe the implementation should be in vgaemu, maybe in the
 * vgaemu client...
 * This is an interface function. 
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char DAC_get_pel_mask(void)
{
#ifdef DEBUG_DAC
  v_printf("VGAemu: DAC_get_pel_mask() returns 0x%02x\n", DAC_pel_mask);
#endif

  return(DAC_pel_mask);
}




/*
 * DANG_BEGIN_FUNCTION DAC_get_state
 *
 * Returns the current state of the DAC
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char DAC_get_state(void)
{
#ifdef DEBUG_DAC
  v_printf("VGAemu: DAC_get_state() returns 0x%02x\n", DAC_state);
#endif

  return(DAC_state);
}

void DAC_set_width(unsigned bits)
{
  int i;

  if(bits > 8) bits = 8;
  if(bits < 4) bits = 4;	/* it's no use to allow other values than 6 or 8, but anyway... */
  if(VGA_DAC_BITS != bits) {
    vga.reconfig.dac = 1;
    VGA_DAC_BITS = bits;
    for(i = 0; i < 256; i++) DAC_dirty[i] = True;
  }
}
