/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
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
 * DANG_END_CHANGELOG
 *
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Define some of the following to generate various debug output.
 * A useful debug level is 1; level 2 can easily lead to insanely
 * large log files.
 */
#define	DEBUG_IO	0	/* port emulation */
#define	DEBUG_MAP	0	/* VGA memory mapping */
#define	DEBUG_UPDATE	0	/* screen update process */
#define	DEBUG_BANK	0	/* bank switching */
#define	DEBUG_COL	0	/* color interpretation changes */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
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
#include <features.h>
#if GLIBC_VERSION_CODE == 2000
#include <sigcontext.h>
#endif
#include <sys/mman.h>		/* root@sjoerd*/
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include "cpu.h"		/* root@sjoerd: for context structure */
#include "emu.h"
#include "dpmi.h"
#include "port.h"
#include "video.h"
#include "remap.h"
#include "vgaemu.h"
#include "priv.h"

/* table with video mode definitions */
#include "vgaemu_modelist.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*
 * functions local to this file
 */

static int vga_emu_protect_page(unsigned, int);
static int vga_emu_protect(unsigned, unsigned, int);
static int vga_emu_adjust_protection(unsigned, unsigned);
static int vga_emu_map(unsigned, unsigned);
static int vgaemu_unmap(unsigned);
#if DEBUG_UPDATE >= 1
static void print_dirty_map(void);
#endif
static int vga_emu_setup_mode(vga_mode_info *, int, unsigned, unsigned, unsigned);
static void vga_emu_setup_mode_table(void);


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

void VGA_emulate_outb(ioport_t port, Bit8u value)
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
      Seq_write_value(value);
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
      if(!vga.config.mono_port) CRTC_write_value(value);
      break;

    case FEATURE_CONTROL_W:		/* 0x3da */
      if(!vga.config.mono_port) Misc_set_feature_ctrl(value);
      break;

    default:
      vga_deb_io(
        "VGA_emulate_outb: write access not emulated (port[0x%03x] = 0x%02x)\n",
        (unsigned) port, (unsigned) value
      );
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
  unsigned page_fault, vga_page = 0, u, access_type, lin_addr;
#if DEBUG_MAP >= 1
  unsigned char *cs_ip = SEG_ADR((unsigned char *), cs, ip);
  static char *txt1[VGAEMU_MAX_MAPPINGS + 1] = { "bank", "lfb", "some" };
#endif

  page_fault = (lin_addr = (unsigned) scp->cr2) >> 12;
  access_type = (scp->err >> 1) & 1; 

  for(i = 0; i < VGAEMU_MAX_MAPPINGS; i++) {
    j = page_fault - vga.mem.map[i].base_page;
    if(j >= 0 && j < vga.mem.map[i].pages) {
      vga_page = j + vga.mem.map[i].first_page;
      break;
    }
  }

  vga_deb_map("vga_emu_fault: %s%s access to %s region, address 0x%05x, page 0x%x, vga page 0x%x\n",
    in_dpmi ? "dpmi " : "", access_type ? "write" : "read",
    txt1[i], lin_addr, page_fault, vga_page
  );

  vga_deb2_map(
    "vga_emu_fault: in_dpmi %d, err 0x%x, scp->cs:eip %04x:%04x, vm86s->cs:eip %04x:%04x\n",
    in_dpmi, (unsigned) scp->err, (unsigned) _cs, (unsigned) _eip,
    (unsigned) REG(cs), (unsigned) REG(eip)
  );

  vga_deb_map(
    "vga_emu_fault: cs:eip = %04x:%04x, instr: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
    (unsigned) REG(cs), (unsigned) REG(eip),
    cs_ip[ 0], cs_ip[ 1], cs_ip[ 2], cs_ip[ 3], cs_ip[ 4], cs_ip[ 5], cs_ip[ 6], cs_ip[ 7],
    cs_ip[ 8], cs_ip[ 9], cs_ip[10], cs_ip[11], cs_ip[12], cs_ip[13], cs_ip[14], cs_ip[15]
  );

  if(i == VGAEMU_MAX_MAPPINGS) {
    if(page_fault >= 0xa0 && page_fault < 0xc0) {	/* unmapped VGA area */
      u = instr_len(SEG_ADR((unsigned char *), cs, ip));
      LWORD(eip) += u;
      if(!in_dpmi && u) {
        vga_msg("vga_emu_fault: attempted write to unmapped area (cs:ip += %u)\n", u);
      }
      else {
        vga_msg("vga_emu_fault: unknown instruction, page at 0x%05x now writable\n", page_fault << 12);
        vga_emu_protect_page(page_fault, RW);
      }
      return True;
    }
    else if(page_fault >= 0xc0 && page_fault < (0xc0 + vgaemu_bios.pages)) {	/* ROM area */
      u = instr_len(SEG_ADR((unsigned char *), cs, ip));
      LWORD(eip) += u;
      if(!in_dpmi && u) {
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
  vga_deb2_map("vga_emu_protect_page: 0x%02x = %s\n", page, prot ? "RW" : "RO");

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
          err, i, page, vga.mem.map[i].base_page + j, prot & PROT_WRITE ? "RW" : "RO"
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

static int vga_emu_adjust_protection(unsigned page, unsigned mapped_page)
{
  int prot = 0;
  int i, err, j, k;

  if(page > vga.mem.pages) {
    vga_deb_map("vga_emu_adjust_protection: invalid page number; page = 0x%x\n", page);
    return 1;
  }

  if(vga.mem.dirty_map[page]) prot = 1;

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
    j = vga.mem.dirty_map[page];
    page &= ~0x30;
    for(k = 0; k < 4; k++, page += 0x10) {
      if(j != vga.mem.dirty_map[page]) {
        vga.mem.dirty_map[page] = j;
        vga_emu_protect(page, 0, prot);
      }
    }
  }

  if(vga.mode_type == PL2) {
    /* it's actually 4 planes, but we let everyone believe it's a 1-plane mode */
    j = vga.mem.dirty_map[page];
    page &= ~0x30;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
    }
    page += 0x20;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
    }
  }

  if(vga.mode_type == CGA) {
    /* CGA uses two 8k banks  */
    j = vga.mem.dirty_map[page];
    page &= ~0x2;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
    }
    page += 0x2;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
    }
  }

  if(vga.mode_type == HERC) {
    /* Hercules uses four 8k banks  */
    j = vga.mem.dirty_map[page];
    page &= ~0x6;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
    }
    page += 0x2;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
    }
    page += 0x2;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
    }
    page += 0x2;
    if(j != vga.mem.dirty_map[page]) {
      vga.mem.dirty_map[page] = j;
      vga_emu_protect(page, 0, prot);
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

  *((volatile char *)(vga.mem.scratch_page << 12));

  i = (int) mmap(
    (caddr_t) (page << 12), 1 << 12,
    VGA_EMU_RW_PROT, MAP_SHARED | MAP_FIXED,
    vga.mem.fd, (off_t) (vga.mem.scratch_page  << 12)
  );

  if(i == -1) return 3;

  return vga_emu_protect_page(page, RO);
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
  PRIV_SAVE_AREA
  int i;
  vga_mapping_type vmt = {0, 0, 0};
  emu_iodev_t io_device;
  static unsigned char *lfb_base = NULL;

  vga.mode = vga.VGA_mode = vga.VESA_mode = 0;

  vga.config.mono_support = config.dualmon ? 0 : 1;

  if(config.vgaemu_memsize)
    vga.mem.size = config.vgaemu_memsize << 10;
  else
    vga.mem.size = 1024 << 10;

  /* force 256k granularity to prevent possible problems
   * (with 4-plane-modes, to be precise)
   */
  vga.mem.size = (vga.mem.size + ~(-1 << 18)) & (-1 << 18);
  vga.mem.pages = vga.mem.size >> 12;

  if((vga.mem.base = (unsigned char *) valloc(vga.mem.size + (1 << 12))) == NULL) {
    vga_msg("vga_emu_init: not enough memory (%u k)\n", vga.mem.size >> 10);
    return 1;
  }
  vga.mem.scratch_page = (unsigned) (vga.mem.base + vga.mem.size) >> 12;
  memset((void *) (vga.mem.scratch_page << 12), 0xff, 1 << 12);
  vga_msg("vga_emu_init: scratch_page at 0x%08x\n", vga.mem.scratch_page << 12);

  if(config.X_lfb) {
    if((lfb_base = (unsigned char *) valloc(vga.mem.size)) == NULL) {
      vga_msg("vga_emu_init: not enough memory (%u k)\n", vga.mem.size >> 10);
    }
  }

  if(lfb_base == NULL) {
    vga_msg("vga_emu_init: linear frame buffer (lfb) disabled\n");
  }

  if((vga.mem.dirty_map = (unsigned char *) malloc(vga.mem.pages)) == NULL) {
    vga_msg("vga_emu_init: not enough memory for dirty map\n");
    return 1;
  }

  /* map the bank of the allocated memory */
  enter_priv_on();	/* for /proc/self/mem I need full privileges */
  vga.mem.fd = open("/proc/self/mem", O_RDWR);
  leave_priv_setting();

  if (vga.mem.fd < 0) {
    vga_msg("vga_emu_init: cannot open /proc/self/mem\n");
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

  vbe_init(vedt);

  /*
   * Make the VGA-BIOS ROM read-only; some dirty programs try to write to the ROM!
   *
   * Note: Unknown instructions will cause the write protection to be removed.
   */
  vga_msg("vga_emu_init: protecting ROM area 0xc0000 - 0x%05x\n", (0xc0 + vgaemu_bios.pages) << 12);
  for(i = 0; i < vgaemu_bios.pages; i++) vga_emu_protect_page(0xc0 + i, RO);

  /*
   * Set some mode; this initializes the DAC, CRTC, etc. as well.
   */
  vga_emu_setmode(3, 80, 25);		/* initialize some mode */

  /* register VGA ports */
  io_device.read_portb = VGA_emulate_inb;
  io_device.write_portb = VGA_emulate_outb;
  io_device.read_portw = NULL;
  io_device.write_portw = NULL;
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
    io_device.handler_name = "VGAEmu Mono/Hercules Card";
    io_device.start_addr = 0x3b0;
    io_device.end_addr = 0x3bf;
    port_register_handler(io_device, 0);
  }

  vga_msg(
    "vga_emu_init: memory: %u kbyte at 0x%x (lfb at 0x%x); %ssupport for mono modes\n",
    vga.mem.size >> 10, (unsigned) vga.mem.base,
    vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page << 12,
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

  vga_msg("dirty_map[0 - 1024k] (0 = RO, 1 = RW = dirty)\n");
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
  if(
    veut->update_pos < veut->display_start ||
    veut->update_pos >= veut->display_end ||
    vga.mode_type == CGA || vga.mode_type == HERC	/* These are special. :-) */
  ) {
    veut->update_pos = veut->display_start;
  }

  start_page = (veut->display_start >> 12);
  end_page = (veut->display_end - 1) >> 12;

  vga_deb_update("vga_emu_update: display = %d (page = %u) - %d (page = %u), update_pos = %d, max_len = %d (max_max_len = %d)\n",
    veut->display_start,
    start_page,
    veut->display_end,
    end_page,
    veut->update_pos,
    veut->max_len,
    veut->max_max_len
  );

#if DEBUG_UPDATE >= 1
  print_dirty_map();
#endif

  for(i = j = veut->update_pos >> 12; i <= end_page && ! vga.mem.dirty_map[i]; i++);
  if(i == end_page + 1) {
    for(i = start_page; i < j && vga.mem.dirty_map[i] == 0; i++);

    if(i == j) {	/* no dirty pages */
/*      veut->update_pos = veut->display_start;	*/
      veut->update_start = veut->update_pos;
      veut->update_len = 0;

      vga_deb_update("vga_emu_update: nothing has changed\n");

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

  vga_deb_update("vga_emu_update: update range: i = %d, j = %d\n", i, j);

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

  vga_deb_update("vga_emu_update: update_start = %d, update_len = %d, update_pos = %d\n",
    veut->update_start,
    veut->update_len,
    veut->update_pos
  );

  return j - i;
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

int vga_emu_setmode(int mode, int width, int height)
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
    vmi = vmi2;
  }

  if(vmi == NULL) {	/* no mode found */
    vga_msg("vga_emu_setmode: no mode 0x%02x found\n", mode);
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
    memset((void *) vga.mem.base, 0, vga.mem.size);
    if(vga.mode_class == TEXT) {
      unsigned *p = (unsigned *) vga.mem.base;
      int i;
      
      for(i = 0; i < 0x2000; i++) p[i] = 0x07200720;
    }
  }

  dirty_all_video_pages();

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

  if(vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page) {
    vga.mem.map[VGAEMU_MAP_LFB_MODE].first_page = 0;
    vga.mem.map[VGAEMU_MAP_LFB_MODE].pages = vga.mem.pages;
    /* vga.mem.map[VGAEMU_MAP_LFB_MODE].base_page is set in vga_emu_init() */
    vga_emu_map(VGAEMU_MAP_LFB_MODE, 0);	/* map the VGA memory to LFB */
  }

  vga_msg("vga_emu_setmode: setting up components...\n");

  DAC_init();
  Attr_init();
  Seq_init();
  CRTC_init();
  GFX_init();
  Misc_init();
  Herc_init();

  vga_msg("vga_emu_setmode: mode initialized\n");

  return True;
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
  if(vga.mode_class != TEXT) {
    vga_msg("vga_emu_set_text_page: not in text mode\n");
    return 1;
  }

  if((page + 1) * page_size > vga.mem.size) {
    vga_msg("vga_emu_set_text_page: page number %d to high\n", page);
    return 2;
  }

  vga_msg("vga_emu_set_text_page %d\n", page);

  vga.display_start = page * page_size;

  return 0;
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

  for(i = 0; i < 256; i++) vga.dac.rgb[i].index = True;		/* index = dirty flag ! */
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

int changed_vga_colors(DAC_entry *de)
{
  int i, j, k;
  unsigned cols;
  unsigned char a;

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
      if(vga.dac.rgb[i].index == True) {
        de[j] = vga.dac.rgb[i]; de[j++].index = i;
        vga.dac.rgb[i].index = False;
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
        vga.dac.rgb[a].index == True ||
        vga.attr.dirty[k] == True
      ) {
        vga.attr.dirty[k] = False;
        de[j] = vga.dac.rgb[a]; de[j++].index = i;
        vga.dac.rgb[a].index = False;
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

  /* apply PEL mask */
  if(j && vga.dac.pel_mask != 0xff) {
    for(i = 0, a = vga.dac.pel_mask; i < j; i++) {
      de[i].r &= a; de[i].g &= a; de[i].b &= a;
    }
  }

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

  if(!vga.config.standard) return;

  switch(what) {
    case CFG_SEQ_ADDR_MODE:
      u0 = vga.seq.addr_mode;
      u = vga.seq.data[4] & 4 ? 0 : 1;
      u = vga.seq.data[4] & 8 ? 2 : u;
      vga.seq.addr_mode = u;
      if(u != u0 && vga.mode_type == P8) {	/* ++HACK++ */
        u1 = vga.seq.addr_mode == 0 ? 4 : 1;
        vga.scan_len = (vga.scan_len * vga.mem.planes) / u1;
        if(u1 != vga.mem.planes) {
          vga.mem.planes = u1; vga.reconfig.mem = 1;   
          vga_msg("vgaemu_adj_cfg: mem reconfig (%u planes)\n", u1);
        }
      }
      if(msg || u != u0) vga_msg("vgaemu_adj_cfg: seq.addr_mode = %s\n", txt1[u]);
    break;

    case CFG_CRTC_ADDR_MODE:
      u0 = vga.crtc.addr_mode;
      u = vga.crtc.data[0x17] & 0x40 ? 0 : 1;
      u = vga.crtc.data[0x14] & 0x40 ? 2 : u;
      vga.crtc.addr_mode = u;
      if(msg || u != u0) vga_msg("vgaemu_adj_cfg: crtc.addr_mode = %s\n", txt2[u]);
    break;

    default:
      vga_msg("vgaemu_adj_cfg: unknown item %u\n", what);
  }
}


