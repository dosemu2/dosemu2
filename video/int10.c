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
Scroll(us *sadr, int x0, int y0, int x1, int y1, int l, int att)
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

  v_printf("VID: cleared screen\n");
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


/* XXX- shouldn't this reset the current video page to 0 ? 
*/

boolean set_video_mode(int mode) {
  static int gfx_flag = 0;
  int type=0;
  
  switch (mode&0x7f) {
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
    if (!Video->setmode) goto Default;
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
    gfx_flag = 0;		/* we're in a text mode now */
    gfx_mode = TEXT;
    clear_scroll_queue();

    li=text_scanlines/vga_font_height;
    if (li>MAX_LINES) li=MAX_LINES;
    WRITE_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1, li-1);
    WRITE_WORD(BIOS_FONT_HEIGHT, vga_font_height);
    WRITE_BYTE(BIOS_VIDEO_MODE, video_mode);
    if (Video->setmode) {
      Video->setmode(type,co,li);
    }
    else {
       v_printf("video: no setmode handler!");
       /* return 0; */
    }
    /* mode change clears screen unless bit7 of AL set */
    if (!(mode & 0x80))
       clear_screen(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), 7);
       
    WRITE_BYTE(BIOS_VIDEO_MODE, video_mode=mode&0x7f);
    break;

  Default:
  default:
    /* handle graphics modes here */
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
       error("int10,0: set_video_mode failed\n");
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
    if (page > 7) {
      error("ERROR: video error (setcur/page>7: %d)\n", page);
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
      error("ERROR: video error(0x3 page>7: %d)\n", page);
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
      error("VID: change page from %d to %d!\n", READ_BYTE(BIOS_CURRENT_SCREEN_PAGE), page);
      if (page > max_page) {
	error("ERROR: video error: set bad page %d\n", page);
	CARRY;
	break;
      }
      if (config.console_video) set_vc_screen_page(page);

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
    v_printf("scroll up %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    /* kludge for ansi.sys' clear screen - we'd better do real clipping */
    if (HI(dx)>=li) HI(dx)=li-1;  
    bios_scroll(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));

/*  scrollup(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
*/
    break;

  case 0x7:			/* scroll down */
    v_printf("scroll dn %d %d %d %d, %d\n", LO(cx), HI(cx), LO(dx), HI(dx), LO(ax));
    bios_scroll(LO(cx), HI(cx), LO(dx), HI(dx), -LO(ax), HI(bx));
/*    scrolldn(LO(cx), HI(cx), LO(dx), HI(dx), LO(ax), HI(bx));
*/
    break;

  case 0x8:			/* read character at x,y + attr */
    page = HI(bx);
    if (page > max_page) {
      error("ERROR: read char from bad page %d\n", page);
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
	while(n--)
	  /* *(sadr++) = *src++ | attr; */
	  WRITE_WORD(sadr++, READ_BYTE(src++) | attr);
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
    char_out(*(char *) &REG(eax), READ_BYTE(BIOS_CURRENT_SCREEN_PAGE)); 
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
    break;

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
    v_printf("video character generator functions ax=0x%04x bx=0x%04x\n",
	     LWORD(eax), LWORD(ebx));
    switch (LO(ax)) {
    case 0x01:                  /* load 8x14 charset */
    case 0x11:
      vga_font_height = 14;
      set_video_mode(0x83);
      break;
    case 0x02:
    case 0x12:
      vga_font_height = 8;
      set_video_mode(0x83);
      break;
    case 0x14:
    case 0x04:
      vga_font_height = 16;
      set_video_mode(0x83);
      break;
    case 0x30:     /* get current character generator info */
      LWORD(ecx)=vga_font_height;
      LO(dx)= READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1);
      /* LWORD(es)=LWORD(ebp)=0;*/  /* return NULL pointer */
      WRITE_SEG_REG(es, 0);   /* return NULL pointer */
      LWORD(ebp)= 0;        /* return NULL pointer */
    }
    break;
      
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
    default:
      error("ERROR: unrecognized video subsys config!!\n");
      show_regs(__FILE__, __LINE__);
    }
    break;
    
  case 0xfe:			/* get shadow buffer..return unchanged */
  case 0xff:			/* update shadow buffer...do nothing */
    break;

  case 0x4f:			/* vesa interrupt */

  default:
    v_printf("new unknown video int 0x%x\n", LWORD(eax));
    CARRY;
    break;
  }
}

