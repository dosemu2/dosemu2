/* this handles the int10 video functions.
   Most functions here change only the video memory and status 
   variables; the actual screen is then rendered asynchronously 
   after these by Video->update_screen.
*/

#include "emu.h"
#include "video.h"
#include "memory.h"
#include "bios.h"
#include "vc.h"

/* a bit brutal, but enough for now */
#if VIDEO_CHECK_DIRTY
#define set_dirty(page) vm86s.screen_bitmap = 0xffffffff
#else
#define set_dirty(page)
#endif

/* this maps the cursor shape given by int10, fn1 to the actually
   displayed cursor start&end values in cursor_shape. This seems 
   to be typical IBM Black Compatiblity Magic.
   I modeled it approximately from the behaviour of my own 
   VGA's BIOS.
   I'm not sure if it is correct for start=end and for font_heights
   other than 16.
*/

inline void set_cursor_shape(ushort shape) {
   int cs,ce;

   cs=CURSOR_START(shape) & 0x1F;
   ce=CURSOR_END(shape) & 0x1F;

   if (shape & 0x6000 || cs>ce) {
      v_printf("no cursor\n");
      cursor_shape=NO_CURSOR;
      cursor_blink=0;
      return;
   }
   
   /* is this correct (my own VGA just turns off the cursor if
      this bit is set)
   */
   cursor_blink = 1;

   cs&=0x0F;
   ce&=0x0F;
   if (ce>3 && ce<12) {
      if (cs>ce-3) cs+=font_height-ce-1;
      else if (cs>3) cs=font_height/2;
      ce=font_height-1;
   }
   v_printf("mapped cursor start=%d end=%d\n",cs,ce);
   CURSOR_START(cursor_shape)=cs;
   CURSOR_END(cursor_shape)=ce;
}

/* This is a better scroll routine, mostly for aesthetic reasons. It was
 * just too horrible to contemplate a scroll that worked 1 character at a
 * time :-)
 * 
 * It may give some performance improvement on some systems (it does
 * on mine) (Andrew Tridgell)
 */
void
Scroll(int x0, int y0, int x1, int y1, int l, int att)
{
  int dx = x1 - x0 + 1;
  int dy = y1 - y0 + 1;
  int x, y;
  us blank = ' ' | (att << 8);
  us tbuf[MAX_COLUMNS];

  if (dx <= 0 || dy <= 0 || x0 < 0 || x1 >= co || y0 < 0 || y1 >= li)
    return;

  /* make a blank line */
  for (x = 0; x < dx; x++)
    tbuf[x] = blank;

  if (l >= dy || l <= -dy)
    l = 0;

  set_dirty(video_page);
  
  if (l == 0) {			/* Wipe mode */
    for (y = y0; y <= y1; y++)
      memcpy(&screen_adr[y * co + x0], tbuf, dx * sizeof(us));
    return;
  }

  if (l > 0) {
    if (dx == co)
      memcpy(&screen_adr[y0 * co], &screen_adr[(y0 + l) * co], (dy - l) * dx * sizeof(us));
    else
      for (y = y0; y <= (y1 - l); y++)
	memcpy(&screen_adr[y * co + x0], &screen_adr[(y + l) * co + x0], dx * sizeof(us));

    for (y = y1 - l + 1; y <= y1; y++)
      memcpy(&screen_adr[y * co + x0], tbuf, dx * sizeof(us));
  }
  else {
    for (y = y1; y >= (y0 - l); y--)
      memcpy(&screen_adr[y * co + x0], &screen_adr[(y + l) * co + x0], dx * sizeof(us));

    for (y = y0 - l - 1; y >= y0; y--)
      memcpy(&screen_adr[y * co + x0], tbuf, dx * sizeof(us));
  }
}

/* Output a character to the screen. */ 
void
char_out(unsigned char ch, int s)
{
  int newline_att = 7;
  int xpos, ypos;
  xpos = bios_cursor_x_position(s);
  ypos = bios_cursor_y_position(s);

  switch (ch) {
  case '\r':         /* Carriage return */
    xpos = 0;
    break;

  case '\n':         /* Newline */
    newline_att = ATTR(SCREEN_ADR(s) + ypos*co + xpos); /* Color newline */
    ypos++;
    xpos = 0;                  /* EDLIN needs this behavior */
    break;

  case 8:           /* Backspace */
    if (xpos > 0) xpos--;
    break;
  
  case '\t':        /* Tab */
    v_printf("tab\n");
    do char_out(' ', s); while (xpos % 8 != 0);
    break;

  case 7:           /* Bell */
    /* Bell should be sounded here, but it's more trouble than its */
    /* worth for now, because printf, addch or addstr or out_char  */
    /* would all interfere by possibly interrupting terminal codes */
    /* Ignore this for now, since this is a hack til NCURSES.      */
    break;

  default:          /* Printable character */
    CHAR(SCREEN_ADR(s) + ypos*co + xpos) = ch;
    xpos++;
    set_dirty(s);
  }

  if (xpos == co) {
    xpos = 0;
    ypos++;
  }
  if (ypos == li) {
    ypos--;
    scrollup(0, 0, co - 1, li - 1, 1, newline_att);
  }

  bios_cursor_x_position(s) = xpos;
  bios_cursor_y_position(s) = ypos;
  cursor_col = xpos;
  cursor_row = ypos;
}

/* The following clears the screen buffer. It does it only to the screen 
 * buffer.  If in termcap mode, the screen will be cleared the next time
 * restore_screen() is called.
 */
void
clear_screen(int s, int att)
{
  u_short *schar, blank = ' ' | (att << 8);
  int lx;

  v_printf("VID: cleared screen\n");
  if (s > max_page) return;
  
  for (schar = SCREEN_ADR(s), 
       lx = 0; lx < (co * li); 
       *(schar++) = blank, lx++);

  set_dirty(s);
  bios_cursor_x_position(s) = bios_cursor_y_position(s) = 0;
  cursor_row = cursor_col = 0;
}


/* XXX- shouldn't this reset the current video page to 0 ? 
*/

boolean set_video_mode(int mode) {
  static int gfx_flag = 0;
  
  switch (mode) {
  case 2:
  case 3:
    v_printf("text mode requested from %d to %d, ", video_mode, mode);
    max_page = 7;
    if ( (configuration & MDA_CONF_SCREEN_MODE) == MDA_CONF_SCREEN_MODE )
       video_mode = 7;
    else
       video_mode = mode & 0x7F;
    v_printf("set to %d\n", video_mode);
    gfx_flag = 0;		/* we're in a text mode now */
    gfx_mode = TEXT;
    
    /* mode change clears screen unless bit7 of AL set */
    if (!(mode & 0x80))
       clear_screen(bios_current_screen_page, 7);
       
    bios_video_mode=video_mode;
    co=80; li=25;
    if (Video->setmode) {
       Video->setmode(0,co,li);
    }
    else {
       v_printf("video: no setmode handler!");
       /* return 0; */
    }
  default:
    v_printf("undefined video mode 0x%x\n", mode);
    return 0;
  }
  return 1;
}    

/******************************************************************/

/* the actual int10 handler */

void int10()
{
  /* some code here is copied from Alan Cox ***************/
  int x, y, i, tmp;
  unsigned int page;
  u_char c;
  us *sm;

  if (d.video >= 3)
    {
      if (d.video >= 4)
	dbug_printf("int10 near %04x:%08lx\n", LWORD(cs), REG(eip));
      if ( (LO(ax) >= ' ') && (LO(ax) < 0x7f) )
	dbug_printf("int10 AH=%02x AL=%02x '%c'\n",
		    HI(ax), LO(ax), LO(ax));
      else
	dbug_printf("int10 AH=%02x AL=%02x\n", HI(ax), LO(ax));
    }

  NOCARRY;

  switch (HI(ax)) {
  case 0x0:			/* define mode */
    v_printf("define mode: 0x%x\n", LO(ax));
    if (!set_video_mode(LO(ax))) {
       error("int10,0: set_video_mode failed\n");
       CARRY;
    }
    break;

  case 0x1:			/* define cursor shape */
    v_printf("define cursor: 0x%04x\n", LWORD(ecx));
    set_cursor_shape(LWORD(ecx));
    bios_cursor_shape = LWORD(ecx);
    break;
    
  case 0x2:			/* set cursor pos */
    page = HI(bx);
    x = LO(dx);
    y = HI(dx);
    if (page > 7) {
      error("ERROR: video error (setcur/page>7: %d)\n", page);
      CARRY;
      return;
    }
    if (x >= co || y >= li)
      break;

    bios_cursor_x_position(page) = x;
    bios_cursor_y_position(page) = y;
    cursor_col = x;
    cursor_row = y;
    break;

  case 0x3:			/* get cursor pos/shape */
    page = HI(bx);
    if (page > 7) {
      error("ERROR: video error(0x3 page>7: %d)\n", page);
      CARRY;
      return;
    }
    REG(edx) = (bios_cursor_y_position(page) << 8) 
              | bios_cursor_x_position(page);
    REG(ecx) = bios_cursor_shape;
    break;

  case 0x5:
    {				/* change page */
      page = LO(ax);
      error("VID: change page from %d to %d!\n", bios_current_screen_page, page);
      if (page > max_page) {
	error("ERROR: video error: set bad page %d\n", page);
	CARRY;
	break;
      }
      if (config.console_video) set_vc_screen_page(page);

      bios_current_screen_page = video_page = page;
      bios_video_memory_address = TEXT_SIZE * page;
      screen_adr = SCREEN_ADR(page);
      screen_mask = 1 << (((int)screen_adr-0xA0000)/4096);
      set_dirty(page);
      cursor_col = bios_cursor_x_position(page);
      cursor_row = bios_cursor_y_position(page);
      break;
    }

  case 0x6:			/* scroll up */
    v_printf("scroll up %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    /* kludge for ansi.sys' clear screen - we'd better do real clipping */
    if (HI(dx)>=li) HI(dx)=li-1;  
    scrollup(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    break;

  case 0x7:			/* scroll down */
    v_printf("scroll dn %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    scrolldn(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    break;

  case 0x8:			/* read character at x,y + attr */
    page = HI(bx);
    if (page > max_page) {
      error("ERROR: read char from bad page %d\n", page);
      CARRY;
      break;
    }
    sm = SCREEN_ADR(page);
    REG(eax) = sm[co * bios_cursor_y_position(page) 
		     + bios_cursor_x_position(page)];
    break;

    /* these two put literal character codes into memory, and do
       * not scroll or move the cursor...
       * the difference is that 0xA ignores color for text modes
       */
  case 0x9:
  case 0xA:
    {
      u_short *sadr;
      u_char attr = LO(bx);

      page = HI(bx);
      sadr = (u_short *) SCREEN_ADR(page) 
	     + bios_cursor_y_position(page) * co
	     + bios_cursor_x_position(page);
      x = (us) REG(ecx);
      c = LO(ax);

      /* XXX - need to make sure this doesn't overrun video memory!
       * we don't really know how large video memory is yet (I haven't
       * abstracted things yet) so we'll just leave it for the
       * Great Video Housecleaning foretold in the TODO file of the
       * ancients
       */
      if (HI(ax) == 9)
	for (i = 0; i < x; i++)
	  *(sadr++) = c | (attr << 8);
      else
	for (i = 0; i < x; i++) {	/* leave attribute as it is */
	  *sadr &= 0xff00;
	  *(sadr++) |= c;
	}
      set_dirty(page);
      break;
    }

  case 0xe:			/* print char */
    char_out(*(char *) &REG(eax), bios_current_screen_page); 
    break;

  case 0x0f:			/* get video mode */
    REG(eax) = (co << 8) | video_mode;
    v_printf("get screen mode: 0x%04x s=%d\n", LWORD(eax), bios_current_screen_page);
    HI(bx) = bios_current_screen_page;
    break;

  case 0x10:			/* ega palette */
    /* Sets blinking or bold background mode.  This is important for 
     * PCTools type programs that uses bright background colors.
     */
    if (LO(ax) == 3) {      
      char_blink = LO(bx) & 1;
    }

#if 0
  case 0xb:			/* palette */
  case 0xc:			/* set dot */
  case 0xd:			/* get dot */
    break;
#endif

  case 0x4:			/* get light pen */
    error("ERROR: video error(no light pen)\n");
#if 0
    CARRY;
#else
    HI(ax) = 0;   /* "light pen switch not pressed" */
                  /* This is how my VGA BIOS behaves [rz] */
#endif    
    return;

  case 0x1a:			/* get display combo */
    if (LO(ax) == 0) {
      v_printf("get display combo!\n");
      LO(ax) = 0x1a;		/* valid function=0x1a */
      LO(bx) = video_combo;	/* active display */
      HI(bx) = 0;		/* no inactive display */
    }
    else {
      v_printf("set display combo not supported\n");
    }
    break;

  case 0x12:			/* video subsystem config */
    v_printf("get video subsystem config ax=0x%04x bx=0x%04x\n",
	     LWORD(eax), LWORD(ebx));
    switch (LO(bx)) {
    case 0x10:
      HI(bx) = video_subsys;
      /* this breaks qedit! (any but 0x10) */
      /* LO(bx)=3;  */
      v_printf("video subsystem 0x10 BX=0x%04x\n", LWORD(ebx));
      REG(ecx) = 0x0809;
      break;
    case 0x20:
      v_printf("select alternate printscreen\n");
      break;
    case 0x30:
      if (LO(ax) == 0)
	tmp = 200;
      else if (LO(ax) == 1)
	tmp = 350;
      else
	tmp = 400;
      v_printf("select textmode scanlines: %d", tmp);
      LO(ax) = 12;		/* ok */
      break;
    case 0x32:			/* enable/disable cpu access to cpu */
      if (LO(ax) == 0)
	v_printf("disable cpu access to video!\n");
      else
	v_printf("enable cpu access to video!\n");
      break;
    default:
      error("ERROR: unrecognized video subsys config!!\n");
      show_regs();
    }
    break;

  case 0xfe:			/* get shadow buffer..return unchanged */
  case 0xff:			/* update shadow buffer...do nothing */
    break;

  case 0x11:			/* ega character generator */
  case 0x4f:			/* vesa interrupt */

  default:
    v_printf("new unknown video int 0x%x\n", LWORD(eax));
    CARRY;
    break;
  }
}

