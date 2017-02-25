/*
 * Containing modifications of first step  VGA mode 12h support which is
 * (C) Copyright 2000 by David Hindman.
 *
 * Final enhanced implementation of PL4/PL2/PL1 and mode-X vga modes
 * (one of it mode 12h) emulation was done by and is
 * (C) Copyright 2000 Bart Oldeman <Bart.Oldeman@bristol.ac.uk>
 * (C) Copyright 2000 Steffen Winterfeldt <wfeldt@suse.de>
 *
 * These modifications are distributed under the terms of the
 * GNU General Public License.  The copyright to the modifications
 * is hereby assigned to the "DOSEMU-Development-Team"
 *
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The VGA emulator for DOSEmu.
 *
 * Emulated are the video memory and the VGA register set (CRTC, DAC, etc.).
 * Parts of the hardware emulation is done in separate files (attremu.c,
 * crtcemu.c, dacemu.c and seqemu.c).
 *
 * VGAEmu uses the video BIOS code in base/bios/int10.c and env/video/vesa.c.
 *
 * For an excellent reference to programming SVGA cards see Finn Thøgersen's
 * VGADOC4, available at http://www.datashopper.dk/~finth
 *
 * /REMARK
 * DANG_END_MODULE
 *
 *
 * Copyright (C) 1995 1996, Erik Mouw and Arjan Filius
 * Copyright (c) 1997 Steffen Winterfeldt
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
 * DANG_BEGIN_CHANGELOG
 *
 * 1996/05/06:
 *   Adam Moss (aspirin@tigerden.com):
 *    - Fixed method 2 of vgaemu_get_changes_in_pages()
 *    - Very simplified vgaemu_get_changes_and_update_XImage_0x13()
 *  Erik Mouw:
 *    - Split VGAemu in three files
 *    - Some minor bug fixes
 *
 * 1996/05/09:
 *   Adam Moss:
 *    - Changed the way that vgaemu_get_changes() and
 *        vgaemu_get_changes_and_update_XImage_0x13() return an area.
 *    - Minor bug fixes
 *
 * 1996/05/20:
 *   Erik:
 *    - Made VESA modes start to work properly!
 *   Adam:
 *    - Fixed method 0 of vgaemu_get_changes_in_pages()
 *    - Fixed vgaemu_get_changes_in_pages to work faster/better with
 *       SVGA modes
 *
 * 28 Dec 1996
 *  Erik Mouw (J.A.K.Mouw@et.tudelft.nl):
 *   - Added sequencer emulation calls
 *   - Added IO port register calls in vga_emu_init()
 *
 * 1997/06/15: Major rework; the interface to the outside now uses the
 * global variable `vga' which holds all VGA related information.
 * The update function now works for all types of modes except for
 * Hercules modes.
 * The VGA memory is now always executable.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * 1997/07/07: Added linear frame buffer (LFB) support, simplified and
 * generalized video mode definitions (now possible via dosemu.conf).
 * vga_emu_init() now gets some info about the display it is actually
 * running on (needed for VESA emulation).
 * vga_emu_setmode() now accepts VESA mode numbers.
 * Hi/true-color modes now work.
 * -- sw
 *
 * 1998/10/25: Added changed_vga_colors(). It replaces the former
 * DAC_get_dirty_entry() functions. PEL mask support now works.
 * -- sw
 *
 * 1998/11/01: Added so far unsupported video modes (CGA, 1 bit & 2 bit VGA).
 * -- sw
 *
 * 1998/12/12: Text mode geometry changes are no longer done via mode set.
 * -- sw
 *
 * 1999/01/05: Some changes needed to support Hercules mode and various changes
 * due to the improved VGA register compatibility.
 * -- sw
 *
 * 2000/05/07: First step VGA 0x12 mode emulation added by
 * David Pinson <dpinson@materials.unsw.EDU.AU> & David Hindman <dhindman@io.com>
 *
 * 2000/05/09: Reworked the mode 0x12 code to work in all PL4 modes and
 * integrated it better into VGAEMU. It works now in (16-bit-)DPMI mode.
 * Fixed quite a few bugs in the code.
 * -- sw
 *
 * 2000/05/10: Added Bart Oldeman's <Bart.Oldeman@bristol.ac.uk> patch
 * for better instruction emulation.
 * -- sw
 *
 * 2000/05/11: Reworked the mapping/page protection stuff. It now remembers
 * each page protection and makes only the necessary *_mapping() calls.
 * Also, there is no longer a framebuffer available for modes with
 * color depth < 8. The PL2 mode (0x0f) now uses the code emulation code, too.
 * And, the 'mapself' mapping works again. Search for 'evil' to see the changes.
 * Included Bart's 3rd patch.
 * --sw
 *
 * 2000/05/18: Added functions to display fonts in graphics modes.
 * --sw
 *
 * 2000/05/18: Moved instr_sim and friends to instremu.c.
 * --beo
 *
 * 2000/06/01: Over the last weeks: speeded up Logical_VGA_{read,write}.
 * --beo
 *
 * 2001/03/14: Added vgaemu_put_pixel and vgaemu_get_pixel.
 * --beo
 *
 * DANG_END_CHANGELOG
 *
 */


/*
  Notes on VGA read/write emulation (as used for PL4 modes like 0x12) and mode-X.

  The memory layout always reflects the setting of the GFX Read Map Select
  register. So in principle we can safely allow reads to the VGA memory. This
  works of course only in read mode 0.

  But even in read mode 0 there is a problem with correct updates of the latch
  registers. Instead, we block read accesses as well. Note that even in this
  case the VGA memory is always mapped according to GFX Read Map Select.

  2000/05/09, sw
  update
  2000/06/01, bo
*/


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Define some of the following to generate various debug output.
 * A useful debug level is 1; level 2 can easily lead to insanely
 * large log files.
 */
#define	DEBUG_IO	0	/* (<= 2) port emulation */
#define	DEBUG_MAP	1	/* (<= 4) VGA memory mapping */
#define	DEBUG_UPDATE	0	/* (<= 1) screen update process */
#define	DEBUG_BANK	0	/* (<= 2) bank switching */
#define	DEBUG_COL	0	/* (<= 1) color interpretation changes */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if !defined True
#define False 0
#define True 1
#endif

#define RW	VGA_PROT_RW
#define RO	VGA_PROT_RO
#define NONE	VGA_PROT_NONE
#define DEF_PROT (vga.inst_emu==EMU_ALL_INST ? NONE : RO)

/*
 * We add PROT_EXEC just because pages should be executable. Of course
 * Intel's x86 processors do not all support non-executable pages, but anyway...
 */
#define VGA_EMU_RW_PROT		(PROT_READ | PROT_WRITE | PROT_EXEC)
#define VGA_EMU_RO_PROT		(PROT_READ | PROT_EXEC)
#define VGA_EMU_NONE_PROT	0

#define vga_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_IO >= 1
#define vga_deb_io(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb_io(x...)
#endif

#if DEBUG_IO >= 2
#define vga_deb2_io(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb2_io(x...)
#endif

#if DEBUG_MAP >= 1
#define vga_deb_map(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb_map(x...)
#endif

#if DEBUG_MAP >= 2
#define vga_deb2_map(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb2_map(x...)
#endif

#if DEBUG_UPDATE >= 1
#define vga_deb_update(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb_update(x...)
#endif

#if DEBUG_BANK >= 1
#define vga_deb_bank(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb_bank(x...)
#endif

#if DEBUG_BANK >= 2
#define vga_deb2_bank(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb2_bank(x...)
#endif

#if DEBUG_COL >= 1
#define vga_deb_col(x...) v_printf("VGAEmu: " x)
#else
#define vga_deb_col(x...)
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include "cpu.h"		/* root@sjoerd: for context structure */
#include "emu.h"
#include "int.h"
#include "dpmi.h"
#include "port.h"
#include "video.h"
#include "bios.h"
#include "memory.h"
#include "render.h"
#include "vgaemu.h"
#include "priv.h"
#include "mapping.h"
#include "utilities.h"
#include "instremu.h"

/* table with video mode definitions */
#include "vgaemu_modelist.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*
 * functions local to this file
 */

static int vga_emu_protect(unsigned, unsigned, int);
static int vga_emu_map(unsigned, unsigned);
static int _vga_emu_adjust_protection(unsigned page, unsigned mapped_page,
	int prot, int dirty);
static void _vgaemu_dirty_page(int page, int dirty);
#if 0
static int vgaemu_unmap(unsigned);
#endif
#if DEBUG_UPDATE >= 1
static void print_dirty_map(void);
#endif
#if DEBUG_MAP >= 3
static void print_prot_map(void);
#endif
static int vga_emu_setup_mode(vga_mode_info *, int, unsigned, unsigned, unsigned);
static void vga_emu_setup_mode_table(void);

static Bit32u rasterop(Bit32u value);
static pthread_mutex_t prot_mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t mode_mtx = PTHREAD_MUTEX_INITIALIZER;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*
 * holds all VGA data
 */
vga_type vga;

/*
 * the structure of our VGA BIOS;
 * initialized by vbe_init()
 */
vgaemu_bios_type vgaemu_bios;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION VGA_emulate_outb
 *
 * description:
 * Emulates writes to VGA ports.
 * This is a hardware emulation function.
 *
 * arguments:
 * port  - The port being written to.
 * value - The value written,
 *
 * DANG_END_FUNCTION
 *
 */

int VGA_emulate_outb(ioport_t port, Bit8u value)
{
  vga_deb2_io("VGA_emulate_outb: port[0x%03x] = 0x%02x\n", (unsigned) port, (unsigned) value);

  switch(port) {
    case HERC_MODE_CTRL:		/* 0x3b8*/
      if(vga.config.mono_port) Herc_set_mode_ctrl(value);
      break;

    case HERC_CFG_SWITCH:		/* 0x3bf*/
      if(vga.config.mono_port) Herc_set_cfg_switch(value);
      break;

    case ATTRIBUTE_INDEX:		/* 0x3c0 */
      Attr_write_value(value);
      break;

    case ATTRIBUTE_DATA:		/* 0x3c1 */
      vga_deb_io("VGA_emulate_outb: Attribute Controller data port is read-only!\n");
      break;

    case MISC_OUTPUT_W:			/* 0x3c2 */
      Misc_set_misc_output(value);
      break;

    case SEQUENCER_INDEX:		/* 0x3c4 */
      Seq_set_index(value);
      break;

    case SEQUENCER_DATA:		/* 0x3c5 */
      pthread_mutex_lock(&mode_mtx);
      Seq_write_value(value);
      pthread_mutex_unlock(&mode_mtx);
      break;

    case DAC_PEL_MASK:			/* 0x3c6 */
      DAC_set_pel_mask(value);
      break;

    case DAC_READ_INDEX:		/* 0x3c7 */
      DAC_set_read_index(value);
      break;

    case DAC_WRITE_INDEX:		/* 0x3c8 */
      DAC_set_write_index(value);
      break;

    case DAC_DATA:			/* 0x3c9 */
      DAC_write_value(value);
      break;

    case GFX_INDEX:			/* 0x3ce */
      GFX_set_index(value);
      break;

    case GFX_DATA:			/* 0x3cf */
      GFX_write_value(value);
      break;

    case CRTC_INDEX_MONO:		/* 0x3b4 */
      if(vga.config.mono_port) CRTC_set_index(value);
      break;

    case CRTC_DATA_MONO:		/* 0x3b5 */
      if(vga.config.mono_port) CRTC_write_value(value);
      break;

    case FEATURE_CONTROL_W_MONO:	/* 0x3ba */
      if(vga.config.mono_port) Misc_set_feature_ctrl(value);
      break;

    case CRTC_INDEX:			/* 0x3d4 */
      if(!vga.config.mono_port) CRTC_set_index(value);
      break;

    case CRTC_DATA:			/* 0x3d5 */
      pthread_mutex_lock(&mode_mtx);
      if(!vga.config.mono_port) CRTC_write_value(value);
      pthread_mutex_unlock(&mode_mtx);
      break;

    case COLOR_SELECT:			/* 0x3d9 */
      if(!vga.config.mono_port) Misc_set_color_select(value);
      break;

    case FEATURE_CONTROL_W:		/* 0x3da */
      if(!vga.config.mono_port) Misc_set_feature_ctrl(value);
      break;

    default:
      vga_deb_io(
        "VGA_emulate_outb: write access not emulated (port[0x%03x] = 0x%02x)\n",
        (unsigned) port, (unsigned) value
      );
      return 0;
  }
  return 1;
#if DEBUG_MAP >= 3
  print_prot_map();
#endif
}


int VGA_emulate_outw(ioport_t port, Bit16u value)
{
    if (VGA_emulate_outb(port,value)==0) return 0;
    return VGA_emulate_outb(port+1,value>>8);
}


/*
 * DANG_BEGIN_FUNCTION VGA_emulate_inb
 *
 * description:
 * Emulates reads from VGA ports.
 * This is a hardware emulation function.
 *
 * arguments:
 * port  - The port being read from.
 *
 * DANG_END_FUNCTION
 *
 */

Bit8u VGA_emulate_inb(ioport_t port)
{
  Bit8u uc = 0xff;

  switch(port) {
    case HERC_MODE_CTRL:		/* 0x3b8*/
      if(vga.config.mono_port) uc = Herc_get_mode_ctrl();
      break;

    case ATTRIBUTE_INDEX:		/* 0x3c0 */
      uc = Attr_get_index();
      break;

    case ATTRIBUTE_DATA:		/* 0x3c1 */
      uc = Attr_read_value();
      break;

    case INPUT_STATUS_0:		/* 0x3c2 */
      uc = Misc_get_input_status_0();
      break;

    case SEQUENCER_INDEX:		/* 0x3c4 */
      uc = Seq_get_index();
      break;

    case SEQUENCER_DATA:		/* 0x3c5 */
      uc = Seq_read_value();
      break;

    case DAC_PEL_MASK:			/* 0x3c6 */
      uc = DAC_get_pel_mask();
      break;

    case DAC_STATE:			/* 0x3c7 */
      uc = DAC_get_state();
      break;

    case DAC_WRITE_INDEX:		/* 0x3c8 */
      /* this is undefined, but we have to do something */
      break;

    case DAC_DATA:			/* 0x3c9 */
      uc = DAC_read_value();
      break;

    case FEATURE_CONTROL_R:		/* 0x3ca */
      uc = Misc_get_feature_ctrl();
      break;

    case MISC_OUTPUT_R:			/* 0x3cc */
      uc = Misc_get_misc_output();
      break;

    case GFX_INDEX:	/* 0x3ce */
      uc = GFX_get_index();
      break;

    case GFX_DATA:			/* 0x3cf */
      uc = GFX_read_value();
      break;

    case CRTC_INDEX_MONO:		/* 0x3b4 */
      if(vga.config.mono_port) uc = CRTC_get_index();
      break;

    case CRTC_DATA_MONO:		/* 0x3b5 */
      if(vga.config.mono_port) uc = CRTC_read_value();
      break;

    case INPUT_STATUS_1_MONO:		/* 0x3ba */
      if(vga.config.mono_port) uc = Misc_get_input_status_1();
      break;

    case CRTC_INDEX:			/* 0x3d4 */
      if(!vga.config.mono_port) uc = CRTC_get_index();
      break;

    case CRTC_DATA:			/* 0x3d5 */
      if(!vga.config.mono_port) uc = CRTC_read_value();
      break;

    case INPUT_STATUS_1:		/* 0x3da */
      if(!vga.config.mono_port) uc = Misc_get_input_status_1();
      break;

    default:
      vga_deb_io(
        "VGA_emulate_inb: read access not emulated (port[0x%03x] = 0x%02x)\n",
        (unsigned) port, (unsigned) uc
      );
      break;
  }

  vga_deb2_io("VGA_emulate_inb: port[0x%03x] = 0x%02x\n", (unsigned) port, (unsigned) uc);

  return uc;
}

Bit16u VGA_emulate_inw(ioport_t port)
{
    Bit16u v = VGA_emulate_inb(port);
    return v | (VGA_emulate_inb(port+1)<<8);
}


/*
 * This routine emulates a logical VGA read (CPU read  within the
 * VGA memory range a0000-bffff).  This usually doesn't just read the
 * byte from memory.  Instead, the VGA controller mediates the
 * read in a very complex fashion that depends on the settings of the
 * VGA control registers.  So we will call this a Logical VGA read.
 *
 * Much of the code in this routine is adapted from bochs.
 * Bochs is Copyright (C) 2000 MandrakeSoft S.A. and distributed under LGPL.
 * Bochs was originally authored by Kevin Lawton.
 */
/* converts a 4-bit color to an 8-pixels representation in PL-4 modes */
static Bit32u color2pixels[16] = {0,0xff,0xff00,0xffff,0xff0000,0xff00ff,0xffff00,
			0x00ffffff,0xff000000,0xff0000ff,0xff00ff00,0xff00ffff,
			0xffff0000,0xffff00ff,0xffffff00,0xffffffff};

#define SetReset	vga.gfx.set_reset
#define EnableSetReset	vga.gfx.enable_set_reset
#define ColorCompare	vga.gfx.color_compare
#define DataRotate	vga.gfx.data_rotate
#define RasterOp	vga.gfx.raster_op
#define ReadMapSelect	vga.gfx.read_map_select
#define WriteMode	vga.gfx.write_mode
#define ReadMode	vga.gfx.read_mode
#define ColorDontCare	vga.gfx.color_dont_care
#define BitMask		vga.gfx.bitmask
#define MapMask		vga.seq.map_mask

#define VGALatch	vga.latch

static unsigned char Logical_VGA_read(unsigned offset)
{

  switch (ReadMode) {
    case 0: /* read mode 0 */
      VGALatch[0] = vga.mem.base[          offset];
      VGALatch[1] = vga.mem.base[1*65536 + offset];
      VGALatch[2] = vga.mem.base[2*65536 + offset];
      VGALatch[3] = vga.mem.base[3*65536 + offset];
      return(VGALatch[ReadMapSelect]);
      break;

    case 1: /* read mode 1 */
      {
      Bit32u latch;
      Bit8u retval;

      latch =  (((VGALatch[0] = vga.mem.base[          offset])        |
                ((VGALatch[1] = vga.mem.base[1*65536 + offset]) <<  8) |
                ((VGALatch[2] = vga.mem.base[2*65536 + offset]) << 16) |
                ((VGALatch[3] = vga.mem.base[3*65536 + offset]) << 24) ) &
		  color2pixels[ColorDontCare & 0xf]) ^
                    color2pixels[ColorCompare & ColorDontCare & 0xf];
	    /* XORing gives all bits that are different */
	    /* after combining through OR the "equal" bits are zero, hence a NOT */

      retval = ~((latch & 0xff) | ((latch>>8) & 0xff) |
                ((latch>>16) & 0xff) | ((latch>>24) & 0xff));
#if 0
      vga_msg(
        "read mode 1: col_cmp 0x%x, col_dont 0x%x, latches = %02x %02x %02x %02x, read = 0x%02x\n",
        color_compare, color_dont_care, VGALatch[0], VGALatch[1],
        VGALatch[2], VGALatch[3], retval
      );
#endif
      return(retval);
      }
      break;

    default:
      return(0);
    }
}

/*
 * This routine emulates a logical VGA write (CPU write to within the
 * VGA memory range a0000-bffff).  This usually doesn't just place the
 * byte into memory.  Instead, the VGA controller mediates the
 * write in a very complex fashion that depends on the settings of the
 * VGA control registers.  So we will call this a Logical VGA write,
 * and the lower level thing which places bytes in video RAM we will
 * call a Physical VGA write.
 *
 * Much of the code in this routine is adapted from bochs.
 * Bochs is Copyright (C) 2000 MandrakeSoft S.A. and distributed under LGPL.
 * Bochs was originally authored by Kevin Lawton.
 */

static Bit32u rasterop(Bit32u value)
{
  Bit32u bitmask = BitMask, vga_latch;
  memcpy(&vga_latch, VGALatch, 4);

  bitmask |= bitmask << 8;
  bitmask |= bitmask << 16; /* bitmask extended over 4 bytes */
  switch (RasterOp) {
    case 0: /* replace */
      return (value & bitmask) | (vga_latch & ~bitmask);
    case 1: /* AND with latch data */
      return (value | ~bitmask) & vga_latch;
    case 2: /* OR with latch data */
      return (value & bitmask) | vga_latch;
    case 3: /* XOR with latch data */
      return (value & bitmask) ^ vga_latch;
  }
  return 0;
}

static Bit32u Logical_VGA_CalcNewVal (unsigned char value)
{
  Bit32u enable_set_reset, new_val = 0;
  Bit8u bitmask;

  switch (WriteMode) {
    case 0: /* write mode 0 */
      /* perform rotate on CPU data in case its needed */
      new_val = (value >> DataRotate) | (value << (8 - DataRotate));
      new_val |= new_val<<8;
      new_val |= new_val<<16; /* value extended over 4 bytes */
      enable_set_reset = color2pixels[EnableSetReset];
      return rasterop((new_val & ~enable_set_reset) |
                         (color2pixels[SetReset] & enable_set_reset));

    case 1: /* write mode 1 */
      memcpy(&new_val, VGALatch, 4);
      return new_val;
      break;

    case 2: /* write mode 2 */
      return rasterop(color2pixels[value & 0xf]);

    case 3: /* write mode 3 */
      bitmask = BitMask;
      /* perform rotate on CPU data */
      BitMask = ((value >> DataRotate) | (value << (8 - DataRotate))) &
                      bitmask; /* Set Bitmask temporarily */
      new_val = rasterop(color2pixels[SetReset]);
      BitMask = bitmask; /* get old bitmask back */
      break;

    default:
      break;
    }

  return new_val;
}

static void Logical_VGA_write(unsigned offset, unsigned char value)
{
  unsigned vga_page;
  unsigned char *p;
  Bit32u new_val;

  new_val = Logical_VGA_CalcNewVal(value);

  vga_page = offset >> 12;
  p = (unsigned char *)(vga.mem.base + offset);

  if(MapMask & 0x01) {
    *p = new_val;
  }
  if(MapMask & 0x02) {
    p[0x10000] = new_val >> 8;
  }
  if(MapMask & 0x04) {
    p[0x20000] = new_val >> 16;
  }
  if(MapMask & 0x08) {
    p[0x30000] = new_val >> 24;
  }

  if(MapMask) {
    // not optimal, but works better with update function -- sw
    pthread_mutex_lock(&prot_mtx);
    vga.mem.dirty_map[vga_page] = 1;
    vga.mem.dirty_map[vga_page + 0x10] = 1;
    vga.mem.dirty_map[vga_page + 0x20] = 1;
    vga.mem.dirty_map[vga_page + 0x30] = 1;
    pthread_mutex_unlock(&prot_mtx);
  }

}

int vga_bank_access(dosaddr_t m)
{
	return (unsigned)(m - vga.mem.bank_base) < vga.mem.bank_len;
}

int vga_read_access(dosaddr_t m)
{
	/* Using a planar mode */
	return vga_bank_access(m);
}

int vga_write_access(dosaddr_t m)
{
	/* unmapped VGA memory, VGA BIOS, or a bank. Note that
	 * the bank can be write-protected even in non-planar mode. */
	if (m >= vga.mem.bank_base + vga.mem.bank_len &&
			m < vga.mem.graph_base + vga.mem.graph_size)
		return 1;
	if (m >= 0xc0000 && m < 0xc0000 + (vgaemu_bios.pages<<12))
		return 1;
	if (!config.umb_f0 && m >= 0xf0000 && m < 0xf4000)
		return 1;
	if (m >= vga.mem.bank_base && m < vga.mem.bank_base + vga.mem.bank_len)
		return 1;
	return 0;
}

int vga_access(dosaddr_t r, dosaddr_t w)
{
	return (vga_read_access(r) | (vga_write_access(w) << 1));
}

unsigned char vga_read(dosaddr_t addr)
{
  if (!vga.inst_emu || !vga_read_access(addr))
    return READ_BYTE(addr);
  return Logical_VGA_read(addr - vga.mem.bank_base);
}

unsigned short vga_read_word(dosaddr_t addr)
{
  if (!vga.inst_emu)
    return READ_WORD(addr);
  return vga_read(addr) | vga_read(addr + 1) << 8;
}

void vga_mark_dirty(dosaddr_t s_addr, int len)
{
  unsigned vga_page, abeg, aend, addr;
  abeg = s_addr & PAGE_MASK;
  aend = (s_addr + len - 1) & PAGE_MASK;
  for (addr = abeg; addr <= aend; addr += PAGE_SIZE) {
    if (!vga_write_access(addr))
      continue;
    vga_page = (addr >> PAGE_SHIFT) -
	vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page +
	vga.mem.map[VGAEMU_MAP_BANK_MODE].first_page;
    _vga_emu_adjust_protection(vga_page, 0, VGA_PROT_RW, 1);
  }
}

void vga_write(dosaddr_t addr, unsigned char val)
{
  if (!vga.inst_emu || !vga_bank_access(addr)) {
    vga_emu_prot_lock();
    vga_mark_dirty(addr, 1);
    WRITE_BYTE(addr, val);
    vga_emu_prot_unlock();
    return;
  }
  Logical_VGA_write(addr - vga.mem.bank_base, val);
}

void vga_write_word(dosaddr_t addr, unsigned short val)
{
  if (!vga.inst_emu || !vga_bank_access(addr)) {
    vga_emu_prot_lock();
    vga_mark_dirty(addr, 2);
    WRITE_WORD(addr, val);
    vga_emu_prot_unlock();
    return;
  }
  vga_write(addr, val & 0xff);
  vga_write(addr + 1, val >> 8);
}

void vga_write_dword(dosaddr_t addr, unsigned val)
{
  if (!vga.inst_emu || !vga_bank_access(addr)) {
    vga_emu_prot_lock();
    vga_mark_dirty(addr, 4);
    WRITE_DWORD(addr, val);
    vga_emu_prot_unlock();
    return;
  }
  vga_write_word(addr, val & 0xffff);
  vga_write_word(addr + 2, val >> 16);
}

void memcpy_to_vga(dosaddr_t dst, const void *src, size_t len)
{
  int i;
  if (!vga.inst_emu) {
    vga_emu_prot_lock();
    vga_mark_dirty(dst, len);
    MEMCPY_2DOS(dst, src, len);
    vga_emu_prot_unlock();
    return;
  }
  for (i = 0; i < len; i++)
    vga_write(dst + i, ((unsigned char *)src)[i]);
}

void memcpy_dos_to_vga(dosaddr_t dst, dosaddr_t src, size_t len)
{
  int i;
  if (!vga.inst_emu) {
    vga_emu_prot_lock();
    vga_mark_dirty(dst, len);
    MEMCPY_DOS2DOS(dst, src, len);
    vga_emu_prot_unlock();
    return;
  }
  for (i = 0; i < len; i++)
    vga_write(dst + i, READ_BYTE(src + i));
}

void memcpy_from_vga(void *dst, dosaddr_t src, size_t len)
{
  int i;
  if (!vga.inst_emu) {
    MEMCPY_2UNIX(dst, src, len);
    return;
  }
  for (i = 0; i < len; i++) {
    ((unsigned char *)dst)[i] = vga_read(src + i);
  }
}

void memcpy_dos_from_vga(dosaddr_t dst, dosaddr_t src, size_t len)
{
  int i;
  if (!vga.inst_emu) {
    MEMCPY_DOS2DOS(dst, src, len);
    return;
  }
  for (i = 0; i < len; i++) {
    WRITE_BYTE(dst + i, vga_read(src + i));
  }
}

void vga_memcpy(dosaddr_t dst, dosaddr_t src, size_t len)
{
  int i;
  if (!vga.inst_emu) {
    vga_emu_prot_lock();
    vga_mark_dirty(dst, len);
    MEMMOVE_DOS2DOS(dst, src, len);
    vga_emu_prot_unlock();
    return;
  }
  for (i = 0; i < len; i++)
    vga_write(dst + i, vga_read(src + i));
}

void vga_memset(dosaddr_t dst, unsigned char val, size_t len)
{
  int i;
  if (!vga.inst_emu) {
    vga_emu_prot_lock();
    vga_mark_dirty(dst, len);
    MEMSET_DOS(dst, val, len);
    vga_emu_prot_unlock();
    return;
  }
  for (i = 0; i < len; i++)
    vga_write(dst + i, val);
}

void vga_memsetw(dosaddr_t dst, unsigned short val, size_t len)
{
  if (!vga.inst_emu) {
    vga_emu_prot_lock();
    vga_mark_dirty(dst, len * 2);
    while (len--) {
      WRITE_WORD(dst, val);
      dst += 2;
    }
    vga_emu_prot_unlock();
    return;
  }
  while (len--) {
    vga_write_word(dst, val);
    dst += 2;
  }
}

void vga_memsetl(dosaddr_t dst, unsigned val, size_t len)
{
  if (!vga.inst_emu) {
    vga_emu_prot_lock();
    vga_mark_dirty(dst, len * 4);
    while (len--) {
      WRITE_DWORD(dst, val);
      dst += 4;
    }
    vga_emu_prot_unlock();
    return;
  }
  while (len--) {
    vga_write_dword(dst, val);
    dst += 4;
  }
}

/*
 * DANG_BEGIN_FUNCTION vga_emu_fault
 *
 * description:
 * vga_emu_fault() is used to catch video access, and handle it.
 * This function is called from arch/linux/async/sigsegv.c::dosemu_fault1().
 * Now it catches only changes in a 4K page, but maybe it is useful to
 * catch each video access. The problem when you do that is, you have to
 * simulate each instruction which could write to the video memory.
 * It is easy to get the place where the exception happens (scp->cr2),
 * but what are those changes?
 * An other problem is, it could eat a lot of time, but it does now also.
 *
 * MODIFICATION: VGA mode 12h under X is supported in exactly the
 * way that was suggested above.  Not every instruction needs to be
 * simulated in order to make this feature useful, just the ones used to
 * access video RAM by key applications (Borland BGI, Protel, etc.).
 *
 * MODIFICATION: all VGA modes now work and almost all instructions are
 * simulated.
 *
 * arguments:
 * scp - A pointer to a struct sigcontext holding some relevant data.
 *
 * DANG_END_FUNCTION
 *
 */

int vga_emu_fault(struct sigcontext *scp, int pmode)
{
  int i, j;
  dosaddr_t lin_addr;
  unsigned page_fault, vga_page = 0, u;
  unsigned char *cs_ip;
#if DEBUG_MAP >= 1
  static char *txt1[VGAEMU_MAX_MAPPINGS + 1] = { "bank", "lfb", "some" };
  unsigned access_type = (scp->err >> 1) & 1;
#endif
  lin_addr = DOSADDR_REL(LINP(scp->cr2));
  page_fault = lin_addr >> 12;

  for(i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
    j = page_fault - vga.mem.map[i].base_page;
    if(j >= 0 && j < vga.mem.map[i].pages) {
      vga_page = j + vga.mem.map[i].first_page;
      break;
    }
  }

  vga_deb_map("vga_emu_fault: %s%s access to %s region, address 0x%05x, page 0x%x, vga page 0x%x\n",
    pmode ? "dpmi " : "", access_type? "write" : "read",
    txt1[i], lin_addr, page_fault, vga_page
  );

  vga_deb2_map(
    "vga_emu_fault: in_dpmi %d, err 0x%x, scp->cs:eip %04x:%04x, vm86s->cs:eip %04x:%04x\n",
    dpmi_active(), (unsigned) scp->err, (unsigned) _cs, (unsigned) _eip,
    (unsigned) SREG(cs), (unsigned) REG(eip)
  );

  if(pmode) {
    dosaddr_t daddr = GetSegmentBase(_cs) + _eip;
    cs_ip = SEL_ADR_CLNT(_cs, _eip, dpmi_mhp_get_selector_size(_cs));
    if (
	  (cs_ip >= &mem_base[0] && cs_ip < &mem_base[0x110000]) ||
	  ((uintptr_t)cs_ip > config.dpmi_base &&
	   (uintptr_t)cs_ip + 20 < config.dpmi_base + config.dpmi * 1024 &&
	   dpmi_is_valid_range(daddr, 15)))
     vga_deb_map(
      "vga_emu_fault: cs:eip = %04x:%04x, instr: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
      (unsigned) _cs, (unsigned) _eip,
      cs_ip[ 0], cs_ip[ 1], cs_ip[ 2], cs_ip[ 3], cs_ip[ 4], cs_ip[ 5], cs_ip[ 6], cs_ip[ 7],
      cs_ip[ 8], cs_ip[ 9], cs_ip[10], cs_ip[11], cs_ip[12], cs_ip[13], cs_ip[14], cs_ip[15]
    );
  }
  else {
    cs_ip = SEG_ADR((unsigned char *), cs, ip);
    vga_deb_map(
      "vga_emu_fault: cs:eip = %04x:%04x, instr: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
      (unsigned) SREG(cs), (unsigned) REG(eip),
      cs_ip[ 0], cs_ip[ 1], cs_ip[ 2], cs_ip[ 3], cs_ip[ 4], cs_ip[ 5], cs_ip[ 6], cs_ip[ 7],
      cs_ip[ 8], cs_ip[ 9], cs_ip[10], cs_ip[11], cs_ip[12], cs_ip[13], cs_ip[14], cs_ip[15]
    );
  }

  if(i == VGAEMU_MAX_MAPPINGS) {
    if ((unsigned)((page_fault << 12) - vga.mem.graph_base) <
	vga.mem.graph_size) {	/* unmapped VGA area */
      if (pmode) {
        u = instr_len((unsigned char *)SEL_ADR(_cs, _eip),
	    dpmi_mhp_get_selector_size(_cs));
        _eip += u;
      }
      else {
        u = instr_len(SEG_ADR((unsigned char *), cs, ip), 0);
        LWORD(eip) += u;
      }
      if(u) {
        vga_msg("vga_emu_fault: attempted write to unmapped area (cs:ip += %u)\n", u);
      }
      else {
        vga_msg("vga_emu_fault: unknown instruction, page at 0x%05x now writable\n", page_fault << 12);
        vga_emu_protect_page(page_fault, RW);
      }
      return True;
    }
    else if((page_fault >= 0xc0 && page_fault < (0xc0 + vgaemu_bios.pages)) ||
	    (!config.umb_f0 && page_fault >= 0xf0 && page_fault < 0xf4)) {	/* ROM area */
      if (pmode) {
        u = instr_len((unsigned char *)SEL_ADR(_cs, _eip),
	    dpmi_mhp_get_selector_size(_cs));
        _eip += u;
      }
      else {
        u = instr_len(SEG_ADR((unsigned char *), cs, ip), 0);
        LWORD(eip) += u;
      }
      if(u) {
        vga_msg("vga_emu_fault: attempted write to ROM area (cs:ip += %u)\n", u);
      }
      else {
        vga_msg("vga_emu_fault: unknown instruction, converting ROM to RAM at 0x%05x\n", page_fault << 12);
        vga_emu_protect_page(page_fault, RW);
      }
      return True;
    }
    else {
      vga_msg(
        "vga_emu_fault: unhandled page fault (not in 0xa0000 - 0x%05x range)\n",
        (0xc0 + vgaemu_bios.pages) << 12
      );
      return False;
    }
  }

  if(vga_page < vga.mem.pages) {
    if(!vga.inst_emu) {
      /* Normal: make the display page writeable then mark it dirty */
      vga_emu_adjust_protection(vga_page, page_fault, RW, 1);
    }
    if(vga.inst_emu) {
      int ret;
#if 1
      /* XXX Hack: dosemu touched the protected page of video mem, which is
       * a bug. However for the video modes that do not require instremu, we
       * can allow that access. For the planar modes we cant and will fail :(
       */
      if(pmode && !DPMIValidSelector(_cs)) {
	error("BUG: dosemu touched the protected video memory!!!\n");
	return False;
      }
#endif

      /* A VGA mode PL4/PL2 video memory access has been trapped
       * while we are using X.  Leave the display page read/write-protected
       * so that each instruction that accesses it can be trapped and
       * simulated. */
      ret = instr_emu(scp, pmode, 0);
      if (!ret) {
        error_once("instruction simulation failure\n%s\n", DPMI_show_state(scp));
        vga_emu_adjust_protection(vga_page, page_fault, RW, 1);
      }
    }
  }
  return True;
}

static void vgaemu_update_prot_cache(unsigned page, int prot)
{
  if(page >= 0xa0 && page < 0xc0) {
    vga.mem.prot_map0[page - 0xa0] = prot;
  }

  if(
    vga.mem.lfb_base_page &&
    page >= vga.mem.lfb_base_page &&
    page < vga.mem.lfb_base_page + vga.mem.pages) {
    vga.mem.prot_map1[page - vga.mem.lfb_base_page] = prot;
  }
}


static int vgaemu_prot_ok(unsigned page, int prot)
{
  if(page >= 0xa0 && page < 0xc0) {
    return vga.mem.prot_map0[page - 0xa0] == prot ? 1 : 0;
  }

  if(
    vga.mem.lfb_base_page &&
    page >= vga.mem.lfb_base_page &&
    page < vga.mem.lfb_base_page + vga.mem.pages) {
    return vga.mem.prot_map1[page - vga.mem.lfb_base_page] == prot ? 1 : 0;
  }

  return 0;
}


/*
 * Change protection of a memory page
 *
 * The protection of `page' is set to RO if `prot' is 0, otherwise to RW.
 * The page will always be executable.
 * This functions returns the error code of the mprotect() call.
 * `page' is an absolute page number.
 *
 */

int vga_emu_protect_page(unsigned page, int prot)
{
  int i;
  int sys_prot;

  sys_prot = prot == RW ? VGA_EMU_RW_PROT : prot == RO ? VGA_EMU_RO_PROT : VGA_EMU_NONE_PROT;

  if(vgaemu_prot_ok(page, sys_prot)) {
    vga_deb2_map(
      "vga_emu_protect_page: 0x%02x = %s (unchanged)\n",
      page, prot == RW ? "RW" : prot == RO ? "RO" : "NONE"
    );

    return 0;
  }

  vga_deb2_map(
    "vga_emu_protect_page: 0x%02x = %s\n",
    page, prot == RW ? "RW" : prot == RO ? "RO" : "NONE"
  );

  if(
    vga.mem.lfb_base_page &&
    page >= vga.mem.lfb_base_page &&
    page < vga.mem.lfb_base_page + vga.mem.pages) {
    unsigned char *p;
    p = &vga.mem.lfb_base[(page - vga.mem.lfb_base_page) << 12];
    i = mprotect(p, 1 << 12, sys_prot);
  }
  else {
    i = mprotect_mapping(MAPPING_VGAEMU, page << 12, 1 << 12, sys_prot);
  }

  if(i == -1) {
    sys_prot = 0xfe;
    switch(errno) {
      case EINVAL: sys_prot = 0xfd; break;
      case EFAULT: sys_prot = 0xfc; break;
      case EACCES: sys_prot = 0xfb; break;
    }
  }

  vgaemu_update_prot_cache(page, sys_prot);

  return i;
}


/*
 * Change protection of a VGA memory page
 *
 * The protection of `page' is set to `prot' in all places where `page' is
 * currently mapped to. `prot' may be either NONE, RO or RW.
 * This functions returns 3 if `mapped_page' is not 0 and not one of the
 * locations `page' is mapped to (cf. vga_emu_adjust_protection()).
 * `page' is relative to the VGA memory.
 * `mapped_page' is an absolute page number.
 *
 */

static int vga_emu_protect(unsigned page, unsigned mapped_page, int prot)
{
  int i, j = 0, err, err_1 = 0, map_ok = 0;
#if DEBUG_MAP >= 1
  int k = 0;
#endif

  if(page > vga.mem.pages) {
    vga_deb_map("vga_emu_protect: invalid page number; page = 0x%x\n", page);
    return 1;
  }

  for(i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
    if(vga.mem.map[i].pages) {
      j = page - vga.mem.map[i].first_page;
      if(j >= 0 && j < vga.mem.map[i].pages) {
#if DEBUG_MAP >= 1
        k = 1;
#endif
        if(vga.mem.map[i].base_page + j == mapped_page) map_ok = 1;
        err = vga_emu_protect_page(vga.mem.map[i].base_page + j, prot);
        if(!err_1) err_1 = err;
        vga_deb2_map(
          "vga_emu_protect: error = %d, region = %d, page = 0x%x --> map_addr = 0x%x, prot = %s\n",
          err, i, page, vga.mem.map[i].base_page + j, prot == RW ? "RW" : prot == RO ? "RO" : "NONE"
        );
      }
    }
  }

#if DEBUG_MAP >= 1
  if(k == 0) {
    vga_deb_map("vga_emu_protect: page not mapped; page = 0x%x\n", page);
  }
#endif

  if(!mapped_page) map_ok = 1;

  if(err_1) return 2;

  return map_ok ? 0 : 3;
}


/*
 * Set protection according to vga.mem.dirty_map[]
 *
 * `mapped_page' is supposed to be either 0 or one of the pages `page' is
 * mapped to. This is just to locate inconsistencies in handling the various
 * VGA memory mappings.
 * If `mapped_page' is not 0 and turns out not to be one of the mapping
 * locations of `page', the protection of `mapped_page' is changed as well
 * and a warning is issued.
 * `page' is relative to the VGA memory.
 * `mapped_page' is an absolute page number.
 *
 */

static int _vga_emu_adjust_protection(const unsigned page, unsigned mapped_page,
	int prot, int dirty)
{
  int i, err, k;
  unsigned page1 = page;

  if(page >= vga.mem.pages) {
    dosemu_error("vga_emu_adjust_protection: invalid page number; page = 0x%x\n", page);
    return 1;
  }

  i = vga_emu_protect(page, mapped_page, prot);

  if(i == 3) {
    vga_msg(
      "vga_emu_adjust_protection: mapping inconsistency; page = 0x%x, map_addr != 0x%x\n",
      page, mapped_page
    );
    err = vga_emu_protect_page(mapped_page, prot);
    if(err) {
      vga_msg(
        "vga_emu_adjust_protection: mapping inconsistency not fixed; page = 0x%x, map_addr = 0x%x\n",
        page, mapped_page
      );
    }
  }

  if(vga.mem.planes == 4) {	/* MODE_X or PL4 */
    page1 &= ~0x30;
    for(k = 0; k < vga.mem.planes; k++, page1 += 0x10)
      vga_emu_protect(page1, 0, prot);
  }

  if(vga.mode_type == PL2) {
    /* it's actually 4 planes, but we let everyone believe it's a 1-plane mode */
    page1 &= ~0x30;
    vga_emu_protect(page1, 0, prot);
    page1 += 0x20;
    vga_emu_protect(page1, 0, prot);
  }

  if(vga.mode_type == CGA) {
    /* CGA uses two 8k banks  */
    page1 &= ~0x2;
    vga_emu_protect(page1, 0, prot);
    page1 += 0x2;
    vga_emu_protect(page1, 0, prot);
  }

  if(vga.mode_type == HERC) {
    /* Hercules uses four 8k banks  */
    page1 &= ~0x6;
    vga_emu_protect(page1, 0, prot);
    page1 += 0x2;
    vga_emu_protect(page1, 0, prot);
    page1 += 0x2;
    vga_emu_protect(page1, 0, prot);
    page1 += 0x2;
    vga_emu_protect(page1, 0, prot);
  }

  _vgaemu_dirty_page(page, dirty);

  return i;
}

int vga_emu_adjust_protection(unsigned page, unsigned mapped_page, int prot,
	int dirty)
{
  int ret;
  vga_emu_prot_lock();
  ret = _vga_emu_adjust_protection(page, mapped_page, prot, dirty);
  vga_emu_prot_unlock();
  return ret;
}

/*
 * Map the VGA memory.
 *
 * `mapping' is an index into vga.mem.map[] describing the mapping in more
 * detail. `first_page' is the first page of VGA memory to appear in this
 * mapping.
 * `first_page' is relative to the VGA memory.
 *
 */

static int vga_emu_map(unsigned mapping, unsigned first_page)
{
  unsigned u;
  vga_mapping_type *vmt;
  int prot, i;

  if(mapping >= VGAEMU_MAX_MAPPINGS) return 1;

  vmt = vga.mem.map + mapping;
  if(vmt->pages == 0) return 0;		/* nothing to do */

  if(vmt->pages + first_page > vga.mem.pages) return 2;

  /* default protection for dirty pages */
  switch(vga.inst_emu) {
    case EMU_WRITE_INST:
      prot = VGA_EMU_RO_PROT;
      break;
    case EMU_ALL_INST:
      prot = VGA_EMU_NONE_PROT;
      break;
    default:
      prot = VGA_EMU_RW_PROT;
      break;
  }

  i = 0;
  pthread_mutex_lock(&prot_mtx);
  if (mapping == VGAEMU_MAP_BANK_MODE)
    i = alias_mapping(MAPPING_VGAEMU,
      vmt->base_page << 12, vmt->pages << 12,
      prot, vga.mem.base + (first_page << 12));
  else /* LFB: mapped at init, just need to set protection */
    i = mprotect(MEM_BASE32(vmt->base_page << 12),
			 vmt->pages << 12, prot);

  if(i == -1) {
    pthread_mutex_unlock(&prot_mtx);
    error("VGA: protect page failed\n");
    return 3;
  }

  vmt->first_page = first_page;

  for(u = 0; u < vmt->pages; u++) {
    vgaemu_update_prot_cache(vmt->base_page + u, prot);
    /* need to fix up protection for clean pages */
    if(vga.mode_class == GRAPH && !vga.mem.dirty_map[vmt->first_page + u] &&
	    prot == VGA_EMU_RW_PROT)
      _vga_emu_adjust_protection(vmt->first_page + u, 0, VGA_PROT_RO, 0);
  }
  pthread_mutex_unlock(&prot_mtx);

  return 0;
}


#if 0
/*
 * Unmap the VGA memory.
 */

static int vgaemu_unmap(unsigned page)
{
  int i;

  if(
    page < 0xa0 ||
    page >= 0xc0 ||
    (!vga.config.mono_support && page >= 0xb0 && page < 0xb8)
  ) return 1;

  *((volatile char *)vga.mem.scratch_page);

  i = alias_mapping(MAPPING_VGAEMU,
    page << 12, 1 << 12,
    VGA_EMU_RW_PROT, vga.mem.scratch_page)
  );

  if (i == -1) return 3;

  return vga_emu_protect_page(page, RO);
}
#endif

/*
 * Put the vga memory mapping into a defined state.
 */
void vgaemu_reset_mapping()
{
  int i;
  int prot, page, startpage, endpage;

  memset(vga.mem.scratch_page, 0xff, 1 << 12);

  prot = VGA_EMU_RW_PROT;
  startpage = (config.umb_a0 ? 0xb0 : 0xa0);
  endpage = (config.umb_b0 ? 0xb0 : 0xb8);
  vga.mem.graph_base = startpage << 12;
  vga.mem.graph_size = (endpage << 12) - vga.mem.graph_base;
  for(page = startpage; page < endpage; page++) {
    i = alias_mapping(MAPPING_VGAEMU,
      page << 12, 1 << 12,
      prot, vga.mem.scratch_page
    );
    if (i == -1) {
      error("VGA: map failed at page %x\n", page);
      return;
    }
    vgaemu_update_prot_cache(page, prot);
  }
  for(page = 0xb8; page < 0xc0; page++) {
    i = alias_mapping(MAPPING_VGAEMU,
      page << 12, 1 << 12,
      prot, vga.mem.scratch_page
    );
    if (i == -1) {
      error("VGA: map failed at page %x\n", page);
      return;
    }
    vgaemu_update_prot_cache(page, prot);
  }
}

static void vgaemu_register_ports(void)
{
  emu_iodev_t io_device;

  /* register VGA ports */
  io_device.read_portb = VGA_emulate_inb;
  io_device.write_portb = (void (*)(ioport_t, Bit8u)) VGA_emulate_outb;
  io_device.read_portw = VGA_emulate_inw;
  io_device.write_portw = (void (*)(ioport_t, Bit16u)) VGA_emulate_outw;
  io_device.read_portd = NULL;
  io_device.write_portd = NULL;
  io_device.irq = EMU_NO_IRQ;
  io_device.fd = -1;

  /* register VGAEmu */
  io_device.handler_name = "VGAEmu VGA Controller";
  io_device.start_addr = VGA_BASE;
  io_device.end_addr = VGA_BASE + 0x0f;
  port_register_handler(io_device, 0);

  /* register CRT Controller */
  io_device.handler_name = "VGAEmu CRT Controller";
  io_device.start_addr = CRTC_INDEX;
  io_device.end_addr = CRTC_DATA;
  port_register_handler(io_device, 0);

  /* register Input Status #1/Feature Control */
  io_device.handler_name = "VGAEmu Input Status #1/Feature Control";
  io_device.start_addr = INPUT_STATUS_1;
  io_device.end_addr = INPUT_STATUS_1;
  port_register_handler(io_device, 0);

  /*
   * Instead of single ports, we take them all - this way we can see
   * if something is missing. -- sw
   */
  if(vga.config.mono_support) {
    io_device.handler_name = "VGAEmu Mono/Hercules Card Range 0";
    io_device.start_addr = 0x3b0;
    io_device.end_addr = 0x3bb;
    port_register_handler(io_device, 0);

    io_device.handler_name = "VGAEmu Mono/Hercules Card Range 1";
    io_device.start_addr = 0x3bf;
    io_device.end_addr = 0x3bf;
    port_register_handler(io_device, 0);
  }
}

/*
 * DANG_BEGIN_FUNCTION vga_emu_init
 *
 * description:
 * vga_emu_init() must be called before using the VGAEmu functions.
 * It is only called from env/video/X.c::X_init() at the moment.
 * This function basically initializes the global variable `vga' and
 * allocates the VGA memory.
 *
 * It does in particular *not* map any memory into the range
 * 0xa0000 - 0xc0000, this is done as part of a VGA mode switch.
 *
 * There should be an accompanying vga_emu_done().
 *
 * arguments:
 * vedt - Pointer to struct describing the type of display we are actually
 *        attached to.
 *
 * DANG_END_FUNCTION
 *
 */

int vga_emu_init(int src_modes, ColorSpaceDesc *csd)
{
    vgaemu_display_type vedt;

    vedt.src_modes = src_modes;
    vedt.bits = csd->bits;
    vedt.r_mask = csd->r_mask;
    vedt.g_mask = csd->g_mask;
    vedt.b_mask = csd->b_mask;
    vedt.r_shift = csd->r_shift;
    vedt.g_shift = csd->g_shift;
    vedt.b_shift = csd->b_shift;
    vedt.r_bits = csd->r_bits;
    vedt.g_bits = csd->g_bits;
    vedt.b_bits = csd->b_bits;
    vbe_init(&vedt);

  return 0;
}

static int vga_emu_post_init(void);

int vga_emu_pre_init(void)
{
  int i;
  vga_mapping_type vmt = {0, 0, 0};

  /* clean it up - just in case */
  memset(&vga, 0, sizeof vga);

  vga.mode = vga.VGA_mode = vga.VESA_mode = 0;
  vga.mode_class = -1;
  vga.config.video_off = 1;
  vga.config.standard = 1;
  vga.mem.plane_pages = 0x10;	/* 16 pages = 64k */
  vga.dac.bits = 6;
  vga.inst_emu = 0;

  vga.config.mono_support = config.dualmon ? 0 : 1;

  open_mapping(MAPPING_VGAEMU);

  if(config.vgaemu_memsize)
    vga.mem.size = config.vgaemu_memsize << 10;
  else
    vga.mem.size = 1024 << 10;

  /* force 256k granularity to prevent possible problems
   * (with 4-plane-modes, to be precise)
   */
  vga.mem.size = (vga.mem.size + ((1 << 18) - 1)) & ~((1 << 18) - 1);
  vga.mem.pages = vga.mem.size >> 12;

  vga.mem.base = alloc_mapping(MAPPING_VGAEMU, vga.mem.size+ (1 << 12));
  if(vga.mem.base == MAP_FAILED) {
    vga_msg("vga_emu_init: not enough memory (%u k)\n", vga.mem.size >> 10);
    config.exitearly = 1;
    return 1;
  }
  vga.mem.scratch_page = vga.mem.base + vga.mem.size;
  memset(vga.mem.scratch_page, 0xff, 1 << 12);
  vga_msg("vga_emu_init: scratch_page at %p\n", vga.mem.scratch_page);

  vga.mem.lfb_base = NULL;
  if(config.X_lfb) {
    unsigned char *p = alias_mapping_high(MAPPING_VGAEMU,
				vga.mem.size, VGA_EMU_RW_PROT, vga.mem.base);
    if(p == MAP_FAILED) {
      vga_msg("vga_emu_init: not enough memory (%u k)\n", vga.mem.size >> 10);
      config.exitearly = 1;
      return 1;
    } else {
      vga.mem.lfb_base = p;
    }
  }

  if(vga.mem.lfb_base == NULL) {
    vga_msg("vga_emu_init: linear frame buffer (lfb) disabled\n");
  }

  if((vga.mem.dirty_map = (unsigned char *) malloc(vga.mem.pages)) == NULL) {
    vga_msg("vga_emu_init: not enough memory for dirty map\n");
    config.exitearly = 1;
    return 1;
  }
  dirty_all_video_pages();		/* all need an update */

  if(
    (vga.mem.prot_map0 = malloc(vgaemu_bios.pages + 0xc0 - 0xa0)) == NULL ||
    (vga.mem.prot_map1 = (unsigned char *) malloc(vga.mem.pages)) == NULL
  ) {
    vga_msg("vga_emu_init: not enough memory for protection map\n");
    config.exitearly = 1;
    return 1;
  }
  memset(vga.mem.prot_map0, 0xff, vgaemu_bios.pages + 0x20);
  memset(vga.mem.prot_map1, 0xff, vga.mem.pages);

  /*
   * vga.mem.map _must_ contain only valid mappings - otherwise _bad_ things will happen!
   * cf. vga_emu_protect()
   * -- sw
   */

  for(i = 0; i < VGAEMU_MAX_MAPPINGS; i++) vga.mem.map[i] = vmt;

  vga.mem.bank = vga.mem.bank_pages = 0;

  if(vga.mem.lfb_base != NULL) {
    memcheck_addtype('e', "VGAEMU LFB");
    register_hardware_ram('e', (uintptr_t)vga.mem.lfb_base, vga.mem.size);
  }

  return vga_emu_post_init();
}

static int vga_emu_post_init(void)
{
  int i;

  if(vga.mem.lfb_base != NULL) {
    dosaddr_t lfb_base = DOSADDR_REL(vga.mem.lfb_base);
    vga.mem.lfb_base_page = lfb_base >> 12;
    map_hardware_ram_manual((uintptr_t)vga.mem.lfb_base, lfb_base);
  }
  vga_emu_setup_mode_table();

  /*
   * Make the VGA-BIOS ROM read-only; some dirty programs try to write to the ROM!
   *
   * Note: Unknown instructions will cause the write protection to be removed.
   */
  vga_msg("vga_emu_init: protecting ROM area 0xc0000 - 0x%05x\n", (0xc0 + vgaemu_bios.pages) << 12);
  for(i = 0; i < vgaemu_bios.pages; i++) vga_emu_protect_page(0xc0 + i, RO);

  vgaemu_register_ports();

  /*
   * init the ROM-BIOS font (the VGA fonts are added in vbe_init())
   */
  MEMCPY_2DOS(GFX_CHARS, vga_rom_08, 128 * 8);
  SETIVEC(0x42, INT42HOOK_SEG, INT42HOOK_OFF);
  vbe_pre_init();

  vga_msg(
    "vga_emu_init: memory: %u kbyte at %p (lfb at %#x); %ssupport for mono modes\n",
    vga.mem.size >> 10, vga.mem.base,
    vga.mem.lfb_base_page << 12,
    vga.config.mono_support ? "" : "no "
  );

  return 0;
}


void vga_emu_done()
{
  /* We should probably do something here - but what ? -- sw */
}


#if DEBUG_UPDATE >= 1
static void print_dirty_map()
{
  int i, j;

  vga_msg("dirty_map[0 - 1024k] (0 = RO/NONE, 1 = RW = dirty)\n");
  for(j = 0; j < 256; j += 64) {
    v_printf("  ");
    for(i = 0; i < 64; i++) {
      if(!(i & 15))
        v_printf(" ");
      else if(!(i & 3))
        v_printf(".");
      v_printf("%d", (int) vga.mem.dirty_map[j + i]);
    }
    v_printf("\n");
  }
}
#endif


#if DEBUG_MAP >= 3
static char prot2char(unsigned char prot)
{
  if(prot < 0x9) return prot + '0';
  switch(prot) {
    case 0xff: return '-';
    case 0xfe: return 'e';
    case 0xfd: return 'i';
    case 0xfc: return 'f';
    case 0xfb: return 'a';
  }

  return '?';
}

static void print_prot_map()
{
  int i, j;

  vga_msg("prot_map: 0xa0000-0xc0000 & lfb\n");
  v_printf("  ");
  for(i = 0; i < 0x20; i++) {
    if(!(i & 15))
      v_printf(" ");
    else if(!(i & 3))
      v_printf(".");
    v_printf("%c", prot2char(vga.mem.prot_map0[i]));
  }
  v_printf("\n");
  for(j = 0; j < 256; j += 64){
    v_printf("  ");
    for(i = 0; i < 64; i++) {
      if(!(i & 15))
        v_printf(" ");
      else if(!(i & 3))
        v_printf(".");
      v_printf("%c", prot2char(vga.mem.prot_map1[j + i]));
    }
    v_printf("\n");
  }
}
#endif


/*
 * DANG_BEGIN_FUNCTION vga_emu_update
 *
 * description:
 * vga_emu_update() scans the VGA memory for dirty (= written to since last
 * update) pages and returns the changed area in *veut. See the definition
 * of vga_emu_update_type in env/video/vgaemu_inside.h for details.
 *
 * You will need to call this function repeatedly until it returns 0 to
 * grab all changes. You can specify an upper limit for the size of the
 * area that will be returned using `veut->max_max_len' and `veut->max_len'.
 * See the example in env/video/X.c how this works.
 *
 * If the return value of vga_emu_update() is >= 0, it is the number of changed
 * pages, -1 means there are still changed pages but the maximum update chunk size
 * (`veut->max_max_len') was exceeded.
 *
 * This function does in its current form not work for Hercules modes; it
 * does, however work for text modes, although this feature is currently
 * not used.
 *
 * arguments:
 * veut - A pointer to a vga_emu_update_type object holding all relevant info.
 *
 * DANG_END_FUNCTION
 *
 */
/* for threaded rendering we need to disable cycling as it can lead
 * to lock starvations */
static int __vga_emu_update(vga_emu_update_type *veut, unsigned display_start,
    unsigned display_end, int pos)
{
  int i, j;
  unsigned end_page, max_len;

  if (pos == -1)
    pos = display_start >> PAGE_SHIFT;
  end_page = (display_end - 1) >> PAGE_SHIFT;

  vga_deb_update("vga_emu_update: display = %d (page = %u) - %d (page = %u), update_pos = %d\n",
    display_start,
    display_start << PAGE_SHIFT,
    display_end,
    end_page,
    pos << PAGE_SHIFT
  );

#if DEBUG_MAP >= 3
  // just testing
  print_prot_map();
#endif

#if DEBUG_UPDATE >= 1
  print_dirty_map();
#endif

  for (i = j = pos; i <= end_page && ! vga.mem.dirty_map[i]; i++);
  if(i == end_page + 1) {
#if 0
    /* FIXME: this code not ready yet */
    for (; i < vga.mem.pages; i++) {
      if (vga.mem.dirty_map[i]) {
        vga.mem.dirty_map[i] = 0;
        _vga_emu_adjust_protection(i, 0, DEF_PROT, 0);
      }
    }
#endif
    return -1;
  }

  for(j = i; j <= end_page && vga.mem.dirty_map[j]; j++) {
    /* if display_start points to the middle of the page, dont clear
     * it immediately: it may still have dirty segments in the beginning,
     * which will be processed after mem wrap. */
    if (j == pos && pos == (display_start >> PAGE_SHIFT) &&
	(display_start & (PAGE_SIZE - 1)) && vga.mem.dirty_map[j] == 1)
      vga.mem.dirty_map[j] = 2;
    else
      vga.mem.dirty_map[j] = 0;
    if (!vga.mem.dirty_map[j])
      _vga_emu_adjust_protection(j, 0, DEF_PROT, 0);
  }

  vga_deb_update("vga_emu_update: update range: i = %d, j = %d\n", i, j);

  if(i == j)
    return -1;

  veut->update_start = i << 12;
  veut->update_len = (j - i) << 12;
  max_len = display_end - veut->update_start;
  if (veut->update_len > max_len) {
    assert(veut->update_len - max_len < PAGE_SIZE);
    veut->update_len = max_len;
  }

  vga_deb_update("vga_emu_update: update_start = %d, update_len = %d, update_pos = %d\n",
    veut->update_start,
    veut->update_len,
    pos << PAGE_SHIFT
  );

  return j;
}

int vga_emu_update(vga_emu_update_type *veut, unsigned display_start,
    unsigned display_end, int pos)
{
  int ret;
  pthread_mutex_lock(&prot_mtx);
  ret = __vga_emu_update(veut, display_start, display_end, pos);
  pthread_mutex_unlock(&prot_mtx);
  return ret;
}

void vga_emu_update_lock(void)
{
  pthread_mutex_lock(&mode_mtx);
}

void vga_emu_update_unlock(void)
{
  pthread_mutex_unlock(&mode_mtx);
}

void vga_emu_prot_lock(void)
{
  pthread_mutex_lock(&prot_mtx);
}

void vga_emu_prot_unlock(void)
{
  pthread_mutex_unlock(&prot_mtx);
}

/*
 * DANG_BEGIN_FUNCTION vgaemu_switch_plane
 *
 * description:
 * vgaemu_switch_plane() maps the specified plane.
 *
 * This function returns True on success and False on error, usually
 * indicating an invalid bank number.
 *
 * arguments:
 * plane (0..3) - The plane to map.
 *
 * DANG_END_FUNCTION
 *
 */

int vgaemu_switch_plane(unsigned plane)
{
  if(plane > 3) {
    vga_msg("vgaemu_switch_plane: plane %d invalid\n", plane);
    return False;
  }

  vga.mem.write_plane = vga.mem.read_plane = plane;
  vga.mem.bank = 0;

  if(vgaemu_map_bank()) {
    vga_msg("vgaemu_switch_plane: failed to access plane %u\n", plane);
    return False;
  }

  vga_deb_bank("vgaemu_switch_plane: switched to plane %u\n", plane);

  return True;
}


/*
 * DANG_BEGIN_FUNCTION vga_emu_switch_bank
 *
 * description:
 * vga_emu_switch_bank() is used to emulate video-bankswitching.
 *
 * This function returns True on success and False on error, usually
 * indicating an invalid bank number.
 *
 * arguments:
 * bank - The bank to switch to.
 *
 * DANG_END_FUNCTION
 *
 */

int vga_emu_switch_bank(unsigned bank)
{
  if((bank + 1) * vga.mem.bank_pages > vga.mem.pages) {
    vga_msg("vga_emu_switch_bank: invalid bank %d\n", bank);
    return False;
  }

  vga.mem.bank = bank;
  vga.mem.write_plane = vga.mem.read_plane = 0;

  if(vgaemu_map_bank()) {
    vga_msg("vga_emu_switch_bank: failed to access bank %u\n", bank);
    return False;
  }

  vga_deb_bank("vga_emu_switch_bank: switched to bank %u\n", bank);

  return True;
}


/*
 * Create a new video mode.
 *
 * This function appends a new mode entry with the required properties to the
 * internal mode list, if such a mode didn't already exist.
 *
 * It returns the number of modes actually created (0 or 1).
 *
 * NOTE: It does *not* set the buffer size and position!
 */

int vga_emu_setup_mode(vga_mode_info *vmi, int mode_index, unsigned width, unsigned height, unsigned color_bits)
{
  int i;

  for(i = 0; i < mode_index; i++) {
    if(vmi[i].width == width && vmi[i].height == height && vmi[i].color_bits == color_bits) {
      if(vmi[i].VESA_mode == -1) vmi[i].VESA_mode = -2;
      return 0;
    }
  }
  vmi += mode_index;

  vmi->VGA_mode = -1;
  vmi->VESA_mode = -2;
  vmi->mode_class = GRAPH;
  switch(vmi->color_bits = color_bits) {
    case  8: vmi->type = P8; break;
    case 15: vmi->type = P15; break;
    case 16: vmi->type = P16; break;
    case 24: vmi->type = P24; break;
    case 32: vmi->type = P32; break;
    default: return 0;
  }
  vmi->width = width;
  vmi->height = height;
  vmi->char_width = 8;
  vmi->char_height = height >= 400 ? 16 : 8;
  if(vmi->char_height == 16 && (height & 15) == 8) vmi->char_height = 8;
  vmi->text_width = width / vmi->char_width;
  vmi->text_height = height / vmi->char_height;

  vga_msg(
    "vga_emu_setup_mode: creating VESA mode %d x %d x %d\n",
    width, height, color_bits
  );

  return 1;
}


/*
 * Sets up the internal mode table. This table can then be accessed
 * via vga_emu_find_mode().
 *
 * It extends the table vga_mode_table[] found in env/video/vgaemu_modelist.h
 * using the mode descriptions in vgaemu_simple_mode_list[][] and
 * user-defined modes in dosemu.conf (in this order). The location and size of
 * this new mode table is then stored in the global variable vgaemu_bios.
 *
 * The memory that was allocated for config.vesamode_list during the parsing
 * of dosemu.conf is freed.
 *
 * Note: a new mode is added only if it didn't already exist.
 *
 */

static void vga_emu_setup_mode_table()
{
  int vbe_modes = (sizeof vga_mode_table) / (sizeof *vga_mode_table);
  int vbe_num = VBE_FIRST_OEM_MODE;
  int last_mode = 0;
  vga_mode_info *vmi_end, *vmi;
  vesamode_type *vmt0, *vmt = config.vesamode_list;
  int vesa_x_modes = 0;
  int i, j, k;

  vgaemu_bios.vbe_last_mode = 0;

  while(vmt) {
    vesa_x_modes += vmt->color_bits ? 1 : 5;	/* 8, 15, 16, 24, 32 bit */
    vmt = vmt->next;
  }
  vesa_x_modes += (sizeof vgaemu_simple_mode_list) / (sizeof *vgaemu_simple_mode_list) * 5;

  vmi = malloc((sizeof *vga_mode_table) * (vbe_modes + vesa_x_modes));

  if(vmi == NULL) {
    vgaemu_bios.vga_mode_table = vga_mode_table;
    vgaemu_bios.mode_table_length = vbe_modes;
    vgaemu_bios.vbe_last_mode = 0;
    return;
  }

  memcpy(vmi, vga_mode_table, sizeof vga_mode_table);

  vgaemu_bios.vga_mode_table = vmi;

  for(i = 0; i < (sizeof vgaemu_simple_mode_list) / (sizeof *vgaemu_simple_mode_list); i++) {
    j = vgaemu_simple_mode_list[i][0];
    k = vgaemu_simple_mode_list[i][1];
    vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, j, k, 8);
    vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, j, k, 15);
    vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, j, k, 16);
    vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, j, k, 24);
    vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, j, k, 32);
  }

  while((vmt0 = vmt = config.vesamode_list) != NULL) {
    if(vmt != NULL) while(vmt->next != NULL) { vmt0 = vmt; vmt = vmt->next; }
    if(vmt != NULL) {
      if(
        vmt->width && vmt->width < (1 << 15) &&
        vmt->height && vmt->height < (1 << 15) && (
          vmt->color_bits == 0 || vmt->color_bits == 8 ||
          vmt->color_bits == 15 || vmt->color_bits == 16 ||
          vmt->color_bits == 24 || vmt->color_bits == 32
        )
      ) {
        if(vmt->color_bits) {
          vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, vmt->width, vmt->height, vmt->color_bits);
        }
        else {
          vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, vmt->width, vmt->height, 8);
          vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, vmt->width, vmt->height, 15);
          vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, vmt->width, vmt->height, 16);
          vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, vmt->width, vmt->height, 24);
          vbe_modes += vga_emu_setup_mode(vmi, vbe_modes, vmt->width, vmt->height, 32);
        }
      }
      else {
        vga_msg(
          "vga_emu_setup_mode_table: invalid VESA mode %d x %d x %d\n",
          vmt->width, vmt->height, vmt->color_bits
        );
      }
      free(vmt);
      if(vmt == config.vesamode_list)
        config.vesamode_list = NULL;
      else
        vmt0->next = NULL;
    }
  }

  vgaemu_bios.mode_table_length = vbe_modes;

  for(vmi_end = vmi + vbe_modes; vmi < vmi_end; vmi++) {
    if(vmi->VESA_mode == -2) vmi->VESA_mode = vbe_num++;
    if(vmi->VESA_mode > last_mode) last_mode = vmi->VESA_mode;
  }

  vgaemu_bios.vbe_last_mode = last_mode;

  /*
   * Fill in appropriate buffer sizes.
   * We used to let the user specify this in vgaemu_modelist.h but to force
   * consistency with the Miscellaneous Register (index 6) in the Graphics
   * Controller we do it here. -- sw
   */
  for(vmi = NULL; (vmi = vga_emu_find_mode(-1, vmi)); ) {
    if((vmi->VGA_mode >= 0 && vmi->VGA_mode <= 7) || vmi->mode_class == TEXT) {
      vmi->buffer_start = vmi->type == TEXT_MONO ? 0xb000 : 0xb800;
      vmi->buffer_len = 32;
    }
    else {
      vmi->buffer_start = 0xa000;
      vmi->buffer_len = 64;
    }
  }
}


/*
 * DANG_BEGIN_FUNCTION vga_emu_find_mode
 *
 * description:
 * Searches a video mode with the requested mode number.
 *
 * The search starts with the mode *after* the mode `vmi' points to.
 * If `vmi' == NULL, starts at the beginning of the internal mode table.
 * `mode' may be a standard VGA mode number (0 ... 0x7f) or a
 * VESA mode number (>= 0x100). The mode number may have its don't-clear-bit
 * (bit 7 or bit 15) or its use-lfb-bit (bit 14) set.
 * The special mode number -1 will match any mode and may be used to
 * scan through the whole table.
 *
 * Returns NULL if no mode was found and a pointer into the mode table
 * otherwise. The returned pointer is a suitable argument for subsequent
 * calls to this function.
 *
 * You should (and can) access the mode table only through this function.
 *
 * arguments:
 * mode   - video mode.
 * vmi    - pointer into internal mode list
 *
 * DANG_END_FUNCTION
 *
 */

vga_mode_info *vga_emu_find_mode(int mode, vga_mode_info* vmi)
{
  vga_mode_info *vmi_end = vgaemu_bios.vga_mode_table + vgaemu_bios.mode_table_length;

  if(mode != -1) {
    mode &= 0x3fff;
    if(mode < 0x100) mode &= ~0x80;
  }

  if(vmi == NULL) {
    vmi = vgaemu_bios.vga_mode_table;
  }
  else {
    if(++vmi >= vmi_end) return NULL;
  }

  if(mode == -1) return vmi;

  for(; vmi < vmi_end; vmi++) {
    if(vmi->VGA_mode == mode || vmi->VESA_mode == mode) return vmi;
  }

  return NULL;
}


/*
 * DANG_BEGIN_FUNCTION vga_emu_setmode
 *
 * description:
 * Set a video mode.
 *
 * Switches to `mode' with text sizes `width' and `height' or (if no such
 * mode was found) at least `width' and `height'.
 *
 * arguments:
 * mode   - The new video mode.
 * width  - Number of text columns.
 * height - Number of text rows.
 *
 * DANG_END_FUNCTION
 *
 */

static int __vga_emu_setmode(int mode, int width, int height)
{
  unsigned u = -1;
  int i;
  vga_mode_info *vmi = NULL, *vmi2 = NULL;

  vga_msg("vga_emu_setmode: requested mode: 0x%02x (%d x %d)\n", mode, width, height);

  while((vmi = vga_emu_find_mode(mode, vmi))) {
    if(vmi->mode_class == GRAPH || (vmi->text_width == width && vmi->text_height == height)) break;
  }

  if(vmi == NULL) {
    /* Play it again, Sam!
     * This is when we can't find the textmode with the appropriate sizes.
     * Use the best matching text mode
     */

    while((vmi = vga_emu_find_mode(mode, vmi))) {
      if(vmi->text_width >= width && vmi->text_height >= height && vmi->text_width * vmi->text_height < u) {
        u = vmi->text_width * vmi->text_height;
        vmi2 = vmi;
      }
    }
    if(vmi == NULL && Video->setmode == NULL)
      vmi2 = vga_emu_find_mode(mode, vmi);
    vmi = vmi2;
  }

  if(vmi == NULL) {	/* no mode found */
    vga_msg("vga_emu_setmode: no mode 0x%02x found\n", mode);
    return False;
  }

  if (vmi->buffer_start == 0xa000 && config.umb_a0) {
    error("VGA: avoid touching a000 as it is used for UMB\n");
    return False;
  }
  if (vmi->buffer_start == 0xb000 && config.umb_b0) {
    error("VGA: avoid touching b000 as it is used for UMB\n");
    return False;
  }

  vga_msg("vga_emu_setmode: mode found in %s run\n", vmi == vmi2 ? "second" : "first");

  vga_msg("vga_emu_setmode: mode 0x%02x, (%d x %d x %d, %d x %d, %d x %d, %dk at 0x%04x)\n",
    mode, vmi->width, vmi->height, vmi->color_bits, vmi->text_width, vmi->text_height,
    vmi->char_width, vmi->char_height, vmi->buffer_len, vmi->buffer_start
  );

  vga.mode = mode;
  vga.VGA_mode = vmi->VGA_mode;
  vga.VESA_mode = vmi->VESA_mode;
  vga.mode_class = vmi->mode_class;
  vga.mode_type = vmi->type;
  vga.width = vmi->width;
  vga.height = vmi->height;
  vga.line_compare = vmi->height;
  vga.scan_len = (vmi->width + 3) & ~3;	/* dword aligned */
  vga.text_width = vmi->text_width;
  vga.text_height = vmi->text_height;
  vga.char_width = vmi->char_width;
  vga.char_height = vmi->char_height;
  vga.color_bits = vmi->color_bits;
  vga.pixel_size = vmi->color_bits;
  if(vga.pixel_size > 8) {
    vga.pixel_size = (vga.pixel_size + 7) & ~7;		/* assume byte alignment for these modes */
    vga.scan_len *= vga.pixel_size >> 3;
  }
  vga.inst_emu = ((vga.mode_type==PL4 || vga.mode_type==PL2) ? EMU_ALL_INST : 0);

  vga_msg("vga_emu_setmode: scan_len = %d\n", vga.scan_len);
  i = vga.scan_len;

  vga.config.standard = vga.VGA_mode >= 0 && vga.VGA_mode <= 0x13 ? 1 : 0;
  vga.config.mono_port =
  vga.config.video_off = 0;

  vga.reconfig.mem = vga.reconfig.display =
  vga.reconfig.dac = vga.reconfig.power =
  vga.reconfig.re_init = 0;
  vga.mem.planes = 1;
  if(vga.mode_type == PL4 || vga.mode_type == NONCHAIN4) {
    vga.scan_len >>= 3;
    vga.mem.planes = 4;
  }
  if(vga.mode_type == PL1 || vga.mode_type == PL2) vga.scan_len >>= 3;
  if(vga.mode_type == CGA) vga.scan_len >>= 4 - vga.pixel_size;		/* 1 or 2 bits */
  vga.mem.write_plane = vga.mem.read_plane = 0;

  if(vga.mode_class == TEXT) vga.scan_len = vga.text_width << 1;

  vga.latch[0] = vga.latch[1] = vga.latch[2] = vga.latch[3] = 0;

  vga.display_start = 0;

  if(i != vga.scan_len) {
    vga_msg("vga_emu_setmode: scan_len changed to %d\n", vga.scan_len);
  }

  /*
   * Clear the whole VGA memory. In addition, in text modes,
   * set the first 32k to 0x0720...
   * -- 1998/04/04 sw
   */

  if(!(mode & 0x8000) && !(mode < 0x100 && (mode & 0x80))) {
    if(vga.mode_class != TEXT && vga.mem.base) {
      /* should be moved to BIOS */
      memset((void *)vga.mem.base, 0, vga.mem.size);
    }
  }

  dirty_all_video_pages();
  /* Put the mapping for the range 0xa0000 - 0xc0000 into some intial state:
   * the scratch page is mapped RW all over the place.
   */
  vgaemu_reset_mapping();

  vga.mem.bank = 0;
  vga.mem.bank_pages = vmi->buffer_len >> 2;

  /*
   * As we don't have 'planes' in the VGA's sense, we *emulate* them by
   * mapping different parts of our memory.
   * This has *nothing* to do with memory banks. It just *looks* similar.
   *
   * Planes are only really needed for VGA 16 and 4 color modes and for
   * non-chain4 256 color modes. And for accessing fonts in text modes.
   *
   * It is (for now) implicitly assumed at various places (the line below
   * being one, for example), that a plane is 64k in size, giving a total
   * of 256k 100%-compatible VGA memory.
   *
   * We could make this a little bit more flexible, but the only reason
   * for this would be an emulation of the 1024x768 16 color mode.
   * Which would be of no particular value anyway. -- sw
   */
  vga.mem.plane_pages = 0x10;	/* 16 pages = 64k */

  vga.buffer_seg = vmi->buffer_start;
  vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page = vmi->buffer_start >> 8;
  vgaemu_map_bank();

  vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page = 0;
  vga.mem.map[VGAEMU_MAP_LFB_MODE].first_page = 0;
  vga.mem.map[VGAEMU_MAP_LFB_MODE].pages = 0;
  vga.mem.wrap = vmi->buffer_len * 1024;
  // unmap ???

  /* In Super-VGA modes, do *not* wrap memory at 256k */
  if(vga.color_bits >= 8 && (vga.mode & 0xffff) > 0x13) {
    vga.mem.wrap = vga.mem.size;
    if(vga.mem.lfb_base_page) {
      vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page = vga.mem.lfb_base_page;
      vga.mem.map[VGAEMU_MAP_LFB_MODE].pages = vga.mem.pages;
      vga_emu_map(VGAEMU_MAP_LFB_MODE, 0);	/* map the VGA memory to LFB */
    }
  }

  vga_msg("vga_emu_setmode: setting up components...\n");

  DAC_init();
  Attr_init();
  Seq_init();
  CRTC_init();
  GFX_init();
  Misc_init();
  Herc_init();

  vgaemu_adj_cfg(CFG_SEQ_ADDR_MODE, 1);
  vgaemu_adj_cfg(CFG_CRTC_ADDR_MODE, 1);
  vgaemu_adj_cfg(CFG_CRTC_WIDTH, 1);
  vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 1);
  vgaemu_adj_cfg(CFG_CRTC_LINE_COMPARE, 1);
#if 0
  /* need to fix vgaemu before this is possible */
  vgaemu_adj_cfg(CFG_MODE_CONTROL, 1);
#endif

  vga_msg("vga_emu_setmode: mode initialized\n");

  return True;
}

int vga_emu_setmode(int mode, int width, int height)
{
  int ret;
  pthread_mutex_lock(&mode_mtx);
  ret = __vga_emu_setmode(mode, width, height);
  pthread_mutex_unlock(&mode_mtx);
  render_update_vidmode();
  return ret;
}

int vgaemu_map_bank()
{
  int i, first;
#if 0
  int j, k0, k1;
#endif

  if((vga.mem.bank + 1) * vga.mem.bank_pages > vga.mem.pages) {
    vga_msg("vgaemu_map_bank: invalid bank %d\n", vga.mem.bank);
    return False;
  }

  if(vga.mem.write_plane > 3) {
    vga_msg("vgaemu_map_bank: invalid plane %d\n", vga.mem.write_plane);
    return False;
  }

  vga.mem.map[VGAEMU_MAP_BANK_MODE].pages = vga.mem.bank_pages;
  vga.mem.bank_base = vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12;
  vga.mem.bank_len = vga.mem.map[VGAEMU_MAP_BANK_MODE].pages << 12;

  if(vga.mem.write_plane) {
    first = vga.mem.plane_pages * vga.mem.write_plane;
  }
  else {
    first = vga.mem.bank_pages * vga.mem.bank;
  }

#if 0
  /*
   * this is too slow !!! -- sw
   */
  k0 = vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page;
  k1 = k0 + vga.mem.map[VGAEMU_MAP_BANK_MODE].pages;
  for(j = 0xa0; j < 0xc0; j++) {
    if(j < k0 || j >= k1) {
      i = vgaemu_unmap(j);
      if(i) {
        vga_msg("vgaemu_map_bank: failed to unmap page at 0x%x; reason: %d\n", j << 12, i);
      }
      else {
        vga_deb2_bank("vgaemu_map_bank: page at 0x%x unmapped\n", j << 12);
      }
    }
  }
#endif

  i = vga_emu_map(VGAEMU_MAP_BANK_MODE, first);

  if(i) {
    vga_msg(
      "vgaemu_map_bank: failed to map %uk (ofs %uk) at 0x%x; reason: %d\n",
      vga.mem.map[VGAEMU_MAP_BANK_MODE].pages << 2,
      first << 2,
      vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12,
      i
    );
  }
  else {
    vga_deb_bank(
      "vgaemu_map_bank: mapped %uk (ofs %uk) at 0x%x\n",
      vga.mem.map[VGAEMU_MAP_BANK_MODE].pages << 2,
      first << 2,
      vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12
    );
  }

  return i;
}


/*
 * DANG_BEGIN_FUNCTION vga_emu_set_textsize
 *
 * description:
 * Sets the text mode resolution. Typically called after
 * a font change.
 *
 * arguments:
 * width  - Number of text columns.
 * height - Number of text rows.
 *
 * DANG_END_FUNCTION
 *
 */

int vga_emu_set_textsize(int width, int height)
{
  if(vga.mode_class != TEXT) return 1;

  vga.text_width = width;
  vga.text_height = height;
  vga.scan_len = vga.text_width << 1;

  return 1;
}


/*
 * DANG_BEGIN_FUNCTION dirty_all_video_pages
 *
 * description:
 * Marks the whole VGA memory as modified.
 *
 * DANG_END_FUNCTION
 *
 */

void dirty_all_video_pages()
{
  pthread_mutex_lock(&prot_mtx);
  if (vga.mem.dirty_map)
    memset(vga.mem.dirty_map, 1, vga.mem.pages);
  pthread_mutex_unlock(&prot_mtx);
}

static void _vgaemu_dirty_page(int page, int dirty)
{
  int k;

  if (page >= vga.mem.pages) {
    dosemu_error("vgaemu: page out of range, %i (%i)\n", page, vga.mem.pages);
    return;
  }
  v_printf("vgaemu: set page %i %s (%i)\n", page, dirty ? "dirty" : "clean",
      vga.mem.dirty_map[page]);
  /* prot_mtx should be locked by caller */
  vga.mem.dirty_map[page] = dirty;

  if(vga.mem.planes == 4) {	/* MODE_X or PL4 */
    page &= ~0x30;
    for(k = 0; k < vga.mem.planes; k++, page += 0x10)
      vga.mem.dirty_map[page] = dirty;
  }

  if(vga.mode_type == PL2) {
    /* it's actually 4 planes, but we let everyone believe it's a 1-plane mode */
    page &= ~0x30;
    vga.mem.dirty_map[page] = dirty;
    page += 0x20;
    vga.mem.dirty_map[page] = dirty;
  }

  if(vga.mode_type == CGA) {
    /* CGA uses two 8k banks  */
    page &= ~0x2;
    vga.mem.dirty_map[page] = dirty;
    page += 0x2;
    vga.mem.dirty_map[page] = dirty;
  }

  if(vga.mode_type == HERC) {
    /* Hercules uses four 8k banks  */
    page &= ~0x6;
    vga.mem.dirty_map[page] = dirty;
    page += 0x2;
    vga.mem.dirty_map[page] = dirty;
    page += 0x2;
    vga.mem.dirty_map[page] = dirty;
    page += 0x2;
    vga.mem.dirty_map[page] = dirty;
  }
}

void vgaemu_dirty_page(int page, int dirty)
{
  pthread_mutex_lock(&prot_mtx);
  _vgaemu_dirty_page(page, dirty);
  pthread_mutex_unlock(&prot_mtx);
}

int vgaemu_is_dirty(void)
{
  int i, ret = 0;
  if (vga.color_modified)
    return 1;
  pthread_mutex_lock(&prot_mtx);
  if (vga.mem.dirty_map) {
    for (i = 0; i < vga.mem.pages; i++) {
      if (vga.mem.dirty_map[i]) {
        ret = 1;
        break;
      }
    }
  }
  pthread_mutex_unlock(&prot_mtx);
  return ret;
}

/*
 * DANG_BEGIN_FUNCTION dirty_all_vga_colors
 *
 * description:
 * Marks all colors as changed.
 *
 * DANG_END_FUNCTION
 *
 */

void dirty_all_vga_colors()
{
  int i;

  vga.color_modified = True;

  for(i = 0; i < 0x10; i++) vga.attr.dirty[i] = True;		/* palette regs */
  vga.attr.dirty[0x11] = True;					/* border color */

  for(i = 0; i < 256; i++) vga.dac.rgb[i].dirty = True;
}


/*
 * DANG_BEGIN_FUNCTION changed_vga_colors
 *
 * description:
 * Checks DAC and Attribute Controller to find all colors with
 * changed RGB-values.
 * Returns number of changed colors.
 * Note: the list _must_ be large enough, that is, have at least
 * min(256, (1 << vga.pixel_size)) entries!
 *
 * arguments:
 * de - list of DAC entries to store changed colors in
 *
 * DANG_END_FUNCTION
 *
 * Note: vga.dac.rgb[i].index holds the dirty flag but in the returned
 * DAC entries .index is the VGA color number.
 *
 */

int changed_vga_colors(void (*upd_func)(DAC_entry *, int, void *), void *arg)
{
  DAC_entry de;
  int i, j, k;
  unsigned cols;
  unsigned char a, m;

#if 0	/* change attr.c first! */
  if(!vga.color_modified) return 0;
#endif

  cols = 1 << vga.pixel_size;
  if(cols > 256) cols = 256;	/* We do not really support > 8 bit palettes. */

 /*
  * Modes with more than 16 colors will not use palette regs.
  * Note: index = dirty flag!
  */
  if(vga.pixel_size > 4) {
    for(i = j = 0; i < cols; i++) {
      if(vga.dac.rgb[i].dirty == True) {
        de = vga.dac.rgb[i];
        m = vga.dac.pel_mask;
        de.r &= m; de.g &= m; de.b &= m;
        if (upd_func)
          upd_func(&de, i, arg);
        j++;
        vga.dac.rgb[i].dirty = False;
        vga_deb_col("changed_vga_colors: color 0x%02x\n", i);
      }
    }
  }
  else {
    for(i = j = 0; i < cols; i++) {

      /* ATTR_MODE_CTL   == 0x10 */
      /* ATTR_COL_SELECT == 0x14 */

      k = i;
      if(vga.mode_type == PL2) if(k >= 2) k += 2;
      if(vga.mode_type == HERC && k) k = 15;	/* use palette 15 for white */

      /*
       * Supply bits 6-7 resp. 4-7 from Attribute Controller;
       * see VGADOC for details.
       * Note: palette entries have only 6 bits.
       */
      if(vga.attr.data[0x10] & 0x80) {
        a = (vga.attr.data[k] & 0x0f) |
            ((vga.attr.data[0x14] & 0x0f) << 4);	/* bits 7-5 */
      }
      else {
        a = vga.attr.data[k] |
            ((vga.attr.data[0x14] & 0x0c) << 4);	/* bits 7-6 */
      }

      if(
        vga.dac.rgb[a].dirty == True ||
        vga.attr.dirty[k] == True
      ) {
        vga.attr.dirty[k] = False;
        de = vga.dac.rgb[a];
        m = vga.dac.pel_mask;
        de.r &= m; de.g &= m; de.b &= m;
        if (upd_func)
          upd_func(&de, i, arg);
        vga.dac.rgb[a].dirty = False;
        j++;
        vga_deb_col(
          "changed_vga_colors: color 0x%02x, pal 0x%02x, rgb 0x%02x 0x%02x 0x%02x\n",
          i, a,
          (unsigned) vga.dac.rgb[a].r,
          (unsigned) vga.dac.rgb[a].g,
          (unsigned) vga.dac.rgb[a].b
        );
      }
    }
  }

  vga.color_modified = False;
  return j;
}


/*
 * DANG_BEGIN_FUNCTION vgaemu_adj_cfg
 *
 * description:
 * Adjust VGAEmu according to VGA register changes.
 *
 * DANG_END_FUNCTION
 *
 */

void vgaemu_adj_cfg(unsigned what, unsigned msg)
{
  unsigned u, u0, u1;
  static char *txt1[] = { "byte", "odd/even (word)", "chain4 (dword)" };
  static char *txt2[] = { "byte", "word", "dword" };

  switch(what) {
    case CFG_SEQ_ADDR_MODE:
      u0 = vga.seq.addr_mode;
      u = vga.seq.data[4] & 4 ? 0 : 1;
      u = vga.seq.data[4] & 8 ? 2 : u;
      vga.seq.addr_mode = u;
      u1 = vga.seq.addr_mode == 0 ? 4 : 1;
      if(u1 != vga.mem.planes) {
        vga.mem.planes = u1; vga.reconfig.mem = 1;
        vga_msg("vgaemu_adj_cfg: mem reconfig (%u planes)\n", u1);
        if (vga.mem.planes) {
          v_printf("Seq_write_value: instemu on\n");
          vga.inst_emu = EMU_ALL_INST;
        } else {
          v_printf("Seq_write_value: instemu off\n");
          vga.inst_emu = 0;
        }
        vgaemu_map_bank();	// update page protection
      }
      if(msg || u != u0) vga_msg("vgaemu_adj_cfg: seq.addr_mode = %s\n", txt1[u]);
      if (vga.mode_class == TEXT && vga.width < 2048) {
        int horizontal_display_end = vga.crtc.data[0x1] + 1;
	int horizontal_blanking_start = vga.crtc.data[0x2] + 1;
	int multiplier = 9 - (vga.seq.data[1] & 1);
	int width = min(horizontal_display_end, horizontal_blanking_start) *
	  multiplier;
	if ((vga.width != width) || (vga.char_width != multiplier)) {
	  vga.width = width;
	  vga.char_width = multiplier;
	  vga.text_width = horizontal_display_end;
	  vga.reconfig.display = 1;
	  vga.reconfig.re_init = 1;
	}
      }
    break;

    case CFG_CRTC_ADDR_MODE:
      if (vga.color_bits > 8)
	return;
      u0 = vga.crtc.addr_mode;
      u1 = vga.scan_len;
      u = vga.crtc.data[0x17] & 0x40 ? 0 : 1;
      u = vga.crtc.data[0x14] & 0x40 ? 2 : u;
      if (vga.scan_len > (255 << (u + 1)))
	return;
      if (vga.scan_len & ((1 << (u + 1)) - 1))
	return;
      vga.crtc.addr_mode = u;
      if (vga.config.standard)
	vga.display_start = (vga.crtc.data[0x0d] + (vga.crtc.data[0x0c] << 8)) <<
	      vga.crtc.addr_mode;
      /* this shift should really be a rotation, depending on mode control bit 5 */
      vga.crtc.cursor_location =  (vga.crtc.data[0x0f] + (vga.crtc.data[0x0e] << 8)) <<
              vga.crtc.addr_mode;
      vga.scan_len = vga.crtc.data[0x13] << (vga.crtc.addr_mode + 1);
      if (vga.scan_len == 0) vga.scan_len = 160;
      if (u1 != vga.scan_len) vga.reconfig.mem = 1;
      if(msg || u != u0) vga_msg("vgaemu_adj_cfg: crtc.addr_mode = %s, scan_len = %d\n",
	txt2[u], vga.scan_len);
    break;

    case CFG_CRTC_HEIGHT:
    {
      int vertical_total;
      int vertical_retrace_start;
      int vertical_retrace_end;
      int vertical_display_end;
      int vertical_multiplier;
      int vertical_blanking_start;
      int vertical_blanking_end;
      int char_height;
      int height;

      if (vga.height > 1024)
	return;

      vertical_total =
	      vga.crtc.data[0x6] +
	      ((vga.crtc.data[0x7] & 0x1) << (8 - 0)) +
	      ((vga.crtc.data[0x7] & 0x20) << (9 - 5));
      vertical_retrace_start =
	      vga.crtc.data[0x10] +
	      ((vga.crtc.data[0x7] & 0x4) << (8 - 2)) +
	      ((vga.crtc.data[0x7] & 0x80) << (9 - 7));
      vertical_retrace_end = vga.crtc.data[0x11]  & 0x0F;
      vertical_display_end =
	      vga.crtc.data[0x12] +
	      ((vga.crtc.data[0x7] & 0x2) << (8 - 1)) +
	      ((vga.crtc.data[0x7] & 0x40) << (9 - 6));
      vertical_blanking_start =
	      vga.crtc.data[0x15] +
	      ((vga.crtc.data[0x7] & 0x8) << (8 - 3)) +
	      ((vga.crtc.data[0x9] & 0x20) << (9 - 5));
      vertical_blanking_end =
	      vga.crtc.data[0x16] & 0x7F;
      char_height = (vga.crtc.data[0x9] & 0x1f) + 1;
      vertical_multiplier = char_height << ((vga.crtc.data[0x9] & 0x80) >> 7);
      /* see VGADOC: CGA is special for reg 9 */
      if(vga.mode_type == CGA) vertical_multiplier = char_height;
      height = (min(vertical_blanking_start, vertical_display_end) + 1) /
	vertical_multiplier;
      vga_msg("vgaemu_adj_cfg: vertical_total = %d\n", vertical_total);
      vga_msg("vgaemu_adj_cfg: vertical_retrace_start = %d\n", vertical_retrace_start);
      vga_msg("vgaemu_adj_cfg: vertical_retrace_end = %d\n", vertical_retrace_end);
      vga_msg("vgaemu_adj_cfg: vertical_blanking_start = %d\n", vertical_blanking_start);
      vga_msg("vgaemu_adj_cfg: vertical_blanking_end = %d\n",   vertical_blanking_end);
      vga_msg("vgaemu_adj_cfg: vertical_display_end = %d\n", vertical_display_end);
      vga_msg("vgaemu_adj_cfg: vertical_multiplier = %d\n", vertical_multiplier);
      vga_msg("vgaemu_adj_cfg: height = %d\n", height);
      if (vga.line_compare != vga.crtc.line_compare / vertical_multiplier) {
        vga.line_compare = vga.crtc.line_compare / vertical_multiplier;
        dirty_all_video_pages();
      }
      if (vga.mode_class == TEXT) {
        height *= char_height;
      } else {
        char_height = height / 25;
        if (char_height >= 16)
          char_height = 16;
        else if (char_height >= 14)
          char_height = 14;
        else
          char_height = 8;
      }
      /* By Eric (eric@coli.uni-sb.de):                        */
      /* Required for 80x100 CGA "text graphics" with 8x2 font */
      if (vga.height != height || vga.char_height != char_height) {
        vga.height = height;
	vga.text_height = height / char_height;
        vga.char_height = char_height;
        vga_msg("vgaemu_adj_cfg: text_height=%d height=%d char_height=%d\n",
                height, vertical_display_end+1, vga.char_height);
        vga.reconfig.display = 1;
	vga.reconfig.re_init = 1;
      }
      break;
    }
    case CFG_CRTC_WIDTH:
    {
      int horizontal_total;
      int horizontal_display_end;
      int horizontal_retrace_start;
      int horizontal_retrace_end;
      int horizontal_blanking_start;
      int horizontal_blanking_end;
      int multiplier;
      int width;

      if (vga.width >= 2048)
	return;

      /* everything below is in characters */
      horizontal_total = vga.crtc.data[0x0] +5;
      horizontal_display_end = vga.crtc.data[0x1] + 1;
      horizontal_blanking_start = vga.crtc.data[0x2] + 1;
      horizontal_blanking_end = vga.crtc.data[0x3] & 0xF;
      horizontal_retrace_start = vga.crtc.data[0x4];
      horizontal_retrace_end = vga.crtc.data[0x5] & 0xF;
      if (vga.mode_class == TEXT) {
        multiplier = 9 - (vga.seq.data[1] & 1);
      } else {
	multiplier = (vga.color_bits == 8 && vga.config.standard) ? 4 : 8;
      }
      if (vga.width % multiplier != 0) /* special user defined mode? */
	return;
      width = min(horizontal_display_end, horizontal_blanking_start) *
	multiplier;
      vga_msg("vgaemu_adj_cfg: horizontal_total = %d\n", horizontal_total);
      vga_msg("vgaemu_adj_cfg: horizontal_retrace_start = %d\n", horizontal_retrace_start);
      vga_msg("vgaemu_adj_cfg: horizontal_retrace_end = %d\n", horizontal_retrace_end);
      vga_msg("vgaemu_adj_cfg: horizontal_blanking_start = %d\n", horizontal_blanking_start);
      vga_msg("vgaemu_adj_cfg: horizontal_blanking_end = %d\n", horizontal_blanking_end);
      vga_msg("vgaemu_adj_cfg: horizontal_display_end = %d\n", horizontal_display_end);
      vga_msg("vgaemu_adj_cfg: multiplier = %d\n", multiplier);
      vga_msg("vgaemu_adj_cfg: width = %d\n", width);
      if ((vga.width != width) || (vga.char_width != multiplier)) {
        vga.width = width;
        vga.char_width = (multiplier >= 8) ? multiplier : 8;
	vga.text_width = min(horizontal_display_end, horizontal_blanking_start);
        vga.reconfig.display = 1;
	vga.reconfig.re_init = 1;
      }
      break;
    }
    case CFG_CRTC_LINE_COMPARE:
    {
      int vertical_multiplier;

      if (vga.height >= 1024)
	return;

      vga.crtc.line_compare =
              vga.crtc.data[0x18] +
              ((vga.crtc.data[0x7] & 0x10) << (8 - 4)) +
              ((vga.crtc.data[0x9] & 0x40) << (9 - 6));
      vertical_multiplier = ((vga.crtc.data[0x9] & 0x1F) +1) <<
	      ((vga.crtc.data[0x9] & 0x80) >> 7);
      vga_msg("vgaemu_adj_cfg: line_compare = %d\n", vga.crtc.line_compare);
      if (vga.line_compare != vga.crtc.line_compare / vertical_multiplier) {
        vga.line_compare = vga.crtc.line_compare / vertical_multiplier;
        dirty_all_video_pages();
      }
      break;
    }
    case CFG_MODE_CONTROL:
    {
      int oldclass, old_color_bits;

      if (vga.color_bits > 8)
	return;

      oldclass = vga.mode_class;
      vga.mode_class = (vga.gfx.data[6] & 1) ? GRAPH : TEXT;
      vga.pixel_size = 4;
      if (vga.mode_class == TEXT) {
	vga.mode_type = vga.config.mono_port ? TEXT_MONO : TEXT;
      } else {
	unsigned char gdata = vga.gfx.data[5];
	unsigned char cdata = vga.crtc.data[0x17];
	unsigned char adata = vga.attr.data[0x12];

	vga.pixel_size = 1;
	if (gdata & 0x40) {
	  vga.mode_type = P8;
	  vga.pixel_size = 8;
	} else if (gdata & 0x20) {
	  vga.mode_type = CGA;
	  vga.pixel_size = 2;
	} else if (!(cdata & 2)) {
	  vga.mode_type = HERC;
	} else if (adata == 5) {
	  vga.mode_type = PL2;
	  vga.pixel_size = 2;
	} else if (adata == 1) {
	  vga.mode_type = (cdata & 1) ? PL1 : CGA;
	} else {
	  vga.mode_type = PL4;
	  vga.pixel_size = 4;
	}
      }
      old_color_bits = vga.color_bits;
      vga.color_bits = vga.pixel_size;
      vga.inst_emu = (vga.mode_type==PL4 || vga.mode_type==PL2)
	? EMU_ALL_INST : 0;
      vgaemu_map_bank();      // update page protection
      if (oldclass != vga.mode_class) {
	vgaemu_adj_cfg(CFG_SEQ_ADDR_MODE, 0);
	vgaemu_adj_cfg(CFG_CRTC_WIDTH, 0);
	vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 0);
      } else if (old_color_bits != vga.color_bits) {
	vgaemu_adj_cfg(CFG_CRTC_WIDTH, 0);
      }
      vga.mem.wrap = (vga.mode_type == CGA || vga.mode_class == TEXT ?
		      32 : 64) * 1024;
      vga.reconfig.re_init = 1;
      break;
    }
    default:
      vga_msg("vgaemu_adj_cfg: unknown item %u\n", what);
  }
}
