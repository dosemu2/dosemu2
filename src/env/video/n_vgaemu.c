/*
 * DANG_BEGIN_MODULE
 *
 * The VGA emulator for DOSEmu.
 *
 * Emulated are the video memory and the VGA register set (CRTC, DAC, etc.).
 * Parts of the hardware emulation is done in separate files (attremu.c,
 * crtcemu.c, dacemu.c and seqemu.c).
 *
 * VGAEmu uses the video BIOS code in base/bios/int10.c and vesa.c.
 *
 * For an excellent reference to programming SVGA cards see Finn Thøgersen's
 * VGADOC4, available at http://www.datashopper.dk/~finth
 *
 * DANG_END_MODULE
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
 * DANG_BEGIN_MODULE
 * 1997/06/15: Major rework; the interface to the outside now uses the
 * global variable `vga' which holds all VGA related information.
 * The update function now works for all types of modes except for
 * Hercules modes.
 * The VGA memory is now always executable.
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
 *
 *
 * DANG_END_CHANGELOG
 *
 */

/*
 * define some the following to generate various debug output
 */

#undef DEBUG_IO			/* port emulation */
#undef DEBUG_MAPPING		/* VGA memory mapping */
#undef DEBUG_UPDATE		/* screen update process */
#undef DEBUG_BANK_SWITCHING	/* bank switching */


#include <features.h>
#if __GLIBC__ > 1
#include <sigcontext.h>
#endif
#include <sys/mman.h>           /* root@sjoerd*/
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "cpu.h"	/* root@sjoerd: for context structure */
#include "emu.h"
#include "port.h"
#include "video.h"
#include "vgaemu.h"
#include "vgaemu_inside.h"
#ifdef VESA
#include "vesa.h"
#endif
#include "priv.h"

#if !defined True
#define False 0
#define True 1
#endif

#define RW 1
#define RO 0
#define VGA_EMU_RW_PROT (PROT_READ | PROT_WRITE | PROT_EXEC)
#define VGA_EMU_RO_PROT (PROT_READ | PROT_EXEC)

/*
 * Functions local to this file
 */

static int vga_emu_protect_page(unsigned, int);
static int vga_emu_protect(unsigned, unsigned, int);
static int vga_emu_adjust_protection(unsigned, unsigned);
static int vga_emu_map(unsigned, unsigned);
#ifdef DEBUG_UPDATE
static void print_dirty_map(void)
#endif
inline static unsigned mode_area(unsigned);


/*
 * holds all VGA data
 */

vga_type vga;


/* **************** General mode data **************** */

/* Table with video mode definitions */
vga_mode_info vga_mode_table[]=
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


/* **************** VGA emulation routines **************** */

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

void VGA_emulate_outb(Bit32u port, Bit8u value)
{
#ifdef DEBUG_IO
  v_printf("VGAemu: VGA_emulate_outb(): outb(0x%03x, 0x%02x)\n", (unsigned) port, (unsigned) value);
#endif

  switch(port)
    {

    case CRTC_INDEX:
      CRTC_set_index(value);
      break;

    case CRTC_DATA:
      CRTC_write_value(value);
      break;

    case SEQUENCER_INDEX:
      Seq_set_index(value);
      break;

    case SEQUENCER_DATA:
      Seq_write_value(value);
      break;

    case ATTRIBUTE_INDEX:
      Attr_write_value(value);
      break;
    
    case ATTRIBUTE_DATA:
      /* The attribute controller data port is a read-only register,
       * so don't do anything at all.
       */
#ifdef DEBUG_IO
      v_printf("VGAemu: ERROR: illegal write to the attribute controller "
               "data port!\n");
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
      v_printf("VGAemu: not (yet) smart enough to emulate write of 0x%02x to"
	       " port 0x%04x\n", (unsigned) value, (unsigned) port);
#endif
      break;
    }
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

Bit8u VGA_emulate_inb(Bit32u port)
{
  Bit8u uc = 0xff;

  switch(port) {

    case CRTC_INDEX:
      uc = CRTC_get_index();
      break;

    case CRTC_DATA:
      uc = CRTC_read_value();
      break;

    case SEQUENCER_INDEX:
      uc = Seq_get_index();
      break;

    case SEQUENCER_DATA:
      uc = Seq_read_value();
      break;

    case ATTRIBUTE_INDEX:
      uc = Attr_get_index();	/* undefined, in fact */
      break;
    
    case ATTRIBUTE_DATA:
      uc = Attr_read_value();
      break;
    
    case DAC_PEL_MASK:
      uc = DAC_get_pel_mask();
      break;

    case DAC_STATE:
      uc = DAC_get_state();
      break;

    case DAC_WRITE_INDEX:	/* this is undefined, but we have to do something */
      break;

    case DAC_DATA:
      uc = DAC_read_value();
      break;

    case INPUT_STATUS_1:
      uc = Attr_get_input_status_1();
      break;

    default:
#ifdef DEBUG_IO
      v_printf("VGAemu: not (yet) smart enough to emulate read from port 0x%04x\n", (unsigned) port);
#endif
      break;
  }

#ifdef DEBUG_IO
  v_printf("VGAemu: VGA_emulate_inb(): inb(0x%03x) = 0x%02x\n", (unsigned) port, (unsigned) uc);
#endif

  return uc;
}


/*
 * DANG_BEGIN_FUNCTION vga_emu_fault
 *
 * description:
 * vga_emu_fault() is used to catch video access, and handle it.
 * This function is called from arch/linux/async/sigsegv.c::dosemu_fault1().
 * The sigcontext_struct is defined in include/cpu.h.
 * Now it catches only changes in a 4K page, but maybe it is useful to
 * catch each video access. The problem when you do that is, you have to 
 * simulate each instruction which could write to the video memory.
 * It is easy to get the place where the exception happens (scp->cr2),
 * but what are those changes?
 * An other problem is, it could eat a lot of time, but it does now also.
 *
 * arguments:
 * scp - A pointer to a struct sigcontext_struct holding some relevant data.
 *
 * DANG_END_FUNCTION                        
 *
 */     

int vga_emu_fault(struct sigcontext_struct *scp)
{
  int i, j;
  unsigned page_fault, vga_page = 0;

  page_fault = ((unsigned) scp->cr2) >> 12;

  for(i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
    j = page_fault - vga.mem.map[i].base_page;
    if(j >= 0 && j < vga.mem.map[i].pages) {
      vga_page = j + vga.mem.map[i].first_page;
      break;
    }
  }

  if(i == VGAEMU_MAX_MAPPINGS) {			/* not in mapped VGA memory */
    if(page_fault >= 0xa0 && page_fault < 0xc0) {	/* ...  but somewhere nearby */
#ifdef DEBUG_MAPPING
      v_printf("VGAEmu: vga_emu_fault: outside mapped region; address = 0x%lx, page = 0x%x\n",
        scp->cr2, page_fault
      );
#endif
      vga_emu_protect_page(page_fault, RW);
      return True;
    }
#ifdef VESA
    if(vesa_emu_fault(scp) == True) {
#ifdef DEBUG_MAPPING
      v_printf("VGAEmu: vga_emu_fault: VESA fault; address = 0x%lx, page = 0x%x\n",
        scp->cr2, page_fault
      );
#endif
      return True;
    }
    else
#endif	/* VESA */
    {
      v_printf(
        "VGAEmu: vga_emu_fault: not in 0xa0000 - 0xc4000 range; page = 0x%02x, address = 0x%lx\n",
        page_fault,
        scp->cr2
      );
      return False;
    }
  }

#ifdef DEBUG_MAPPING
  v_printf("VGAEmu: vga_emu_fault: region = %d, address = 0x%lx, page = 0x%x, vga page = 0x%x\n",
    i, scp->cr2, page_fault, vga_page
  );
#endif

  if(vga_page < vga.mem.pages) {
    vga.mem.dirty_map[vga_page] = 1;
    vga_emu_adjust_protection(vga_page, page_fault);
  }

  return True;
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

static int vga_emu_protect_page(unsigned page, int prot)
{
  return mprotect((void *) (page << 12), 1 << 12, prot ? VGA_EMU_RW_PROT : VGA_EMU_RO_PROT);
} 


/*
 * Change protection of a VGA memory page
 *
 * The protection of `page' is set to `prot' in all places where `page' is
 * currently mapped to. `prot' may be either 0 (RO) or 1 (RW).
 * This functions returns 3 if `mapped_page' is not 0 and not one of the
 * locations `page' is mapped to (cf. vga_emu_adjust_protection()).
 * `page' is relative to the VGA memory.
 * `mapped_page' is an absolute page number.
 *
 */

static int vga_emu_protect(unsigned page, unsigned mapped_page, int prot)
{
  int i, j = 0, err, err_1 = 0, map_ok = 0;
#ifdef DEBUG_MAPPING
  int k = 0;
#endif

  if(page > vga.mem.pages) {
#ifdef DEBUG_MAPPING
    v_printf("VGAEmu: vga_emu_protect: invalid page number; page = 0x%x\n", page);
#endif
    return 1;
  }

  for(i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
    if(vga.mem.map[i].pages) {
      j = page - vga.mem.map[i].first_page;
      if(j >= 0 && j < vga.mem.map[i].pages) {
#ifdef DEBUG_MAPPING
        k = 1;
#endif
        if(vga.mem.map[i].base_page + j == mapped_page) map_ok = 1;
        err = vga_emu_protect_page(vga.mem.map[i].base_page + j, prot);
        if(!err_1) err_1 = err;
#ifdef DEBUG_MAPPING
        v_printf(
          "VGAEmu: vga_emu_protect: error = %d, region = %d, page = 0x%x --> map_addr = 0x%x, prot = %s\n",
          err, i, page, vga.mem.map[i].base_page + j, prot & PROT_WRITE ? "RW" : "RO"
        );
#endif
      }
    }
  }

#ifdef DEBUG_MAPPING
  if(k == 0) {
    v_printf("VGAEmu: vga_emu_protect: page not mapped; page = 0x%x\n", page);
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

static int vga_emu_adjust_protection(unsigned page, unsigned mapped_page)
{
  int prot = 0;
  int i, err, j, k;

  if(page > vga.mem.pages) {
#ifdef DEBUG_MAPPING
    v_printf("VGAEmu: vga_emu_adjust_protection: invalid page number; page = 0x%x\n", page);
#endif
    return 1;
  }

  if(vga.mem.dirty_map[page]) prot = 1;

  i = vga_emu_protect(page, mapped_page, prot);

  if(i == 3) {
    v_printf(
      "VGAEmu: vga_emu_adjust_protection: mapping inconsistency; page = 0x%x, map_addr != 0x%x\n",
      page, mapped_page
    );
    err = vga_emu_protect_page(mapped_page, prot);
    if(err) {
      v_printf(
        "VGAEmu: vga_emu_adjust_protection: mapping inconsistency not fixed; page = 0x%x, map_addr = 0x%x\n",
        page, mapped_page
      );
    }
  }

  if(vga.mem.planes == 4) {	/* MODE_X or PL4 */
    j = vga.mem.dirty_map[page];
    page &= ~0x30;
    for(k = 0; k < 4; k++, page += 0x10) {
      if(j != vga.mem.dirty_map[page]) {
        vga.mem.dirty_map[page] = j;
        vga_emu_protect(page, 0, prot);
      }
    }
  }

  return i;
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
  int i;
  unsigned u;
  vga_mapping_type *vmt;

  if(mapping >= VGAEMU_MAX_MAPPINGS) return 1;

  vmt = vga.mem.map + mapping;
  if(vmt->pages == 0) return 0;		/* nothing to do */

  if(vmt->pages + first_page > vga.mem.pages) return 2;

  /* Touch all pages before mmap()-ing,
   * else Linux >= 1.3.78 will return -EINVAL. (Hans, 96/04/16)
   */
  for (u = 0; u < vmt->pages; u++) *((volatile char *)(vga.mem.base + ((first_page + u) << 12)));

  i = (int) mmap(
    (caddr_t) (vmt->base_page << 12), vmt->pages << 12,
    VGA_EMU_RW_PROT, MAP_SHARED | MAP_FIXED,
    vga.mem.fd, (off_t) (vga.mem.base + (first_page << 12))
  );

  if(i == -1) return 3;

  vmt->first_page = first_page;

  for(u = 0; u < vmt->pages; u++) {
    /* page is writable by default */
    if(!vga.mem.dirty_map[vmt->first_page + u]) vga_emu_adjust_protection(vmt->first_page + u, 0);
  }

  return 0;
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
 * DANG_END_FUNCTION                        
 *
 */     

int vga_emu_init()
{
  int i;
  vga_mapping_type vmt = {0, 0, 0};
  emu_iodev_t io_device;

  if(config.vgaemu_memsize)
    vga.mem.size = config.vgaemu_memsize << 10;
  else
    vga.mem.size = 1024 << 10;

  /* force 256k granularity to prevent possible problems (with nonchain4
   * (4 planes) addressing, to be precise)
   */
  vga.mem.size = (vga.mem.size + ~(-1 << 18)) & (-1 << 18);
  vga.mem.pages = vga.mem.size >> 12;

  if((vga.mem.base = (unsigned char *) valloc(vga.mem.size)) == NULL) {
    v_printf("VGAEmu: vga_emu_init: not enough memory (%u k)\n", vga.mem.size >> 10);
    return 1;
  }

  if((vga.mem.dirty_map = (unsigned char *) malloc(vga.mem.pages)) == NULL) {
    v_printf("VGAEmu: vga_emu_init: not enough memory for dirty map\n");
    return 1;
  }

  /* map the bank of the allocated memory */
  enter_priv_on();	/* for /proc/self/mem I need full privileges */
  vga.mem.fd = open("/proc/self/mem", O_RDWR);
  leave_priv_setting();

  if (vga.mem.fd < 0) {
    v_printf("VGAEmu: vga_emu_init: cannot open /proc/self/mem\n");
    return 2;
  }

  dirty_all_video_pages();		/* all need an update */

  /*
   * vga.mem.map _must_ contain only valid mappings - otherwise _bad_ things will happen!
   * cf. vga_emu_protect()
   * -- sw
   */

  for(i = 0; i < VGAEMU_MAX_MAPPINGS; i++) vga.mem.map[i] = vmt;

  vga.mem.bank = vga.mem.bank_pages = 0;

  /* initialize other parts */
  DAC_init();
  Attr_init();
  Seq_init();
  CRTC_init();

  /* register VGA ports */
  io_device.read_portb = VGA_emulate_inb;
  io_device.write_portb = VGA_emulate_outb;
  io_device.read_portw = NULL;
  io_device.write_portw = NULL;
  io_device.irq = EMU_NO_IRQ;
  
  /* register attribute controller */
  io_device.handler_name = "VGAEmu Attribute controller";
  io_device.start_addr = ATTRIBUTE_INDEX;
  io_device.end_addr = INPUT_STATUS_0;
  port_register_handler(io_device);

  /* register sequencer */
  io_device.handler_name = "VGAEmu Sequencer";
  io_device.start_addr = SEQUENCER_INDEX;
  io_device.end_addr = SEQUENCER_DATA;
  port_register_handler(io_device);

  /* register DAC */
  io_device.handler_name = "VGAEmu DAC";
  io_device.start_addr = DAC_BASE;
  io_device.end_addr = DAC_DATA;
  port_register_handler(io_device);

  /* register CRT controller */
  io_device.handler_name = "VGAEmu CRT Controller";
  io_device.start_addr = CRTC_INDEX;
  io_device.end_addr = CRTC_DATA;
  port_register_handler(io_device);

#ifdef VESA
  vesa_init();
#endif

  v_printf("VGAEmu: vga_emu_init: memory = %u kbyte\n", vga.mem.size >> 10);

  return 0;
}

#ifdef DEBUG_UPDATE
static void print_dirty_map()
{
  int i, j;

  v_printf("VGAEmu: dirty_map[0 - 256k]\n");
  for(j = 0; j < 64; j += 16) {
    v_printf("  ");
    for(i = 0; i < 16; i++) {
      if(!(i & 3)) v_printf(" ");
      v_printf("%d", (int) vga.mem.dirty_map[j + i]);
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

int vga_emu_update(vga_emu_update_type *veut)
{
  int i, j;
  unsigned start_page, end_page;

  if(veut->display_end > vga.mem.size) veut->display_end = vga.mem.size;
  if(veut->update_pos < veut->display_start || veut->update_pos >= veut->display_end) {
    veut->update_pos = veut->display_start;
  }

  start_page = (veut->display_start >> 12);
  end_page = (veut->display_end - 1) >> 12;

#ifdef DEBUG_UPDATE
  v_printf("VGAEmu: vga_emu_update: display = %d (page = %u) - %d (page = %u), update_pos = %d, max_len = %d (max_max_len = %d)\n",
    veut->display_start,
    start_page,
    veut->display_end,
    end_page,
    veut->update_pos,
    veut->max_len,
    veut->max_max_len
  );

  print_dirty_map();
#endif

  for(i = j = veut->update_pos >> 12; i <= end_page && ! vga.mem.dirty_map[i]; i++);
  if(i == end_page + 1) {
    for(i = start_page; i < j && vga.mem.dirty_map[i] == 0; i++);

    if(i == j) {	/* no dirty pages */
/*      veut->update_pos = veut->display_start;	*/
      veut->update_start = veut->update_pos;
      veut->update_len = 0;
      return 0;
    }
  }

  for(
    j = i;
    j <= end_page && vga.mem.dirty_map[j] && (veut->max_max_len == 0 || veut->max_len > 0);
    j++
  ) {
    vga.mem.dirty_map[j] = 0;
    vga_emu_adjust_protection(j, 0);
    if(veut->max_max_len) veut->max_len -= 1 << 12;
  }

#ifdef DEBUG_UPDATE
  v_printf("VGAEmu: vga_emu_update: update range: i = %d, j = %d\n", i, j);
#endif

  if(i == j) {
    if(veut->max_max_len) veut->max_len = 0;
    return -1;
  }

  veut->update_start = i << 12;
  veut->update_len = j << 12;
  if(veut->update_gran > 1) {
    veut->update_start -= (veut->update_start - veut->display_start) % veut->update_gran;
    veut->update_len += veut->update_gran - 1;
    veut->update_len -= (veut->update_len - veut->display_start) % veut->update_gran;
  }

  if(veut->update_start < veut->display_start) veut->update_start = veut->display_start;
  if(veut->update_len > veut->display_end) veut->update_len = veut->display_end;

  veut->update_pos = veut->update_len;
  veut->update_len -= veut->update_start;

#ifdef DEBUG_UPDATE
  v_printf("VGAEmu: vga_emu_update: update_start = %d, update_len = %d, update_pos = %d\n",
    veut->update_start,
    veut->update_len,
    veut->update_pos
  );
#endif

  return j - i;
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
  int i;

  if((bank + 1) * vga.mem.bank_pages > vga.mem.pages) {
    v_printf("VGAEmu: vga_emu_switch_bank: Invalid bank number %d\n", bank);
    return False;
  }

  vga.mem.bank = bank;

  if((i = vga_emu_map(0, vga.mem.bank_pages * bank))) {
    v_printf("VGAEmu: vga_emu_switch_bank: Remapping failed; reason: %d\n", i);
    return False;
  }
  
#ifdef DEBUG_BANK_SWITCHING
  v_printf("VGAEmu: vga_emu_switch_bank: switched to bank %d\n", vga.mem.bank);
#endif

  return True;
}


/*
 * Only used by vga_emu_setmode
 */

inline static unsigned mode_area(unsigned mode_index)
{
  return vga_mode_table[mode_index].x_char * vga_mode_table[mode_index].y_char;
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

int vga_emu_setmode(int mode, int width, int height)
{
  int i;
  int index = 0;
  int found = False;
  
  /* Search for the first valid mode */
  for(i = 0; (vga_mode_table[i].mode != -1) && (found == False); i++) {
    if(vga_mode_table[i].mode == (mode & 0x7f)) {
      if(vga_mode_table[i].type == TEXT) {
        /* TEXT modes can use different char boxes, like the
         * mode 0x03 (80x25). Mode 0x03 uses a 9x16 char box
         * for 80x25 and a 8x8 charbox for 80x50
         */
         if(vga_mode_table[i].x_char == width &&
            vga_mode_table[i].y_char == height
         ) {
            found = True;
            index = i;
         }
      }
      else {
        /* GRAPH modes use only one format */
        found = True;
        index = i;
      }
    }
  }

  if(found == True)
    v_printf("VGAEmu: vga_emu_setmode: mode found in first run\n");

  /* Play it again, Sam!
   * This is when we can't find the textmode with the appropriate sizes.
   * Use the best matching text mode
   */
  if(found == False) {
    for(i = 0; (vga_mode_table[i].mode != -1); i++) {
      if(vga_mode_table[i].mode == (mode & 0x7f) &&
         /* make sure everything is visible! */
         vga_mode_table[i].x_char >= width &&
         vga_mode_table[i].y_char >= height
      ) {
        if(found == True && mode_area(i) >= mode_area(index)) {
          continue;
        }
        else {
          found = True;
          index = i;
        }
      }
    }

    if(found == True)
      v_printf("VGAEmu: vga_emu_setmode: mode found in second run\n");
  }

  if(found == False) return False;	/* failed */

  vga.mode_info = vga_mode_table + index;

  v_printf("VGAEmu: vga_emu_setmode: mode = 0x%02x, (%d x %d, %d x %d, %d x %d)\n",
    vga.mode_info->mode,
    vga.mode_info->x_res, vga.mode_info->y_res,
    vga.mode_info->x_char, vga.mode_info->y_char,
    vga.mode_info->x_box, vga.mode_info->y_box
  );
       
  vga.mode = vga.mode_info->mode;
  vga.VESA_mode = 0;
  vga.mode_class = vga.mode_info->type;
  vga.mode_type = vga.mode_info->memorymodel;
  vga.width = vga.mode_info->x_res;
  vga.height = vga.mode_info->y_res;
  vga.scan_len = vga.mode_info->x_res;
  vga.text_width = vga.mode_info->x_char;
  vga.text_height = vga.mode_info->y_char;
  vga.char_width = vga.mode_info->x_box;
  vga.char_height = vga.mode_info->y_box;

  vga.mem.reconfigured = 0;
  vga.mem.planes = 1;
  if(vga.mode_type == PL4) {
    vga.scan_len >>= 3;		/* hack */
    vga.mem.planes = 4;
  }
  vga.mem.write_plane = vga.mem.read_plane = 0;

  if(vga.mode_type == TEXT) vga.scan_len = vga.text_width << 1;

  vga.display_start = 0;

  /* Does the BIOS clear all of video memory, or just enough
   for the viewport?  This clears all of it... */

  /* Actually if the bios has any sense it clears fills the screen
   * with spaces (Background Black, Foreground White).  This
   * happens to be the same as all zeros in graphics mode.
   * I've patched the bios end of it to handle the text mode case
   * so don't do that here.  There are no real video modes >= 0x80
   * so test the incoming mode number instead of the name after
   * the stripping.
   */

  /* FIXME: the interface between the bios setmode and this
     setmode are weird!! */

  if(vga_mode_table[index].type != TEXT && !(mode & 0x80)) {
    memset((void *) vga.mem.base, 0, vga.mem.size);
  }

  dirty_all_video_pages();

  vga.mem.bank = 0;
  vga.mem.bank_pages = vga.mode_info->bufferlen >> 12;

  vga.mem.map[0].base_page = vga.mode_info->bufferstart >> 12;
  vga.mem.map[0].first_page = 0;
  vga.mem.map[0].pages = vga.mem.bank_pages;

  vga_emu_map(0, 0);	/* map the VGA memory */

  vga.dac.bits = 6;
  DAC_init();		/* Re-initialize the DAC */

  vga.seq.chain4 = 1;	/* ??? */
  vga.seq.map_mask = 1;

  return True;
}


/*
 * DANG_BEGIN_FUNCTION get_vgaemu_mode_info
 *
 * description:
 * Returns a pointer to the vga_mode_info structure for the
 * requested mode or NULL if an invalid mode was given.
 *
 * DANG_END_FUNCTION
 *
 * to be removed -- sw
 */

vga_mode_info* get_vgaemu_mode_info(int mode)
{
  vga_mode_info *vmi = vga_mode_table;

  do {
    if(vmi->mode == mode) return vmi;
  }
  while(vmi++->mode != -1);

  return NULL;
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
  int i;

  for(i = 0; i < vga.mem.pages; i++) vga.mem.dirty_map[i] = 1;
}


/*
 * DANG_BEGIN_FUNCTION vga_emu_set_text_page
 *
 * description:
 * Set visible text page.
 *
 * `vga.display_start' is set to `page' * `page_size'.
 * This function works only in text modes.
 *
 * arguments:
 * page      - Number of the text page.
 * page_size - Size of one text page.
 *
 * DANG_END_FUNCTION                        
 *
 */     

int vga_emu_set_text_page(unsigned page, unsigned page_size)
{
  if(vga.mode_type != TEXT) {
    v_printf("VGAEmu: vga_emu_set_text_page: not in text mode\n");
    return 1;
  }

  if((page + 1) * page_size > vga.mem.size) {
    v_printf("VGAEmu: vga_emu_set_text_page: page number %d to high\n", page);
    return 2;
  }

  v_printf("VGAEmu: vga_emu_set_text_page %d\n", page);

  vga.display_start = page * page_size;

  return 0;
}

