#include <termcap.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/kd.h>
#include <sys/vt.h>

#include "emu.h"
#include "termio.h"
#include "video.h"

extern int wait_vc_active(void);

void set_console_video(void)
{
  /* clear Linux's (unmapped) screen */
#ifdef USE_NCURSES 
  clear();
  refresh();
#else
  tputs(cl, 1, outch);
#endif
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

extern u_char in_linux_video;

void clear_console_video(void)
{

  if (scr_state.current) {
    set_linux_video();
    put_video_ram();		/* unmap the screen */
  }

  /* XXX - must be current console! */

  if (!config.vga)
    show_cursor();		/* restore the cursor */
  else {
    ioctl(ioc_fd, KIOCSOUND, 0);/* turn off any sound */
  }
  if (config.console_video) {
    v_printf("VID: Release mouse control\n");
    ioctl(ioc_fd, KDSETMODE, KD_TEXT);
  }

}

