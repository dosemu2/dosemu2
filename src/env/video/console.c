/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include <sys/kd.h>
#include <sys/vt.h>
#endif

#include "emu.h"
#include "termio.h"
#include "video.h"
#include "vc.h"
#include "mapping.h"
#include "bios.h"
#include "vgaemu.h"

static void console_update_cursor(void)
{
  /* This routine updates the state of the cursor, including its cursor
   * position and visibility.  The linux console recognizes the ANSI code 
   * "\033[y;xH" where y is the cursor row, and x is the cursor position.  
   * For example "\033[15;8H" printed to stdout will move the cursor to 
   * row 15, column 8.   The "\033[?25l" string hides the cursor and the 
   * "\033[?25h" string shows the cursor.
   */

  /* Static integers to preserve state of variables from last call. */
  static int oldx = -1;
  static int oldy = -1;
  static int oldblink = 0;
  
  int xpos = ((vga.crtc.cursor_location - vga.display_start) % vga.scan_len) / 2;
  int ypos = (vga.crtc.cursor_location - vga.display_start) / vga.scan_len;
  int blinkflag = !(vga.crtc.cursor_shape & 0x6000);

  if ((vga.display_start / PAGE_SIZE) != scr_state.pageno) {
    /* page flipping, if possible */
    set_vc_screen_page();
  }

  /* The cursor is off-screen, so disable its blinking */
  if ((unsigned) xpos >= vga.text_width || (unsigned) ypos >= vga.text_height)
    blinkflag = 0;
  
  if (blinkflag) {
    /* Enable blinking if it has not already */
    if (!oldblink)
      fprintf(stdout,"\033[?25h");
    /* Update cursor position if it has moved since last time in this func */
    if ((xpos != oldx) || (ypos != oldy)) 
      fprintf(stdout,"\033[%d;%dH",ypos+1,xpos+1);
  }
  else {
    /* Disable blinking if it hasnt already */ 
    if (oldblink)
      fprintf(stdout,"\033[?25l");
  }
  
  /* Save current state of cursor for next call to this function. */
  oldx = xpos; oldy = ypos; oldblink = blinkflag;
}

static int console_post_init(void)
{
  int kdmode;

  set_process_control();
  k_printf("KBD: Taking mouse control\n");  /* Actually only in KD_GRAPHICS... */
  /* Some escape sequences don't work in KD_GRAPHICS... */
  kdmode = config.vga? KD_GRAPHICS: KD_TEXT;
  ioctl(console_fd, KDSETMODE, kdmode);

  /* Clear the Linux console screen. The console recognizes these codes: 
   * \033[?25h = show cursor.
   * \033[0m = reset color.  
   * \033[H = Move cursor to upper-left corner of screen.  
   * \033[2J = Clear screen.  
   */
  vga_emu_init(0, NULL);
  if (!config.vga) {
    int co, li;
    gettermcap(0, &co, &li);
    fprintf(stdout,"\033[?25h\033[0m\033[H\033[2J");
    vga_emu_setmode(config.cardtype == CARD_MDA ? 7 : 3, co, li);
  }
  scr_state.mapped = 0;
  allow_switch();

  /*
     Switch to dosemu VT if config.forcevtswitch is set.
     The idea of this action is that in case of config.console_video
     dosemu need to work at active VT (at least at start) for accessing
     video memory. So to be able to run dosemu for instance
     through cron we need config option like forcevtswitch.
     The action seems sensible only if config.console_video.
                                                   saw@shade.msu.ru
  */
  
  if (config.force_vt_switch && !vc_active()) {
    if (ioctl(console_fd, VT_ACTIVATE, scr_state.console_no)<0)
      v_printf("VID: error VT switching %s\n", strerror(errno));
  }

  /* XXX - get this working correctly! */
#define OLD_SET_CONSOLE 1
#ifdef OLD_SET_CONSOLE
  init_get_video_ram(WAIT);
  scr_state.mapped = 1;
#endif
  if (vc_active()) {
    int other_no = (scr_state.console_no == 1 ? 2 : 1);
    v_printf("VID: we're active, waiting...\n");
#ifndef OLD_SET_CONSOLE
    init_get_video_ram(WAIT);
    scr_state.mapped = 1;
#endif
    if (!config.vga) {
      ioctl(console_fd, VT_ACTIVATE, other_no);
/*
 *    Fake running signal_handler() engine to process release/acquire
 *    vt's.
 */
      while(vc_active())handle_signals();
      ioctl(console_fd, VT_ACTIVATE, scr_state.console_no);
      while(!vc_active())handle_signals();
      handle_signals();
    }
  }
  else
    v_printf("VID: not active, going on\n");

  allow_switch();
  return 0;
}

void clear_console_video(void)
{
  v_printf("VID: video_close():clear console video\n");
  if (scr_state.current) {
    set_linux_video();
    release_perm();
    put_video_ram();		/* unmap the screen */
  }

  k_printf("KBD: Release mouse control\n");
  ioctl(console_fd, KDSETMODE, KD_TEXT);
  clear_process_control();
}

static int consolesize;

int console_size(void)
{
  return consolesize;
}

static int console_init(void)
{
  int co, li;
  gettermcap(0, &co, &li);
  consolesize = TEXT_SIZE(co,li);
  register_hardware_ram('v', VGA_PHYS_TEXT_BASE, TEXT_SIZE(co,li));
  return 0;
}

static void console_close(void)
{
  clear_console_video();
  fprintf(stdout,"\033[?25h\r");      /* Turn on the cursor */
}

#define console_setmode NULL

struct video_system Video_console = {
   console_init,
   console_post_init,
   console_close,
   console_setmode,
   NULL,             /* update_screen */
   console_update_cursor,
   NULL,
   NULL              /* handle_events */
};

