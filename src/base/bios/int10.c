/* 
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Video BIOS implementation.
 *
 * This module handles the int10 video functions.
 * Most functions here change only the video memory and status 
 * variables; the actual screen is then rendered asynchronously 
 * after these by Video->update_screen.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 5/24/95, Erik Mouw (J.A.K.Mouw@et.tudelft.nl) and 
 * Arjan Filius (I.A.Filius@et.tudelft.nl)
 * changed int10() to make graphics work with X.
 *
 * 1998/04/05: Put some work into set_video_mode() (made it
 * more VGA comaptible) and removed new_set_video_mode().
 * Removed (useless) global variable "gfx_mode".
 * -- sw (Steffen.Winterfeldt@itp.uni-leipzig.de)
 * 
 * Readded new_set_video_mode, its needed for non-X compiles.
 * -- EB 3 June 1998
 *
 * Renamed new_set_video_mode to X_set_video_mode to avoid confusion
 * in the future
 * -- Hans 980614
 *
 * DANG_END_CHANGELOG
 */


#include <string.h>
#include "config.h"

#include "emu.h"
#include "video.h"
#include "memory.h"
#include "bios.h"
#include "vc.h"

#if X_GRAPHICS
#include "X.h"
#include "vgaemu.h"
#endif

/* a bit brutal, but enough for now */
#if VIDEO_CHECK_DIRTY
#define set_dirty(page) vm86s.screen_bitmap = 0xffffffff
#else
#define set_dirty(page)
#endif

#if USE_DUALMON
  #if USE_SCROLL_QUEUE
    #error "dualmon: You can't have defined USE_SCROLL_QUEUE together with USE_DUALMON"
  #endif
  #define BIOS_CONFIG_SCREEN_MODE (READ_WORD(BIOS_CONFIGURATION) & 0x30)
  #define IS_SCREENMODE_MDA (BIOS_CONFIG_SCREEN_MODE == 0x30)
  /* This is the text screen base, the DOS program actually has to use.
   * Programs that support simultaneous dual monitor support rely on
   * the fact, that the BIOS takes B0000 for EQUIP-flags 4..5 = 3
   * else B8000 as regenbuffer address. Each compatible PC-BIOS behaves so.
   * This is ugly, but there is no screen buffer address in the BIOS-DATA
   * at 0x400. (Hans)
   */
  #define BIOS_SCREEN_BASE (IS_SCREENMODE_MDA ? MDA_PHYS_TEXT_BASE : VGA_PHYS_TEXT_BASE)
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
   if (ce>3 && ce<12 && (config.cardtype != CARD_MDA)) {
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
Scroll(us *sadr, int x0, int y0, int x1, int y1, int l, int att)
{
  int dx = x1 - x0 + 1;
  int dy = y1 - y0 + 1;
  int x, y;
  us blank = ' ' | (att << 8);
  us tbuf[MAX_COLUMNS];

  if ( (config.cardtype == CARD_MDA) && ((att & 7) != 0) && ((att & 7) != 7) )
    {
      blank = ' ' | ((att | 7) << 8);
    }


  if (x1 >= co || y1 >= li)
    {
      v_printf("VID: Scroll parameters out of bounds, in Scroll!\n");
      v_printf("VID: Attempting to fix with clipping!\n");
    /* kludge for ansi.sys' clear screen - we'd better do real clipping */
    /* Also a cludge to fix list, but in the other dimension */
      if (x1 >= co) x1 = co -1;
      if (y1 >= li) y1 = li -1;
      dx = x1 - x0 +1;
      dy = y1 - x0 +1;
    }
  if (dx <= 0 || dy <= 0 || x0 < 0 || x1 >= co || y0 < 0 || y1 >= li)
    {
      v_printf("VID: Scroll parameters impossibly out of bounds, giving up!\n");
    return;
    }

  /* make a blank line */
  for (x = 0; x < dx; x++)
    tbuf[x] = blank;

  if (l >= dy || l <= -dy)
    l = 0;

  set_dirty(video_page);
  
  if (l == 0) {			/* Wipe mode */
    for (y = y0; y <= y1; y++)
      /*memcpy(&sadr[y * co + x0], tbuf, dx * sizeof(us));*/
      MEMCPY_2DOS(&sadr[y * co + x0], tbuf, dx * sizeof(us));
    return;
  }

  if (l > 0) {
    if (dx == co)
      /*memcpy(&sadr[y0 * co], &sadr[(y0 + l) * co], (dy - l) * dx * sizeof(us));*/
      MEMCPY_DOS2DOS(&sadr[y0 * co], &sadr[(y0 + l) * co], (dy - l) * dx * sizeof(us));
    else
      for (y = y0; y <= (y1 - l); y++)
	/*memcpy(&sadr[y * co + x0], &sadr[(y + l) * co + x0], dx * sizeof(us));*/
	MEMCPY_DOS2DOS(&sadr[y * co + x0], &sadr[(y + l) * co + x0], dx * sizeof(us));

    for (y = y1 - l + 1; y <= y1; y++)
      /*memcpy(&sadr[y * co + x0], tbuf, dx * sizeof(us));*/
      MEMCPY_2DOS(&sadr[y * co + x0], tbuf, dx * sizeof(us));
  }
  else {
    for (y = y1; y >= (y0 - l); y--)
      /*memcpy(&sadr[y * co + x0], &sadr[(y + l) * co + x0], dx * sizeof(us));*/
      MEMCPY_DOS2DOS(&sadr[y * co + x0], &sadr[(y + l) * co + x0], dx * sizeof(us));

    for (y = y0 - l - 1; y >= y0; y--)
      /*memcpy(&sadr[y * co + x0], tbuf, dx * sizeof(us));*/
      MEMCPY_2DOS(&sadr[y * co + x0], tbuf, dx * sizeof(us));
  }
}

/**************************************************************/
/* scroll queue */

#if USE_SCROLL_QUEUE

int sq_head=0, sq_tail=0;

#define SQ_INC(i) ((i+1)%(SQ_MAXLENGTH+1))

struct scroll_entry *get_scroll_queue() {
   if (sq_head==sq_tail) return NULL;
   sq_tail=SQ_INC(sq_tail);
   return &scroll_queue[sq_tail];
}

void clear_scroll_queue() {
   while(get_scroll_queue());
}

volatile int video_update_lock = 0;

void bios_scroll(int x0,int y0,int x1,int y1,int n,byte attr) {
   struct scroll_entry *s;
   int sqh2;

   VIDEO_UPDATE_LOCK();
   sqh2=SQ_INC(sq_head);
   if (n!=0 && !Video->is_mapped && sqh2!=sq_tail) {
      s=&scroll_queue[sq_head];
      if (sq_head!=sq_tail && 
	 s->x0==x0 && s->y0==y0 && 
	 s->x1==x1 && s->y1==y1 &&
	 s->attr==attr)
      {
         s->n+=n;
      }
      else {
	 s=&scroll_queue[sqh2];
         s->x0=x0; s->y0=y0;
         s->x1=x1; s->y1=y1;
         s->n=n;   s->attr=attr;
         sq_head=sqh2;
      }
   }
   Scroll(screen_adr,x0,y0,x1,y1,n,attr);
   VIDEO_UPDATE_UNLOCK();
}
#else
  #if USE_DUALMON
    #define bios_scroll(x0,y0,x1,y1,n,attr) ({\
     if (IS_SCREENMODE_MDA) Scroll((void *)MDA_PHYS_TEXT_BASE,x0,y0,x1,y1,n,attr); \
     else Scroll(screen_adr,x0,y0,x1,y1,n,attr);\
    })
  #else
    #define bios_scroll(x0,y0,x1,y1,n,attr) Scroll(screen_adr,x0,y0,x1,y1,n,attr)
  #endif
#endif

/* Output a character to the screen. */ 
void
char_out(unsigned char ch, int s)
{
  int newline_att = 7;
  int xpos, ypos;
#if USE_DUALMON
  int virt_text_base = BIOS_SCREEN_BASE;
#endif
  xpos = get_bios_cursor_x_position(s);
  ypos = get_bios_cursor_y_position(s);

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
    do {
	char_out(' ', s); 
  	xpos = get_bios_cursor_x_position(s);
    } while (xpos % 8 != 0);
    break;

  case 7:           /* Bell */
#if 0
    /* Bell should be sounded here, but it's more trouble than its */
    /* worth for now, because printf, addch or addstr or out_char  */
    /* would all interfere by possibly interrupting terminal codes */
    /* Ignore this for now, since this is a hack til NCURSES.      */
#else
    putchar(0x07);
#endif
    break;

  default:          /* Printable character */
    WRITE_BYTE(SCREEN_ADR(s) + ypos*co + xpos, ch);
    xpos++;
    set_dirty(s);
  }

  if (xpos == co) {
    xpos = 0;
    ypos++;
  }
  if (ypos == li) {
    ypos--;
    bios_scroll(0,0,co-1,li-1,1,newline_att);
  }
  set_bios_cursor_x_position(s, xpos);
  set_bios_cursor_y_position(s, ypos);
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
#if USE_DUALMON
  int virt_text_base = BIOS_SCREEN_BASE;
#endif

  v_printf("VID: cleared screen: %d %d %p\n", s, att, SCREEN_ADR(s));
  if (s > max_page) return;
  
  for (schar = SCREEN_ADR(s), 
       lx = 0; lx < (co * li); 
/*       *(schar++) = blank, lx++); */
       WRITE_WORD(schar++, blank), lx++);

  set_dirty(s);
  set_bios_cursor_x_position(s, 0);
  set_bios_cursor_y_position(s, 0);
  cursor_row = cursor_col = 0;
  clear_scroll_queue();
}


#if X_GRAPHICS

/*
 * set_video_mode() accepts both (S)VGA and VESA mode numbers
 * It should be fully compatible with the old set_video_mode()
 * function.
 * Note: bit 16 in "mode" is used internally to indicate that
 * nothing should be done except adjusting the font size.
 * -- 1998/04/04 sw
 */

static boolean X_set_video_mode(int mode) {
  vga_mode_info *vmi;
  int clear_mem = 1;
  int adjust_font_size = 0;

  if(mode & (1 << 16)) {
    adjust_font_size = 1;
    mode &= ~(1 << 16);
  }

  mode &= 0xffff;

  if(mode != video_mode) adjust_font_size = 0;

  v_printf("set_video_mode: mode = 0x%02x%s\n", mode, adjust_font_size ? " (adjust font size)" : "");

  if((vmi = vga_emu_find_mode(mode, NULL)) == NULL) {
    v_printf("set_video_mode: undefined video mode\n");
    return 0;
  }

  if(Video->setmode == NULL) {
    v_printf("set_video_mode: no setmode handler!\n");
    return 0;
  }

  if(adjust_font_size) {
    text_scanlines = vmi->height;
    li = text_scanlines / vga_font_height;
    if(li > MAX_LINES) li = MAX_LINES;
    WRITE_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1, li - 1);
    WRITE_WORD(BIOS_FONT_HEIGHT, vga_font_height);
    Video->setmode(vmi->mode_class, co, li);
    return 1;
  }

  video_mode = mode;

  if(mode >= 0x80 && mode < 0x100) {
    mode &= 0x7f;
    clear_mem = 0;
  }
  if(mode & 0x8000) {
    mode &= ~0x8000;
    clear_mem = 0;
  }

  if(config.cardtype == CARD_MDA) video_mode = 7;

  /*
   * We store the SVGA mode number (if possible) even when setting a VESA mode. -- sw
   */
  WRITE_BYTE(BIOS_VIDEO_MODE, vmi->mode & 0x7f);
  WRITE_BYTE(BIOS_CURRENT_SCREEN_PAGE, 0);

  if(vmi->mode_class == TEXT) {
    clear_scroll_queue();
  }

  li = vmi->text_height;
  co = vmi->text_width;
  if(li > MAX_LINES) li = MAX_LINES;
  vga_font_height = vmi->char_height;
  text_scanlines = vmi->height;

  WRITE_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1, li - 1);
  WRITE_WORD(BIOS_SCREEN_COLUMNS, co);
  WRITE_WORD(BIOS_FONT_HEIGHT, vga_font_height);

#if USE_DUALMON
  /*
   * The following code (& comment) is taken literally
   * from the old set_video_mode(). Don't know what it's
   * for (or if it works). -- 1998/04/04 sw 
   */

  /*
   * This to be sure in case of older DOS programs probing HGC.
   * There was no secure way to detect a HGC before VGA was invented.
   * ( Now we can do INT 10, AX=1A00 ).
   * Some older DOS programs do it by modifying EQUIP-flags
   * and then let the BIOS say, if it can ?!?!)
   * If we have config.dualmon, this happens legaly.
   */
  if(config.dualmon)
    Video->setmode(7, co, li);
  else
#endif

  /* setmode needs video_mode to _still have_ the memory-clear bit -- sw */
  Video->setmode(vmi->mode_class, co, li);

  /*
   * video_mode is expected to be the mode number _without_ the
   * memory-clear bit
   * -- sw
   */
  video_mode = mode;

  if(clear_mem && vmi->mode_class == TEXT) clear_screen(0, 7);

  WRITE_BYTE(BIOS_VIDEO_INFO_0, clear_mem ? 0x60 : 0xe0);
  memset((void *) 0x450, 0, 0x10);	/* equiv. to set_bios_cursor_(x/y)_position(0..7, 0) */

  video_page = 0;
  WRITE_WORD(BIOS_VIDEO_MEMORY_ADDRESS, 0);
  screen_adr = SCREEN_ADR(0);
  screen_mask = 0;
  cursor_col = get_bios_cursor_x_position(0);
  cursor_row = get_bios_cursor_y_position(0);

  /*
   * There are still some BIOS variables that need to be updated.
   * (cursor shape, CRTC address...)
   * -- sw
   */

  return 1;
}    
#endif /* X_GRAPHICS */


/* XXX- shouldn't this reset the current video page to 0 ? 
*/

boolean set_video_mode(int mode) {
  int type=0;
  int old_video_mode,oldco;

  v_printf("set_video_mode: mode = 0x%02x\n",mode);
  
#ifdef X_GRAPHICS
  if (Video == &Video_X) return X_set_video_mode(mode);
#endif  
  oldco = co;
  old_video_mode = video_mode;
  video_mode=mode&0x7f;

  if (Video->setmode == 0)
    { 
      v_printf("video: no setmode handler!\n");
      goto error;
    }


  switch (mode&0x7f) {
  case 0x50:
  case 0x51:
  case 0x52:
    co=80;
    goto do_text_mode;
  case 0x53:
  case 0x54:
  case 0x55:
  case 0x56:
  case 0x57:
  case 0x58:
  case 0x59:
  case 0x5a:
    co=132;
    goto do_text_mode;
  
  case 0:
  case 1:
    /* 40 column modes... */
    co=40;
    goto do_text_mode;
    
  case 7:
    /*
     * This to be sure in case of older DOS programs probing HGC.
     * There was no secure way to detect a HGC before VGA was invented.
     * ( Now we can do INT 10, AX=1A00 ).
     * Some older DOS programs do it by modifying EQUIP-flags
     * and then let the BIOS say, if it can ?!?!)
     * If we have config.dualmon, this happens legaly.
     */
#if USE_DUALMON
    if (config.dualmon) {
      type=7;
      text_scanlines = 400;
      vga_font_height=text_scanlines/25;
    }
#endif
    /* fall through */
     
  case 2:
  case 3:
    /* set 80 column text mode */
#if USE_DUALMON
    WRITE_WORD(BIOS_SCREEN_COLUMNS, co = CO); /*80*/
#else
    co=80;
#endif
    
do_text_mode:
    clear_scroll_queue();

    li=text_scanlines/vga_font_height;
    if (li>MAX_LINES) li=MAX_LINES;
    WRITE_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1, li-1);
    WRITE_WORD(BIOS_FONT_HEIGHT, vga_font_height);
    WRITE_BYTE(BIOS_VIDEO_MODE, video_mode);
    Video->setmode(type,co,li);
    /* mode change clears screen unless bit7 of AL set */
    if (!(mode & 0x80))
       clear_screen(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), 7);

    if ( config.cardtype == CARD_MDA )
      mode = 7;
       
    break;


case 0x13:	/*Not finished ! */
case 0x5c:
case 0x5d:
case 0x5e:
case 0x62:
  /* 0x01 == GRAPH for us, but now it's sure! */
  if (Video->setmode != NULL)
  Video->setmode(0x01,0,0);
  break;

  default:
    /* handle graphics modes here */
    v_printf("undefined video mode 0x%x\n", mode);
  goto error;
  }

  WRITE_BYTE(BIOS_VIDEO_MODE, video_mode=mode&0x7f);

  if (oldco != co)
    WRITE_WORD(BIOS_SCREEN_COLUMNS, co);
  return 1;

error:
  /* don't change any state on failure */
  video_mode = old_video_mode;
  return 0;
}    


/******************************************************************/

/* the actual int10 handler */

void int10()
{
  /* some code here is copied from Alan Cox ***************/
  int x, y;
  unsigned int page;
  u_char c;
  us *sm;

#if USE_DUALMON
  int virt_text_base = BIOS_SCREEN_BASE;
  static int last_equip=-1;

  if (config.dualmon && (last_equip != BIOS_CONFIG_SCREEN_MODE)) {
    extern struct video_system *Video_default;
    v_printf("VID: int10 entry, equip-flags=0x%04x\n",READ_WORD(BIOS_CONFIGURATION));
    last_equip = BIOS_CONFIG_SCREEN_MODE;
    if (IS_SCREENMODE_MDA) Video->is_mapped = 1;
    else Video->is_mapped = Video_default->is_mapped;
    li= READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1) + 1;
    co= READ_WORD(BIOS_SCREEN_COLUMNS);
  }
#endif

  if (d.video >= 3)
    {
      if (d.video >= 4)
/*	dbug_printf("int10 near %04x:%08lx\n", LWORD(cs), REG(eip));*/
	dbug_printf("int10 near %04x:%08lx\n", READ_SEG_REG(cs), REG(eip));
      if ( (LO(ax) >= ' ') && (LO(ax) < 0x7f) )
	dbug_printf("int10 AH=%02x AL=%02x '%c'\n",
		    HI(ax), LO(ax), LO(ax));
      else
	dbug_printf("int10 AH=%02x AL=%02x\n", HI(ax), LO(ax));
    }

#if 0
  v_printf("VID: int10, ax=%04x bx=%04x\n",LWORD(eax),LWORD(ebx));
#endif
  NOCARRY;

  switch (HI(ax)) {
  case 0x0:			/* define mode */
    v_printf("define mode: 0x%x\n", LO(ax));
    /* set appropriate font height for 25 lines */
    vga_font_height=text_scanlines/25;
    if (!set_video_mode(LO(ax))) {
       v_printf("int10,0: set_video_mode failed\n");
       CARRY;
    }
    break;

  case 0x1:			/* define cursor shape */
    v_printf("define cursor: 0x%04x\n", LWORD(ecx));
    set_cursor_shape(LWORD(ecx));
    WRITE_WORD(BIOS_CURSOR_SHAPE, LWORD(ecx));
    break;
    
  case 0x2:			/* set cursor pos */
    page = HI(bx);
    x = LO(dx);
    y = HI(dx);
    v_printf("set cursor: pg:%d x:%d y:%d\n", page, x, y);
    if (page > 7) {
      v_printf("ERROR: video error (setcur/page>7: %d)\n", page);
      CARRY;
      return;
    }
    if (x >= co || y >= li)
      break;

    set_bios_cursor_x_position(page, x);
    set_bios_cursor_y_position(page, y);
    cursor_col = x;
    cursor_row = y;
    break;

  case 0x3:			/* get cursor pos/shape */
    page = HI(bx);
    if (page > 7) {
      v_printf("ERROR: video error(0x3 page>7: %d)\n", page);
      CARRY;
      return;
    }
    REG(edx) = (get_bios_cursor_y_position(page) << 8) 
              | get_bios_cursor_x_position(page);
    REG(ecx) = READ_WORD(BIOS_CURSOR_SHAPE);
    break;

  case 0x5:
    {				/* change page */
#if USE_DUALMON
      if (config.dualmon && IS_SCREENMODE_MDA) break;
#endif
      page = LO(ax);
      v_printf("VID: change page from %d to %d!\n", READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), page);
      if (page > max_page) {
	v_printf("ERROR: video error: set bad page %d\n", page);
	CARRY;
	break;
      }
      if (config.console_video) set_vc_screen_page(page);
#if X_GRAPHICS
      if (config.X) vga_emu_set_text_page(page, TEXT_SIZE);
#endif

      WRITE_BYTE(BIOS_CURRENT_SCREEN_PAGE, video_page = page);
      WRITE_WORD(BIOS_VIDEO_MEMORY_ADDRESS, TEXT_SIZE * page);
      screen_adr = SCREEN_ADR(page);
      screen_mask = 1 << page;
      set_dirty(page);
      cursor_col = get_bios_cursor_x_position(page);
      cursor_row = get_bios_cursor_y_position(page);
      break;
    }

  case 0x6:			/* scroll up */
    v_printf("scroll up %d %d, %d %d, %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    bios_scroll(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
    break;

  case 0x7:			/* scroll down */
    v_printf("scroll dn %d %d, %d %d, %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax),HI(bx));
    bios_scroll(LO(cx), HI(cx), LO(dx), HI(dx), -LO(ax), HI(bx));
    break;

  case 0x8:			/* read character at x,y + attr */
    page = HI(bx);
    if (page > max_page) {
      v_printf("ERROR: read char from bad page %d\n", page);
      CARRY;
      break;
    }
    sm = SCREEN_ADR(page);
    REG(eax) = sm[co * get_bios_cursor_y_position(page) 
		     + get_bios_cursor_x_position(page)];
    break;

    /* these two put literal character codes into memory, and do
       * not scroll or move the cursor...
       * the difference is that 0xA ignores color for text modes
       */
  case 0x9:
  case 0xA:
    {
      u_short *sadr;
      u_short c_attr;
      int n;

      page = HI(bx);
      sadr = (u_short *) SCREEN_ADR(page) 
	     + get_bios_cursor_y_position(page) * co
	     + get_bios_cursor_x_position(page);
      n = LWORD(ecx);
      c = LO(ax);

      /* XXX - need to make sure this doesn't overrun video memory!
       * we don't really know how large video memory is yet (I haven't
       * abstracted things yet) so we'll just leave it for the
       * Great Video Housecleaning foretold in the TODO file of the
       * ancients
       */
      if (HI(ax) == 9) {                /* use attribute from BL */
	 c_attr = c | LO(bx)<<8;
	 while(n--)
	    /* *(sadr++) = c_attr; */
	    WRITE_WORD(sadr++, c_attr);
      }
      else {                            /* leave attribute as it is */
	 while(n--)
	    /* *(char*)(sadr++) = c; */
	    WRITE_BYTE(sadr++, c);
      }
      set_dirty(page);
      break;
    }

  case 0x13:                    /* write string */
    {
      u_short *sadr;
      u_short attr;
      char *src;
      int n,x,y;

      page = HI(bx);
      x = LO(dx);
      y = HI(dx);
      sadr = (u_short *) SCREEN_ADR(page) + y*co + x;
      n = LWORD(ecx);
      src = SEG_ADR((char*),es,bp);
      
      if (LO(ax) < 2) {                  /* use attribute in BL */
        attr=LO(bx)<<8;
#if 1					/* 1998/03/13 sengoku@intsys.co.jp */
        x += n;
	while(n--) {
	  /* *(sadr++) = *src++ | attr; */
          if (*src <= 0x1a) {
            switch(*src) {
              case 0x0d :
                sadr = (u_short *) SCREEN_ADR(page) + y*co, x = n;
                src++;
                break;
              case 0x0a :
                sadr += co, x--, y++;
                src++;
                break;
              case 0x07 : /* beep */
#if 0  /* ignored, we have to check wether it breaks the speaker stuff --Hans */
                putchar(0x07);
#else
                /* this should do what we want, says Eric 980425 */
                speaker_on(125, 0x637);
#endif
                x--;
                src++;
                break;
              case 0x08 : /* backspace */
                sadr--, x--;
                src++;
                break;
              default:
                WRITE_WORD(sadr++, READ_BYTE(src++) | attr);
                break;
            }
          } else {
            WRITE_WORD(sadr++, READ_BYTE(src++) | attr);
          }
        }
#else
	while(n--)
	  /* *(sadr++) = *src++ | attr; */
            WRITE_WORD(sadr++, READ_BYTE(src++) | attr);
#endif
      }
      else {                             /* use attributes in buffer */
	/*memcpy(sadr,src,n*2);*/
	MEMCPY_DOS2DOS(sadr,src,n*2);
      }

      if (LO(ax)&1) {                    /* update cursor position */
         n = x+co*y;
	 set_bios_cursor_x_position(page, n%co);
	 set_bios_cursor_y_position(page, n/co);
      }
      set_dirty(page);
    }
    break;

  case 0xe:			/* print char */
    char_out(LO(ax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)); 
    break;

  case 0x0f:			/* get video mode */
#if USE_DUALMON
    if (IS_SCREENMODE_MDA) LWORD(eax) = (co << 8) | READ_BYTE(BIOS_VIDEO_MODE);
    else 
#endif
    LWORD(eax) = (co << 8) | video_mode;
    v_printf("get screen mode: 0x%04x s=%d\n", LWORD(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));
    HI(bx) = READ_BYTE(BIOS_CURRENT_SCREEN_PAGE);
    break;

  case 0x10:			/* ega palette */
    /* Sets blinking or bold background mode.  This is important for 
     * PCTools type programs that uses bright background colors.
     */
    if (LO(ax) == 3) {      
      char_blink = LO(bx) & 1;
    }
#if X_GRAPHICS
    /* root@zaphod */
    /* Palette register stuff. Only for the VGA emulator used by X */
    if(config.X)
      {
        int i, count;
        unsigned char* src;
        unsigned char r, g, b, index;
	DAC_entry rgb;

        switch(LO(ax))
          {
          case 0x10:
            DAC_set_entry((unsigned char)HI(dx), (unsigned char)HI(cx),
                          (unsigned char)LO(cx), (unsigned char)LO(bx));
            break;

          case 0x12:
            index=(unsigned char)LO(bx);
            count=LWORD(ecx);
            src=SEG_ADR((unsigned char*),es,dx);
            for(i=0; i<count; i++, index++)
              {
                r=src[i*3];
                g=src[i*3+1];
                b=src[i*3+2];
                DAC_set_entry(r, g, b, index);
              }
            break;

          case 0x15:  /* Read Individual DAC Register */
            DAC_read_entry(&rgb, (unsigned char)LO(bx));
            (unsigned char)HI(dx) = rgb.r;
            (unsigned char)HI(cx) = rgb.g;
            (unsigned char)LO(cx) = rgb.b;
            break;

          case 0x17:  /* Read Block of DAC Registers */
            index=(unsigned char)LO(bx);
            count=LWORD(ecx);
            src=SEG_ADR((unsigned char*),es,dx);
            for(i=0; i<count; i++, index++)
              {
                DAC_read_entry(&rgb, index); 
                src[i*3]=rgb.r;
                src[i*3+1]=rgb.g;
                src[i*3+2]=rgb.b;
              }
            break;

          default:
            break;
          }
      }
#endif
    break;

#if 0
  case 0xb:			/* palette */
  case 0xc:			/* set dot */
  case 0xd:			/* get dot */
    break;
#endif

  case 0x4:			/* get light pen */
    v_printf("ERROR: video error(no light pen)\n");
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
#if USE_DUALMON
      if (config.dualmon) {
        if (IS_SCREENMODE_MDA) {
          LO(bx) = MDA_VIDEO_COMBO;  /* active display */
          HI(bx) = video_combo;
        }
        else {
          LO(bx) = video_combo;     /* active display */
          HI(bx) = MDA_VIDEO_COMBO;
        }
        break;
      }
#endif
      LO(bx) = video_combo;	/* active display */
      HI(bx) = 0;		/* no inactive display */
    }
    else {
      v_printf("set display combo not supported\n");
    }
    break;

  case 0x11:                    /* character generator functions */
    {
      int old_li = li;           /* preserve the state in case of error */
      int old_font_height = vga_font_height;

      v_printf("video character generator functions ax=0x%04x bx=0x%04x\n",
	       LWORD(eax), LWORD(ebx));
      switch (LO(ax)) {
      case 0x01:                  /* load 8x14 charset */
      case 0x11:
	vga_font_height = 14;
	goto more_lines;
	
      case 0x02:
      case 0x12:
	vga_font_height = 8;
	goto more_lines;
	
      case 0x04:
      case 0x14:
	vga_font_height = 16;
	goto more_lines;
	
	/* load a custom font */
	/* for now just ust it's size to set things */
      case 0x00:
      case 0x10:
	vga_font_height = HI(bx);
	v_printf("loaded font completely ignored except size!\n");
	/* the rest is ignored for now */
	goto more_lines;
	
      more_lines:
	{
	  if(!set_video_mode(video_mode | (1 << 16))) 
	    {
	      li = old_li; /* not that it changed */
	      vga_font_height = old_font_height;
	      CARRY;
	    }
	  else if (old_li < li) 
	    {
	      /* The attribute is just like my Bios, weird! */
	      bios_scroll(0,old_li,co-1,li-1,0,07);
	    }
	  break;
	}
	
      case 0x30:     /* get current character generator info */
	LWORD(ecx)=vga_font_height;
	LO(dx)= READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1);
	/* LWORD(es)=LWORD(ebp)=0;*/  /* return NULL pointer */
	WRITE_SEG_REG(es, 0);   /* return NULL pointer */
	LWORD(ebp)= 0;        /* return NULL pointer */
      }
      break;
    }
      
  case 0x12:			/* video subsystem config */
    v_printf("video subsystem config ax=0x%04x bx=0x%04x\n",
	     LWORD(eax), LWORD(ebx));
    switch (LO(bx)) {
    case 0x10:
      HI(bx) = video_subsys;
      LO(bx)=3;
      v_printf("video subsystem 0x10 BX=0x%04x\n", LWORD(ebx));
      HI(cx)=0xf;  /* feature bits (no feature controller) */
      LO(cx)=READ_BYTE(BIOS_VIDEO_INFO_1) & 0xf;
      break;
    case 0x20:
      v_printf("select alternate printscreen\n");
      break;
    case 0x30:
      {  static int scanlines[3] = {200, 350, 400};
	 if ((unsigned)LO(ax)<3) {
	    text_scanlines = scanlines[LO(ax)];
            v_printf("select textmode scanlines: %d", text_scanlines);
	    LO(ax) = 0x12;  
	 }
      }
      break;
    case 0x32:          /* enable/disable cpu access to video ram */
      if (LO(ax) == 0)
	v_printf("disable cpu access to video!\n");
      else
	v_printf("enable cpu access to video!\n");
      break;
#if 0
    case 0x34:
      /* enable/disable cursor emulation */
      cursor_emulation = LO(ax);
      LO(ax)=0x12;
      break;
#endif
    case 0x36:          /* video screen ON/OFF */
      if (LO(ax) == 0)
	v_printf("turn video screen off!\n");
      else
	v_printf("turn video screen on!\n");
#if 0
      LO(ax)=0x12;  
#endif
      break;
      
    default:
      v_printf("ERROR: unrecognized video subsys config!!\n");
      show_regs(__FILE__, __LINE__);
    }
    break;
    
  case 0xcc:			/* called from NC 5.0 */
    _CX = 0; _AL = 0xff;
    break;

  case 0xfe:			/* get shadow buffer..return unchanged */
  case 0xff:			/* update shadow buffer...do nothing */
    break;

#if 0
  case 0x1b:                    /* return state */
  case 0x1c:                    /* return save/restore */
#endif

#if X_GRAPHICS
  case 0x4f:                    /* vesa interrupt */
    if(config.X)
      do_vesa_int();
    break;
#endif

  default:
    v_printf("new unknown video int 0x%x\n", LWORD(eax));
    CARRY;
    break;
  }
}

