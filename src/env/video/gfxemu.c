/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
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

#define NEWBITS(a) ((vga.gfx.data[ind] ^ data) & (a))


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
  vga.gfx.data[8] = 0xff;

  /* initialize non-standard modes */
  if(j == 15) {
    if(vga.mode_class == TEXT) {
      vga.gfx.data[5] = 0x10;
      vga.gfx.data[6] = 0x0e;
    }
  }

  vga.gfx.index = 0;

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
  unsigned u1, ind = vga.gfx.index;

  if(ind > GFX_MAX_INDEX) {
    gfx_deb("GFX_write_value: data (0x%02x) ignored\n", u);
    return;
  }

  gfx_deb2("GFX_write_value: gfx[0x%02x] = 0x%02x\n", ind, u);

  if(vga.gfx.data[ind] == data) return;

  switch(ind) {
    case 0x04:		/* Read Map Select */
      if(NEWBITS(0x03)) {
        u1 = data & 3;
        gfx_deb("GFX_write_value: read plane = %u\n", u1);
        vgaemu_switch_plane(u1);
      }
      break;

    case 0x05:		/* Mode */
      if(NEWBITS(0x03)) {
        gfx_deb("GFX_write_value: write mode = %u (ignored)\n", data & 3);
      }
      if(NEWBITS(0x08)) {
        gfx_deb("GFX_write_value: read mode = %u (ignored)\n", (data >> 3) & 1);
      }
      if(NEWBITS(0x10)) {
        gfx_deb("GFX_write_value: odd/even = %s (ignored)\n", (data & 0x10) ? "on" : "off");
      }
      if(NEWBITS(0x20)) {
        gfx_deb("GFX_write_value: CGA 4 color mode = %s (ignored)\n", (data & 0x20) ? "on" : "off");
      }
      if(NEWBITS(0x40)) {
        gfx_deb("GFX_write_value: VGA 256 color mode = %s (ignored)\n", (data & 0x40) ? "on" : "off");
      }
      break;

    case 0x06:		/* Miscellaneous */
      if(NEWBITS(0x01)) {
        gfx_deb("GFX_write_value: %s mode (ignored)\n", (data & 1) ? "text" : "graphics");
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

  }

  vga.gfx.data[ind] = data;
}


unsigned char GFX_read_value()
{
  unsigned char uc;

  uc = vga.gfx.index > GFX_MAX_INDEX ? 0x00 : vga.gfx.data[vga.gfx.index];

  gfx_deb2("GFX_read_value: gfx[0x%02x] = 0x%02x\n", (unsigned) vga.gfx.index, (unsigned) uc);

  return uc;
}

