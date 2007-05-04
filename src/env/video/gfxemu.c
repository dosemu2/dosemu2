/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The VGA Graphics Controller emulator for VGAEmu.
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1999/01/05: Correct initial values; memory mapping regs are emulated
 * (as far as possible).
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * 2000/05/10: Apparently gfx.color_dont_care should be initialized
 * with 0x0f, not 0x00.
 * -- sw
 *
 * DANG_END_CHANGELOG
 *
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the Graphics Controller.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define	DEBUG_GFX	0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if !defined True
#define False 0
#define True 1
#endif

#define gfx_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_GFX >= 1
#define gfx_deb(x...) v_printf("VGAEmu: " x)
#else
#define gfx_deb(x...)
#endif

#if DEBUG_GFX >= 2
#define gfx_deb2(x...) v_printf("VGAEmu: " x)
#else
#define gfx_deb2(x...)
#endif

#define NEWBITS(a) ((olddata ^ data) & (a))


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "vgaemu.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned char gfx_ival[16][2] = {
  {0x10, 0x0e},
  {0x10, 0x0e},
  {0x10, 0x0e},
  {0x10, 0x0e},
  {0x30, 0x0f},
  {0x30, 0x0f},
  {0x00, 0x0d},
  {0x10, 0x0a},

  {0x00, 0x05},
  {0x00, 0x05},
  {0x00, 0x05},
  {0x00, 0x05},
  {0x00, 0x05},
  {0x00, 0x05},
  {0x40, 0x05},

  {0x00, 0x05}
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION GFX_init
 *
 * Initializes the Graphics Controller.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void GFX_init()
{
  unsigned i, j = 15;

  if(vga.VGA_mode >= 0 && vga.VGA_mode <= 7)
    j = vga.VGA_mode;
  else if(vga.VGA_mode >= 0x0d && vga.VGA_mode <= 0x13)
    j = vga.VGA_mode - 5;

  for(i = 0; i <= GFX_MAX_INDEX; i++) vga.gfx.data[i] = 0;
  vga.gfx.data[5] = gfx_ival[j][0];
  vga.gfx.data[6] = gfx_ival[j][1];
  vga.gfx.data[7] = 0x0f;
  vga.gfx.data[8] = 0xff;

  /* initialize non-standard modes */
  if(j == 15) {
    if(vga.mode_class == TEXT) {
      vga.gfx.data[5] = 0x10;
      vga.gfx.data[6] = 0x0e;
    }
  }

  vga.gfx.index = 0;

  vga.gfx.set_reset        = vga.gfx.data[0] & 0x0f;
  vga.gfx.enable_set_reset = vga.gfx.data[1] & 0x0f;
  vga.gfx.color_compare    = vga.gfx.data[2] & 0x0f;
  vga.gfx.data_rotate      = vga.gfx.data[3] & 0x07;
  vga.gfx.raster_op        = vga.gfx.data[3] >> 3;
  vga.gfx.read_map_select  = vga.gfx.data[4] & 0x03;
  vga.gfx.write_mode       = vga.gfx.data[5] & 0x03;
  vga.gfx.read_mode        = (vga.gfx.data[5] >> 3) & 0x01;
  vga.gfx.color_dont_care  = vga.gfx.data[7] & 0x0f;
  vga.gfx.bitmask          = vga.gfx.data[8];

  gfx_msg("GFX_init done\n");
}


void GFX_set_index(unsigned char index)
{
  gfx_deb2(
    "GFX_set_index: 0x%02x%s\n",
    (unsigned) index,
    index > GFX_MAX_INDEX ? " (too large)" : ""
  );

  vga.gfx.index = index;
}


unsigned char GFX_get_index()
{
  gfx_deb2("GFX_get_index: 0x%02x\n", (unsigned) vga.gfx.index);

  return vga.gfx.index;
}


void GFX_write_value(unsigned char data)
{
#if 0
  unsigned u = data;
#endif
  unsigned ind = vga.gfx.index;
  unsigned char olddata;

  if(ind > GFX_MAX_INDEX) {
    gfx_deb("GFX_write_value: data (0x%02x) ignored\n", data);
    return;
  }

  gfx_deb2("GFX_write_value: gfx[0x%02x] = 0x%02x\n", ind, data);

  if(vga.gfx.data[ind] == data) return;
  olddata = vga.gfx.data[ind];
  vga.gfx.data[ind] = data;

  switch(ind) {
    case 0x00:		/* Set/Reset */
      vga.gfx.set_reset = data & 0x0f;
      if(NEWBITS(0x0f)) {
        gfx_deb("GFX_write_value: set_reset = 0x%x\n", vga.gfx.set_reset);
      }
      break;

    case 0x01:		/* Enable Set/Reset */
      vga.gfx.enable_set_reset = data & 0x0f;
      if(NEWBITS(0x0f)) {
        gfx_deb("GFX_write_value: enable_set_reset = 0x%x\n", vga.gfx.enable_set_reset);
      }
      break;

    case 0x02:		/* Color Compare */
      vga.gfx.color_compare = data & 0x0f;
      if(NEWBITS(0x0f)) {
        gfx_deb("GFX_write_value: color_compare = 0x%x\n", vga.gfx.color_compare);
      }
      break;

    case 0x03:		/* Data Rotate */
      vga.gfx.data_rotate = data & 7;
      vga.gfx.raster_op = data >> 3;
      if(NEWBITS(0x07)) {
        gfx_deb("GFX_write_value: data_rotate = %u\n", vga.gfx.data_rotate);
      }
      if(NEWBITS(0x0c)) {
        gfx_deb("GFX_write_value: raster_op = %u\n", vga.gfx.raster_op);
      }
      break;

    case 0x04:		/* Read Map Select */
      vga.gfx.read_map_select = data & 3;
      if(NEWBITS(0x03)) {
        gfx_deb("GFX_write_value: read_map_select = %u\n", vga.gfx.read_map_select);
        vgaemu_switch_plane(vga.gfx.read_map_select);
      }
      break;

    case 0x05:		/* Mode */
      vga.gfx.write_mode = data & 3;
      vga.gfx.read_mode = (data >> 3) & 1;
      if(NEWBITS(0x03)) {
        gfx_deb("GFX_write_value: write mode = %u\n", vga.gfx.write_mode);
      }
      if(NEWBITS(0x08)) {
        gfx_deb("GFX_write_value: read mode = %u\n", vga.gfx.read_mode);
      }
      if(NEWBITS(0x10)) {
        gfx_deb("GFX_write_value: odd/even = %s (ignored)\n", (data & 0x10) ? "on" : "off");
      }
      if(NEWBITS(0x20)) {
        gfx_deb("GFX_write_value: CGA 4 color mode = %s\n", (data & 0x20) ? "on" : "off");
      }
      if(NEWBITS(0x40)) {
        gfx_deb("GFX_write_value: VGA 256 color mode = %s\n", (data & 0x40) ? "on" : "off");
      }
      if(NEWBITS(0x60)) {
        vgaemu_adj_cfg(CFG_MODE_CONTROL, 0);
      }
      break;

    case 0x06:		/* Miscellaneous */
      if(NEWBITS(0x01)) {
        gfx_deb("GFX_write_value: %s mode\n", (data & 1) ? "graphics" : "text");
        vgaemu_adj_cfg(CFG_MODE_CONTROL, 0);
      }
      if(NEWBITS(0x02)) {
        gfx_deb("GFX_write_value: odd/even address mode = %s (ignored)\n", (data & 2) ? "on" : "off");
      }
      if(NEWBITS(0x0c)) {
        vga.mem.bank = 0;	/* reset this? */
        switch((data >> 2) & 3) {
          case 0:
            vga.mem.bank_pages = 32;
            vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page = 0xa0;
            break;

          case 1:
            vga.mem.bank_pages = 16;
            vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page = 0xa0;
            break;

          case 2:
            vga.mem.bank_pages = 8;
            vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page = 0xb0;
            break;

          case 3:
            vga.mem.bank_pages = 8;
            vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page = 0xb8;
            break;
        }
        gfx_deb(
          "GFX_write_value: memory map = %dk@0x%x\n",
          vga.mem.bank_pages << 2,
          vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12
        );
        vgaemu_map_bank();
      }
      break;

    case 0x07:		/* Color Don't Care */
      vga.gfx.color_dont_care = data & 0x0f;
      if(NEWBITS(0x0f)) {
        gfx_deb("GFX_write_value: color_dont_care = 0x%x\n", vga.gfx.color_dont_care);
      }
      break;

    case 0x08:		/* Bit Mask */
      vga.gfx.bitmask = data;
      if(NEWBITS(0xff)) {
        gfx_deb("GFX_write_value: bitmask = 0x%02x\n", vga.gfx.bitmask);
      }
      break;

  }
}


unsigned char GFX_read_value()
{
  unsigned char uc;

  uc = vga.gfx.index > GFX_MAX_INDEX ? 0x00 : vga.gfx.data[vga.gfx.index];

  gfx_deb2("GFX_read_value: gfx[0x%02x] = 0x%02x\n", (unsigned) vga.gfx.index, (unsigned) uc);

  return uc;
}

