/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Emulate miscellaneous registers for VGAEmu.
 * /REMARK
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 1996/05/06:
 *  - Changed Misc_get_input_status_1() to get it _slower_.
 *    Idea from Adam D. Moss (aspirin@tigerden.com).
 *
 * 1996/05/09:
 *  - Added horizontal retrace too (--adm)
 *
 * 1998/01/05: Moved Misc_get_input_status_1() from attr.c into a separate
 * file (this one). Added emulation for the VGA regs which have their own
 * port addresses.
 * -- sw (Steffen Winterfeldt <wfeldt@suse.de>)
 *
 * DANG_END_CHANGELOG
 */


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the Attribute Controller.
 * 0 - normal / 1 - useful / 2 - too much
 */
#define DEBUG_MISC	0


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#if !defined True
#define False 0
#define True 1
#endif

#define misc_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_MISC >= 1
#define misc_deb(x...) v_printf("VGAEmu: " x)
#else
#define misc_deb(x...)
#endif

#if DEBUG_MISC >= 2
#define misc_deb2(x...) v_printf("VGAEmu: " x)
#else
#define misc_deb2(x...)
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "vgaemu.h"
#include "timers.h"


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
hitimer_t t_vretrace = 0;		/* cf. base/dev/misc/timers.c */
 

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Misc_get_input_status_1() is used not only by VGAEmu (cf. emu-i386/ports.c);
 * hence the following #ifdef X_SUPPORT.
 */

#ifdef X_SUPPORT
/*
 * DANG_BEGIN_FUNCTION Misc_init
 *
 * Initializes the Miscellaneous Output Register.
 * This function should be called during VGA mode initialization.
 * This is an interface function.
 *
 * DANG_END_FUNCTION
 *
 */
void Misc_init()
{
  unsigned char u;

  u = 0x23;	/* CRTC port = 0x3d4, CPU access enabled */

  if(vga.VGA_mode == 7 || vga.VGA_mode == 15) {
    /* mono modes --> CRTC port = 0x3b4 */
    u &= ~1;
  }

  /* clock select */
  if(vga.VGA_mode >= 0) {
    if(vga.VGA_mode <= 3 || vga.VGA_mode == 7)
      u |= 4;	/* clock #1 */
    else if(vga.VGA_mode > 0x13)
      u |= 0xc;	/* clock #3 */
  }
  else {
    u |= 0xc;	/* clock #3 */
  }

  /* # of lines, aka hsync/vsync polarity */
  if(vga.VGA_mode >= 0 && vga.VGA_mode <= 0x13) {
    switch(vga.VGA_mode) {
      case 0x0f:
      case 0x10: u |= 0x80; break;	/* 350 lines */
      case 0x11:
      case 0x12: u |= 0xc0; break;	/* 480 lines */
      default:   u |= 0x40; break;	/* 400 lines */
    }
  }
  else {
    u |= 0xc0;				/* 480 lines */
  }

  vga.misc.misc_output = u;
  vga.misc.feature_ctrl = 0;

  vga.config.mono_port = (vga.misc.misc_output & 1) ^ 1;

  misc_msg("Misc_init done\n");
}


/*
 * DANG_BEGIN_FUNCTION Misc_set_misc_output
 *
 * Emulate Miscellaneous Output Register.
 * For now just stores the value.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void Misc_set_misc_output(unsigned char data)
{
  unsigned u;

  misc_deb2("Misc_set_misc_output: 0x%02x\n", (unsigned) data);

  vga.misc.misc_output = data;

  u = ~(vga.misc.misc_output & 1);

  u = (vga.misc.misc_output & 1) ^ 1;

  if(u != vga.config.mono_port) {
    vga.config.mono_port = u;
    misc_msg("Misc_set_misc_output: VGA changed to %s mode\n", u ? "mono" : "color");
  }
}


/*
 * DANG_BEGIN_FUNCTION Misc_get_misc_output
 *
 * Emulate Miscellaneous Output Register.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Misc_get_misc_output()
{
  misc_deb("Misc_get_misc_output: 0x%02x\n", (unsigned) vga.misc.misc_output);

  return vga.misc.misc_output;
}


/*
 * DANG_BEGIN_FUNCTION Misc_set_feature_ctrl
 *
 * Emulate Feature Control Register.
 * We just store the value.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
void Misc_set_feature_ctrl(unsigned char data)
{
  misc_deb("Misc_set_feature_ctrl: 0x%02x\n", (unsigned) data);

  vga.misc.feature_ctrl = data;
}


/*
 * DANG_BEGIN_FUNCTION Misc_get_feature_ctrl
 *
 * Emulate Feature Control Register.
 * We simply return what was written into it by Misc_set_feature_ctrl().
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Misc_get_feature_ctrl()
{
  misc_deb("Misc_get_feature_ctrl: 0x%02x\n", (unsigned) vga.misc.feature_ctrl);

  return vga.misc.feature_ctrl;
}


/*
 * DANG_BEGIN_FUNCTION Misc_get_input_status_0
 *
 * Emulate Input Status #0 Register.
 * The only bit of (possible) interest could be bit 7 indicating an irq 2
 * due to vertical retrace.
 * But we don't do this, so we always return 0x00.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Misc_get_input_status_0()
{
  unsigned char u = 0;

  misc_deb(
    "Misc_get_input_status_0: 0x%02x%s\n",
    (unsigned) u, u & 0x80 ? " (IRQ 2)" : ""
  );

  return u;
}
#endif	/* X_SUPPORT */


/*
 * DANG_BEGIN_FUNCTION Misc_get_input_status_1
 *
 * Emulate Input Status #1 Register.
 * The essential part is to simulate the retrace signals.
 * Clears the Attribute Controller's flip-flop.
 * This is a hardware emulation function.
 *
 * DANG_END_FUNCTION
 *
 */
unsigned char Misc_get_input_status_1()
{
  /* 
   * Graphic status - many programs will use this port to sync with
   * the vert & horz retrace so as not to cause CGA snow. On VGAs this
   * register is used to get full (read: fast) access to the video memory 
   * during the vertical retrace.
   *
   * bit 0 is Display Enable, bit 3 is Vertical Retrace
   * 00=display 01=horiz.retrace 09=vert.retrace
   * We're in vertical retrace?  If so, set VR and DE flags
   * We're in horizontal retrace?  If so, just set DE flag, 0 in VR
   *
   * Idea from Adam Moss:
   * Wait 20 milliseconds before we tell the DOS program that the VGA is
   * in a vertical retrace. This is to avoid that some programs run too
   * _fast_ in Dosemu (yes, I know, this sounds odd, but such programs
   * really exist!). This option works only if the system has
   * gettimeofday().
   *
   * Now simpler and more 'realtime', for better or for worse.  Implements
   * horizontal retrace too.  (--adm)
   *
   */
  static unsigned char hretrace = 0, vretrace = 0, first = 1;
  static hitimer_t t_vretrace = 0;
  /* Timings are 'ballpark' guesses and may vary from mode to mode, but
     such accuracy is probably not important... I hope. (--adm) */
  static int vvfreq = 17000;	/* 70 Hz - but the best we'll get with
  				 * current PIC will be 50 Hz */
  hitimer_t t, tdiff;
  unsigned char retval;

#ifdef X_SUPPORT
  vga.attr.flipflop = 0;	 	/* ATTR_INDEX_FLIPFLOP */
#endif

#ifdef OLD_CGA_SNOW_CODE
  /* old 'cga snow' code with the new variables - looks terrible,
   * but since it sometimes works we keep it for emergencies */
  hretrace ^= 0x01; vretrace++;
  retval = 0xc6 | hretrace | (vretrace & 0xfc ? 0 : 0x09);
#else
  t = GETusTIME(0);

  if(first) { t_vretrace = t; first = 0; }
  tdiff = t - t_vretrace;

#if DEBUG_MISC >= 2
  r_printf("VGAEmu: Misc: VR diff = %ld\n", tdiff);
#endif

  if(vretrace) {
    /* We're in 'display' mode and should return 0 in DE and VR */
    /* set display after 1ms from retrace start */
    if(tdiff > 1000) vretrace = hretrace = 0;
  }
  else {
    /* set retrace on timeout */
    if(tdiff > vvfreq) {
      /* We're in vertical retrace?  If so, set VR and DE flags */
      vretrace = 0x09; t_vretrace = t;
    }
    else {
      /* We're in horizontal retrace?  If so, just set DE flag, 0 in VR */
      hretrace = (tdiff%49) > 35;
    }
  }

  retval = 0xc4 | hretrace | vretrace;

#ifdef X_SUPPORT
  /*
   * Hercules cards use bit 7 for vertical retrace. This bit is not used
   * by VGA cards, so there should be no conflict here. -- sw
   */
  if(vga.config.mono_port && vretrace) retval &= ~0x80;
#endif

#endif	/* OLD_CGA_SNOW_CODE */

  misc_deb(
    "Misc_get_input_status_1: 0x%02x%s\n",
    (unsigned) retval,
    vretrace ? " (V_RETRACE)" : hretrace ? " (H_RETRACE)" : ""
  );

  return retval;
}

