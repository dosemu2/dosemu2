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
 * Read vgaemu.c for details
 *
 */

#include "config.h"

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


vga_mode_info* get_vgaemu_mode_info(int mode);


/* **************** DACemu functions **************** */
inline void DAC_set_read_index(unsigned char index);
inline void DAC_set_write_index(unsigned char index);
unsigned char DAC_read_value(void);
void DAC_write_value(unsigned char value);
inline void DAC_set_pel_mask(unsigned char mask);


/* **************** ATTRemu functions **************** */
void Attr_init(void);
void Attr_write_value(unsigned char data);
unsigned char Attr_read_value(void);
inline unsigned char Attr_get_index(void);
unsigned char Attr_get_input_status_1(void);

/* **************** Sequencer emu functions **************** */
void Seq_init(void);
void Seq_set_index(unsigned char data);
unsigned char Seq_get_index(void);
void Seq_write_value(unsigned char data);
unsigned char Seq_read_value(void);

#endif /* !defined __ASM__ */

#endif
