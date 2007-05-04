/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* mouse support for dosemu 0.48p2+
 *     Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef MOUSE_H
#define MOUSE_H

#include "config.h"
#include <termios.h>

#define MOUSE_BASE_VERSION	0x0700	/* minimum driver version 7.00 */
#define MOUSE_EMU_VERSION	0x0005	/* my driver version 0.05 */
/* this is the version returned to DOS programs */
#define MOUSE_VERSION	  (MOUSE_BASE_VERSION + MOUSE_EMU_VERSION)

#define MOUSE_NONE -1
#define MOUSE_MICROSOFT 0
#define MOUSE_MS3BUTTON 1
#define MOUSE_MOUSESYSTEMS 2
#define MOUSE_MMSERIES 3
#define MOUSE_LOGITECH 4
#define MOUSE_BUSMOUSE 5
#define MOUSE_MOUSEMAN 6
#define MOUSE_PS2 7
#define MOUSE_HITACHI 8
#define MOUSE_X 9
#define MOUSE_IMPS2 10
#define MOUSE_XTERM 11
#define MOUSE_GPM 12
#define MOUSE_SDL 13

/* types of mouse events */
#define DELTA_CURSOR		1
#define DELTA_LEFTBDOWN		2	/* pressed  */
#define DELTA_LEFTBUP		4	/* released */
#define DELTA_RIGHTBDOWN	8
#define DELTA_RIGHTBUP		16
#define DELTA_MIDDLEBDOWN  32
#define DELTA_MIDDLEBUP    64

#define MICKEY			9	/* mickeys per move */
#define M_DELTA			8

#define HEIGHT 16
#define PLANES 4

typedef struct  {
  char *dev;
  int fd;
  int type;
  int flags;
  boolean intdrv;
  /* whether we use the native DOS cursor, or the system cursor (X, GPM) */
  boolean native_cursor;
  boolean use_absolute; /* use absolute mouse addressing */
  boolean emulate3buttons;
  boolean has3buttons;
  boolean cleardtr;
  int baudRate;
  int sampleRate;
  int lastButtons;
  int chordMiddle;
  short init_speed_x, init_speed_y;

  struct termios *oldset;

} mouse_t;

/* this entire structure gets saved on a driver state save */
/* therefore we try to keep it small where appropriate */
struct mouse_struct {
  boolean enabled;
  unsigned char lbutton, mbutton, rbutton;
  unsigned char oldlbutton, oldmbutton, oldrbutton;

  short lpcount, lrcount, mpcount, mrcount, rpcount, rrcount;

  /* positions for last press/release for each button */
  short lpx, lpy, mpx, mpy, rpx, rpy;
  short lrx, lry, mrx, mry, rrx, rry;

  /* TRUE if we're in a graphics mode */
  boolean gfx_cursor;

  unsigned char xshift, yshift;

  /* text cursor definition (we only support software cursor) */
  unsigned short textscreenmask, textcursormask;

  /* graphics cursor defnition */
  signed char hotx, hoty;
  unsigned short graphscreenmask[16], graphcursormask[16];

  /* exclusion zone */
  short exc_ux, exc_uy, exc_lx, exc_ly;

  /* these are clipped to min and max x; they are *not* rounded. */
  short x, y;
  /* unscaled ones, to not loose the precision */
  short unsc_x, unsc_y;
  /* coordinates at which the cursor was last drawn */
  short oldrx, oldry;
  /* these are the cursor extents; they are rounded off. */
  short minx, maxx, miny, maxy;
  /* same as above except can be played with */
  short virtual_minx, virtual_maxx, virtual_miny, virtual_maxy;

  /* these are for sensitivity options */
  short speed_x, speed_y;
  short threshold;
  short language;

  /* accumulated motion counters */
  short mickeyx, mickeyy;
  short old_mickeyx, old_mickeyy;

  /* zero if cursor is on, negative if it's off */
  short cursor_on;

  /* this is for the user-defined subroutine */
  unsigned short cs, ip;
  unsigned short mask;

  /* true if mouse has three buttons (third might be emulated) */
  boolean threebuttons;

  short display_page;

  /* These are to enable work arounds for broken applications */
  short min_max_x, min_max_y;

  /* ignore application's x/y speed settings?  might not be necessary
  	anymore if I managed to get the speed settings correct. */
  boolean ignorexy;

  struct {
    boolean state;
    unsigned short pkg;
    unsigned short cs, ip;
  } ps2;
};
extern volatile struct mouse_struct mouse;

struct mouse_client {
  const char *name;
  int    (*init)(void);
  void   (*close)(void);
  void   (*run)(void);         /* handle mouse events */
  void   (*set_cursor)(int action, int mx, int my, int x_range, int y_range);
  struct mouse_client *next;
};

void register_mouse_client(struct mouse_client *mouse);

extern struct mouse_client *Mouse;
extern struct mouse_client Mouse_raw;

#include "keyboard.h"
void mouse_keyboard(Boolean make, t_keysym key);
void mouse_curtick(void);

EXTERN int keyboard_mouse;

extern void dosemu_mouse_init(void);
extern void dosemu_mouse_reset(void);
extern void mouse_ps2bios(void);
extern void int33_post(void);
extern int mouse_int(void);
extern void dosemu_mouse_close(void);
extern void freeze_mouse(void);
extern void unfreeze_mouse(void);
extern void mouse_post_boot(void);
extern void int74(void);

extern int DOSEMUMouseEvents(int);

extern void do_mouse_irq(void);
extern void mouse_io_callback(void);

extern void mouse_move_buttons(int lbutton, int mbutton, int rbutton);
extern void mouse_move_relative(int dx, int dy);
extern void mouse_move_absolute(int x, int y, int x_range, int y_range);
extern void mouse_reset_to_current_video_mode(int mode);
extern void mouse_enable_native_cursor(int flag);

#endif /* MOUSE_H */
