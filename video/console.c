#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/vt.h>

#include "emu.h"
#include "termio.h"
#include "video.h"

extern int wait_vc_active(void);

inline void
console_update_cursor(int xpos, int ypos, int blinkflag, int forceflag)
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
  
  /* If forceflag is set, then this ensures that the cursor is actually
   * updated irregardless of its previous state. */
  if (forceflag) {
    oldx = -1;
    oldy = -1;
    oldblink = !blinkflag;
  }

  /* The cursor is off-screen, so disable its blinking */
  if ((unsigned) xpos > co || (unsigned) ypos > li)
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

void set_console_video(void)
{
  /* Clear the Linux console screen. The console recognizes these codes: 
   * \033[?25h = show cursor.
   * \033[0m = reset color.  
   * \033[H = Move cursor to upper-left corner of screen.  
   * \033[2J = Clear screen.  
   */
  fprintf(stdout,"\033[?25h\033[0m\033[H\033[2J");

  scr_state.mapped = 0;
  allow_switch();

  if (config.vga) {
    v_printf("VID: Taking mouse control\n");
    ioctl(ioc_fd, KDSETMODE, KD_GRAPHICS);
  }

  /* warning! this must come first! the VT_ACTIVATES which some below
     * cause set_dos_video() and set_linux_video() to use the modecr
     * settings.  We have to first find them here.
     */
  if (config.vga) {
    int permtest;

    permtest = set_ioperm(0x3d4, 2, 1);	/* get 0x3d4 and 0x3d5 */
    permtest |= set_ioperm(0x3da, 1, 1);
    permtest |= set_ioperm(0x3c0, 2, 1);	/* get 0x3c0 and 0x3c1 */
    if (config.chipset == S3) {
      permtest |= set_ioperm(0x102, 1, 1);
      permtest |= set_ioperm(0x2ea, 4, 1);
    }
  }

  /* XXX - get this working correctly! */
#define OLD_SET_CONSOLE 1
#ifdef OLD_SET_CONSOLE
  get_video_ram(WAIT);
  scr_state.mapped = 1;
#endif
  if (vc_active()) {
    int other_no = (scr_state.console_no == 1 ? 2 : 1);

    v_printf("VID: we're active, waiting...\n");
#ifndef OLD_SET_CONSOLE
    get_video_ram(WAIT);
    scr_state.mapped = 1;
#endif
    if (!config.vga) {
      ioctl(ioc_fd, VT_ACTIVATE, other_no);
      ioctl(ioc_fd, VT_ACTIVATE, scr_state.console_no);
    }
  }
  else
    v_printf("VID: not active, going on\n");

  allow_switch();
  clear_screen(0, 7);
}

void clear_console_video(void)
{
  if (scr_state.current) {
    set_linux_video();
    put_video_ram();		/* unmap the screen */
  }

  /* XXX - must be current console! */

  if (config.vga) 
    ioctl(ioc_fd, KIOCSOUND, 0);	/* turn off any sound */
  else
    fprintf(stdout,"\033[?25h");        /* Turn on the cursor */

  if (config.console_video) {
    v_printf("VID: Release mouse control\n");
    ioctl(ioc_fd, KDSETMODE, KD_TEXT);
  }
}

