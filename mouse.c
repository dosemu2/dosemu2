/* mouse.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/04/30 01:05:16 $
 * $Source: /home/src/dosemu0.60/RCS/mouse.c,v $
 * $Revision: 1.16 $
 * $State: Exp $
 *
 * $Log: mouse.c,v $
 * Revision 1.16  1994/04/30  01:05:16  root
 * Clean up.
 *
 * Revision 1.15  1994/04/27  23:39:57  root
 * Lutz's patches to get dosemu up under 1.1.9.
 *
 * Revision 1.14  1994/04/27  21:34:15  root
 * Jochen's Latest.
 *
 * Revision 1.13  1994/04/23  20:51:40  root
 * Get new stack over/underflow working in VM86 mode.
 *
 * Revision 1.12  1994/04/18  22:52:19  root
 * Ready pre51_7.
 *
 * Revision 1.11  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.10  1994/04/07  20:50:59  root
 * More updates.
 *
 * Revision 2.9  1994/04/06  00:57:16  root
 * Made serial config more flexible, and up to 4 ports.
 *
 * Revision 1.8  1994/04/04  22:51:55  root
 * Patches for PS/2 mice.
 *
 * Revision 1.7  1994/03/23  23:24:51  root
 * Prepare to split out do_int.
 *
 * Revision 1.6  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.5  1994/03/04  14:46:13  root
 * Jochen patches.
 *
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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "bios.h"
#include "emu.h"
#include "memory.h"
#include "video.h"		/* video base address */
#include "mouse.h"
#include "serial.h"

extern struct config_info config;

void mouse_reset(void), mouse_cursor(int), mouse_pos(void), mouse_curpos(void),
 mouse_setxminmax(void), mouse_setyminmax(void), mouse_set_tcur(void),
 mouse_set_gcur(void), mouse_setsub(void), mouse_bpressinfo(void), mouse_brelinfo(void),
 mouse_mickeys(void), mouse_version(void);

/* mouse movement functions */
void mouse_updown(void), mouse_leftright(void),
 mouse_lb(void), mouse_rb(void), mouse_do_cur(void);

/* called when mouse changes */
void mouse_delta(int);

mouse_t mice[MAX_MOUSE];

static long mousecursormask[HEIGHT] =  {
  0x00000000L,  /*0000000000000000*/
  0x40000000L,  /*0100000000000000*/
  0x60000000L,  /*0110000000000000*/
  0x70000000L,  /*0111000000000000*/
  0x78000000L,  /*0111100000000000*/
  0x7c000000L,  /*0111110000000000*/
  0x7e000000L,  /*0111111000000000*/
  0x7f000000L,  /*0111111100000000*/
  0x7f800000L,  /*0111111110000000*/
  0x7f000000L,  /*0111111100000000*/
  0x7c000000L,  /*0111110000000000*/
  0x46000000L,  /*0100011000000000*/
  0x06000000L,  /*0000011000000000*/
  0x03000000L,  /*0000001100000000*/
  0x03000000L,  /*0000001100000000*/
  0x00000000L   /*0000000000000000*/
};

static long mousescreenmask[HEIGHT] =  {
  0x3fffffffL,  /*0011111111111111*/
  0x1fffffffL,  /*0001111111111111*/
  0x0fffffffL,  /*0000111111111111*/
  0x07ffffffL,  /*0000011111111111*/
  0x03ffffffL,  /*0000001111111111*/
  0x01ffffffL,  /*0000000111111111*/
  0x00ffffffL,  /*0000000011111111*/
  0x007fffffL,  /*0000000001111111*/
  0x003fffffL,  /*0000000000111111*/
  0x007fffffL,  /*0000000001111111*/
  0x01ffffffL,  /*0000000111111111*/
  0x10ffffffL,  /*0001000011111111*/
  0xb0ffffffL,  /*1011000011111111*/
  0xf87fffffL,  /*1111100001111111*/
  0xf87fffffL,  /*1111100001111111*/
  0xfcffffffL   /*1111110011111111*/
};

void
mouse_int(void)
{
  unsigned short tmp1, tmp2, tmp3;

	m_printf("MOUSEALAN: int 0x%x\n", LWORD(eax));
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

  case 0x11: 
    LWORD(eax) = 0xffff;	/* Genius mouse driver, return 2 buttons */
    LWORD(ebx) = 2;
    break;

  case 0x14:
    tmp1 = LWORD(ecx);
    tmp2 = REG(es);
    tmp3 = LWORD(edx);
    LWORD(ecx) = mouse.mask;
    REG(es) = mouse.cs;
    LWORD(edx) = mouse.ip;
    mouse.mask = tmp1;
    mouse.cs = tmp2;
    mouse.ip = tmp3;
    break;

  case 0x15:			/* Get size of buffer needed to hold state */
    LWORD(ebx) = sizeof(mouse);
    m_printf("MOUSE: get size of state buffer 0x%04x\n", LWORD(ebx));
    break;

  case 0x1a:			/* SET MOUSE SENSITIVITY */
    mouse.horzsen = LWORD(ebx);
    mouse.vertsen = LWORD(ecx);
    mouse.threshold = LWORD(edx);
    break;

  case 0x1b:
    LWORD(ebx) = mouse.horzsen;		/* horizontal speed */
    LWORD(ecx) = mouse.vertsen;		/* vertical speed */
    LWORD(edx) = mouse.threshold;	/* double speed threshold */
    break;

  case 0x21:			
    m_printf("MOUSE: software reset on mouse\n");
    LWORD(eax) = 0xffff;
    LWORD(ebx) = 2;
    break;

  case 0x22:			/* Set language for messages */
    m_printf("MOUSE: Set LANGUAGE\n");
    break;			/* Ignore as we are US version only */

  case 0x23:
    m_printf("MOUSE: Get LANGUAGE\n");
    LWORD(ebx) = 0x0000;	/* Return US/ENGLISH */
    break;

  case 0x24:			/* Get driver version/mouse type/IRQ */
    mouse_version();
    break;

  case 0x25:
    LWORD(eax) = 0x4103;	/* Set interrupt rate to 30 times sec */
    break;

  case 0x30:
    LWORD(eax) = 0xFFFF;	/* We are currently v7.01, need 7.04+ for this */
    break;
 
  case 0x42:
    LWORD(eax) = 0x42;	/* Configures mouse for Microsoft Mode */
  
/*    LWORD(eax) = 0xffff;		 Uncomment this for PC Mouse Mode */
/*    LWORD(ebx) = sizeof(mouse);	 applies to Genius Mouse too */
    break;

  default:
    error("MOUSE: function 0x%04x not implemented\n", LWORD(eax));
    break;
  }
}

void 
mouse_reset(void)
{
    m_printf("MOUSE: reset mouse/installed!\n");
    LWORD(eax) = 0xffff;
    LWORD(ebx)=2; 

  /* default is top left of the screen */
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

  mouse.horzsen = 75;
  mouse.vertsen = 75;
  mouse.threshold = 20;

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
  m_printf("MOUSE: Button press info\n");
  if (LWORD(ebx) == 0) {		/* left button */
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
	m_printf("MOUSE: Button release info\n");
  if (LWORD(ebx) == 0) {		/* left button */
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
  HI(cx) = 4;			/* ps2 mouse */
  LO(cx) = 0;			/* 0=ps2 */
  m_printf("MOUSE: get version %04x\n", LWORD(ebx));
}

void 
mouse_keyboard(int sc)
{
  switch (sc) {
  case 0x50:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_updown();
    break;
  case 0x4b:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_leftright();
    break;
  case 0x4d:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_leftright();
    break;
  case 0x48:
    mouse.rbutton = mouse.lbutton = 0;
    mouse_updown();
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
mouse_updown(void)
{
  m_printf("MOUSE: mouse moved from %d to %d\n", mouse.y, mouse.y - 1);
  if (mouse.y < 0)
    mouse.y = 0;
  if (mouse.y > 199)
    mouse.y = 199;
  mouse.cy = mouse.y / 8;
  mouse.mickeyy -= MICKEY;
  mouse_delta(DELTA_CURSOR);
}

void 
mouse_leftright(void)
{
  m_printf("MOUSE: mouse moved from %d to %d\n", mouse.x, mouse.x - 1);
  if (mouse.x < 0)
    mouse.x = 0;
  if (mouse.x > 639)
    mouse.x = 639;
  mouse.cx = mouse.x / 8;
  mouse.mickeyx -= MICKEY;
  mouse_delta(DELTA_CURSOR);
}

void 
mouse_lb(void)
{
  m_printf("MOUSE: left button %s\n", mouse.lbutton ? "released" : "pressed");
  if (!mouse.lbutton) {
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
  if (!mouse.rbutton) {
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
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *)(REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, vflags);
  pushw(ssp, sp, LWORD(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 6;
}

void 
fake_call(int cs, int ip)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *)(REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  m_printf("MOUSE: fake_call() shows S: %04x:%04x\n",
	   LWORD(ss), LWORD(esp));
  pushw(ssp, sp, cs);
  pushw(ssp, sp, ip);
  LWORD(esp) -= 4;
}

void 
fake_pusha(void)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *)(REG(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, LWORD(eax));
  pushw(ssp, sp, LWORD(ecx));
  pushw(ssp, sp, LWORD(edx));
  pushw(ssp, sp, LWORD(ebx));
  pushw(ssp, sp, LWORD(esp));
  pushw(ssp, sp, LWORD(ebp));
  pushw(ssp, sp, LWORD(esi));
  pushw(ssp, sp, LWORD(edi));
  LWORD(esp) -= 16;
}

void
mouse_delta(int event)
{
  if (mouse.mask & event) {
    fake_int();
    fake_pusha();

    LWORD(eax) = event;
    LWORD(ecx) = mouse.cx * 8;
    LWORD(edx) = mouse.cy * 8;
    LWORD(edi) = mouse.mickeyx;
    LWORD(esi) = mouse.mickeyy;
    LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);

    fake_call(Mouse_SEG, Mouse_OFF + 4);	/* skip to popa */

    REG(cs) = mouse.cs;
    REG(eip) = mouse.ip;

    /* REG(ds) = *mouse.csp;  	put DS in user routine */

    m_printf("MOUSE: event type %d, "
	     "should call %04x:%04x (actually %04x:%04x)\n"
	     ".........jmping to %04x:%04x\n",
	     event, mouse.cs, mouse.ip, *mouse.csp, *mouse.ipp,
	     LWORD(cs), LWORD(eip));
	
/*
	REG(eflags) &= 0xfffffcff;
*/
    return;
  }
}

void
mouse_do_cur(void)
{
  unsigned short *p = SCREEN_ADR(bios_current_screen_page);

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

/* PS/2 Mouse Interrupt 0x12h-hardware 0x74-software */
void
int74(void)
{
  unsigned char buffer[64];
  static unsigned char buffer2[8];
  int bytes, i, dx, dy, lbutton, mbutton, rbutton;
  static int dirty = 0;

  bytes = RPT_SYSCALL(read(mice->fd, (char *)buffer, sizeof(buffer)));

  for (i=0; i<bytes; i++) {
    if (dirty == 0 && (buffer[i] & 0xC0) != 0) continue;
    buffer2[dirty++] = buffer[i];
    if (dirty != 3) continue;
    mbutton = (buffer2[0] & 0x04) >> 2;   
    rbutton = (buffer2[0] & 0x02) >> 1;
    lbutton = (buffer2[0] & 0x01);        
    dx = (buffer2[0] & 0x10) ?    buffer2[1]-256  :  buffer2[1];
    dy = (buffer2[0] & 0x20) ?  -(buffer2[2]-256) : -buffer2[2];
    dirty = 0;
    mouse.x = mouse.x +dx;
    mouse.y = mouse.y +dy;
    if (dx) mouse_leftright();
    if (dy) mouse_updown();
    mouse.lbutton = lbutton;
    mouse_lb();
    mouse.rbutton = rbutton;
    mouse_rb();
/*
 *  mouse.mbutton = mbutton;
 *  mouse_mb();
 */
  }
}

void
mouse_init(void)
{
  serial_t *sptr;
  if ((mice->type == MOUSE_PS2) || (mice->type == MOUSE_BUSMOUSE)) {
    mice->fd = DOS_SYSCALL(open(mice->dev, O_RDWR | O_NONBLOCK)); 
  }
  else {
    sptr = &com[config.num_ser];
    if (!(sptr->mouse)) {
      m_printf("MOUSE: Failed to add serial mouse, no 'mouse' keyword in serial config!\n");
      mice->intdrv = FALSE;
    }
  }
}

void
mouse_close(void)
{
  if ((mice->type == MOUSE_PS2) || (mice->type == MOUSE_BUSMOUSE))
  DOS_SYSCALL(close(mice->fd));
}
	
#undef MOUSE_C
