/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The VGA CRT Controller emulator for VGAEmu.
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1999/01/05: Correct initial values for standard VGA modes.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * DANG_END_CHANGELOG
 *
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the CRT Controller.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define	DEBUG_CRTC	0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if !defined True
#define False 0
#define True 1
#endif

#define crtc_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_CRTC >= 1
#define crtc_deb(x...) v_printf("VGAEmu: " x)
#else
#define crtc_deb(x...)
#endif

#if DEBUG_CRTC >= 2
#define crtc_deb2(x...) v_printf("VGAEmu: " x)
#else
#define crtc_deb2(x...)
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "vgaemu.h"
#include "video.h"
#include "memory.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned char crtc_ival[16][CRTC_MAX_INDEX + 1] = {
  {0x2d, 0x27, 0x28, 0x90, 0x2b, 0xa0, 0xbf, 0x1f, 0x00, 0x4f, 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x14, 0x1f, 0x96, 0xb9, 0xa3, 0xff},
  {0x2d, 0x27, 0x28, 0x90, 0x2b, 0xa0, 0xbf, 0x1f, 0x00, 0x4f, 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x14, 0x1f, 0x96, 0xb9, 0xa3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f, 0x00, 0x4f, 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x28, 0x1f, 0x96, 0xb9, 0xa3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f, 0x00, 0x4f, 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x28, 0x1f, 0x96, 0xb9, 0xa3, 0xff},
  {0x2d, 0x27, 0x28, 0x90, 0x2b, 0x80, 0xbf, 0x1f, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x14, 0x00, 0x96, 0xb9, 0xa2, 0xff},
  {0x2d, 0x27, 0x28, 0x90, 0x2b, 0x80, 0xbf, 0x1f, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x14, 0x00, 0x96, 0xb9, 0xa2, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x28, 0x00, 0x96, 0xb9, 0xc2, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f, 0x00, 0x4f, 0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x28, 0x0f, 0x96, 0xb9, 0xa3, 0xff},

  {0x2d, 0x27, 0x28, 0x90, 0x2b, 0x80, 0xbf, 0x1f, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x14, 0x00, 0x96, 0xb9, 0xe3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x28, 0x00, 0x96, 0xb9, 0xe3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x85, 0x5d, 0x28, 0x0f, 0x63, 0xba, 0xe3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x83, 0x85, 0x5d, 0x28, 0x0f, 0x63, 0xba, 0xe3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0x0b, 0x3e, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x8c, 0xdf, 0x28, 0x00, 0xe7, 0x04, 0xc3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0x0b, 0x3e, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x8c, 0xdf, 0x28, 0x00, 0xe7, 0x04, 0xe3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0xbf, 0x1f, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x28, 0x40, 0x96, 0xb9, 0xa3, 0xff},
  {0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0x0b, 0x3e, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xea, 0x8c, 0xdf, 0x28, 0x00, 0xe7, 0x04, 0xe3, 0xff}
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION CRTC_init
 *
 * Initializes the CRT Controller.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void CRTC_init()
{
  unsigned i, j = 15;

  if(vga.VGA_mode >= 0 && vga.VGA_mode <= 7)
    j = vga.VGA_mode;
  else if(vga.VGA_mode >= 0x0d && vga.VGA_mode <= 0x13)
    j = vga.VGA_mode - 5;

  for(i = 0; i <= CRTC_MAX_INDEX; i++) vga.crtc.data[i] = crtc_ival[j][i];

  vga.crtc.index = 0;
  vga.crtc.readonly = 1;

  if(j == 15) {
    /* adjust crtc values for vesa modes that fit certain conditions */
    if (vga.width < 2048 && vga.width % vga.char_width == 0)
      vga.crtc.data[0x1] = vga.crtc.data[0x2] =
	vga.width / vga.char_width - 1;
    if (vga.height <= 1024) {
      int h = vga.height - 1;
      vga.crtc.data[0x12] = vga.crtc.data[0x15] = h & 0xff;
      vga.crtc.data[0x7] &= ~0x4a;
      vga.crtc.data[0x7] |= ((h & 0x100) >> (8 - 1))|((h & 0x200) >> (9 - 6))|
	(h & 0x100) >> (8 - 3);
      vga.crtc.data[0x9] &= ~0x20;
      vga.crtc.data[0x9] |= (h & 0x200) >> (9 - 5);
    }
    if (vga.scan_len < 2048 && vga.color_bits == 8) {
      vga.crtc.data[0x13] = vga.scan_len / 8;
      vga.crtc.data[0x14] = 0x40;
    } else if (vga.scan_len < 1024 && vga.mode_class == TEXT) {
      vga.crtc.data[0x13] = vga.scan_len / 4;
      vga.crtc.data[0x17] = 0xa3;
    } else if (vga.scan_len < 512) { /* 4 bpp */
      vga.crtc.data[0x13] = vga.scan_len / 2;
    }
  }

  vgaemu_adj_cfg(CFG_CRTC_ADDR_MODE, 1);
  vgaemu_adj_cfg(CFG_CRTC_WIDTH, 1);
  vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 1);
  vgaemu_adj_cfg(CFG_CRTC_LINE_COMPARE, 1);
#if 0
  /* need to fix vgaemu before this is possible */
  vgaemu_adj_cfg(CFG_MODE_CONTROL, 1);
#endif
  
  crtc_msg("CRTC_init done\n");
}


void CRTC_set_index(unsigned char index)
{
  crtc_deb2(
    "CRTC_set_index: 0x%02x%s\n",
    (unsigned) index,
    index > CRTC_MAX_INDEX ? " (too large)" : ""
  );

  vga.crtc.index = index;
}


unsigned char CRTC_get_index()
{
  crtc_deb2("CRTC_get_index: 0x%02x\n", (unsigned) vga.crtc.index);

  return vga.crtc.index;
}


void CRTC_write_value(unsigned char data)
{
#define NEWBITS(a) (delta & (a))
  unsigned u = data, u1, delta, ind = vga.crtc.index;

  if(ind > CRTC_MAX_INDEX) {
    crtc_deb("CRTC_write_value: data (0x%02x) ignored\n", u);
    return;
  }

  crtc_deb2("CRTC_write_value: crtc[0x%02x] = 0x%02x\n", ind, u);

  if(vga.crtc.readonly) {
    /* read only regs 00h-07h with the exception of bit4 in 07h */
    if (ind <= 6)
      return;
    if (ind == 7)
      data = (vga.crtc.data[ind] & 0xef) | (data & 0x10);
  }

  delta = vga.crtc.data[ind] ^ data;
  if(!delta) return;
  vga.crtc.data[ind] = data;

  switch(ind) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
      if(NEWBITS(0xFF)) {
	vgaemu_adj_cfg(CFG_CRTC_WIDTH, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x06:
      if(NEWBITS(0xFF)) {
	vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x07:
      if(NEWBITS(0x10)) {
	vgaemu_adj_cfg(CFG_CRTC_LINE_COMPARE, 0);
      }
      if(NEWBITS(0xEF)) {
	vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x09:
      if(NEWBITS(0x40)) {
	vgaemu_adj_cfg(CFG_CRTC_LINE_COMPARE, 0);
      }
      if(NEWBITS(0xBF)) {
	vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break; 
     
    case 0x0a:          /* cursor shape start */
      CURSOR_START(vga.crtc.cursor_shape) = u;
      crtc_deb("CRTC_write_value: cursor shape start = 0x%02x\n", u);
      break; 
     
    case 0x0b:          /* cursor shape end   */
      CURSOR_END(vga.crtc.cursor_shape) = u;
      crtc_deb("CRTC_write_value: cursor shape end = 0x%02x\n", u);
      break; 
     
    case 0x0c:		/* Start Address High */
      /* these shifts involving vga.crtc.addr_mode should really be rotations, 
         depending on mode control bit 5 */
      vga.display_start = (vga.crtc.data[0x0d] + (u << 8)) << vga.crtc.addr_mode;
      crtc_deb("CRTC_write_value: Start Address = 0x%04x, high changed\n", vga.display_start);
      break;

    case 0x0d:		/* Start Address Low */
      vga.display_start = (u + (vga.crtc.data[0x0c] << 8)) << vga.crtc.addr_mode;
      /* this shift should really be a rotation, depending on mode control bit 5 */
      crtc_deb("CRTC_write_value: Start Address = 0x%04x, low changed\n", vga.display_start);
      break;

    case 0x0e:		/* Cursor Location High */
      vga.crtc.cursor_location = (vga.crtc.data[0x0f] + (u << 8)) << vga.crtc.addr_mode; 
      crtc_deb("CRTC_write_value: Cursor Location = 0x%04x\n", vga.crtc.cursor_location);
      break;

    case 0x0f:		/* Cursor Location  Low */
      vga.crtc.cursor_location = (u + (vga.crtc.data[0x0e] << 8)) << vga.crtc.addr_mode;
      crtc_deb("CRTC_write_value: Cursor Location = 0x%04x\n", vga.crtc.cursor_location);
      break;

    case 0x11:
      if(NEWBITS(0x80)) {
        vga.crtc.readonly = (data >= 0x80);
      }
      if(NEWBITS(0x7F)) {
	vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x10:
    case 0x12:
      if(NEWBITS(0xFF)) {
	vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;
  
    case 0x13:          /* Number of bytes in a scanline */
      if(NEWBITS(0xFF)) {
	vgaemu_adj_cfg(CFG_CRTC_ADDR_MODE, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x14:		/* Underline Location */
      if(NEWBITS(0x40)) {
	vgaemu_adj_cfg(CFG_CRTC_ADDR_MODE, 0);
      }
      break;

    case 0x15:
    case 0x16:
      if(NEWBITS(0xFF)) {
	vgaemu_adj_cfg(CFG_CRTC_HEIGHT, 0);
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x17:		/* Mode Control */
      if(NEWBITS(0x03)) {
	crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
	vgaemu_adj_cfg(CFG_MODE_CONTROL, 0);
      }
      if(NEWBITS(0x40)) {
	vgaemu_adj_cfg(CFG_CRTC_ADDR_MODE, 0);
      }
      if(NEWBITS(0x80)) {
        crtc_deb("CRTC_write_value: %svideo access\n", (data & 0x80) ? "" : "no ");

        u1 = vga.config.video_off;
        vga.config.video_off = (vga.config.video_off & ~4) + (((data >> 5) & 4) ^ 4);
        if(u1 != vga.config.video_off) {
          crtc_deb("CRTC_write_value: video signal turned %s\n", vga.config.video_off ? "off" : "on");
        }
      }
      break;

    case 0x18: /* line compare */
      if(NEWBITS(0xFF)) {
	vgaemu_adj_cfg(CFG_CRTC_LINE_COMPARE, 0);
      }
      break;
      
    default:
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (ignored)\n", ind, u);
  }
}


unsigned char CRTC_read_value()
{
  unsigned char uc;

  uc = vga.crtc.index > CRTC_MAX_INDEX ? 0x00 : vga.crtc.data[vga.crtc.index];

  crtc_deb2("CRTC_read_value: crtc[0x%02x] = 0x%02x\n", (unsigned) vga.crtc.index, (unsigned) uc);

  return uc;
}

