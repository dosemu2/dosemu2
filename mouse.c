/* mouse.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/01/20 21:14:24 $
 * $Source: /home/src/dosemu0.49pl4g/RCS/mouse.c,v $
 * $Revision: 1.4 $
 * $State: Exp $
 *
 * $Log: mouse.c,v $
 * Revision 1.4  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.3  1993/11/30  22:21:03  root
 * Final Freeze for release pl3
 *
 * Revision 1.2  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.3  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.2  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.1  1993/02/24  11:33:24  root
 * Initial revision
 *
 */
#define MOUSE_C

#include <sys/types.h>

#include "config.h"
#include "emu.h"
#include "memory.h"
#include "mouse.h"

extern struct config_info config;

void mouse_reset(void), mouse_cursor(int), mouse_pos(void), mouse_curpos(void),
 mouse_setxminmax(void), mouse_setyminmax(void), mouse_set_tcur(void),
 mouse_set_gcur(void), mouse_setsub(void), mouse_bpressinfo(void), mouse_brelinfo(void),
 mouse_mickeys(void), mouse_version(void);

/* mouse movement functions */
void mouse_down(void), mouse_up(void), mouse_left(void), mouse_right(void),
 mouse_lb(void), mouse_rb(void), mouse_do_cur(void);

/* called when mouse changes */
void mouse_delta(int);

mouse_int(void)
{
  switch (LWORD(eax)) {
  case 0:			/* Mouse Reset/Get Mouse Installed Flag */
    mouse_reset();
    break;

  case 1:			/* Show Mouse Cursor */
    mouse_cursor(1);
    break;

  case 2:			/* Hide Mouse Cursor */
    mouse_cursor(0);
    break;

  case 3:			/* Get Mouse Position and Button Status */
    mouse_pos();
    break;

  case 4:			/* Get Cursor Position */
    mouse_curpos();
    break;

  case 5:			/* Get mouse button press info */
    mouse_bpressinfo();
    break;

  case 6:			/* Get mouse button release info */
    mouse_brelinfo();
    break;

  case 7:			/* Set Mouse Horizontal Min/Max Position */
    mouse_setxminmax();
    break;

  case 8:			/* Set Mouse Vertical Min/Max Position */
    mouse_setyminmax();
    break;

  case 9:			/* Set mouse gfx cursor */
    mouse_set_gcur();
    break;

  case 0xa:			/* Set mouse text cursor */
    mouse_set_tcur();
    break;

  case 0xb:			/* Read mouse motion counters */
    mouse_mickeys();
    break;

  case 0xc:			/* Set mouse subroutine */
    mouse_setsub();
    break;

  case 0x15:			/* Get size of buffer needed to hold state */
    LWORD(ebx) = sizeof(mouse);
    m_printf("MOUSE: get size of state buffer 0x%04x\n", LWORD(ebx));
    break;

  case 0x21:			/* Reset mouse software..this is BAD wrong */
    m_printf("MOUSE: reset mouse software\n");
    mouse_reset();
    LWORD(eax) = 0xffff;
    LWORD(ebx) = 2;
    break;

  case 0x24:			/* Get driver version/mouse type/IRQ */
    mouse_version();
    break;

  default:
    error("MOUSE: function 0x%04x not implemented\n", LWORD(eax));
    break;
  }
}

void 
mouse_reset(void)
{
  if (config.mouse_flag) {
    m_printf("MOUSE: reset mouse/installed!\n");
    LWORD(eax) = 0xffff;
    /*      LWORD(ebx)=M_BUTTONS; */
  }
  else {
    m_printf("MOUSE: reset mouse/not installed\n");
    LWORD(eax) = 0;
  }

  /* default is middle of screen */
  mouse.x = 320;
  mouse.y = 100;
  mouse.cx = 40;
  mouse.cy = 12;

  mouse.minx = mouse.miny = 0;
  mouse.maxx = 640;
  mouse.maxy = 200;
  mouse.ratio = 1;
  mouse.cursor_on = 0;
  mouse.cursor_type = 0;
  mouse.lbutton = mouse.mbutton = mouse.rbutton = 0;
  mouse.lpcount = mouse.mpcount = mouse.rpcount = 0;
  mouse.lrcount = mouse.mrcount = mouse.rrcount = 0;

  mouse.ip = mouse.cs = 0;
  mouse.mask = 0;		/* no interrupts */

  mouse.lpcount = mouse.lrcount = mouse.rpcount = mouse.rrcount = 0;
  mouse.lpx = mouse.lpy = mouse.lrx = mouse.lry = 0;
  mouse.rpx = mouse.rpy = mouse.rrx = mouse.rry = 0;

  mouse.mickeyx = mouse.mickeyy = 0;
}

void 
mouse_cursor(int flag)
{
  mouse.cursor_on = flag;
  m_printf("MOUSE: %s mouse cursor\n", flag ? "show" : "hide");
}

void 
mouse_pos(void)
{
  m_printf("MOUSE: get mouse position x:%d, y:%d, b(l%d r%d)\n", mouse.x,
	   mouse.y, mouse.lbutton, mouse.rbutton);
  LWORD(ecx) = mouse.x;
  LWORD(edx) = mouse.y;
  LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
}

void 
mouse_curpos(void)
{
  m_printf("MOUSE: get cursor pos x:%d, y:%d\n", mouse.cx, mouse.cy);
  LWORD(ecx) = mouse.cx;
  LWORD(edx) = mouse.cy;
}

void 
mouse_bpressinfo(void)
{
  if (LWORD(ebx) = 0) {		/* left button */
    LWORD(ebx) = mouse.lpcount;
    mouse.lpcount = 0;
    LWORD(ecx) = mouse.lpx;
    LWORD(edx) = mouse.lpy;
  }
  else {
    LWORD(ebx) = mouse.rpcount;
    mouse.rpcount = 0;
    LWORD(ecx) = mouse.rpx;
    LWORD(edx) = mouse.rpy;
  }
  LWORD(eax) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
}

void 
mouse_brelinfo(void)
{
  if (LWORD(ebx) = 0) {		/* left button */
    LWORD(ebx) = mouse.lrcount;
    mouse.lrcount = 0;
    LWORD(ecx) = mouse.lrx;
    LWORD(edx) = mouse.lry;
  }
  else {
    LWORD(ebx) = mouse.rrcount;
    mouse.rrcount = 0;
    LWORD(ecx) = mouse.rrx;
    LWORD(edx) = mouse.rry;
  }
  LWORD(eax) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
}

void 
mouse_setxminmax(void)
{
  m_printf("MOUSE: set horz. min: %d, max: %d\n", LWORD(ecx), LWORD(edx));

  /* if the min > max, they are swapped */
  mouse.minx = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.maxx = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
}

void 
mouse_setyminmax(void)
{
  m_printf("MOUSE: set vert. min: %d, max: %d\n", LWORD(ecx), LWORD(edx));

  /* if the min > max, they are swapped */
  mouse.miny = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.maxy = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
}

void 
mouse_set_gcur(void)
{
  m_printf("MOUSE: set gfx cursor...hspot: %d, vspot: %d, masks: %04x:%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(es), LWORD(edx));
}

void 
mouse_set_tcur(void)
{
  m_printf("MOUSE: set text cursor...type: %d, start: 0x%04x, end: 0x%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(edx));
}

void 
mouse_setsub(void)
{
  mouse.cs = REG(es);
  mouse.ip = LWORD(edx);

  *mouse.csp = mouse.cs;
  *mouse.ipp = mouse.ip;

  mouse.mask = LWORD(ecx);

  m_printf("MOUSE: user defined sub %04x:%04x, mask 0x%02x\n",
	   LWORD(es), LWORD(edx), LWORD(ecx));
}

void 
mouse_mickeys(void)
{
  m_printf("MOUSE: read mickeys %d %d\n", mouse.mickeyx, mouse.mickeyy);
  LWORD(ecx) = mouse.mickeyx;
  LWORD(edx) = mouse.mickeyy;
  mouse.mickeyx = mouse.mickeyy = 0;
}

void 
mouse_version(void)
{
  LWORD(ebx) = MOUSE_VERSION;
  HI(cx) = 2;			/* serial mouse */
  m_printf("MOUSE: get version %04x\n", LWORD(ebx));
}

void 
mouse_keyboard(int sc)
{
  switch (sc) {
  case 0x50:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_down();
    break;
  case 0x4b:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_left();
    break;
  case 0x4d:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_right();
    break;
  case 0x48:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_up();
    break;
  case 0x47:
    mouse_lb();
    break;
  case 0x4f:
    mouse_rb();
    break;
  default:
    m_printf("MOUSE: keyboard_mouse(), sc 0x%02x unknown!\n", sc);
  }
}

void 
mouse_down(void)
{
  m_printf("MOUSE: mouse moved down from %d to %d\n", mouse.y, mouse.y + 1);
  mouse.y += M_DELTA;
  if (mouse.y > 199)
    mouse.y = 199;
  mouse.cy = mouse.y / 8;
  mouse.mickeyy += MICKEY;
  mouse_delta(DELTA_CURSOR);
}

void 
mouse_up(void)
{
  m_printf("MOUSE: mouse moved up from %d to %d\n", mouse.y, mouse.y - 1);
  mouse.y -= M_DELTA;
  if (mouse.y < 0)
    mouse.y = 0;
  mouse.cy = mouse.y / 8;
  mouse.mickeyy -= MICKEY;
  mouse_delta(DELTA_CURSOR);
}

void 
mouse_right(void)
{
  m_printf("MOUSE: mouse moved right from %d to %d\n", mouse.x, mouse.x + 1);
  mouse.x += M_DELTA;
  if (mouse.x > 639)
    mouse.x = 639;
  mouse.cx = mouse.x / 8;
  mouse.mickeyx += MICKEY;
  mouse_delta(DELTA_CURSOR);
}

void 
mouse_left(void)
{
  m_printf("MOUSE: mouse moved left from %d to %d\n", mouse.x, mouse.x - 1);
  mouse.x -= M_DELTA;
  if (mouse.x < 0)
    mouse.x = 0;
  mouse.cx = mouse.x / 8;
  mouse.mickeyx -= MICKEY;
  mouse_delta(DELTA_CURSOR);
}

void 
mouse_lb(void)
{
  m_printf("MOUSE: left button %s\n", mouse.lbutton ? "released" : "pressed");
  if (mouse.lbutton) {
    mouse.lbutton = 0;
    mouse.lpcount++;
    mouse.lpx = mouse.x;
    mouse.lpy = mouse.y;
    mouse_delta(DELTA_LEFTBUP);
  }
  else {
    mouse.lbutton = 1;
    mouse.lrcount++;
    mouse.lrx = mouse.x;
    mouse.lry = mouse.y;
    mouse_delta(DELTA_LEFTBDOWN);
  }
}

void 
mouse_rb(void)
{
  m_printf("MOUSE: right button %s\n", mouse.rbutton ? "released" : "pressed");
  if (mouse.rbutton) {
    mouse.rbutton = 0;
    mouse.rpcount++;
    mouse.rpx = mouse.x;
    mouse.rpy = mouse.y;
    mouse_delta(DELTA_RIGHTBUP);
  }
  else {
    mouse.rbutton = 1;
    mouse.rrcount++;
    mouse.rrx = mouse.x;
    mouse.rry = mouse.y;
    mouse_delta(DELTA_RIGHTBDOWN);
  }
}

void 
fake_int(void)
{
  unsigned short *ssp;

  ssp = SEG_ADR((us *), ss, sp);
  *--ssp = LWORD(eflags);
  *--ssp = REG(cs);
  *--ssp = LWORD(eip);
  REG(esp) -= 6;
}

void 
fake_call(int cs, int ip)
{
  unsigned short *ssp;

  ssp = SEG_ADR((us *), ss, sp);
  m_printf("MOUSE: fake_call() shows S: %04x:%04x = %p\n",
	   LWORD(ss), LWORD(esp), (void *) ssp);
  *--ssp = cs;
  *--ssp = ip;
  REG(esp) -= 4;
}

void 
fake_pusha(void)
{
  unsigned short *ssp;

  ssp = SEG_ADR((unsigned short *), ss, sp);
  *--ssp = LWORD(eax);
  *--ssp = LWORD(ecx);
  *--ssp = LWORD(edx);
  *--ssp = LWORD(ebx);
  *--ssp = LWORD(esp);
  *--ssp = LWORD(ebp);
  *--ssp = LWORD(esi);
  *--ssp = LWORD(edi);
  REG(esp) -= 16;
}

void 
mouse_delta(int event)
{
  mouse_do_cur();

  if (mouse.mask & event) {
    fake_int();
    fake_pusha();

    LWORD(eax) = event;
    LWORD(ecx) = mouse.cx;
    LWORD(edx) = mouse.cy;
    LWORD(edi) = mouse.mickeyx;
    LWORD(esi) = mouse.mickeyy;
    LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);

    fake_call(Mouse_SEG, Mouse_OFF + 4);	/* skip to popa */

    REG(cs) = mouse.cs;
    REG(eip) = mouse.ip;

    REG(ds) = *mouse.csp;	/* put DS in user routine */

    m_printf("MOUSE: event type %d, "
	     "should call %04x:%04x (actually %04x:%04x)\n"
	     ".........jmping to %04x:%04x\n",
	     event, mouse.cs, mouse.ip, *mouse.csp, *mouse.ipp,
	     LWORD(cs), LWORD(eip));
    return;
  }
}

void 
mouse_do_cur(void)
{
  unsigned short *p = SCREEN_ADR(SCREEN);

  p[mouse.hidx + mouse.hidy * 80] = mouse.hidchar;

  /* save old char/attr pair */
  mouse.hidx = mouse.cx;
  mouse.hidy = mouse.cy;
  mouse.hidchar = p[mouse.cx + mouse.cy * 80];

  p[mouse.cx + mouse.cy * 80] = 0x0b5c;
}

void 
mouse_curtick(void)
{
  unsigned short *p = SCREEN_ADR(0);

  if (!mouse.cursor_on)
    return;

  m_printf("MOUSE: curtick x: %d  y:%d\n", mouse.cx, mouse.cy);
  mouse_do_cur();
}

/* are ip and cs in right order?? */
void 
mouse_sethandler(void *f, us * cs, us * ip)
{
  m_printf("MOUSE: sethandler...%p, %p, %p\n",
	   (void *) f, (void *) cs, (void *) ip);
  m_printf("...............CS:%04x.....IP:%04x\n", *cs, *ip);
  mouse.csp = cs;
  mouse.ipp = ip;
}

#undef MOUSE_C
