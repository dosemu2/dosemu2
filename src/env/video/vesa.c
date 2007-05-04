/* 
 * Copyright (c) 1997 Steffen Winterfeldt (original code)
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * VESA BIOS Extensions for VGAEmu.
 *
 * Supported are VBE version 2.0, including a linear frame buffer
 * and display power management support.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1995/1996: Initial version by Erik Mouw and Arjan Filius
 *
 * 1997/07/07: Nearly full rewrite. We now support full VBE 2.0
 * functionality, including power management functions (although
 * our only frontend, X, doesn't use this info yet).
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * 1998/12/12: Changed (fixed?) VBE set/get palette function.
 * -- sw
 *
 * 1998/12/14: Another attempt to fix the pm version of the
 * VBE set palette function.
 * -- sw
 *
 * 1999/01/11: Removed vesa_emu_fault(), the last remaining old code piece.
 * -- sw
 *
 * 2000/05/18: Copy VGA fonts into BIOS ROM area.
 * -- sw
 *
 * DANG_END_CHANGELOG
 *
 */


/*
 * define to enable some debug information
 */

#define	DEBUG_VBE		/* general info */


#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "cpu.h"
#include "emu.h"
#include "video.h"
#include "remap.h"
#include "vgaemu.h"
#include "dpmi.h"
#include "vesa.h"
#include "int.h"

#define VBE_BIOS_MAXPAGES	2	/* max. 8k BIOS size, more than enough */

/* identity of our VBE implementation */
#define VBE_OEMVendorName	"DOSEMU-Development-Team"
#define VBE_OEMProdName		"VBE2Emu"
#define VBE_OEMSoftRev		0x100		/* 1.0 */
#define VBE_OEMProductRev	"1.0"

/*
 * Type of display we are attached to. This info is needed to report
 * the color masks of hi/true-color displays back to the DOS app.
 */

static vgaemu_display_type vbe_screen;


/*
 * function prototypes
 */

static int vbe_info(unsigned char *);
static int vbe_mode_info(unsigned, unsigned char *);
static int vbe_set_mode(unsigned);
static int vbe_get_mode(void);
static int vbe_save_restore(unsigned, unsigned, unsigned char *);
static int vbe_display_window(unsigned, unsigned, unsigned);
static int vbe_scan_length(unsigned, unsigned);
static int vbe_display_start(unsigned, unsigned, unsigned);
static int vbe_dac_format(unsigned, unsigned);
static int vbe_palette_data(unsigned, unsigned, unsigned, unsigned char *);
static int vbe_pm_interface(unsigned);
static int vbe_power_state(unsigned, unsigned);


/*
 * DANG_BEGIN_FUNCTION vbe_init
 *
 * description:
 * Initializes the VGA/VBE BIOS and the VBE support.
 *
 * arguments:
 * vedt - Pointer to struct describing the type of display we are actually
 *        attached to.
 *
 * DANG_END_FUNCTION
 *
 */

void vbe_init(vgaemu_display_type *vedt)
{
  int i;
  vga_mode_info *vmi = NULL;
  unsigned char *dos_vga_bios = (unsigned char *) 0xc0000;
  int bios_ptr = (char *) vgaemu_bios_end - (char *) vgaemu_bios_start;

  static struct {
    char modes[3]; 
    char reserved1[4];
    char scanlines; 
    char character_blocks;
    char max_active_blocks;
    short support_flags;
    short reserved2;
    char save_function_flags;
    char reserved3;
  } vgaemu_bios_functionality_table = 
  { modes:		 {0xff,0xe0,0x0f}, /* Modes 0-7, 0dh-0fh, 10h-13h supported */
    scanlines:		 7,		   /* scanlines 200,350,400 supported */	
    character_blocks:	 2,		   /* This all corresponds to a real BIOS */		
    max_active_blocks:	 8,		   /* See Ralf Brown's interrupt list */
    support_flags:	 0xeff,		   /* INT 10h, AH=1b for documentation */
    save_function_flags:0x3f 
  };

  memset(dos_vga_bios, 0, VBE_BIOS_MAXPAGES << 12);	/* one page */
  memcpy(dos_vga_bios, vgaemu_bios_start, bios_ptr);

  vgaemu_bios.prod_name = (char *) vgaemu_bios_prod_name - (char *) vgaemu_bios_start;

  if (Video->setmode) {
    i = (char *) vgaemu_bios_pm_interface_end - (char *) vgaemu_bios_pm_interface;

    if(i + bios_ptr > (VBE_BIOS_MAXPAGES << 12) - 8) {
      error("VBE: vbe_init: protected mode interface to large, disabling\n");
      vgaemu_bios.vbe_pm_interface_len =
	vgaemu_bios.vbe_pm_interface = 0;
    }

    vgaemu_bios.vbe_pm_interface_len = i;
    vgaemu_bios.vbe_pm_interface = bios_ptr;
    memcpy(dos_vga_bios + bios_ptr, vgaemu_bios_pm_interface, i);
    bios_ptr += i;

    bios_ptr = (bios_ptr + 3) & ~3;
    vgaemu_bios.vbe_mode_list = bios_ptr;

    /* set up video mode list */
    for(i = 0x100; i <= vgaemu_bios.vbe_last_mode; i++) {
      if((vmi = vga_emu_find_mode(i, NULL))) {
	if(vmi->VESA_mode != -1 && bios_ptr < ((VBE_BIOS_MAXPAGES << 12) - 4)) {
	  *((unsigned short *) (dos_vga_bios + bios_ptr)) = vmi->VESA_mode;
	  bios_ptr += 2;
	}
      }
    }

    *((short *) (dos_vga_bios + bios_ptr)) = -1;
    bios_ptr += 2;

    /* add fonts */
    vgaemu_bios.font_8 = bios_ptr;
    memcpy(dos_vga_bios + bios_ptr, vga_rom_08, sizeof vga_rom_08);
    bios_ptr += sizeof vga_rom_08;

    vgaemu_bios.font_14 = bios_ptr;
    memcpy(dos_vga_bios + bios_ptr, vga_rom_14, sizeof vga_rom_14);
    bios_ptr += sizeof vga_rom_14;

    vgaemu_bios.font_16 = bios_ptr;
    memcpy(dos_vga_bios + bios_ptr, vga_rom_16, sizeof vga_rom_16);
    bios_ptr += sizeof vga_rom_16;

    vgaemu_bios.font_14_alt = bios_ptr;
    memcpy(dos_vga_bios + bios_ptr, vga_rom_14_alt, sizeof vga_rom_14_alt);
    bios_ptr += sizeof vga_rom_14_alt;

    vgaemu_bios.font_16_alt = bios_ptr;
    memcpy(dos_vga_bios + bios_ptr, vga_rom_16_alt, sizeof vga_rom_16_alt);
    bios_ptr += sizeof vga_rom_16_alt;
  } else {
    /* only support the initial video mode, can't change it */
    vgaemu_bios_functionality_table.modes[0] = 1 << video_mode;
    vgaemu_bios_functionality_table.modes[1] =
      vgaemu_bios_functionality_table.modes[2] = 0;
  }
  vgaemu_bios.functionality = bios_ptr;
  memcpy(dos_vga_bios + bios_ptr, &vgaemu_bios_functionality_table,
      sizeof vgaemu_bios_functionality_table);
  bios_ptr += sizeof vgaemu_bios_functionality_table;

  vgaemu_bios.size = bios_ptr;

  dos_vga_bios[2] = (bios_ptr + ((1 << 9) - 1)) >> 9;
  vgaemu_bios.pages = (bios_ptr + ((1 << 12) - 1)) >> 12;

  if (config.vbios_file) {
    /* EXPERIMENTAL: load and boot the Bochs BIOS */
    int fd = open(config.vbios_file, O_RDONLY);
    if (fd != -1) {
      read(fd, (void*)0xc0000, 32768);
      close(fd);
      vgaemu_bios.pages = 8;
      config.vbios_post = 1;
    }
  }

  memcheck_addtype('V', "VGAEMU Video BIOS");
  memcheck_reserve('V', (size_t)dos_vga_bios, vgaemu_bios.pages << 12);

  if(!config.X_pm_interface) {
    v_printf("VBE: vbe_init: protected mode interface disabled\n");
  }

  v_printf(
    "VBE: vbe_init: %d pages for VGA BIOS, vga.mem.base = %p\n",
    vgaemu_bios.pages, vga.mem.base
  );

  if(!vedt)
    return;

  vbe_screen = *vedt;

  v_printf(
    "VBE: vbe_init: real display: bits/pixel = %d, color bits (rgb) = %d/%d/%d\n",
    vbe_screen.bits, vbe_screen.r_bits, vbe_screen.g_bits, vbe_screen.b_bits
  );

  v_printf("VBE: vbe_init: supported VBE mode types = 0x%x\n", vbe_screen.src_modes);
}


/*
 * DANG_BEGIN_FUNCTION do_vesa_int
 *
 * description:
 * This is the VESA interrupt handler.
 *
 * It is called from base/bios/int10.c::int10(). The VESA interrupt is called
 * with 0x4f in AH and the function number (0x00 ... 0x10) in AL.
 *
 * DANG_END_FUNCTION
 */

void do_vesa_int()
{
  int err_code = VBE_ERROR_GENERAL_FAIL;
  
#if 0
  v_printf(
    "VBE: function 0x%02x, bx = 0x%04x cx = 0x%04x, dx = 0x%04x, es = 0x%04x, di = 0x%04x\n",
    (unsigned) _AL, (unsigned) _BX, (unsigned) _CX, (unsigned) _DX, (unsigned) _ES, (unsigned) _DI
  );
#endif

  switch(_AL) {
    case 0x00:		/* return VBE controller info */
      err_code = vbe_info(SEG_ADR((unsigned char *), es, di));
      break;

    case 0x01:		/* return VBE mode info */
      err_code = vbe_mode_info(_CX, SEG_ADR((unsigned char *), es, di));
      break;  

    case 0x02:		/* set VBE mode */
      err_code = vbe_set_mode(_BX);
      break;

    case 0x03:		/* get current VBE mode */
      err_code = vbe_get_mode();
      break;

    case 0x04:		/* save/restore state */
      err_code = vbe_save_restore(_DL, _CX, SEG_ADR((unsigned char *), es, bx));
      break;

    case 0x05:		/* display window control (aka set/get bank) */
      err_code = vbe_display_window(_BH, _BL, _DL /* must be _DX !!!*/);
      break;

    case 0x06:		/* set/get logical scan line length */
      err_code = vbe_scan_length(_BL, _CX);
      break;

    case 0x07:		/* set/get display start */
      err_code = vbe_display_start(_BL, _CX, _DX);
      break;

    case 0x08:		/* set/get DAC palette format */
      err_code = vbe_dac_format(_BL, _BH);
      break;

    case 0x09:		/* set/get palette data */
      err_code = vbe_palette_data(_BL, _CX, _DX, SEG_ADR((unsigned char *), es, di));
      break;

    case 0x0a:		/* return VBE PM interface */
      err_code = vbe_pm_interface(_BL);
      break;

    case 0x10:		/* set/get display power state */
      err_code = vbe_power_state(_BL, _BH);
      break;

    default:
      err_code = VBE_ERROR_UNSUP;
#ifdef DEBUG_VBE
      v_printf(
        "VBE: unsupported function 0x%02x, retval = %d, bx = 0x%04x cx = 0x%04x, dx = 0x%04x, es = 0x%04x, di = 0x%04x\n",
        (unsigned) _AL, err_code, (unsigned) _BX, (unsigned) _CX, (unsigned) _DX, (unsigned) _ES, (unsigned) _DI
      );
#endif
  }

 if(err_code >= 0) {
   _AL = 0x4f;
   _AH = (unsigned char) err_code;
 }
}


/*
 * VBE function 0x00
 *
 * Return VBE controller info.
 *
 * This function never fails (of course).
 *
 * arguments:
 * vbei - pointer to VBE info block
 *
 */

#define VBE_BUFFER vbei
static int vbe_info(unsigned char *vbei)
{
  int vbe2 = 0;
  int i;

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x] vbe_info: es:di = 0x%04x:0x%04x\n",
    (unsigned) _AL, (unsigned) _ES, (unsigned) _DI
  );
#endif

  if(VBE_viVBESig == 0x32454256 /* "VBE2" */) vbe2 = 1;

#ifdef DEBUG_VBE
  if(vbe2) v_printf("VBE: [0x%02x] vbe_info: VBE2 info requested\n", (unsigned) _AL);
#endif

  memset(vbei, 0, vbe2 ? 0x200 : 0x100);

  VBE_viVBESig = 0x41534556;		/* "VESA" */
  VBE_viVESAVersion = 0x200;		/* 2.0 */
  VBE_viOEMID = VBE_SEG_OFS(0xc000, vgaemu_bios.prod_name);
  VBE_viCapabilities = 1;		/* 6/8 bit switchable DAC */
  VBE_viModeList = VBE_SEG_OFS(0xc000, vgaemu_bios.vbe_mode_list);
  VBE_viMemory = vga.mem.pages >> 4;	/* multiples of 64 kbyte */

  if(vbe2) {
    i = VBE_OEMData;
    VBE_viOEMSoftRev = VBE_OEMSoftRev;

    strcpy(vbei + i, VBE_OEMVendorName);
    VBE_viOEMVendorName = VBE_SEG_OFS(_ES, _DI + i);
    i += strlen(VBE_OEMVendorName) + 1;

    strcpy(vbei + i, VBE_OEMProdName);
    VBE_viOEMProdName = VBE_SEG_OFS(_ES, _DI + i);
    i += strlen(VBE_OEMProdName) + 1;

    strcpy(vbei + i, VBE_OEMProductRev);
    VBE_viOEMProductRev = VBE_SEG_OFS(_ES, _DI + i);
    i += strlen(VBE_OEMProductRev) + 1;
  }

#ifdef DEBUG_VBE
  v_printf("VBE: [0x%02x] vbe_info: return value 0\n", (unsigned) _AL);
#endif

  return 0;
}
#undef VBE_BUFFER


/*
 * VBE function 0x01
 *
 * Return VBE mode info.
 *
 * Fills out a special VBE mode info block. Fails if `mode' is
 * not in our mode list.
 *
 * arguments:
 * mode - VBE mode number
 * vbemi - pointer to VBE mode info block
 *
 */

#define VBE_BUFFER vbemi
static int vbe_mode_info(unsigned mode, unsigned char *vbemi)
{
  vga_mode_info *vmi;
  int err_code = VBE_ERROR_GENERAL_FAIL;
  unsigned u;
  unsigned mode_size;
  unsigned scan_len;

  mode &= 0xffff;

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x] vbe_mode_info: cx = 0x%04x, es:di = 0x%04x:0x%04x\n",
    (unsigned) _AL, (unsigned) _CX, (unsigned) _ES, (unsigned) _DI
  );
#endif

  memset(vbemi, 0, 0x100);	/* Do it always or only for valid modes? -- sw */

  if((vmi = vga_emu_find_mode(mode, NULL))) {
    err_code = 0;

    if(vmi->mode_class == GRAPH) {
      scan_len = vmi->color_bits;
      if(vmi->type == PL4) scan_len = 1;	/* 4 planes ! */
      if(vmi->type == P15) scan_len = 16;
      scan_len *= vmi->width;
      scan_len >>= 3;	/* in bytes */
      mode_size = scan_len * vmi->height;
    }
    else {
      scan_len = vmi->text_width * 2;
      mode_size = 0;
    }

    u = 1 << 1;		/* reserved bit, always 1 */

    if(
      (vmi->type == P8 && vbe_screen.src_modes & (MODE_PSEUDO_8 | MODE_TRUE_8)) ||
      (vmi->type == P15 && vbe_screen.src_modes & MODE_TRUE_15) ||
      (vmi->type == P16 && vbe_screen.src_modes & MODE_TRUE_16) ||
      (vmi->type == P24 && vbe_screen.src_modes & MODE_TRUE_24) ||
      (vmi->type == P32 && vbe_screen.src_modes & MODE_TRUE_32)
      /* no PL4 and PL1 modes for now */
    ) {
      u |= (1 << 0);					/* mode supported */
    }

    if(mode_size > vga.mem.size) u &= ~(1 << 0);	/* mode not supported */

#if 0
    if(vmi->mode_class == TEXT) u |= 0 << 2;		/* TTY support */
#else
    u |= 1 << 2;				/* TTY support */
#endif
    if(vmi->type != TEXT_MONO && vmi->type != HERC)
      u |= 1 << 3;				/* color mode */
    if(vmi->mode_class == GRAPH) u |= 1 << 4;	/* graphics mode */
    if(! vmi->buffer_start) u |= 1 << 6;	/* window support */
    if(vmi->color_bits >= 8 && vga.mem.lfb_base_page) u |= 1 << 7;	/* LFB support */
    VBE_vmModeAttrib = u;

    VBE_vmWinAAttrib = 7;
    VBE_vmWinBAttrib = 0;			/* not supported */

    VBE_vmWinGran = vmi->buffer_len;		/* in kbyte */
    VBE_vmWinSize = vmi->buffer_len;		/* in kbyte */

    VBE_vmWinASeg = vmi->buffer_start;		/* segment address */
    VBE_vmWinBSeg = 0;				/* not supported */

    VBE_vmWinFuncPtr = VBE_SEG_OFS(0xc000, (char *) vgaemu_bios_win_func - (char *) vgaemu_bios_start);

    VBE_vmBytesPLine = scan_len;
    if(vmi->mode_class == GRAPH) {
      VBE_vmXRes = vmi->width;
      VBE_vmYRes = vmi->height;
    }
    else {
      VBE_vmXRes = vmi->text_width;
      VBE_vmYRes = vmi->text_height;
    }
    VBE_vmXCharSize = vmi->char_width;
    VBE_vmYCharSize = vmi->char_height;

    VBE_vmNumPlanes = (vmi->mode_class == TEXT || vmi->type == PL4 || vmi->type == NONCHAIN4) ? 4 : 1;
    VBE_vmBitsPPixel = vmi->color_bits;
    VBE_vmBanks = vmi->type == HERC ? 4 : vmi->type == CGA ? 2 : 1;
    VBE_vmBankSize = (vmi->type == HERC || vmi->type == CGA) ? 8 : 0;	/* in kbyte */

    VBE_vmMemModel = vmi->type >= P15 && vmi->type <= P32 ? DIRECT : vmi->type;

    if(mode_size) {
      u = vga.mem.size / mode_size;
      if(u > 0x100) u = 0x100;
      if(u == 0) u = 1;
      VBE_vmPages = u - 1;
    }

    if(VBE_vmPages > 7) VBE_vmPages = 7;

    VBE_vmReserved1 = 1;

    if(vmi->type >= P15 && vmi->type <= P32) {
      int r_bits, g_bits, b_bits, v_bits = 0;
      int r_shift, g_shift, b_shift, v_shift = 0;
      int color_bits = 0;

      if(
        (vmi->type == P15 && vbe_screen.bits == 15) ||
        (vmi->type == P16 && vbe_screen.bits == 16) ||
        (vmi->type == P24 && vbe_screen.bits == 24) ||
        (vmi->type == P32 && vbe_screen.bits == 32)
      ) {
        r_bits = vbe_screen.r_bits;
        g_bits = vbe_screen.g_bits;
        b_bits = vbe_screen.b_bits;
        r_shift = vbe_screen.r_shift;
        g_shift = vbe_screen.g_shift;
        b_shift = vbe_screen.b_shift;
        color_bits = vbe_screen.bits;
      }
      else {
        switch(vmi->type) {
          case P15:
            r_bits = g_bits = b_bits = 5;
            b_shift = 0; g_shift = 5; r_shift = 10;
            color_bits = 15;
            break;

          case P16:
            r_bits = b_bits = 5; g_bits = 6;
            b_shift = 0; g_shift = 5; r_shift = 11;
            color_bits = 16;
            break;

          case P24:
            r_bits = g_bits = b_bits = 8;
            b_shift = 0; g_shift = 8; r_shift = 16;
            color_bits = 24;
            break;

          case P32:
            r_bits = g_bits = b_bits = 8;
            b_shift = 0; g_shift = 8; r_shift = 16;
            color_bits = 32;
            break;

          default:
            r_bits = g_bits = b_bits = r_shift = g_shift = b_shift = 0;
            v_printf("VBE: vbe_mode_info: internal error\n");
        }
      }

      /* We cannot (reasonably) detect the position of the reserved field
       * here, so we do some simple guessing for now.
       */
      v_shift = r_bits + g_bits + b_bits;
      v_bits = color_bits - v_shift;
      if(v_bits < 0) v_bits = 0;

      VBE_vmRedMaskSize = r_bits;
      VBE_vmRedFieldPos = r_shift;
      VBE_vmGreenMaskSize = g_bits;
      VBE_vmGreenFieldPos = g_shift;
      VBE_vmBlueMaskSize = b_bits;
      VBE_vmBlueFieldPos = b_shift;
      VBE_vmRsvdMaskSize = v_bits;
      VBE_vmRsvdFieldPos = v_shift;
    }

    VBE_vmDirectColor = 0;
    if(vmi->color_bits >= 8 && vga.mem.lfb_base_page)
      VBE_vmPhysBasePtr = vga.mem.lfb_base_page << 12;	/* LFB support */

    VBE_vmOffScreenOfs = 0;
    VBE_vmOffScreenMem = 0;
  }

#ifdef DEBUG_VBE
  v_printf("VBE: [0x%02x] vbe_mode_info: return value %d\n", (unsigned) _AL, err_code);
#endif

  return err_code;
}
#undef VBE_BUFFER


/*
 * VBE function 0x02
 * 
 * Set VBE mode.
 *
 * Calls base/bios/int10.c::set_video_mode() to actually set
 * the mode. Accepts standard VGA mode numbers (<= 0x7f) as well.
 *
 * arguments:
 * mode - the mode number
 *
 */

static int vbe_set_mode(unsigned mode)
{
  vga_mode_info *vmi;
  int err_code = VBE_ERROR_GENERAL_FAIL;

  mode &= 0xffff;

#ifdef DEBUG_VBE
  v_printf("VBE: [0x%02x] vbe_set_mode: bx = 0x%04x\n", (unsigned) _AL, (unsigned) _BX);
#endif

  if((vmi = vga_emu_find_mode(mode, NULL))) {
    if(mode < 0x80 || mode >= 0x100) {
      err_code = 0;
      if(!set_video_mode(mode)) err_code = 1;
    }
  }

#ifdef DEBUG_VBE
  v_printf("VBE: [0x%02x] vbe_set_mode: return value %d\n", (unsigned) _AL, err_code);
#endif

  return err_code;
}


/*
 * VBE function 0x03
 *
 * Return the current video mode.
 *
 * This function will return the very mode number that was used
 * to set the mode (including memory-clear & lfb bits).
 *
 */

static int vbe_get_mode()
{
#ifdef DEBUG_VBE
  v_printf("VBE: [0x%02x] vbe_get_mode\n", (unsigned) _AL);
#endif

  _BX = (unsigned short) vga.mode;

#ifdef DEBUG_VBE
  v_printf("VBE: [0x%02x] vbe_get_mode: return value 0, bx = 0x%04x\n", (unsigned) _AL, (unsigned) _BX);
#endif
  return 0;
}


/*
 * VBE function 0x04
 *
 * Save/restore graphics state.
 *  - unimplemented for now -
 *
 * arguments:
 * sub_func - save/restore/get info
 * mask     - select which part to save/restore
 * buffer   - save/restore buffer
 *
 */

static int vbe_save_restore(unsigned sub_func, unsigned mask, unsigned char *buffer)
{
  int err_code = 0;

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_save_restore: dl = 0x%02x, cx = 0x%04x, es:bx = 0x%04x:0x%04x\n",
    (unsigned) _AL, sub_func, (unsigned) _DL, (unsigned) _CX, (unsigned) _ES, (unsigned) _BX
  );
#endif

  err_code = VBE_ERROR_UNSUP;	/* not implemented */

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u]: return value = %d, bx = 0x%04x\n",
    (unsigned) _AL, sub_func, err_code, (unsigned) _BX
  );
#endif
  
  return err_code;
}


/*
 * VBE function 0x05
 * 
 * Set or get the currently mapped memory bank.
 * This function fails when called in an LFB mode.
 *
 * arguments:
 * sub_func - subfunction (0 = set, 1 = get)
 * window   - the window, (0 = A, 1 = B; only A is supported)
 * bank     - the bank number
 *
 */

static int vbe_display_window(unsigned sub_func, unsigned window, unsigned bank)
{
  int err_code = 0;
  int direct = 0;

  if(sub_func & 0x40) {
    sub_func &= ~0x40;
    direct = 1;
  }

#ifdef DEBUG_VBE
  if(sub_func) {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_window::get: bl = 0x%02x\n",
      (unsigned) _AL, sub_func, (unsigned) _BL
    );
  }
  else {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_window::set: bl = 0x%02x, dx = 0x%04x\n",
      (unsigned) _AL, sub_func, (unsigned) _BL, (unsigned) _DX
    );
  }
  if(direct) {
    v_printf("VBE: [0x%02x.%u] vbe_display_window: accessed via direct call\n", (unsigned) _AL, sub_func);
  }
#endif

  if(vga.mode & (1 << 14)) {
    err_code = VBE_ERROR_MODE_FAIL;	/* fail in LFB mode */
  }
  else if(window != 0) {
    err_code = VBE_ERROR_GENERAL_FAIL;	/* we support only Window A */
  }
  else if(sub_func > 1) {
    err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
  }
  else if(sub_func) {
    _DX = vga.mem.bank;
  }
  else {
    if(! vga_emu_switch_bank(bank)) {
       err_code = VBE_ERROR_GENERAL_FAIL;	/* failed */
    }
  }

#ifdef DEBUG_VBE
  if(sub_func) {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_window::get: return value %d, dx = 0x%04x\n",
      (unsigned) _AL, sub_func, err_code, (unsigned) _DX
    );
  }
  else {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_window::set: return value %d\n",
      (unsigned) _AL, sub_func, err_code
    );
  }
#endif
  
  return err_code;
}


/*
 * VBE function 0x06
 *
 * Set or get the length of a logical scan line, query the maximum
 * scan line length.
 *
 * arguments:
 * sub_func - subfunction (0 = set, 1 = get, 2 = set, 3 = get max len)
 * scan_len - the new length (sub_func = 0: in pixels, sub_func = 2: in bytes)
 *
 */

int vbe_scan_length(unsigned sub_func, unsigned scan_len)
{
  int err_code = 0;
  unsigned u0 = 0, u1 = 0, u2 = 0;
  unsigned max_pixels, max_bytes, max_mem;
  unsigned planes = 1;

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_scan_length: bl = 0x%02x, cx = 0x%04x\n",
    (unsigned) _AL, sub_func, (unsigned) _BL, (unsigned) _CX
  );
#endif

  if(vga.mode_class == TEXT) {
    max_bytes = (0x8000 / vga.text_height) & ~1;
    max_pixels = (max_bytes >> 1) * vga.char_width;

    switch(sub_func) {
      case 0:	/* set scan len in pixels */
        if(scan_len < vga.width || scan_len % vga.char_width || scan_len > max_pixels) {
          err_code = VBE_ERROR_HARDWARE_FAIL;
          break;
        }
        u0 = vga.scan_len = (scan_len / vga.char_width) << 1;
        u1 = scan_len;
        break;

      case 1:	/* get current config */
        u0 = vga.scan_len;
        u1 = (vga.scan_len >> 1) * vga.char_width;
        break;

      case 2:	/* set scan len in bytes */
        if(scan_len < (vga.width / vga.char_width) << 1 || scan_len > max_bytes || scan_len & 1) {
          err_code = VBE_ERROR_HARDWARE_FAIL;
          break;
        }
        u0 = vga.scan_len = scan_len;
        u1 = (scan_len >> 1) * vga.char_width;
        break;

      case 3:	/* get maximum config */
        u0 = max_bytes;
        u1 = max_pixels;
        break;

      default:
        err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
    }

    if(!err_code) {
      u2 = 0x8000 / u0;		/* max scan lines */
      u2 *= vga.char_height;
    }
  }
  else {
    max_mem = vga.mem.size;
    if((vga.mode_type == PL4 || vga.mode_type == NONCHAIN4 ||
       vga.mode_type == HERC || vga.mode_type == CGA) && max_mem > 256 * 1024
    ) {
      max_mem = 256 * 1024;	/* to avoid brain damage... ;-) */
    }

    if(vga.mode_type == PL4 || vga.mode_type == NONCHAIN4) planes = 4;
    max_bytes = ((max_mem / vga.height) / planes) & ~3;		/* 4 byte aligned */
    max_pixels = ((max_bytes << 3) / vga.pixel_size) * planes;

    switch(sub_func) {
      case 0:	/* set scan len in pixels */
        u0 = (((scan_len * vga.pixel_size) >> 3) / planes  + 3) & ~3;
        u1 = ((u0 << 3) / vga.pixel_size) * planes;
        if(u1 < vga.width || u1 > max_pixels || u1 > 0x7fff || u0 > 0x7fff /* || u1 > vga.width + 100 */) {
          err_code = VBE_ERROR_HARDWARE_FAIL;
          break;
        }
        vga.scan_len = u0;
        vga.reconfig.display = 1;
	vga.reconfig.re_init = 1;
        break;

      case 1:	/* get current config */
        u0 = vga.scan_len;
        u1 = ((u0 << 3) / vga.pixel_size) * planes;
        break;

      case 2:	/* set scan len in bytes */
        u0 = (scan_len + 3) & ~3;
        u1 = ((u0 << 3) / vga.pixel_size) * planes;
        if(u1 < vga.width || u1 > max_pixels || u1 > 0x7fff || u0 > 0x7fff) {
          err_code = VBE_ERROR_HARDWARE_FAIL;
          break;
        }
        vga.scan_len = u0;
        vga.reconfig.display = 1;
	vga.reconfig.re_init = 1;
        break;

      case 3:	/* get maximum config */
        u0 = max_bytes;
        u1 = max_pixels;
        break;

      default:
        err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
    }

    if(!err_code) {
      u2 = (max_mem / u0) / planes;		/* max scan lines */
    }

  }

  if(!err_code) {
    _BX = u0;
    _CX = u1;
    _DX = u2;
  }

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_scan_length: return value %d, bx = 0x%04x, cx = 0x%04x, dx = 0x%04x\n",
    (unsigned) _AL, sub_func, err_code, (unsigned) _BX, (unsigned) _CX, (unsigned) _DX
  );
#endif
  
  return err_code;
}


/*
 * VBE function 0x07
 *
 * Set or get the display start.
 *  - does currently not work in text modes -
 *
 * arguments:
 * sub_func - subfunction (0 = set, 1 = get)
 * x, y     - display start
 *
 */

int vbe_display_start(unsigned sub_func, unsigned x, unsigned y)
{
  int err_code = 0;
  unsigned u;
  int direct = 0;

  sub_func &= ~0x80;	/* we don't have vertical retrace anyway */
  if(sub_func & 0x40) {
    sub_func &= ~0x40;
    direct = 1;
  }

#ifdef DEBUG_VBE
  if(sub_func) {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_start::get: bl = 0x%02x\n",
      (unsigned) _AL, sub_func, (unsigned) _BL
    );
  }
  else {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_start::set: bl = 0x%02x, cx = 0x%04x, dx = 0x%04x\n",
      (unsigned) _AL, sub_func, (unsigned) _BL, (unsigned) _CX, (unsigned) _DX
    );
  }
  if(direct) {
    v_printf("VBE: [0x%02x.%u] vbe_display_start: accessed via direct call\n", (unsigned) _AL, sub_func);
  }
#endif

  if(sub_func > 1) {
    err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
  }
  else if(sub_func) {
    u = vga.display_start / vga.scan_len;
    _DX = u;
    u = vga.display_start % vga.scan_len;
    if(vga.pixel_size > 8) u /= (vga.pixel_size + 7) >> 3;
    _CX = u;
  }
  else {
    if(direct) {
      vga.display_start = ((y << 16) + x) << 2;
    }
    else {
      if(vga.pixel_size > 8) x *= (vga.pixel_size + 7) >> 3;
      u = y * vga.scan_len + x;
      vga.display_start = u;
    }
  }

#ifdef DEBUG_VBE
  if(sub_func) {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_start::get: return value %d, cx = 0x%04x, dx = 0x%04x\n",
      (unsigned) _AL, sub_func, err_code, (unsigned) _CX, (unsigned) _DX
    );
  }
  else {
    v_printf(
      "VBE: [0x%02x.%u] vbe_display_start::set: return value %d\n",
      (unsigned) _AL, sub_func, err_code
    );
  }
#endif
  
  return err_code;
}


/*
 * VBE function 0x08
 *
 * Set or get DAC palette format.
 *
 * arguments:
 * sub_func - subfunction (0 = set, 1 = get)
 * bits     - new DAC width (should 6 or 8; our DAC emulation supports 4...8)
 *
 */

int vbe_dac_format(unsigned sub_func, unsigned bits)
{
  int err_code = 0;

#ifdef DEBUG_VBE
  if(sub_func) {
    v_printf(
      "VBE: [0x%02x.%u] vbe_dac_format::get: bl = 0x%02x\n",
      (unsigned) _AL, sub_func, (unsigned) _BL
    );
  }
  else {
    v_printf(
      "VBE: [0x%02x.%u] vbe_dac_format::set: bl = 0x%02x, bh = 0x%02x\n",
      (unsigned) _AL, sub_func, (unsigned) _BL, (unsigned) _BH
    );
  }
#endif

  if(sub_func > 1) {
    err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
  }
  else {
    if(vga.mode_type == YUV || vga.mode_type == DIRECT || (vga.mode_type >= P15 && vga.mode_type <= P32)) {
      err_code = VBE_ERROR_MODE_FAIL;
    }
    if(sub_func) {
      _BH = vga.dac.bits;
    }
    else {
      DAC_set_width(bits);
      _BH = vga.dac.bits;
    }
  }

#ifdef DEBUG_VBE
  if(sub_func) {
    v_printf(
      "VBE: [0x%02x.%u] vbe_dac_format::get: return value %d, bh = 0x%02x\n",
      (unsigned) _AL, sub_func, err_code, (unsigned) _BH
    );
  }
  else {
    v_printf(
      "VBE: [0x%02x.%u] vbe_dac_format::set: return value %d, bh = 0x%02x\n",
      (unsigned) _AL, sub_func, err_code, (unsigned) _BH
    );
  }
#endif
  
  return err_code;
}


/*
 * VBE function 0x09
 *
 * Set or get palette data.
 * We don't support a secondary palette.
 *
 * arguments:
 * sub_func - subfunction (0 = set, 1 = get)
 * len      - number of registers to update
 * first    - the first palette to update
 * buffer   - table with RGB values (4 bytes per palette, lowest byte for alignment)
 *
 */

int vbe_palette_data(unsigned sub_func, unsigned len, unsigned first, unsigned char *buffer)
{
  int err_code = 0;
  DAC_entry dac;

  if(sub_func & 0x40) {		/* called via pm interface */
    unsigned base, ofs;

    sub_func &= ~0x40;
    ofs = (_SI << 16) + _DI;
    base = dpmi_GetSegmentBaseAddress(_BP);
    buffer = (unsigned char *)(uintptr_t) (base + ofs);
#ifdef DEBUG_VBE
    v_printf(
      "VBE: [0x%02x.%u] vbe_palette_data: called via pm interface, es.sel = 0x%04x, es.base = 0x%08x, es.ofs = 0x%08x\n",
      (unsigned) _AL, sub_func, (unsigned) _BP, base, ofs
    );
#endif
  }

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_palette_data: bl = 0x%02x, cx = 0x%04x, dx = 0x%04x, es:di = 0x%04x:0x%04x\n",
    (unsigned) _AL, sub_func, (unsigned) _BL, (unsigned) _CX, (unsigned) _DX, (unsigned) _ES, (unsigned) _DI
  );
#endif

  switch(sub_func) {
    case    0:
    case 0x80:	/* set palette */
      while(first < 256 && len) {
        DAC_set_entry(first, buffer[2], buffer[1], buffer[0]);	/* index, r, g, b */
        first++; len--; buffer += 4;
      }
      break;

    case 1:
      while(first < 256 && len) {
        dac.index = first;
        DAC_get_entry(&dac);
        buffer[2] = dac.r;
        buffer[1] = dac.g;
        buffer[0] = dac.b;
        first++; len--; buffer += 4;
      }
      break;

    case 2:
    case 3:
      err_code = VBE_ERROR_HARDWARE_FAIL;	/* no secondary palette */
      break;

    default:
      err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
  }

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_palette_data: return value %d\n",
    (unsigned) _AL, sub_func, err_code
  );
#endif
  
  return err_code;
}


/*
 * VBE function 0x0a
 *
 * Return VBE protected mode interface.
 * This function fails if X { pm_interface } in dosemu.conf is 'off'.
 *
 * arguments:
 * sub_func - subfunction (0 = get)
 *
 */

int vbe_pm_interface(unsigned sub_func)
{
  int err_code = 0;

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_pm_interface: bl = 0x%02x\n",
    (unsigned) _AL, sub_func, (unsigned) _BL
  );
#endif

  if(sub_func == 0) {
    if(vgaemu_bios.vbe_pm_interface_len && config.X_pm_interface) {
      _CX = vgaemu_bios.vbe_pm_interface_len;
      _ES = 0xc000;
      _DI = vgaemu_bios.vbe_pm_interface;
    }
    else {
      err_code = VBE_ERROR_UNSUP;
    }

  } else {
    err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
  }

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_pm_interface: return value %d, cx = 0x%04x, es:di = 0x%04x:0x%04x\n",
    (unsigned) _AL, sub_func, err_code, (unsigned) _CX, (unsigned) _ES, (unsigned) _DI
  );
#endif
  
  return err_code;
}


/*
 * VBE/PM function 0x10
 *
 * Set or get display power state.
 * This just sets a variable in the vga state. It's up to
 * the frontend to do some useful things.
 *
 * arguments:
 * sub_func - subfunction (0 = get info, 1 = set, 2 = get)
 * state    - power state (bitmask: 0, 1, 2, 4, 8)
 *
 */

int vbe_power_state(unsigned sub_func, unsigned state)
{
  int err_code = 0;

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_power_state: bl = 0x%02x, bh = 0x%02x\n",
    (unsigned) _AL, sub_func, (unsigned) _BL, (unsigned) _BH
  );
#endif

  switch(sub_func) {
    case 0:	/* get capabilities */
      _BL = 0x20;	/* version (2.0) */
      _BH = 0x0f;	/* support everything (bit 0 - 3: standby, suspend, off, reduced) */
      break;

    case 1:
      if(state == 0 || state == 1 || state == 2 || state == 4 || state == 8) {
        vga.power_state = state;
      }
      else {
        err_code = VBE_ERROR_GENERAL_FAIL;
      }
      break;

    case 2:
      _BH =  vga.power_state;
      break;

    default:
      err_code = VBE_ERROR_GENERAL_FAIL;	/* invalid subfunction */
  }

#ifdef DEBUG_VBE
  v_printf(
    "VBE: [0x%02x.%u] vbe_power_state: return value %d, bl = 0x%02x, bh = 0x%02x\n",
    (unsigned) _AL, sub_func, err_code, (unsigned) _BL, (unsigned) _BH
  );
#endif
  
  return err_code;
}

