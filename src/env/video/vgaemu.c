/*
 * vgaemu.c
 *
 * VGA emulator for dosemu
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
 * This code emulates a VGA (Video Graphics Array, a video adapter for IBM
 * compatible PC's) for DOSEMU, the Linux DOS emulator. Emulated are the 
 * video memory and the register set (CRTC, DAC, etc.).
 *
 * Lots of VGA information comes from Finn Thoergersen's VGADOC3, available
 * at every Simtel mirror in vga/vgadoc3.zip, and in the dosemu directory at 
 * tsx-11.mit.edu.
 *
 *
 * DANG_BEGIN_MODULE
 *
 * The VGA emulator for dosemu. Emulated are the video meory and the VGA
 * register set (CRTC, DAC, etc.).
 *
 * DANG_END_MODULE
 *
 */

/*#undef GRAPH
#define GRAPH 2
*/

/*
 * Defines to enable debug information for:
 * DEBUG_IO        -- inb/outb emulation
 * DEBUG_DAC       -- DAC (Digital to Analog Converter)
 * DEBUG_ATTR      -- attribute controller
 */
#define DEBUG_IO
#define DEBUG_ATTR

/*
#define DEBUG_DISASM

#define DEBUG_IMAGE
#define DEBUG_UPDATE
*/



/* **************** include files **************** */
#include <sys/mman.h>           /* root@sjoerd*/
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>

#include "cpu.h"	/* root@sjoerd: for context structure */
#include "emu.h"
#include "video.h"
#include "vgaemu.h"
#include "vgaemu_inside.h"
#ifdef VESA /* root@zaphod */
#include "vesa.h"
#endif


/* **************** Definition of the port addresses **************** */

#define CRTC_BASE           0x3d4
#define CRTC_INDEX          CRTC_BASE
#define CRTC_DATA           CRTC_BASE+0x01

#define INPUT_STATUS_1      0x3da
#define FEATURE_CONTROL_1   INPUT_STATUS_1


#define VGA_BASE            0x3c0

#define ATTRIBUTE_BASE      VGA_BASE
#define ATTRIBUTE_INDEX     ATTRIBUTE_BASE
#define ATTRIBUTE_DATA      ATTRIBUTE_BASE+0x01

#define INPUT_STATUS_0      VGA_BASE+0x02
#define MISC_OUTPUT_0       VGA_BASE+0x02
#define SUBSYSTEM_ENABLE    VGA_BASE+0x03

#define SEQUENCER_BASE      VGA_BASE+0x04
#define SEQUENCER_INDEX     SEQUENCER_BASE
#define SEQUENCER_DATA      SEQUENCER_BASE+0x01

#define GFX_CTRL_BASE       VGA_BASE+0x0e

#define DAC_BASE            VGA_BASE+0x06
#define DAC_PEL_MASK        DAC_BASE
#define DAC_STATE           DAC_BASE+0x01
#define DAC_READ_INDEX      DAC_BASE+0x01
#define DAC_WRITE_INDEX     DAC_BASE+0x02
#define DAC_DATA            DAC_BASE+0x03

#define FEATURE_CONTROL_0   VGA_BASE+0x0a
#define GRAPHICS_2_POSITION FEATURE_CONTROL
#define MISC_OUTPUT_1       VGA_BASE+0x0c
#define GRAPHICS_1_POSITION MISC_OUTPUT_1

#define GRAPHICS_BASE       VGA_BASE+0x0e
#define GRAPHICS_INDEX      GRAPHICS_BASE
#define GRAPHICS_DATA       GRAPHICS_BASE+0x01


#if !defined True
#define False 0
#define True 1
#endif




/* **************** Structures **************** */

/*
 * Type of indexed registers
 */
enum register_type
{
  reg_read_write,         /* value read == value written */
  reg_read_only,          /* write to this type of register is undefined */
  reg_write_only,         /* read from this type of register returns 0 */
  reg_double_function     /* value read != value written */
};


/*
 * Indexed register data structure
 */
typedef struct
{
  unsigned char read;     /* value read */
  unsigned char write;    /* value written */
  int type;               /* register type, choose one of enum register_type */
  int dirty;              /* register changed? */
} indexed_register;




/* **************** General mode data **************** */

/* Table with video mode definitions */
const vga_mode_info vga_mode_table[]=
{
  /* The standard CGA/EGA/MCGA/VGA modes */
  {0x00,   TEXT,   360,  400,   9, 16,   40, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x01,   TEXT,   360,  400,   9, 16,   40, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x02,   TEXT,   720,  400,   9, 16,   80, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   720,  400,   9, 16,   80, 25,   16,  0xb8000,  0x8000,  TEXT},

  /* The additional mode 3: 80x21, 80x28, 80x43, 80x50 and 80x60 */
  {0x03,   TEXT,   720,  336,   9, 16,   80, 21,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   720,  448,   9, 16,   80, 28,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   640,  448,   8, 14,   80, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   640,  400,   8,  8,   80, 50,   16,  0xb8000,  0x8000,  TEXT},
  {0x03,   TEXT,   640,  480,   8,  8,   80, 60,   16,  0xb8000,  0x8000,  TEXT},

  {0x04,  GRAPH,   320,  200,   8,  8,   40, 25,    4,  0xb8000,  0x8000,   CGA},
  {0x05,  GRAPH,   320,  200,   8,  8,   40, 25,    4,  0xb8000,  0x8000,   CGA},
  {0x06,  GRAPH,   640,  200,   8,  8,   80, 25,    2,  0xb8000,  0x8000,  HERC},
  {0x07,   TEXT,   720,  400,   9, 16,   80, 25,    2,  0xb0000,  0x8000,  TEXT},
  
  /* Forget the PCjr modes (forget the PCjr :-) */
  
  /* Standard EGA/MCGA/VGA modes */
  {0x0d,  GRAPH,   320,  200,   8,  8,   40, 25,   16,  0xa0000, 0x10000,  PL4},
  {0x0e,  GRAPH,   640,  200,   8,  8,   80, 25,   16,  0xa0000, 0x10000,  PL4},
  {0x0f,  GRAPH,   640,  350,   8, 14,   80, 25,    2,  0xa0000, 0x10000,  HERC},
  {0x10,  GRAPH,   640,  350,   8, 14,   80, 25,   16,  0xa0000, 0x10000,   PL4},
  {0x11,  GRAPH,   640,  480,   8, 16,   80, 30,    2,  0xa0000, 0x10000,  HERC},
  {0x12,  GRAPH,   640,  480,   8, 16,   80, 30,   16,  0xa0000, 0x10000,   PL4},
  {0x13,  GRAPH,   320,  200,   8,  8,   40, 25,  256,  0xa0000, 0x10000,    P8},
  
  /* SVGA modes. Maybe we are going to emulate a Trident 8900, so
   * we already use the Trident mode numbers in advance.
   */
   
  {0x50,   TEXT,   640,  480,   8, 16,   80, 30,   16,  0xb8000,  0x8000,  TEXT},
  {0x51,   TEXT,   640,  473,   8, 11,   80, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x52,   TEXT,   640,  480,   8,  8,   80, 60,   16,  0xb8000,  0x8000,  TEXT},
  
  {0x53,   TEXT,  1056,  350,   8, 14,  132, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x54,   TEXT,  1056,  480,   8, 16,  132, 30,   16,  0xb8000,  0x8000,  TEXT},
  {0x55,   TEXT,  1056,  473,   8, 11,  132, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x56,   TEXT,  1056,  480,   8,  8,  132, 60,   16,  0xb8000,  0x8000,  TEXT},
  
  {0x57,   TEXT,  1188,  350,   9, 14,  132, 25,   16,  0xb8000,  0x8000,  TEXT},
  {0x58,   TEXT,  1188,  480,   9, 16,  132, 30,   16,  0xb8000,  0x8000,  TEXT},
  {0x59,   TEXT,  1188,  473,   9, 11,  132, 43,   16,  0xb8000,  0x8000,  TEXT},
  {0x5a,   TEXT,  1188,  480,   9,  8,  132, 60,   16,  0xb8000,  0x8000,  TEXT},
  
  {0x5b,  GRAPH,   800,  600,   8,  8,  100, 75,   16,  0xa0000, 0x10000,   PL4},
  {0x5c,  GRAPH,   640,  400,   8, 16,   80, 25,  256,  0xa0000, 0x10000,    P8},
  {0x5d,  GRAPH,   640,  480,   8, 16,   80, 30,  256,  0xa0000, 0x10000,    P8},
  {0x5e,  GRAPH,   800,  600,   8,  8,  100, 75,  256,  0xa0000, 0x10000,    P8},
  {0x5f,  GRAPH,  1024,  768,   8, 16,  128, 48,   16,  0xa0000, 0x10000,   PL4},
  {0x60,  GRAPH,  1024,  768,   8, 16,  128, 48,    4,  0xa0000, 0x10000,   CGA}, /* ??? */
  {0x61,  GRAPH,   768, 1024,   8, 16,   96, 64,   16,  0xa0000, 0x10000,   PL4},
  {0x62,  GRAPH,  1024,  768,   8, 16,  128, 48,  256,  0xa0000, 0x10000,    P8},
  {0x63,  GRAPH,  1280, 1024,   8, 16,  160, 64,   16,  0xa0000, 0x10000,   PL4},
  {  -1,     -1,    -1,   -1,  -1, -1,   -1, -1,   -1,       -1,      -1,    -1}
};


/* pointer to the current mode */
static vga_mode_info *current_mode_info=NULL;





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




/* **************** Video_page dirty registers ************* */
static int vgaemu_graphic_dirty_page[16]=
{
  True, True, True,True,
  True, True, True,True,
  True, True, True,True,
  True, True, True,True
};

static int vgaemu_text_dirty_page[8]=
{
  True, True, True,True,
  True, True, True,True
};



/* *************** The emulated videomemory and some scratch memory *** */

unsigned char* vga_emu_memory=NULL;
unsigned char* vga_emu_memory_scratch=NULL;




/* ****** Own memory to map allocated vga_emu_memory *********/

int selfmem_fd;




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
  v_printf("vgaemu: DAC_init()\n");
#endif

  for(i=0; i<256; i++)
    {
      DAC[i].r=DAC_default_values[i].r;
      DAC[i].g=DAC_default_values[i].g;
      DAC[i].b=DAC_default_values[i].b;
      DAC_dirty[i]=True;
    }

  DAC_pel_mask=0xff;
  DAC_state=DAC_READ_MODE;
  DAC_read_index=0;
  DAC_write_index=0;
  DAC_pel_index='r';
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
/* root@zaphod */
inline void DAC_set_read_index(unsigned char index)
{
#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_set_read_index(%i)\n", index);
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
/* root@zaphod */
inline void DAC_set_write_index(unsigned char index)
{
#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_set_write_index(%i)\n", index);
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
      error("vgaemu: DAC_read_value(): DAC_pel_index out of range\n");
      rv=0;
      DAC_pel_index='r';
      break;
    }

#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_read_value() returns %i\n", rv);
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
  DAC_state=DAC_WRITE_MODE;

#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_write_value(%i)\n", value);
#endif

  DAC_dirty[DAC_write_index]=True;

  switch(DAC_pel_index)
    {
    case 'r':
      DAC[DAC_write_index].r=value&0x3f;
      DAC_pel_index='g';
      break;

    case 'g':
      DAC[DAC_write_index].g=value&0x3f;
      DAC_pel_index='b';
      break;

    case 'b':
      DAC[DAC_write_index].b=value&0x3f;
      DAC_pel_index='r';
      DAC_write_index++;
      break;

    default:
      error("vgaemu: DAC_write_value(): DAC_pel_index out of range\n");
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
/* root@zaphod */
inline void DAC_set_pel_mask(unsigned char mask)
{
#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_set_pel_mask(%i)\n", mask);
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
  entry->r=DAC[index].r&DAC_pel_mask;
  entry->g=DAC[index].g&DAC_pel_mask;
  entry->b=DAC[index].b&DAC_pel_mask;
  DAC_dirty[index]=False;

#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_get_entry(0x%02x): (0x%02x, 0x%02x, "
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
#ifdef DEBUG_DAC
	  v_printf("vgaemu: DAC_get_dirty_entry() returns dirty entry "
	           "0x%02x\n", i);
#endif

	  return(i);
	}
    }

#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_get_dirty_entry() returns -1\n");
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
void DAC_set_entry(unsigned char r, unsigned char g, unsigned char b, 
                   unsigned char index)
{
#ifdef DEBUG_DAC
  v_printf("vgaemu: DAC_set_entry(r=0x%02x, g=0x%02x, b=0x%02x, "
           "index=0x%02x)\n", r, g, b, index);
#endif

  DAC[index].r=r&0x3f;
  DAC[index].g=g&0x3f;
  DAC[index].b=b&0x3f;
  DAC_dirty[index]=True;
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
  v_printf("vgaemu: DAC_get_pel_mask() returns 0x%02x\n", DAC_pel_mask);
#endif

  return(DAC_pel_mask);
}




/* root@zaphod */
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
  v_printf("vgaemu: Attr_init()\n");
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
          v_printf("vgaemu: Attr_write_value(0x%02x): ERROR index too big. "
                   "Attr_index set to 0x%02x\n", data, ATTR_MAX_INDEX+1);
#endif
        }
      else
        {
          Attr_index=data;

#ifdef DEBUG_ATTR
          v_printf("vgaemu: Attr_write_value(0x%02x): Attr_index set\n", data);
#endif
        }
    }
  else /* Attr_flipflop==ATTR_DATA_FLIPFLOP */
    {
      Attr_flipflop=ATTR_INDEX_FLIPFLOP;
      
#ifdef DEBUG_ATTR
      v_printf("vgaemu: Attr_write_value(0x%02x): data written in port "
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
  v_printf("vgaemu: Attr_read_value() returns 0x%02x\n", 
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
 * Attribute controller interface function should come here.
 */




/* **************** VGA emulation routines **************** */

/*
 * DANG_BEGIN_FUNCTION VGA_emulate_outb
 *
 * Emulates writes to VGA ports.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void VGA_emulate_outb(int port, unsigned char value)
{
#ifdef DEBUG_IO
  v_printf("vgaemu: VGA_emulate_outb(): outb(0x%03x, 0x%02x)\n", port, value);
#endif

  switch(port)
    {
    case ATTRIBUTE_INDEX:       /* root@zaphod */
      Attr_write_value(value);
      break;
    
    case ATTRIBUTE_DATA:        /* root@zaphod */
#ifdef DEBUG_IO
      v_printf("vgaemu: ERROR: illegal write to port 0x%03x\n", port);
#endif
      break;
    
    case DAC_PEL_MASK:
      DAC_set_pel_mask(value);
      break;
    
    case DAC_READ_INDEX:
      DAC_set_read_index(value);
      break;

    case DAC_WRITE_INDEX:
      DAC_set_write_index(value);
      break;

    case DAC_DATA:
      DAC_write_value(value);
      break;

    default:
#ifdef DEBUG_IO
      v_printf("vgaemu: not (yet) smart enough to emulate write of 0x%02x to"
	       " port 0x%04x\n", value, port);
#endif
      break;
    }
}




/*
 * DANG_BEGIN_FUNCTION VGA_emulate_inb
 *
 * Emulates reads from VGA ports.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char VGA_emulate_inb(int port)
{
#ifdef DEBUG_IO
  v_printf("vgaemu: VGA_emulate_inb(): inb(0x%03x)\n", port);
#endif

  switch(port)
    {
    case ATTRIBUTE_INDEX:        /* root@zaphod */
      return(Attr_index);        /* undefined, in fact */
      break;
    
    case ATTRIBUTE_DATA:         /* root@zaphod */
      return(Attr_read_value());
      break;
    
    case DAC_PEL_MASK:
      return(DAC_pel_mask);
      break;

    case DAC_STATE:
      return(DAC_state);
      break;

    case DAC_WRITE_INDEX: /* this is undefined, but we have to do something */
      return(0);
      break;

    case DAC_DATA:
      return(DAC_read_value());
      break;

    case INPUT_STATUS_1: /* root@zaphod */
      {
	/* graphic status - many programs will use this port to sync with
	 * the vert & horz retrace so as not to cause CGA snow */
	static unsigned int cga_r=0;
	
        Attr_flipflop=ATTR_INDEX_FLIPFLOP; /* root@zaphod */
        
	return((cga_r ^= 1) ? 0xcf : 0xc6);
	break;
      }

    default:
#ifdef DEBUG_IO
      v_printf("vgaemu: not (yet) smart enough to emulate read from"
	       " port 0x%04x\n", port);
#endif
      return(0); /* do something */
      break;
    }
}




/*
 * DANG_BEGIN_FUNCTION vga_emu_fault(struct sigcontext_struct *scp)        
 *
 * description:
 *  vga_emu_fault() is used to catch video acces, and handle it.
 *  This function is called from dosemu/sigsegv.c:dosemu_fault()
 *  The sigcontext_struct is defined in include/cpu.h
 *  Now it catches only changes in a 4K page, but maybe it is useful to
 *  catch each video acces. The problem when you do that is, you have to 
 *  simulate each intruction which could write to the video-memory.
 *  it is easy to get the place where the exeption happens (scp->cr2),
 *  but what are those changes?
 *  An other problem is, it could eat a lot time, but it does now also.
 *  
 * DANG_END_FUNCTION                        
 */     

int vga_emu_fault(struct sigcontext_struct *scp)
{
  int page_fault_number=0;

/*printf("exeption adr: 0x%0lx\n",scp->cr2);  
*/
  page_fault_number=(scp->cr2-0xA0000)/(4*1024);

  if( (page_fault_number >=0) && (page_fault_number <16) )
    {
      vgaemu_graphic_dirty_page[page_fault_number]=True;
      mprotect((void *)(0xA0000+page_fault_number*0x1000),0x1000,
	       PROT_READ|PROT_WRITE);

      return True;
    } 
  else /* Exeption was not caused in the vga_emu_ram, maybe in the vga_emu_rom*/
    {
      page_fault_number=(scp->cr2-0xB8000)/(4*1024);
      if( (page_fault_number >=0) && (page_fault_number <8) )
	{
	  vgaemu_text_dirty_page[page_fault_number]=True;
	  mprotect((void *)(0xB8000+page_fault_number*0x1000),0x1000,
		   PROT_READ|PROT_WRITE);
	  
	  return True;
	} 
      else /* Exeption was not caused in the vga_emu_ram, maybe in the vga_emu_rom*/
	if(vesa_emu_fault(scp)==True)
	  return True;
	else 
	  v_printf("vga_emu_fault: Not in 0xB8000-0xC0000range\n page= 0x%02x"
		 " adress: 0x%lx \n",page_fault_number,scp->cr2); 
    }
  return False;
}
 
 
 
/*
 * DANG_BEGIN_FUNCTION vga_emu_init(void)        
 *
 * description:
 *  vga_emu_init() is used to emulate video.
 *  This function is only called from video/X.c at the moment.
 *  This function has to set a global variable to detect it in other functions
 *  it has to map the right video-bank to the 0xA0000 adress.
 *
 * DANG_END_FUNCTION                        
 *
 * I don't think it is wise to return a pointer to the video memory...
 * I'd like to program object oriented, data hiding, etc. -- Erik
 *
 */     
unsigned char* vga_emu_init(void)
{
  vga_emu_memory=(unsigned char*)malloc(VGAEMU_BANK_SIZE*VGAEMU_BANKS);
  if(vga_emu_memory==NULL)
    v_printf("vga_emu_init:Alocated memory is NULL\n");

  vga_emu_memory_scratch=(unsigned char*)malloc(VGAEMU_BANK_SIZE);
  if(vga_emu_memory_scratch==NULL)
    v_printf("vga_emu_init:Alocated memory is NULL\n");

  /* mapping the one bank of the allocated memmory */
  selfmem_fd = open("/proc/self/mem", O_RDWR);
  if (selfmem_fd < 0)
    v_printf("vga_emu_init: cannot open /proc/self/mem:\n");

  *vga_emu_memory=0;	/* touch it */
  *vga_emu_memory_scratch=0; /* */
 

  if(mmap((caddr_t)0xA0000, VGAEMU_BANK_SIZE, PROT_READ|PROT_WRITE,
          MAP_SHARED | MAP_FIXED,selfmem_fd,(off_t)vga_emu_memory )<0)
    v_printf("Mapping failed\n");

  mprotect((void *)0xA0000,0xFFFF,PROT_READ);

/* add here something like for text-mode */


  DAC_init();
  Attr_init();
#ifdef VESA
  vesa_init();
#endif

  return vga_emu_memory;
}
 
/*
 * DANG_BEGIN_FUNCTION int vgaemu_get_changes_in_pages(method,*first_dirty_page,*last_dirty_page)        
 *
 * description:
 *  vgaemu_get_changes_in_pages() is vgaemu_get_changes() is used 
 *  to get the changed 4K pages .
 *  This function is only called from video/vgaemu.c .
 *  It has to called several times to make sure grabbing all the changed
 *  pages.
 *
 * should be updated for other video modes than 0x13
 *
 * DANG_END_FUNCTION                        
 */     

int vgaemu_get_changes_in_pages( int method,int *first_dirty_page,int *last_dirty_page)
{
  int low,high,i;

  switch(current_mode_info->type)
    { 
    case GRAPH:

      low=16;
      high=16;

      /* Find the first dirty one */
      for(i=0;i<16;i++) /* now split in more areas */
	{
	  if(vgaemu_graphic_dirty_page[i]==True)
	    {
	      mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
	      vgaemu_graphic_dirty_page[i]=False;
	      low=i;
	      break;
	    }   
	}
      
      /* No dirty pages */
      if(low==16) 
	{ 
	  /*mprotect((void *)0xA0000,0xFFFF,PROT_READ);*/	/* protect whole area */
	  return False;
	}
      
      switch(method)
	{
	case 0:			/* find all dirty pages, which are connected */
	  for(++i;i<16;i++)
	    {
	      if(vgaemu_graphic_dirty_page[i]==True)
		{
		  mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
		  vgaemu_graphic_dirty_page[i]=False;
		}   
	      else
		{
		  high=i--;
		  break;
		}
	    }
	  break;
	  
	case 1:		/* Get only the first dirty page */
	  high=low+1;;
	  break;
	  
	case 2:		/* Get first and last page */
	  for(i=15;i>0;i--)
	    {
	      if(vgaemu_graphic_dirty_page[i]==True)
		{
		  mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
		  vgaemu_graphic_dirty_page[i]=False;
		  break;
		}
	    }
	  for(;i>low;i--)	/* clear dirty flags and protect it */
	    {
	      mprotect((void *)(0xA0000+0x1000*i),0x1000,PROT_READ);
	      vgaemu_graphic_dirty_page[i]=False;
	    }    
	  break;
	  
	default:
	  v_printf("vgaemu_get_changes_in_pages: No such method\n");
	  break;
	}
 
      *first_dirty_page=low;
      *last_dirty_page=high;  
      
      return True;	/* False= nothing has changed */
      break;
      
    case TEXT:
      low=8;
      high=8;
      
      /* Find the first dirty one */
      for(i=0;i<8;i++) /* now split in more areas */
	{
	  if(vgaemu_text_dirty_page[i]==True)
	    {
	      mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
	      vgaemu_text_dirty_page[i]=False;
	      low=i;
	      break;
	    }   
	}
      
      /* No dirty pages */
      if(low==8) 
	{ 
	  /*mprotect((void *)0xA0000,0xFFFF,PROT_READ);*/	/* protect whole area */
	  return False;
	}
      
      switch(method)
	{
	case 0:			/* find all dirty pages, which are connected */
	  for(++i;i<8;i++)
	    {
	      if(vgaemu_text_dirty_page[i]==True)
		{
		  mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
		  vgaemu_text_dirty_page[i]=False;
		}   
	      else
		{
		  high=i--;
		  break;
		}
	    }
	  break;
	  
	case 1:		/* Get only the first dirty page */
	  high=low+1;;
	  break;
	  
	case 2:		/* Get first and last page */
	  for(i=7;i>0;i--)
	    {
	      if(vgaemu_text_dirty_page[i]==True)
		{
		  mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
		  vgaemu_text_dirty_page[i]=False;
		  break;
		}
	    }
	  for(;i>low;i--)	/* clear dirty flags and protect it */
	    {
	      mprotect((void *)(0xB8000+0x1000*i),0x1000,PROT_READ);
	      vgaemu_text_dirty_page[i]=False;
	    }    
	  break;
	  
	default:
	  v_printf("vgaemu_get_changes_in_pages: No such method\n");
	  break;
	}
      
      *first_dirty_page=low;
      *last_dirty_page=high;  
      
      return True;	/* False= nothing has changed */
      
      
      
      break;
    default:break;
    }
  
  
  return True;	/* False= nothing has changed */
}




/*
 * DANG_BEGIN_FUNCTION vgaemu_get_changes_and_update_XImage_0x13
 *
 * description:
 *  vgaemu_get_changes() is used to get the changed area and update the image.
 *  This function is only called from video/X.c at the moment.
 *  It has to called several times to make sure grabbing all the changed
 *  areas.
 *
 * This is only for mode 0x13: 256 colors
 *
 * DANG_END_FUNCTION                        
 */     

int vgaemu_get_changes_and_update_XImage_0x13(unsigned char * data,int method, int *xx, int *yy, int *ww, int *hh)
{
  int first_dirty_page,last_dirty_page;

#ifdef DEBUG_IMAGE
  v_printf("vgaemu_get_changes_and_update_XImage: Ximage is:\n"
	 "width = %d, height = %d\n"
	 "depth = %d, b/l = %d, b/p = %d\n"
	 "bitmap_pad = %d\n"
	 "*data= %p\n",
	 image->width,image->height,image->depth,
	 image->bytes_per_line,image->bits_per_pixel,image->bitmap_pad,image->data);       
  
#endif
  
  if(vgaemu_get_changes_in_pages(VGAEMU_UPDATE_METHOD_G_C_IN_PAGES,
				 &first_dirty_page,&last_dirty_page)==True)
    {
      int x, y, left=current_mode_info->x_res, top=current_mode_info->y_res, right=0, bottom=0;
      unsigned char* vid_p;
      unsigned char* scr_p;
      unsigned char color;
      
      *xx=0;
      *ww=current_mode_info->x_res;
      *yy=(first_dirty_page*0x1000)/current_mode_info->x_res;
      *hh=((last_dirty_page-first_dirty_page)*0x1000+current_mode_info->x_res-1)/current_mode_info->x_res;
      
      if((*yy+*hh)>current_mode_info->y_res )
	*hh=current_mode_info->y_res-*yy; /* 0- */
      
      for(y=*yy; y<(*yy+*hh); y++)
	{	 
	  vid_p=(unsigned char*)(current_mode_info->bufferstart+(*xx+current_mode_info->x_res*(y)) );
	  scr_p=(unsigned char*)(vga_emu_memory_scratch+(*xx+current_mode_info->x_res*y) );
	  
	  for(x=(*xx); x<(*xx+*ww); x++)
	    {
	      
	      color=*vid_p;
	      
	      if(color!=*scr_p)
		{
		  
		  *scr_p=color;	/* update scratch space*/
#ifdef DEBUG_UPDATE
		  v_printf("PutPixel: x= %d, y= %d\n",x,y);
#endif
		  
/*		  XPutPixel(image,x,y,(long int)color);*/
		  data[y*current_mode_info->x_res + x]=(unsigned char)color;
		  
		  if(x<left)
		    left=x;
		  
		  if((x)>right)
		    right=x;
		  
		  if(y<top)
		    top=y;
		  
		  if(y>bottom)
		    bottom=y;
		}
	      vid_p++;
	      scr_p++;
	    }
	}  
      
      *xx=left;
      *yy=top;
      *ww=right-left+1;
      *hh=bottom-top+1;
      
      if((*ww<0)||(*hh<0))
	{
	  /*printf("Error: Could not find any changed pixel(could be the same color\n");*/
	  *ww=0;*hh=0;*xx=0;*yy=0;
	}
      return True;    
    }
  else
    return False;
  
  return False;	/* False= nothing has changed */
}
























/*
 * DANG_BEGIN_FUNCTION vga_emu_switch_page(unsigned int pagenumber)        
 *
 * description:
 *  vga_emu_switch_page() is used to emulate video-bankswitching.
 *  This function isn't called anywhere, but has to be used, with
 *  other videomodes.
 *  This function just remaps his 'own' memory into the 0xA000-0xB0000
 *  area and returns True on succes and False on error.
 *
 *  At the moment just a stupid function, but it is a start.
 *  Jou must be sure, you've got all changes before you switch a bank!
 *
 * DANG_END_FUNCTION                        
 */     

int vgaemu_switch_page(unsigned int pagenumber)
{
  /* remapping the one bank of the allocated memmory */
 
 /* Is this < or <= ? -- Erik */
  if(pagenumber<=VGAEMU_BANKS)
    {
      if(mmap((caddr_t)0xA0000, VGAEMU_BANK_SIZE, PROT_READ|PROT_WRITE,
	      MAP_SHARED | MAP_FIXED,selfmem_fd,
	      (off_t)(vga_emu_memory+pagenumber*VGAEMU_BANK_SIZE) )<0)
	{
	  v_printf("vga_emu_switch_page: Remapping failed\n");
	  return False;
	}
    }
  else
    {
      v_printf("vga_emu_switch_page: Invalid page number\n");
    }
  
  mprotect((void *)0xA0000,0xFFFF,PROT_READ); /* should not be needed, but... */
  return True;
}



int set_vgaemu_mode(int mode, int width, int height)
{
  int index=0;
  int i;
  int found=False;
  
  /* unprotect vgaemu memory, maybe should also be executable ???? */
  mprotect((void*)current_mode_info->bufferstart,current_mode_info->bufferlen,PROT_READ|PROT_WRITE);
  
  /* Search for the first valid mode */
  for(i=0; (vga_mode_table[i].mode!=-1) && (found==False); i++)
    {
      if(vga_mode_table[i].mode==mode)
        {
          if(vga_mode_table[i].type==TEXT)
            {
              /* TEXT modes can use different char boxes, like the
               * mode 0x03 (80x25). Mode 0x03 uses a 9x16 char box
               * for 80x25 and a 8x8 charbox for 80x50
               */
              if( (vga_mode_table[i].x_char==width) &&
                  (vga_mode_table[i].y_char==height) )
                {
                  found=True;
                  index=i;
                }
            }
          else /* type==GRAPH */
            {
              /* GRAPH modes use only one format */
              found=True;
              index=i;
            }
        }
    }

  if(found==True)
    v_printf("set_vgaemu_mode(): mode found in first run!\n");
    
  /* Play it again, Sam!
   * This is when we can't find the textmode with the appropriate sizes.
   * Use the first possible mode available.
   */
  if(found==False)
    {
      for(i=0; (vga_mode_table[i].mode!=-1) && (found==False); i++)	
        {
          if(vga_mode_table[i].mode==mode)
            {
              found=True;
              index=i;
            }
        }
        
      if(found==True)
        v_printf("set_vgaemu_mode(): mode found in second run!\n");
    }
    
    
  v_printf("set_vgaemu_mode(): mode=0x%02x, (%ix%i, %ix%i, %ix%i)\n",
         vga_mode_table[index].mode,
         vga_mode_table[index].x_res,
         vga_mode_table[index].y_res,
         vga_mode_table[index].x_char,
         vga_mode_table[index].y_char,
         vga_mode_table[index].x_box,
         vga_mode_table[index].y_box);
         
  if(found==True)
    {
      current_mode_info=&vga_mode_table[index];
  
      /*protect the vgaemu memory, so it is possible to trap everything */
      /* PROT_READ defined in /usr/include/asm/mman.h  root@sjoerd*/
      mprotect((void*)current_mode_info->bufferstart,current_mode_info->bufferlen,PROT_READ);
      /* I think we have to write something to the bios area */

      /* Re-initialize the DAC */
      DAC_init();

      return(True);
    }
  else
    return(False);
}


int get_vgaemu_width(void)
{
  /* printf("get_vgaemu_width= %d\n",current_mode_info->x_res);
     return 320; */
  return current_mode_info->x_res;
}

int get_vgaemu_heigth(void)
{
  /* printf("get_vgaemu_heigth= %d\n",current_mode_info->y_res);
     return 200;*/
  return current_mode_info->y_res;
}


void print_vgaemu_mode(int mode)
{
  /* int mode;*/
  for(mode=0;(vga_mode_table[mode]).mode!=-1;mode++)
    v_printf("mode = 0x%02x  mode = 0x%02x type = %d w = %d h = %d\n",mode,
	   (vga_mode_table[mode]).mode,
	   (vga_mode_table[mode]).type,
	   
	   (vga_mode_table[mode]).x_res,
	   (vga_mode_table[mode]).y_res);
}




/*
 * DANG_BEGIN_FUNCTION get_vga_mode_info
 *
 * Returns a pointer to the vga_mode_info structure for the
 * requested mode or NULL if an invalid mode was given.
 *
 * DANG_END_FUNCTION
 */
vga_mode_info* get_vgaemu_mode_info(int mode)
{
  int i;

  for(i=0; 1; i++)	
    {
      if(vga_mode_table[i].mode==mode)
	  return(&vga_mode_table[i]);
	
      if(vga_mode_table[i].mode==-1)
	  return NULL;
    }
  
  /* catch all */
  return NULL;
}



int get_vgaemu_tekens_x(void)
{
  return current_mode_info->x_char;
}

int get_vgaemu_tekens_y(void)
{
  return current_mode_info->y_char;
}

int get_vgaemu_type(void)
{
  return current_mode_info->type;
}

int vgaemu_update(unsigned char *data, int method, int *x, int *y, int *width, int *heigth)
{

/*  if(vgaemu_info->update!=NULL)
    return vgaemu_info->update(image, method,x, y,width,heigth);
  else*/ 
  
  switch(current_mode_info->memorymodel)
    {
    case P8:
      return(vgaemu_get_changes_and_update_XImage_0x13(data, method, x, y, 
        width, heigth));
      break;
    
    default:
      v_printf("vgaemu_update(): No update function for memory model 0x%02x\n", 
             current_mode_info->memorymodel);
      return False;
      break;
    }
}
     
     
int set_vgaemu_page(unsigned int page)
{
  v_printf("set_vgaemu_page %d\n",page);
/*  if(page<=current_mode_info->pages)
    {
      vgaemu_info->active_page=page;
 */     
      /* some bankswitching to do */
   /*   
      
      return True;
    }*/
  return False;
}     
