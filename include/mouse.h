/* mouse support for dosemu 0.48p2+
 *     Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_BASE_VERSION	0x0700	/* minimum driver version 7.00 */
#define MOUSE_EMU_VERSION	0x0000	/* my driver version 0.00 */
/* this is the version returned to DOS programs */
#define MOUSE_VERSION	  (MOUSE_BASE_VERSION + MOUSE_EMU_VERSION)

#define MOUSE_NONE -1
#define MOUSE_MICROSOFT 0
#define MOUSE_MOUSESYSTEMS 1
#define MOUSE_MMSERIES 2
#define MOUSE_LOGITECH 3
#define MOUSE_BUSMOUSE 4
#define MOUSE_MOUSEMAN 5
#define MOUSE_PS2 6
#define MOUSE_HITACHI 7
#define MOUSE_X 8

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

#define MAX_MOUSE 1
#define HEIGHT 16
#define PLANES 4

typedef struct  {
  char dev[255];
  int fd;
  int type;
  int flags;
  boolean add_to_io_select;
  boolean intdrv;
  boolean emulate3buttons;
  boolean cleardtr;
  int baudRate;
  int sampleRate;
  int lastButtons;
  int chordMiddle;
} mouse_t;

EXTERN struct  {
  unsigned char lbutton, mbutton, rbutton;
  unsigned char oldlbutton, oldmbutton, oldrbutton;

  int lpcount, lrcount, mpcount, mrcount, rpcount, rrcount;

  /* positions for last press/release for each button */
  int lpx, lpy, mpx, mpy, rpx, rpy;
  int lrx, lry, mrx, mry, rrx, rry;

  /* exclusion zone */
  int exc_ux, exc_uy, exc_lx, exc_ly;

  /* these are for MOUSE position */
  int x, y;
  int points;
  int minx, maxx, miny, maxy;
  float speed_x, speed_y;

  /* these are for CURSOR position */
  int cx, cy;

  /* these are for GRAPHIC cursor stuff */
  boolean gfx_cursor;
  unsigned short gfx_width, gfx_height;
  unsigned short gfx_segment, gfx_offset;
  unsigned char gsave[HEIGHT][PLANES][2];
  int goldx, goldy;
  boolean ginit;

  /* these are for sensitivity options */
  int threshold;
  int language;

  signed short mickeyx, mickeyy;

  unsigned int cursor_on;

  /* this is for the user-defined subroutine */
  unsigned short cs, ip;
  unsigned short *csp, *ipp;
  unsigned short mask;

  unsigned short hidchar, newchar;
  unsigned int hidx, hidy;

  boolean mode;

  int display_page;

  boolean ignorexy;

  struct {
    boolean state;
    unsigned short pkg;
  } ps2;
} mouse;

void mouse_keyboard(int), mouse_curtick(void), mouse_sethandler(void *, unsigned short *, unsigned short *);

EXTERN mouse_t mice[MAX_MOUSE] ;

EXTERN int keyboard_mouse;

extern void mouse_init(void);
extern void mouse_int(void);
extern void mouse_close(void);
extern void int74(void);

extern void mouse_move(void);
extern void mouse_lb(void);
extern void mouse_mb(void);
extern void mouse_rb(void);

extern void DOSEMUMouseEvents(void);

extern void mouse_event(void);

#endif /* MOUSE_H */
