/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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

#define NEWBITS(a) ((vga.crtc.data[ind] ^ data) & (a))


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

  vgaemu_adj_cfg(CFG_CRTC_ADDR_MODE, 1);

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
  unsigned u = data, u1, ind = vga.crtc.index;
  unsigned todo_ind, todo[4];

  if(ind > CRTC_MAX_INDEX) {
    crtc_deb("CRTC_write_value: data (0x%02x) ignored\n", u);
    return;
  }

  crtc_deb2("CRTC_write_value: crtc[0x%02x] = 0x%02x\n", ind, u);

  if(vga.crtc.data[ind] == data) return;

  for(todo_ind = 0; todo_ind < sizeof todo / sizeof *todo; todo_ind++) todo[todo_ind] = 0;
  todo_ind = 0;

  switch(ind) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x04:
    case 0x05:
      if(NEWBITS(0xFF)) {
        todo[todo_ind++] = CFG_CRTC_WIDTH;
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x06:
      if(NEWBITS(0xFF)) {
        todo[todo_ind++] = CFG_CRTC_HEIGHT;
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x07:
      if(NEWBITS(0x10)) {
        todo[todo_ind++] = CFG_CRTC_LINE_COMPARE;
      }
      if(NEWBITS(0xEF)) {
        todo[todo_ind++] = CFG_CRTC_HEIGHT;
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x09:
      if(NEWBITS(0x40)) {
        todo[todo_ind++] = CFG_CRTC_LINE_COMPARE;
      }
      if(NEWBITS(0xBF)) {
        todo[todo_ind++] = CFG_CRTC_HEIGHT;
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break; 
     
    case 0x0c:		/* Start Address High */
      /* these shifts involving vga.crtc.addr_mode should really be rotations, 
         depending on mode control bit 5 */
      vga.display_start = (vga.crtc.data[0x0d] + (u << 8)) << vga.crtc.addr_mode;
      screen_adr = SCREEN_ADR(0) + vga.display_start/2;
      crtc_deb("CRTC_write_value: Start Address = 0x%04x, high changed\n", vga.display_start);
      break;

    case 0x0d:		/* Start Address Low */
      vga.display_start = (u + (vga.crtc.data[0x0c] << 8)) << vga.crtc.addr_mode;
      /* this shift should really be a rotation, depending on mode control bit 5 */
      screen_adr = SCREEN_ADR(0) + vga.display_start/2;
      crtc_deb("CRTC_write_value: Start Address = 0x%04x, low changed\n", vga.display_start);
      break;

    case 0x0e:		/* Cursor Location High */
      vga.crtc.cursor_location = (vga.crtc.data[0x0f] + (u << 8)) << vga.crtc.addr_mode; 
      cursor_row = (vga.crtc.cursor_location - vga.display_start) / vga.scan_len;
      cursor_col = ((vga.crtc.cursor_location - vga.display_start) % vga.scan_len) / 2;
      crtc_deb("CRTC_write_value: Cursor Location = 0x%04x\n", vga.crtc.cursor_location);
      break;

    case 0x0f:		/* Cursor Location  Low */
      vga.crtc.cursor_location = (u + (vga.crtc.data[0x0e] << 8)) << vga.crtc.addr_mode;
      cursor_row = (vga.crtc.cursor_location - vga.display_start) / vga.scan_len;
      cursor_col = ((vga.crtc.cursor_location - vga.display_start) % vga.scan_len) / 2;
      crtc_deb("CRTC_write_value: Cursor Location = 0x%04x\n", vga.crtc.cursor_location);
      break;

    case 0x10:
    case 0x11:
    case 0x12:
      if(NEWBITS(0xFF)) {
        todo[todo_ind++] = CFG_CRTC_HEIGHT;
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;
  
    case 0x13:          /* Number of bytes in a scanline */
      if(NEWBITS(0xFF)) {
        todo[todo_ind++] = CFG_CRTC_ADDR_MODE;
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x14:		/* Underline Location */
      if(NEWBITS(0x40)) {
        todo[todo_ind++] = CFG_CRTC_ADDR_MODE;
      }
      break;

    case 0x15:
    case 0x16:
      if(NEWBITS(0xFF)) {
        todo[todo_ind++] = CFG_CRTC_HEIGHT;
      }
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (guessed)\n", ind, u);
      break;

    case 0x17:		/* Mode Control */
      if(NEWBITS(0x40)) {
        todo[todo_ind++] = CFG_CRTC_ADDR_MODE;
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
        todo[todo_ind++] = CFG_CRTC_LINE_COMPARE;
      }
      break;
      
    default:
      crtc_deb("CRTC_write_value: crtc[0x%02x] = 0x%02x (ignored)\n", ind, u);
  }

  vga.crtc.data[ind] = data;

  for(todo_ind = 0; todo_ind < sizeof todo / sizeof *todo; todo_ind++) {
    if(todo[todo_ind]) vgaemu_adj_cfg(todo[todo_ind], 0);
  }
}


unsigned char CRTC_read_value()
{
  unsigned char uc;

  uc = vga.crtc.index > CRTC_MAX_INDEX ? 0x00 : vga.crtc.data[vga.crtc.index];

  crtc_deb2("CRTC_read_value: crtc[0x%02x] = 0x%02x\n", (unsigned) vga.crtc.index, (unsigned) uc);

  return uc;
}

