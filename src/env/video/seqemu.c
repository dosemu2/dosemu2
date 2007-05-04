/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * seqemu.c 
 *
 * VGA sequencer emulator for VGAemu
 *
 * Copyright (C) 1996, Erik Mouw
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
 * email: J.A.K.Mouw@et.tudelft.nl
 *
 *
 * This code emulates the VGA Sequencer for VGAEmu.
 * The sequencer is part of the VGA (Video Graphics Array, a video
 * adapter for IBM compatible PC's). The sequencer is modeled after
 * the Trident 8900 Super VGA chipset, so a lot of programs with Trident
 * compatible video drivers will work with VGAEmu.
 *
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * The VGA Sequencer emulator for VGAEmu.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1999/01/05: Correct initial values for standard VGA modes. Nearly
 * all standard VGA regs are emulated.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * DANG_END_CHANGELOG
 *
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the Sequencer.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define DEBUG_SEQ	0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if 0
  Trident Chip ID from VGADOC4

   1  = TR 8800BR
   2  = TR 8800CS
   3  = TR 8900B
   4  = TVGA8900C
  13h = TVGA8900C
  23h = TR 9000
  33h = TVGA8900CL, TVGA8900D or TVGA 9000C
  43h = TVGA9000i
  53h = TR 9200CXr
  63h = TLCD9100B
  73h = TGUI9420
  83h = TR LX8200
  93h = TGUI9400CXi
  A3h = TLCD9320
  C3h = TGUI9420DGi
  D3h = TGUI9660XGi
  E3h = TGUI9440AGi
  F3h = TGUI9430       One source says 9420 ??
  The 63h, 73h, 83h, A3h and F3h entries are still in doubt.
#endif

#define CHIP_ID 0x13		/* TVGA8900C */
#define OLD_MODE 0
#define NEW_MODE 1

#if !defined True
#define False 0
#define True 1
#endif

#define seq_msg(x...) v_printf("VGAEmu: Seq: " x)

#if DEBUG_SEQ >= 1
#define seq_deb(x...) v_printf("VGAEmu: Seq: " x)
#else
#define seq_deb(x...)
#endif

#if DEBUG_SEQ >= 2
#define seq_deb2(x...) v_printf("VGAEmu: Seq: " x)
#else
#define seq_deb2(x...)
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "vgaemu.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned char seq_ival[16][5] = {
  {0x03, 0x08, 0x03, 0x00, 0x02},
  {0x03, 0x08, 0x03, 0x00, 0x02},
  {0x03, 0x00, 0x03, 0x00, 0x02},
  {0x03, 0x00, 0x03, 0x00, 0x02},
  {0x03, 0x09, 0x03, 0x00, 0x02},
  {0x03, 0x09, 0x03, 0x00, 0x02},
  {0x03, 0x01, 0x01, 0x00, 0x06},
  {0x03, 0x00, 0x03, 0x00, 0x02},

  {0x03, 0x09, 0x0f, 0x00, 0x06},
  {0x03, 0x01, 0x0f, 0x00, 0x06},
  {0x03, 0x01, 0x0f, 0x00, 0x06},
  {0x03, 0x01, 0x0f, 0x00, 0x06},
  {0x03, 0x01, 0x0f, 0x00, 0x06},
  {0x03, 0x01, 0x0f, 0x00, 0x06},
  {0x03, 0x01, 0x0f, 0x00, 0x0e},

  {0x03, 0x01, 0x0f, 0x00, 0x06}
};


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* calculate font bank offsets from map select value */
static void seq_map_select(unsigned data)
{
  /* map select processing: prepare font mem offsets */
  /* actually, the single "8k" bit should be ignored on EGA! */
  vga.seq.fontofs[0] = ((data & 3) << 1) + /* 16k bits */
    ((data & 0x10) >> 4); /* 8k bit */
  vga.seq.fontofs[1] = ((data & 0x0c) >> 1) + /* 16k bits */
    ((data & 0x20) >> 5); /* 8k bit */
  vga.seq.fontofs[0] <<= 13; /* factor is 8k */
  vga.seq.fontofs[1] <<= 13; /* factor is 8k */
  seq_msg("Setting font offsets to 0x%04x/0x%04x (0x%02x)\n",
          vga.seq.fontofs[0], vga.seq.fontofs[1], data);
  vga.reconfig.display = 1;
  vga.reconfig.re_init = 1;
}

void Seq_init()
{
  unsigned i, j = 15;

  if(vga.VGA_mode >= 0 && vga.VGA_mode <= 7)
    j = vga.VGA_mode;
  else if(vga.VGA_mode >= 0x0d && vga.VGA_mode <= 0x13)
    j = vga.VGA_mode - 5;

  for(i = 0; i < 5; i++) {
    vga.seq.data[i] = seq_ival[j][i];
  }
  if(j == 15 ) {
    /* adjust reg 4 for vesa modes */
    vga.seq.data[4] = vga.mode_class == TEXT ? 0x02 :
      vga.pixel_size == 4 ? 0x06 : 0x0a;
  }
  /* font offset processing, see below: */
  seq_map_select(vga.seq.data[3]);
  while(i <= SEQ_MAX_INDEX) vga.seq.data[i++] = 0;
  vga.seq.data[0x0b] = CHIP_ID;

  /* initialize non-standard modes */
  if(j == 15) {
    if(vga.mode_class == TEXT) {
      if(vga.char_width == 9) vga.seq.data[1] = 0;
      vga.seq.data[2] = 3;
      vga.seq.data[4] = 2;
    }
  }

  vga.seq.mode_ctrl_1_bak = vga.seq.mode_ctrl_2_bak = 0;
  vga.seq.mode = NEW_MODE;

  vga.seq.index = 0;

  vgaemu_adj_cfg(CFG_SEQ_ADDR_MODE, 1);
  vga.seq.map_mask = vga.seq.data[2] & 0xf;

  seq_msg("Seq_init done\n");
}


void Seq_set_index(unsigned char index)
{
  seq_deb2(
    "Seq_set_index: 0x%02x%s\n",
    (unsigned) index,
    index > SEQ_MAX_INDEX ? " (too large)" : ""
  );

  vga.seq.index = index;
}


unsigned char Seq_get_index()
{
  seq_deb2("Seq_get_index: 0x%02x\n", (unsigned) vga.seq.index);

  return vga.seq.index;
}


void Seq_write_value(unsigned char data)
{
#define NEWBITS(a) (delta & (a))
  unsigned u, u1, delta, ind = vga.seq.index;
  unsigned char uc1;

  if(ind > SEQ_MAX_INDEX) {
    seq_deb("Seq_write_value: data (0x%02x) ignored\n", (unsigned) data);
    return;
  }

  seq_deb2("Seq_write_value: seq[0x%02x] = 0x%02x\n", ind, (unsigned) data);

  /* Chip Version */
  if(vga.seq.index == 0x0b && vga.seq.mode == NEW_MODE) {
    vga.seq.mode = OLD_MODE;

    uc1 = vga.seq.data[0x0d];
    vga.seq.data[0x0d] = vga.seq.mode_ctrl_2_bak;
    vga.seq.mode_ctrl_2_bak = uc1;

    uc1 = vga.seq.data[0x0e];
    vga.seq.data[0x0e] = vga.seq.mode_ctrl_1_bak;
    vga.seq.mode_ctrl_1_bak = uc1;
  }

  /* this one is tricky... :-) -- sw */
  if(ind == 0x0e && vga.seq.mode == NEW_MODE) {
    if(vga.seq.data[0x0e] == (data ^ 0x02)) return;
  }
  else {
    if(vga.seq.data[ind] == data) return;
  }

  delta = vga.seq.data[ind] ^ data;
  vga.seq.data[ind] = data;

  switch(ind) {
    case 0x00:		/* Reset */
      break;

    case 0x01:		/* Clocking Mode */
      if(NEWBITS(0x20)) {
        seq_deb("Seq_write_value: %svideo access\n", (data & 0x20) ? "no " : "");

        u1 = vga.config.video_off;
        vga.config.video_off = (vga.config.video_off & ~2) + ((data >> 4) & 2);
        if(u1 != vga.config.video_off) {
          seq_deb("Seq_write_value: video signal turned %s\n", vga.config.video_off ? "off" : "on");
        }
      }
      if(NEWBITS(0x9)) {
        vgaemu_adj_cfg(CFG_SEQ_ADDR_MODE, 0);
      }
      break;

    case 0x02:		/* Map Mask */
      u = data;
      if (vga.color_bits != 8)
        u &= 0xf;
      vga.seq.map_mask = u;
      u1 = 0;
      if(u) while(!(u & 1)) u >>= 1, u1++;
      seq_deb("Seq_write_value: map mask = 0x%x, write plane = %u\n",
        (unsigned) vga.seq.map_mask, u1
      );
      if(vga.color_bits == 8) {	// experimental: just for 8bpp modes
        if (((vga.seq.map_mask | 0xf8) & (vga.seq.map_mask - 1)) == 0
        ) {      /* test for power of 2: 1,2,4,8 match */
          if(vga.inst_emu) {
            vga.inst_emu = 0;
            vgaemu_map_bank();
            seq_deb("Seq_write_value: instemu off\n");
          }
        } else {
          if(vga.inst_emu != 2) {	// EMU_ALL_INST; cf. vgaemu.c
            vga.inst_emu = 2;
            seq_deb("Seq_write_value: instemu on\n");
            vgaemu_map_bank();
          }
        }
      }
      // ##### FIXME: drop this altogether and always use
      // the gfx.read_map_select reg? -- sw
      if(!vga.inst_emu) {
        vgaemu_switch_plane(u1);
      }
      break;

    case 0x03:		/* Character Map Select */
      if (NEWBITS(0xff))
        seq_map_select(data);
      break;


    case 0x04:		/* Memory Mode */
      if(NEWBITS(0x0c)) {
        vgaemu_adj_cfg(CFG_SEQ_ADDR_MODE, 0);
      }
      break;

    case 0x0b:		/* Trident: Chip Version */
      return;		/* read-only! */
      break;

    case 0x0c:		/* Trident: Power Up Mode 1 */
      if(!(vga.seq.mode == NEW_MODE && (vga.seq.data[0x0e] & 0x80))) return;
      break;

    case 0x0d:		/* Trident: Old/New Mode Control 2 */
      break;

    case 0x0e:		/* Trident: Old/New Mode Control 1 */
      if(vga.seq.mode == NEW_MODE) {
        u = (data ^ 0x02) & 0x0f;	/* XOR 0x02, used for Trident detection */
        vga_emu_switch_bank(u);
        vga.seq.data[0x0e] = data ^ 0x02;
      }
      else {	/* Seq_mode == OLD_MODE */
        /* don't know what to do, we don't support 128K pages -- Erik */
      }
      break;

    case 0x0f:		/* Trident: Power Up Mode 2 */
      break;
  }
}


unsigned char Seq_read_value()
{
  unsigned char uc, uc1;

  uc = vga.seq.index > SEQ_MAX_INDEX ? 0x00 : vga.seq.data[vga.seq.index];

  /* Chip Version */
  if(vga.seq.index == 0x0b && vga.seq.mode == OLD_MODE) {
    vga.seq.mode = NEW_MODE;

    uc1 = vga.seq.data[0x0d];
    vga.seq.data[0x0d] = vga.seq.mode_ctrl_2_bak;
    vga.seq.mode_ctrl_2_bak = uc1;

    uc1 = vga.seq.data[0x0e];
    vga.seq.data[0x0e] = vga.seq.mode_ctrl_1_bak;
    vga.seq.mode_ctrl_1_bak = uc1;
  }

  seq_deb2("Seq_read_value: seq[0x%02x] = 0x%02x\n", (unsigned) vga.seq.index, (unsigned) uc);

  return uc;
}

