/* mouse support for dosemu 0.48p2+
 *     Robert Sanders, gt8134b@prism.gatech.edu
 */
#ifndef MOUSE_H
#define MOUSE_H

#define MOUSE_BASE_VERSION	0x0700	/* minimum driver version 7.00 */
#define MOUSE_EMU_VERSION	0x0001	/* my driver version 0.01 */
/* this is the version returned to DOS programs */
#define MOUSE_VERSION	  (MOUSE_BASE_VERSION + MOUSE_EMU_VERSION)

/* types of mouse events */
#define DELTA_CURSOR		1
#define DELTA_LEFTBDOWN		2	/* pressed  */
#define DELTA_LEFTBUP		4	/* released */
#define DELTA_RIGHTBDOWN	8
#define DELTA_RIGHTBUP		16

#define MICKEY			9	/* mickeys per move */
#define M_DELTA			8

/* don't change these for now, they're hardwired! */
#define Mouse_SEG  BIOSSEG
#define Mouse_OFF  0x1500
#define Mouse_ADD  ((Mouse_SEG << 4)+Mouse_OFF)

struct mouse_struct {
  unsigned char lbutton, mbutton, rbutton;

  int lpcount, lrcount, mpcount, mrcount, rpcount, rrcount;

  /* positions for last press/release for each button */
  int lpx, lpy, rpx, rpy;
  int lrx, lry, rrx, rry;

  /* these are for MOUSE position */
  int x, y;
  int minx, maxx, miny, maxy;

  /* these are for CURSOR position */
  int cx, cy;

  signed short mickeyx, mickeyy;

  int ratio;
  unsigned char cursor_on;
  unsigned long cursor_type;

  /* this is for the user-defined subroutine */
  unsigned short cs, ip;
  unsigned short *csp, *ipp;
  unsigned short mask;

  unsigned short hidchar;
  unsigned int hidx, hidy;
}

mouse;

int mouse_int(void);
void mouse_keyboard(int), mouse_curtick(void), mouse_sethandler(void *, unsigned short *, unsigned short *);

#ifndef MOUSE_C
#define MEX extern
#else
#define MEX
#endif
MEX int keyboard_mouse;

#endif /* MOUSE_H */
