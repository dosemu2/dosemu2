/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Emulate the Hercules-specific parts.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1999/01/05: We emulate just the basics for the moment.
 * -- sw ()
 *
 * DANG_END_CHANGELOG
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the Hercules Card.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define DEBUG_HERC	0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if !defined True
#define False 0
#define True 1
#endif

#define herc_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_HERC >= 1
#define herc_deb(x...) v_printf("VGAEmu: " x)
#else
#define herc_deb(x...)
#endif

#if DEBUG_HERC >= 2
#define herc_deb2(x...) v_printf("VGAEmu: " x)
#else
#define herc_deb2(x...)
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "vgaemu.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION Misc_init
 *
 * Initializes the Hercules Emulation.
 * This function should be called during VGA mode initialization.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void Herc_init()
{
  vga.herc.cfg_switch = 0;
  vga.herc.mode_ctrl = 0;

  herc_msg("Herc_init done\n");
}


/*
 * DANG_BEGIN_FUNCTION Herc_set_cfg_switch
 *
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void Herc_set_cfg_switch(unsigned char data)
{
  herc_deb2("Herc_set_cfg_switch: 0x%02x\n", (unsigned) data);

  if(((vga.herc.cfg_switch ^ data) & 1)) {
    herc_deb("Herc_set_cfg_switch: graphics mode%s allowed\n", (data & 1) ? "" : " not");
  }

  if(((vga.herc.cfg_switch ^ data) & 2)) {
    herc_deb(
      "Herc_set_cfg_switch: %s mode (ignored)\n",
      (data & 2) ? "full (64k)" : "half (32k)"
    );
  }

  vga.herc.cfg_switch = data;
}


/*
 * DANG_BEGIN_FUNCTION Herc_set_mode_ctrl
 *
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void Herc_set_mode_ctrl(unsigned char data)
{
  int m = 0;
  unsigned u1;

  herc_deb2("Herc_set_mode_ctrl: 0x%02x\n", (unsigned) data);

  if(((vga.herc.mode_ctrl ^ data) & 2)) {
    if((data & 2) && (vga.herc.cfg_switch & 1)) m = 2;
    if((data & 2) == 0) m = 1;
    herc_deb(
      "Herc_set_mode_ctrl: entering %s mode%s\n",
      data & 2 ? "graphics" : "text", m == 0 ? " (ignored)" : ""
    );
  }

  if(((vga.herc.mode_ctrl ^ data) & 8)) {
    herc_deb("Herc_set_mode_ctrl: %svideo access\n", (data & 8) ? "" : "no ");

    u1 = vga.config.video_off;
    vga.config.video_off = (vga.config.video_off & ~8) + ((data & 8) ^ 8);
    if(u1 != vga.config.video_off) {
      herc_deb("Herc_set_mode_ctrl: video signal turned %s\n", vga.config.video_off ? "off" : "on");
    }
  }

  if(((vga.herc.mode_ctrl ^ data) & 0x20)) {
    herc_deb(
      "Herc_set_mode_ctrl: blinking %s (ignored)\n",
      (data & 0x20) ? "on" : "off"
    );
  }

  if(((vga.herc.mode_ctrl ^ data) & 0x80)) {
    herc_deb("Herc_set_mode_ctrl: switched to graphics page %d (ignored)\n", (data >> 7) & 1);
  }

  vga.herc.mode_ctrl = data;

  if(m == 1) {	/* switch to text mode */
    vga.mode_class = TEXT;
    vga.mode_type = TEXT_MONO;

    vga.width = 720;
    vga.height = 400;
    vga.scan_len = 160;
    vga.text_width = 80;
    vga.text_height = 25;
    vga.char_width = 9;
    vga.char_height = 16;
    vga.pixel_size = 4;
    vga.display_start = 0;

    dirty_all_vga_colors();

    vga.reconfig.re_init = 1;
  }

  if(m == 2) {	/* switch to graphics mode */
    vga.mode_class = GRAPH;
    vga.mode_type = HERC;

    vga.width = 720;
    vga.height = 348;
    vga.scan_len = 90;
    vga.text_width = 90;
    vga.text_height = 25;
    vga.char_width = 8;
    vga.char_height = 14;
    vga.pixel_size = 1;
    vga.display_start = 0;

    dirty_all_vga_colors();

    vga.reconfig.re_init = 1;
  }
}


/*
 * DANG_BEGIN_FUNCTION Herc_get_mode_ctrl
 *
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Herc_get_mode_ctrl()
{
  herc_deb("Herc_get_mode_ctrl: 0x%02x\n", (unsigned) vga.herc.mode_ctrl);

  return vga.herc.mode_ctrl;
}

