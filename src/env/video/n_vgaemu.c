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
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
 *
 * 1997/07/07: Added linear frame buffer (LFB) support, simplified and
 * generalized video mode definitions (now possible via dosemu.conf).
 * vga_emu_init() now gets some info about the display it is actually
 * running on (needed dor VESA emulation).
 * vga_emu_setmode() now accepts VESA mode numbers.
 * Hi/true-color modes now work.
 * -- sw
 *
 * DANG_END_CHANGELOG
 *
 */

/*
 * define some the following to generate various debug output
 */

#undef	DEBUG_IO		/* port emulation */
#undef	DEBUG_MAPPING		/* VGA memory mapping */
#undef	DEBUG_UPDATE		/* screen update process */
#undef	DEBUG_BANK_SWITCHING	/* bank switching */


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
#include "remap.h"
#include "vgaemu.h"
#include "priv.h"

#if !defined True
#define False 0
#define True 1
#endif

#define RW 1
#define RO 0
/*
 * We add PROT_EXEC just because pages should be executable. Of course
 * Intel's x86 processors do not support non-executable pages, but anyway...
 */
#define VGA_EMU_RW_PROT (PROT_READ | PROT_WRITE | PROT_EXEC)
#define VGA_EMU_RO_PROT (PROT_READ | PROT_EXEC)


/*
 * functions local to this file
 */

static int vga_emu_protect_page(unsigned, int);
static int vga_emu_protect(unsigned, unsigned, int);
static int vga_emu_adjust_protection(unsigned, unsigned);
static int vga_emu_map(unsigned, unsigned);
#ifdef DEBUG_UPDATE
static void print_dirty_map(void);
#endif
static int vga_emu_setup_mode(vga_mode_info *, int, unsigned, unsigned, unsigned);
static void vga_emu_setup_mode_table(void);


/*
 * holds all VGA data
 */
vga_type vga;

/*
 * table with video mode definitions
 */
#include "vgaemu_modelist.h"


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
    if(vesa_emu_fault(scp) == True) {
#ifdef DEBUG_MAPPING
      v_printf("VGAEmu: vga_emu_fault: VESA fault; address = 0x%lx, page = 0x%x\n",
        scp->cr2, page_fault
      );
#endif
      return True;
    }
    else {
      v_printf(
        "VGAEmu: vga_emu_fault: not in 0xa0000 - 0x%05x range; page = 0x%02x, address = 0x%lx\n",
        0xc0000 + (vgaemu_bios.pages << 12),
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
 * arguments:
 * vedt - Pointer to struct describing the type of display we are actually
 *        attached to.
 *
 * DANG_END_FUNCTION                        
 *
 */     

int vga_emu_init(vgaemu_display_type *vedt)
{
  int i;
  vga_mapping_type vmt = {0, 0, 0};
  emu_iodev_t io_device;
  static unsigned char *lfb_base = NULL;

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

  if(config.X_lfb) {
    if((lfb_base = (unsigned char *) valloc(vga.mem.size)) == NULL) {
      v_printf("VGAEmu: vga_emu_init: not enough memory (%u k)\n", vga.mem.size >> 10);
    }
  }

  if(lfb_base == NULL) {
    v_printf("VGAEmu: vga_emu_init: linear frame buffer (lfb) disabled\n");
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

  if(lfb_base != NULL) {
    vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page = (unsigned) lfb_base >> 12;
  }

  vga_emu_setup_mode_table();

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

  vbe_init(vedt);

  v_printf(
    "VGAEmu: vga_emu_init: memory = %u kbyte at 0x%x, lfb = 0x%x\n",
    vga.mem.size >> 10, (unsigned) vga.mem.base, vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page << 12
  );

  return 0;
}


void vga_emu_done()
{
  /* We should probably do something here - but what ? -- sw */
}


#ifdef DEBUG_UPDATE
static void print_dirty_map()
{
  int i, j;

  v_printf("VGAEmu: dirty_map[0 - 1024k]\n");
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

  if((i = vga_emu_map(VGAEMU_MAP_BANK_MODE, vga.mem.bank_pages * bank))) {
    v_printf("VGAEmu: vga_emu_switch_bank: Remapping failed; reason: %d\n", i);
    return False;
  }
  
#ifdef DEBUG_BANK_SWITCHING
  v_printf("VGAEmu: vga_emu_switch_bank: switched to bank %d\n", vga.mem.bank);
#endif

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

  vmi->mode = -1;
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
  vmi->buffer_start = 0xa000;
  vmi->buffer_len = 64;
  vmi->width = width;
  vmi->height = height;
  vmi->char_width = 8;
  vmi->char_height = height >= 400 ? 16 : 8;
  if(vmi->char_height == 16 && (height & 15) == 8) vmi->char_height = 8;
  vmi->text_width = width / vmi->char_width;
  vmi->text_height = height / vmi->char_height;

  v_printf(
    "VGAEmu: vga_emu_setup_mode: creating VESA mode %d x %d x %d\n",
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
        v_printf(
          "VGAEmu: vga_emu_setup_mode_table: invalid VESA mode %d x %d x %d\n",
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
    if(vmi->mode == mode || vmi->VESA_mode == mode) return vmi;
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

int vga_emu_setmode(int mode, int width, int height)
{
  unsigned u = -1;
  vga_mode_info *vmi = NULL, *vmi2 = NULL;
  
  v_printf("VGAEmu: vga_emu_setmode: requested mode: 0x%02x (%d x %d)\n", mode, width, height);

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
    vmi = vmi2;
  }

  if(vmi == NULL) {	/* no mode found */
    v_printf("VGAEmu: vga_emu_setmode: no mode 0x%02x found\n", mode);
    return False;
  }

  v_printf("VGAEmu: vga_emu_setmode: mode found in %s run\n", vmi == vmi2 ? "second" : "first");

  vga.mode_info = vmi;

  v_printf("VGAEmu: vga_emu_setmode: mode = 0x%02x, (%d x %d, %d x %d, %d x %d)\n",
    mode,
    vga.mode_info->width, vga.mode_info->height,
    vga.mode_info->text_width, vga.mode_info->text_height,
    vga.mode_info->char_width, vga.mode_info->char_height
  );
       
  vga.mode = mode;
  vga.VESA_mode = vga.mode_info->VESA_mode;
  vga.mode_class = vga.mode_info->mode_class;
  vga.mode_type = vga.mode_info->type;
  vga.width = vga.mode_info->width;
  vga.height = vga.mode_info->height;
  vga.scan_len = (vga.mode_info->width + 3) & ~3;	/* dword aligned */
  vga.text_width = vga.mode_info->text_width;
  vga.text_height = vga.mode_info->text_height;
  vga.char_width = vga.mode_info->char_width;
  vga.char_height = vga.mode_info->char_height;
  vga.pixel_size = vga.mode_info->color_bits;
  if(vga.pixel_size > 8) {
    vga.pixel_size = (vga.pixel_size + 7) & ~7;		/* assume byte alignment for these modes */
    vga.scan_len *= vga.pixel_size >> 3;
  }
  v_printf("VGAEmu: vga_emu_setmode: scan_len = %d\n", vga.scan_len);

  vga.reconfig.mem = vga.reconfig.display =
  vga.reconfig.dac = vga.reconfig.power = 0;
  vga.mem.planes = 1;
  if(vga.mode_type == PL4 || vga.mode_type == NONCHAIN4) {
    vga.scan_len >>= 3;
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

  if(vga.mode_class == GRAPH && !(mode & 0x80)) {
    memset((void *) vga.mem.base, 0, vga.mem.size);
  }

  dirty_all_video_pages();

  vga.mem.bank = 0;
  vga.mem.bank_pages = vga.mode_info->buffer_len >> 2;

  vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page = vga.mode_info->buffer_start >> 8;
  vga.mem.map[VGAEMU_MAP_BANK_MODE].first_page = 0;
  vga.mem.map[VGAEMU_MAP_BANK_MODE].pages = vga.mem.bank_pages;
  vga_emu_map(VGAEMU_MAP_BANK_MODE, 0);	/* map the VGA memory */

  if(vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page) {
    vga.mem.map[VGAEMU_MAP_LFB_MODE].first_page = 0;
    vga.mem.map[VGAEMU_MAP_LFB_MODE].pages = vga.mem.pages;
    /* vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page is set in vga_emu_init() */
    vga_emu_map(VGAEMU_MAP_LFB_MODE, 0);	/* map the VGA memory to LFB */
  }

  vga.dac.bits = 6;
  DAC_init();		/* Re-initialize the DAC */

  vga.seq.chain4 = 1;	/* ??? */
  vga.seq.map_mask = 1;

  return True;
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

