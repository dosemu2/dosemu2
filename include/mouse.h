/* mouse support for dosemu 0.48p2+
 *     Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_BASE_VERSION	0x0700	/* minimum driver version 7.00 */
#define MOUSE_EMU_VERSION	0x0001	/* my driver version 0.01 */
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

/* don't change these for now, they're hardwired! */
#define Mouse_SEG  BIOSSEG
#define Mouse_OFF  0x1500
#define Mouse_ROUTINE_OFF  0x1550
#define Mouse_ADD  ((Mouse_SEG << 4)+Mouse_OFF)
#define Mouse_ROUTINE  ((Mouse_SEG << 4)+Mouse_ROUTINE_OFF)

#define MAX_MOUSE 1
#define HEIGHT 16

typedef struct mouse_structure {
  char dev[255];
  int fd;
  int type;
  int flags;
  boolean intdrv;
  boolean emulate3buttons;
  boolean cleardtr;
  int baudRate;
  int sampleRate;
  int lastButtons;
  int chordMiddle;
} mouse_t;

extern struct mouse_struct {
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
  int speed_x, speed_y;

  /* these are for CURSOR position */
  int cx, cy;

  /* these are for sensitivity options */
  int threshold;
  int language;

  signed short mickeyx, mickeyy;

  int ratio;
  unsigned int cursor_on;
  unsigned long cursor_type;

  /* this is for the user-defined subroutine */
  unsigned short cs, ip;
  unsigned short *csp, *ipp;
  unsigned short mask;

  unsigned short hidchar;
  unsigned int hidx, hidy;

  boolean mode;
} mouse;

void mouse_keyboard(int), mouse_curtick(void), mouse_sethandler(void *, unsigned short *, unsigned short *);

extern mouse_t mice[MAX_MOUSE] ;

#ifndef MOUSE_C
#define MEX extern
#else
#define MEX
#endif
MEX int keyboard_mouse;

extern void mouse_init(void);
extern void mouse_int(void);
extern void mouse_close(void);
extern void int74(void);

extern void mouse_move(void);
extern void mouse_lb(void);
extern void mouse_mb(void);
extern void mouse_rb(void);

extern void mouse_event(void);

#endif /* MOUSE_H */
