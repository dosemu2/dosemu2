/* 
 * DANG_BEGIN_MODULE
 *
 * This module contains a text-mode only video interface for the X Window 
 * System. It has either mouse or selection 'cut' support, but not yet
 * both.
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * $Id: X.c,v 1.13 1995/05/06 16:25:24 root Exp root $
 *
 * 1995-08-30: Merged with code for pasting, removed some restrictions 
 * from Pasi's code: cut and paste and x_mouse, 
 * (bon@elektron.ikp.physik.th-darmstadt.de)
 *
 * 1995-08-22: Lots of cleaning and rewriting, more comments, MappingNotify 
 * event support, selection support (cut only), fixed some cursor redrawing 
 * bugs. -- Pasi Eronen (peronen@vipunen.hut.fi)
 *
 *
 * DANG_END_CHANGELOG
 */

#define CONFIG_X_SELECTION 1
#define CONFIG_X_MOUSE 1
#define DOSEMU060 1
#define DOSEMU061 0


#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>

#if DOSEMU060
#include "emu.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#if CONFIG_X_MOUSE
#include "mouse.h"
#endif

#define Bit8u byte
#define Bit16u us
#define Boolean boolean
#endif
#if DOSEMU061
#include "dosemu.h"
#include "base/bios.h"
#include "env/video.h"
#include "base/memory.h"
#include "base/mouse.h"
#include "env/setup.h"
#include "X.h"
#endif

static const u_char dos_to_latin[]={
   0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,  /* 80-87 */ 
   0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,  /* 88-8F */
   0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf3, 0xfb, 0xf9,  /* 90-97 */
   0xff, 0xd6, 0xdc, 0xa2, 0xa3, 0xa5, 0x00, 0x00,  /* 98-9F */
   0xe1, 0xed, 0xf2, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,  /* A0-A7 */
   0xbf, 0x00, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,  /* A8-AF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* B0-B7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* B8-BF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* C0-C7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* C8-CF */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* D0-D7 */
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  /* D8-DF */
   0x00, 0xdf, 0x00, 0x00, 0x00, 0x00, 0xb5, 0x00,  /* E0-E7 */
   0x00, 0x00, 0x00, 0xf0, 0x00, 0xd8, 0x00, 0x00,  /* E8-EF */
   0x00, 0xb1, 0x00, 0x00, 0x00, 0x00, 0xf7, 0x00,  /* F0-F7 */
   0xb0, 0xb7, 0x00, 0x00, 0xb3, 0xb2, 0x00, 0x00   /* F8-FF */
};  



/**************************************************************************/

/* From Xkeyb.c */
extern void X_process_key(XKeyEvent *);

/* Sorry... */
#if CONFIG_X_SELECTION && CONFIG_X_MOUSE
/*#error You cannot define both CONFIG_X_SELECTION and CONFIG_X_MOUSE!*/
#endif

/* "Fine tuning" options for X_update_screen */
#define MAX_UNCHANGED	3

/* Kludge for incorrect ASCII 0 char in vga font. */
#define XCHAR(w) (CHAR(w)?CHAR(w):' ')

#if CONFIG_X_SELECTION
#define SEL_ACTIVE(w) (visible_selection && ((w) >= sel_start) && ((w) <= sel_end))
#define SEL_ATTR(attr) (((attr) >> 4) ? (attr) :(((attr) & 0x0f) << 4))
#define XATTR(w) (SEL_ACTIVE(w) ? SEL_ATTR(ATTR(w)) : ATTR(w))
#else
#define XATTR(w) (ATTR(w))
#endif
#define XREAD_WORD(w) ((XATTR(w)<<8)|XCHAR(w))

/**************************************************************************/

static Display *dpy;
static int scr;
static Window root_win, win;
static Cursor X_standard_cursor;
#if CONFIG_X_MOUSE
static Cursor X_mouse_cursor;
#endif
static GC gc;
static Font vga_font;
static Atom proto_atom = None, delete_atom = None;

static int font_width, font_height, font_shift;
static int prev_cursor_row=-1, prev_cursor_col=-1;
static ushort prev_cursor_shape=NO_CURSOR;
static int blink_state = 1;
static int blink_count = 8;

#if CONFIG_X_SELECTION
static int sel_start_row = -1, sel_end_row = -1, sel_start_col, sel_end_col;
static unsigned short *sel_start = NULL, *sel_end = NULL;
static u_char *sel_text = NULL;
static Atom compound_text_atom = None;
static Boolean doing_selection = FALSE, visible_selection = FALSE;
#endif

static Boolean have_focus = FALSE;
static Boolean is_mapped = FALSE;

/* Initialization is used just in case XAllocColor fails. */
static unsigned long vga_colors[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

static void set_sizehints(int xsize, int ysize);


/**************************************************************************/
/*                         INITIALIZATION                                 */
/**************************************************************************/

/* Allocate normal VGA colors, and store color indices to vga_colors[]. */
static void get_vga_colors(void)
{
/* The following are almost IBM standard color codes, with some slight
   * gamma correction for the dim colors to compensate for bright X
   * screens.
 */
static struct {
   unsigned char r,g,b;
} crgb[16]={
{0x00,0x00,0x00},{0x18,0x18,0xB2},{0x18,0xB2,0x18},{0x18,0xB2,0xB2},
{0xB2,0x18,0x18},{0xB2,0x18,0xB2},{0xB2,0x68,0x18},{0xB2,0xB2,0xB2},
{0x68,0x68,0x68},{0x54,0x54,0xFF},{0x54,0xFF,0x54},{0x54,0xFF,0xFF},
    {0xFF,0x54,0x54},{0xFF,0x54,0xFF},{0xFF,0xFF,0x54},{0xFF,0xFF,0xFF}
  };
	int i;
        XColor xcol;
  Colormap cmap = DefaultColormap(dpy, scr);

  X_printf("X: Getting VGA colors\n");
	for(i=0;i<16;i++) {
	   xcol.red   = crgb[i].r<<8;
	   xcol.green = crgb[i].g<<8;
	   xcol.blue  = crgb[i].b<<8;
	   if (!XAllocColor(dpy, cmap, &xcol)) {
      error("X: Couldn't allocate all VGA colors!\n");
	      return;
	   }
	   vga_colors[i] = xcol.pixel;
	}
}

  
/* Load the main text font. Try first the user specified font, then
 * vga, then 9x15 and finally fixed. If none of these exists and
 * is monospaced, give up (shouldn't happen).
 */
static void load_text_font(void)
{
  const char *fonts[] = {config.X_font, "vga", "9x15", "fixed", NULL}, **p;
  XFontStruct *font;
  p = fonts;
  while (1) {
    if ((font=XLoadQueryFont(dpy, *p)) == NULL) {
      error("X: Unable to open font \"%s\"", *p);
    }
    else if (font->min_bounds.width != font->max_bounds.width) {
      error("X: Font \"%s\" isn't monospaced", *p);
      XFreeFont(dpy, font);
      font = NULL;
    }
    else {
      font_width = font->max_bounds.width;
      font_height = font->max_bounds.ascent + font->max_bounds.descent;
      font_shift = font->max_bounds.ascent;
      vga_font = font->fid;
      X_printf("X: Using font \"%s\", size=%dx%d\n", *p, font_width, font_height);
      break;     
    }  
    if (p[1] != NULL) {
      error(", trying \"%s\"...\n", p[1]);
      p++;
    }
    else {
      /* If the user doesn't have "fixed", he's in big trouble anyway. */
      error(", aborting!\n");
      leavedos(99);
    }  
}
}


/* Load the mouse cursor shapes. */
static void load_cursor_shapes(void)
{
  Colormap cmap;
  XColor fg, bg;
  Font cfont;
#if CONFIG_X_MOUSE  
  Font decfont;
#endif  

  /* Use a white on black cursor as the background is normally dark. */
  cmap = DefaultColormap(dpy, scr);
  XParseColor(dpy, cmap, "white", &fg);
  XParseColor(dpy, cmap, "black", &bg);

  cfont = XLoadFont(dpy, "cursor");
  if (!cfont) {
    error("X: Unable to open font \"cursor\", aborting!\n");
    leavedos(99);
  }
#if CONFIG_X_SELECTION
/*  X_standard_cursor = XCreateGlyphCursor(dpy, cfont, cfont, 
    XC_xterm, XC_xterm+1, &fg, &bg);
*/
#endif
#if CONFIG_X_MOUSE
  X_standard_cursor = XCreateGlyphCursor(dpy, cfont, cfont, 
    XC_top_left_arrow, XC_top_left_arrow+1, &fg, &bg);
  /* IMHO, the DEC cursor font looks nicer, but if it's not there, 
   * use the standard cursor font. 
   */
  decfont = XLoadFont(dpy, "decw$cursor");
  if (!decfont) {
    X_mouse_cursor = XCreateGlyphCursor(dpy, cfont, cfont, 
      XC_hand2, XC_hand2+1, &bg, &fg);
  } 
  else {
    X_mouse_cursor = XCreateGlyphCursor(dpy, decfont, decfont, 2, 3, &fg, &bg);
    XUnloadFont(dpy, decfont);  
  }
#endif /* CONFIG_X_MOUSE */
  XUnloadFont(dpy, cfont);
}


/* Initialize everything X-related. */
static int X_init(void)
{
   XGCValues gcv;
   XClassHint xch;
   XSetWindowAttributes attr;
   
  X_printf("X: X_init\n");
#if DOSEMU061
  fake_video_port_init();
#endif
   co = 80;
   li = 25;
#if DOSEMU060  
  set_video_bios_size();		/* Make it stick */
#endif  
  
  /* Open X connection. */
   dpy=XOpenDisplay(config.X_display);
   if (!dpy) {
    const char *display_name; 
	display_name = config.X_display;
	if(!display_name)
		display_name = getenv("DISPLAY");
	if(!display_name)
      error("X: DISPLAY variable isn't set!\n");
    else
      error("X: Can't open display \"%s\"\n", display_name);
      leavedos(99);
   }
   scr=DefaultScreen(dpy);
  root_win = RootWindow(dpy, scr);
   
  get_vga_colors();
  load_text_font();

  win = XCreateSimpleWindow(dpy, root_win,
    0, 0,                               /* Position */
    co*font_width, li*font_height,      /* Size */
    0, 0,                               /* Border */
    vga_colors[0]);                     /* Background */
   set_sizehints(co,li);

  load_cursor_shapes();

  gcv.font = vga_font;
  gc = XCreateGC(dpy, win, GCFont, &gcv);
  attr.event_mask=KeyPressMask|KeyReleaseMask|KeymapStateMask|
                   ButtonPressMask|ButtonReleaseMask|
                   EnterWindowMask|LeaveWindowMask|PointerMotionMask|
                   ExposureMask|StructureNotifyMask|FocusChangeMask;
  attr.cursor = X_standard_cursor;
  XChangeWindowAttributes(dpy, win, CWEventMask|CWCursor, &attr);
      
  XStoreName(dpy, win, config.X_title);
  XSetIconName(dpy, win, config.X_icon_name);
  xch.res_name = (char *) "XDosEmu";
  xch.res_class = (char *) "XDosEmu";
  XSetClassHint(dpy, win, &xch);

#if CONFIG_X_SELECTION
  /* Get atom for COMPOUND_TEXT type. */
  compound_text_atom = XInternAtom(dpy, "COMPOUND_TEXT", False);
#endif
  /* Delete-Window-Message black magic copied from xloadimage. */
   proto_atom  = XInternAtom(dpy,"WM_PROTOCOLS", False);
   delete_atom = XInternAtom(dpy,"WM_DELETE_WINDOW", False);
  if ((proto_atom != None) && (delete_atom != None)) {
    XChangeProperty(dpy, win, proto_atom, XA_ATOM, 32,
                      PropModePrepend, (char*)&delete_atom, 1);
  }
                                    
  XMapWindow(dpy, win);
            
  X_printf("X: X_init done, scr=%d, root=0x%lx, win=0x%lx\n",
    scr, (unsigned long) root_win, (unsigned long) win);
  return(0);                        
}


/* Free resources and close X connection. */
static void X_close(void) 
{
  X_printf("X: X_close\n");
  if (dpy == NULL) 
    return;
   XUnloadFont(dpy,vga_font);
  XDestroyWindow(dpy, win);
   XFreeGC(dpy,gc);
   XCloseDisplay(dpy);
}


/**************************************************************************/
/*                            MISC FUNCTIONS                              */
/**************************************************************************/

/* Set window manager size hints. */
static void set_sizehints(int xsize, int ysize)
{
  XSizeHints sh;
  X_printf("X: Setting size hints, max=%dx%d, inc=%dx%d\n", xsize, ysize, 
    font_width, font_height);
  sh.max_width = xsize*font_width;
  sh.max_height = ysize*font_height;
  sh.width_inc = font_width;
  sh.height_inc = font_height;
  sh.flags = PMaxSize|PResizeInc;
  XSetWMNormalHints(dpy, win, &sh);
}


/* Set video mode. Type 0 means text mode, which is all we support
 * for now. 
 */
static int X_setmode(int type, int xsize, int ysize) 
{
  X_printf("X: Setting mode, type=%d, size=%dx%d\n", type, xsize, ysize);
  if (type == 0) { 
    XResizeWindow(dpy, win, xsize*font_width, ysize*font_height);
      set_sizehints(xsize,ysize);
   }
  return(0);
}


/* Change color values in our graphics context 'gc'. */
static inline void set_gc_attr(Bit8u attr)
{
   XGCValues gcv;
   gcv.foreground=vga_colors[ATTR_FG(attr)];
   gcv.background=vga_colors[ATTR_BG(attr)];
   XChangeGC(dpy,gc,GCForeground|GCBackground,&gcv);
}


/**************************************************************************/
/*                              CURSOR                                    */
/**************************************************************************/

/* Draw the cursor (normal if we have focus, rectangle otherwise). */
static void draw_cursor(int x, int y)
{  
  set_gc_attr(XATTR(screen_adr+y*co+x));
  if (!have_focus) {
    XDrawRectangle(dpy, win, gc,
      x*font_width, y*font_height,
      font_width-1, font_height-1);
  }
  else if (blink_state) {
    int cstart = CURSOR_START(cursor_shape) * font_height / 16,
      cend = CURSOR_END(cursor_shape) * font_height / 16;
    XFillRectangle(dpy, win, gc,
                     x*font_width, y*font_height+cstart,
                     font_width, cend-cstart+1);
   }
}


/* Restore a character cell (i.e., remove the cursor). */
static inline void restore_cell(int x, int y)
{
  Bit16u *sp = screen_adr+y*co+x, *oldsp = prev_screen+y*co+x;
   char c = XCHAR(sp);
  set_gc_attr(XATTR(sp));
  *oldsp = XREAD_WORD(sp);
  XDrawImageString(dpy, win, gc, x*font_width, y*font_height+font_shift, &c, 1);
}

  
/* Move cursor to a new position (and erase the old cursor). */
static void redraw_cursor(void) 
{
  if (!is_mapped) 
    return;
  if (prev_cursor_shape!=NO_CURSOR)
    restore_cell(prev_cursor_col, prev_cursor_row);
   if (cursor_shape!=NO_CURSOR)
    draw_cursor(cursor_col, cursor_row);
   XFlush(dpy);

   prev_cursor_row=cursor_row;
   prev_cursor_col=cursor_col;
   prev_cursor_shape=cursor_shape;
}


/* Redraw the cursor if it's necessary. */
static void X_update_cursor(void) 
   {
  if ((cursor_row != prev_cursor_row) || (cursor_col != prev_cursor_col) ||
    (cursor_shape != prev_cursor_shape))
  {
    redraw_cursor();
   }
}


/* Blink the cursor. This is called from SIGALRM handler. */
void X_blink_cursor(void)
{
  if (!have_focus || --blink_count)
      return;
   blink_count = config.X_blinkrate;
   blink_state = !blink_state;
   if (cursor_shape!=NO_CURSOR) {
    if ((cursor_row != prev_cursor_row) || (cursor_col != prev_cursor_col)) {
      restore_cell(prev_cursor_col, prev_cursor_row);
      prev_cursor_row = cursor_row;
      prev_cursor_col = cursor_col;
      prev_cursor_shape = cursor_shape;
    }
      if (blink_state)
      draw_cursor(cursor_col,cursor_row);
      else
      restore_cell(cursor_col, cursor_row);
   }
}

 
/**************************************************************************/
/*                          SCREEN UPDATES                                */
/**************************************************************************/

/* Redraw the entire screen. Used for expose events. */
static void redraw_screen(void)
{
  Bit16u *sp, *oldsp;
  char charbuf[MAX_COLUMNS], *bp;
  int x, y, start_x;
  Bit8u attr;

  if (!is_mapped)
    return;
   
  X_printf("X: redraw_screen, co=%d, li=%d, screen_adr=0x%x\n",
            co,li,(int)screen_adr);

   sp=screen_adr;
  oldsp = prev_screen;
  for(y = 0; (y < li); y++) {
      x=0;
      do {
      /* Scan in a string of chars of the same attribute. */
      bp = charbuf;
         start_x=x;
      attr = XATTR(sp);
         do {
        *oldsp++ = XREAD_WORD(sp);
            *bp++=XCHAR(sp);
        sp++; 
	x++;
      } while ((XATTR(sp)==attr) && (x < co));

      /* Ok, we've got the string. Now send it to the X server. */
      set_gc_attr(attr);
      XDrawImageString(dpy, win, gc,
                          font_width*start_x,font_height*y+font_shift,
        charbuf, x-start_x);
      } while(x<co);
   }
  prev_cursor_shape = NO_CURSOR;        /* It was overwritten. */
  redraw_cursor();
   clear_scroll_queue();
   
  X_printf("X: redraw_screen done\n");
}



#if USE_SCROLL_QUEUE

#error X does not do scroll queueing
  
/* XCopyArea can't be used if the window is obscured and backing store
 * isn't used!
 */
void X_scroll(int x,int y,int width,int height,int n,byte attr) 
{
  x*=font_width;
  y*=font_height;
  width*=font_width;
  height*=font_height;
  n*=font_height;
  
  XSetForeground(dpy,gc,vga_colors[attr>>4]);
  
  if (n > 0) {                          /* Scroll up */
     if (n>=height) {
        n=height;
     }
     else {
        height-=n;
      XCopyArea(dpy, win, win, gc, x, y+n, width, height, x, y);
     }
     /*
    XFillRectangle(dpy,win,gc,x,y+height,width,n);
     */
  }
  else if (n < 0) {                     /* Scroll down */
     if (-n>=height) {
        n=-height;
     }
     else {
        height+=n;
      XCopyArea(dpy, win, win, gc, x, y, width, height, x, y-n);
     }
     /*
    XFillRectangle(dpy,win,gc,x,y,width,-n);
     */
  }
}


/* Process the scroll queue */
static void do_scroll(void) 
{
   struct scroll_entry *s;

  while (s =get_scroll_queue()) {
      if (s->n!=0) {
         X_scroll(s->x0,s->y0,s->x1-s->x0+1,s->y1-s->y0+1,s->n,s->attr);
         Scroll(prev_screen,s->x0,s->y0,s->x1,s->y1,s->n,0xff);
      if ((prev_cursor_col >= s->x0) && (prev_cursor_col <= s->x1) &&
        (prev_cursor_row >= s->y0) && (prev_cursor_row <= s->y1))
         {
        prev_cursor_shape = NO_CURSOR;  /* Cursor was overwritten. */
         }
      }
   }
}
#endif   /* USE_SCROLL_QUEUE */



/* Update the part of the screen which has changed. Usually called 
 * from the SIGALRM handler. 
*/
static int X_update_screen(void)
{
  Bit16u *sp, *oldsp;
  char charbuf[MAX_COLUMNS], *bp;

  int x, y;	                        /* Position of character being updated. */
  int start_x, len, unchanged;
  Bit8u attr;

  static int yloop = -1;
  int lines;                            /* Number of lines to redraw. */
  int numscan = 0;	                /* Number of lines scanned. */
  int numdone = 0;                      /* Number of lines actually updated. */

  if (!is_mapped) 
    return(0);

#if USE_SCROLL_QUEUE
  do_scroll();
#endif
  
  /* The following determines how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  This is set by the "updatelines" keyword
   * in /etc/dosemu.conf.
   */
  lines = config.X_updatelines;
  if (lines < 2) 
    lines = 2;
  else if (lines > li)
    lines = li;

  /* The highest priority is given to the current screen row for the
   * first iteration of the loop, for maximum typing response.  
   * If y is out of bounds, then give it an invalid value so that it
   * can be given a different value during the loop.
   */
  y = cursor_row;
  if ((y < 0) || (y >= li)) 
    y = -1;

#if 0  
  X_printf("X: X_update_screen, co=%d, li=%d, yloop=%d, y=%d, lines=%d\n",
           co,li,yloop,y,lines);
#endif

  /* The following loop scans lines on the screen until the maximum number
   * of lines have been updated, or the entire screen has been scanned.
   */
  while ((numdone < lines) && (numscan < li)) {
    /* The following sets the row to be scanned and updated, if it is not
     * the first iteration of the loop, or y has an invalid value from
     * loop pre-initialization.
     */
    if ((numscan > 0) || (y < 0)) {
      yloop = (yloop+1) % li;
      if (yloop == cursor_row)
        yloop = (yloop+1) % li;
      y = yloop;
    }
    numscan++;

    sp = screen_adr + y*co;
    oldsp = prev_screen + y*co;

    x=0;
    do {
        /* find a non-matching character position */
      start_x = x;
      while (XREAD_WORD(sp)==*oldsp) {
           sp++; oldsp++; x++;
        if (x==co) {
	  if (start_x == 0)
	    goto chk_cursor;
	  else
	    goto line_done;
	}
        }

      /* Now scan in a string of changed chars of the same attribute.
       * To keep the number of X calls (and thus the overhead) low,
       * we tolerate a few unchanged characters (up to MAX_UNCHANGED in 
       * a row) in the 'changed' string. 
        */
      bp = charbuf;
        start_x=x;
      attr = XATTR(sp);
      unchanged = 0;                    /* Counter for unchanged chars. */

        while(1) {
           *bp++=XCHAR(sp);
	*oldsp++ = XREAD_WORD(sp);
        sp++; 
	x++;

        if ((XATTR(sp) != attr) || (x == co))
	  break;
        if (XREAD_WORD(sp) == *oldsp) {
          if (unchanged > MAX_UNCHANGED) 
	    break;
              unchanged++;
           }
	else
	  unchanged=0;
        } 
        len=x-start_x-unchanged;

      /* Ok, we've got the string now. Send it to the X server. */
      set_gc_attr(attr);
      XDrawImageString(dpy, win, gc,
                         font_width*start_x,font_height*y+font_shift,
        charbuf, len);

      if ((prev_cursor_row == y) && 
        (prev_cursor_col >= start_x) && (prev_cursor_col < start_x+len))
        {
        prev_cursor_shape = NO_CURSOR;  /* Old cursor was overwritten. */
        }
    } while(x<co);
line_done:
    /* Increment the number of successfully updated lines counter */
    numdone++;

chk_cursor:
    /* Update the cursor. We do this here to avoid the cursor 'running 
     * behind' when using a fast key-repeat.
    */
    if (y == cursor_row)
      X_update_cursor();
  }
  XFlush(dpy);
#if 0
  X_printf("X: X_update_screen done, %d lines updated\n", numdone);
#endif
  if (numdone) {
     if (numscan==li)
      return(1);                        /* Changed, entire screen updated. */
     else
      return(2);                        /* Changed, part of screen updated. */
  }
  else {
     /* The updates look a bit cleaner when reset to top of the screen
     * if nothing had changed on the screen in this call to X_update_screen.
      */
     yloop = -1;
    return(1);
  }
}


/**************************************************************************/
/*                                MOUSE                                   */
/**************************************************************************/

/* Change the mouse cursor shape (called from the mouse code). */
void X_change_mouse_cursor(void) 
{
#if CONFIG_X_MOUSE
  /* Yes, this is illogical, but blame the author of the mouse code. 
   * mouse.cursor_on is zero if the cursor is on! 
   */
  if (mouse.cursor_on != 0)
    XDefineCursor(dpy, win, X_standard_cursor);
  else
    XDefineCursor(dpy, win, X_mouse_cursor);
#endif    
}

#if CONFIG_X_MOUSE
static void set_mouse_position(int x, int y)
{
  /* XXX - This works in text mode only */
   x = x*8/font_width;
   y = y*8/font_height;
  if ((x != mouse.x) || (y != mouse.y)) {
      mouse.x=x; 
      mouse.y=y;
      mouse_move();
   }
}   

static void set_mouse_buttons(int state) 
{
   mouse.oldlbutton=mouse.lbutton;
   mouse.oldmbutton=mouse.mbutton;
   mouse.oldrbutton=mouse.rbutton;
   mouse.lbutton = ((state & Button1Mask) != 0);
   mouse.mbutton = ((state & Button2Mask) != 0);
   mouse.rbutton = ((state & Button3Mask) != 0);
  if (mouse.lbutton != mouse.oldlbutton) 
    mouse_lb();
  if (mouse.threebuttons && (mouse.mbutton != mouse.oldmbutton)) 
    mouse_mb();
  if (mouse.rbutton != mouse.oldrbutton) 
    mouse_rb();
}
#endif /* CONFIG_X_MOUSE */


/**************************************************************************/
/*                               SELECTION                                */
/**************************************************************************/

#if CONFIG_X_SELECTION
/* Convert X coordinate to column, with bounds checking. */
static int x_to_col(int x)
{
  int col = x/font_width;
  if (col < 0)
    col = 0;
  else if (col >= co)
    col = co-1;
  return(col);
}

/* Convert Y coordinate to row, with bounds checking. */
static int y_to_row(int y)
{
  int row = y/font_height;
  if (row < 0)
    row = 0;
  else if (row >= li)
    row = li-1;
  return(row);
}

/* Calculate sel_start and sel_end pointers from sel_start|end_col|row. */
static void calculate_selection(void)
{
  if ((sel_end_row < sel_start_row) || 
    ((sel_end_row == sel_start_row) && (sel_end_col < sel_start_col)))
  {
    sel_start = screen_adr+sel_end_row*co+sel_end_col;
    sel_end = screen_adr+sel_start_row*co+sel_start_col-1;
  }
  else
  {
    sel_start = screen_adr+sel_start_row*co+sel_start_col;
    sel_end = screen_adr+sel_end_row*co+sel_end_col-1;
  }
}


/* Clear visible part of selection (but not the selection text buffer). */
static void clear_visible_selection(void)
{
  sel_start_col = sel_start_row = sel_end_col = sel_end_row = 0;
  sel_start = sel_end = NULL;
  visible_selection = FALSE;
}

/* Free the selection text buffer. */
static void clear_selection_data(void)
{
  X_printf("X: Selection data cleared\n");
  if (sel_text != NULL)
  {
    free(sel_text);
    sel_text = NULL;
  }
  doing_selection = FALSE;
  clear_visible_selection();
}

/* Check if we should clear selection. 
   Clear if cursor is in or before selected area  
*/
static void clear_if_in_selection()
{
  X_printf("X:start selection , cursor at %d %d\n",cursor_col,cursor_row);
  if (((sel_start_row <= cursor_row)&&(cursor_row <= sel_end_row)&&
       (sel_start_col <= cursor_col)&&(cursor_col <= sel_end_col)) ||
      /* cursor either inside selectio */
       (( cursor_row <= sel_start_row)&&(cursor_col <= sel_start_col)))
      /* or infront of selection */
    {
      X_printf("X:selection clear, key-event!\n");
      clear_visible_selection(); 
    }
}

/* Start the selection process (mouse button 1 pressed). */
static void start_selection(int x, int y)
{
  int col = x_to_col(x), row = y_to_row(y);
  sel_start_col = sel_end_col = col;
  sel_start_row = sel_end_row = row;
  calculate_selection();
  doing_selection = visible_selection = TRUE;
  X_printf("X:start selection , start %d %d, end %d %d\n",
	   sel_start_col,sel_start_row,sel_end_col,sel_end_row);
}

/* Extend the selection (mouse motion). */
static void extend_selection(int x, int y)
{
  int col = x_to_col(x), row = y_to_row(y);
  if ((sel_end_col == col) && (sel_end_row == row))
    return;
  sel_end_col = col;
  sel_end_row = row;
  calculate_selection();
  X_printf("X:extend selection , start %d %d, end %d %d\n",
	   sel_start_col,sel_start_row,sel_end_col,sel_end_row);
}

/* Start extending the selection (mouse button 3 pressed). */
static void start_extend_selection(int x, int y)
{
  /* Try to extend selection, visiblity is handled by extend_selection*/
    doing_selection =  visible_selection = TRUE;  
    extend_selection(x, y);
  
}


/* Copy the selected text to sel_text, and inform the X server about it. */
static void save_selection_data(void)
{
  int col, row, col1, row1, col2, row2, line_start_col, line_end_col;
  u_char *p;
  if ((sel_end-sel_start) < 0)
  {
    visible_selection = FALSE;
    return;
  }
  row1 = (sel_start-screen_adr)/co;
  row2 = (sel_end-screen_adr)/co;
  col1 = (sel_start-screen_adr)%co;
  col2 = (sel_end-screen_adr)%co;
  
  /* Allocate space for text. */
  if (sel_text != NULL)
    free(sel_text);
  p = sel_text = malloc((row2-row1+1)*(co+1)+2);
  
  /* Copy the text data. */
  for (row = row1; (row <= row2); row++)
  {
    line_start_col = ((row == row1) ? col1 : 0);
    line_end_col = ((row == row2) ? col2 : co-1);
    for (col = line_start_col; (col <= line_end_col); col++)
    {
      *p++ = XCHAR(screen_adr+row*co+col);
    }
    /* Remove end-of-line spaces and add a newline. */
    if (col == co)
    { 
      p--;
      while ((*p == ' ') && (p > sel_text))
        p--;
      p++;
      *p++ = '\n';
    }
  }
  *p = '\0';
  if (strlen(sel_text) == 0)
    return;
    
  /* Inform the X server. */
  XSetSelectionOwner(dpy, XA_PRIMARY, win, CurrentTime);
  if (XGetSelectionOwner(dpy, XA_PRIMARY) != win)
  {
    X_printf("X: Couldn't get primary selection!\n");
    return;
}
  XChangeProperty(dpy, root_win, XA_CUT_BUFFER0, XA_STRING,
    8, PropModeReplace, sel_text, strlen(sel_text));

  X_printf("X: Selection, %d,%d->%d,%d, size=%d\n", 
    col1, row1, col2, row2, strlen(sel_text));
}

/* End of selection (button released). */
static void end_selection(void)
{
  if (!doing_selection)
    return;
  doing_selection = FALSE;
  save_selection_data();
}


/* Send selection data to other window. */
static void send_selection(Time time, Window requestor, Atom target, Atom property)
{
  int i;
  XEvent e;
  e.xselection.type = SelectionNotify;
  e.xselection.selection = XA_PRIMARY;
  e.xselection.requestor = requestor;
  e.xselection.time = time;
  if (sel_text == NULL) {
    X_printf("X: Window 0x%lx requested selection, but it's empty!\n",   
      (unsigned long) requestor);
    e.xselection.property = None;
  }
  else if ((target == XA_STRING) || (target == compound_text_atom)) {
    X_printf("X: selection (dos): %s\n",sel_text);   
    e.xselection.target = target;
    for (i=0; i<strlen(sel_text);i++)
      switch (sel_text[i]) 
	{
	case 21 : /* § */
	  sel_text[i] = 0xa7;
	  break;
	case 20 : /* ¶ */
	  sel_text[i] = 0xb6;
	  break;
	case 124 : /* ¦ */
	  sel_text[i] = 0xa6;
	  break;
	case 0x80 ... 0xff: 
	  sel_text[i]=dos_to_latin[ sel_text[i] - 0x80 ];
	} 
    X_printf("X: selection (latin): %s\n",sel_text);  
    XChangeProperty(dpy, requestor, property, target, 8, PropModeReplace, 
      sel_text, strlen(sel_text));
    e.xselection.property = property;
    X_printf("X: Selection sent to window 0x%lx as %s\n", 
      (unsigned long) requestor, (target==XA_STRING)?"string":"compound_text");
  }
  else
  {
    e.xselection.property = None;
    X_printf("X: Window 0x%lx requested unknown selection format %ld\n",
      (unsigned long) requestor, (unsigned long) target);
  }
  XSendEvent(dpy, requestor, False, 0, &e);
}
#endif /* CONFIG_X_SELECTION */


/**************************************************************************/
/*                           X EVENT HANDLER                              */
/**************************************************************************/

/* This handles pending X events (called from SIGALRM handler). */
void X_handle_events(void)
{
   static int busy = 0;
   XEvent e;

   if (busy) return;
   busy=1;

   while (XPending(dpy) > 0) {
      XNextEvent(dpy,&e);

      switch(e.type) {
       case Expose:  
      X_printf("X: Expose event\n");
      if (e.xexpose.count == 0)         /* Avoid redundant redraws. */
        redraw_screen();
                     break;

    case MapNotify:
      X_printf("X: Window mapped\n");
      is_mapped = TRUE;
                     break;

    case UnmapNotify:
      X_printf("X: Window unmapped\n");
      is_mapped = FALSE;
                     break;

       case FocusIn:
      X_printf("X: Focus in\n");
      have_focus = TRUE;
      redraw_cursor();
                     break;

       case FocusOut:
      X_printf("X: Focus out\n");
      have_focus = FALSE;
      redraw_cursor();
                     break;

    case DestroyNotify:
      error("X: Window got destroyed!\n");
      leavedos(99);
      break;

    case ClientMessage:
       /* If we get a client message which has the value of the delete
        * atom, it means the window manager wants us to die.
        */
      if (e.xclient.data.l[0] == delete_atom) {
        error("X: Got delete message!\n");
	/* XXX - Is it ok to call this from a SIGALRM handler? */
      	leavedos(0);    
      }
                     break;

    /* Selection-related events */

#if CONFIG_X_SELECTION
    /* The user made a selection in another window, so our selection
     * data is no longer needed. 
*/
    case SelectionClear:
      clear_selection_data();
                    break;
                    
    case SelectionNotify: 
       scr_paste_primary(dpy,e.xselection.requestor,
           e.xselection.property,True);
       X_printf("X: SelectionNotify event\n");
		    break;
		    
    /* Some other window wants to paste our data, so send it. */
    case SelectionRequest:
      send_selection(e.xselectionrequest.time,
	e.xselectionrequest.requestor,
        e.xselectionrequest.target,
        e.xselectionrequest.property);
		    break;
#endif /* CONFIG_X_SELECTION */
		   
    /* Keyboard events */

    case KeyPress:
    case KeyRelease:
#if 0    
      /* Keyboard is disabled while selecting. */
      if (doing_selection)              
        break;
#endif	  
/* 
      Clears the visible selection if the cursor is inside the selection
*/
      if (visible_selection)
	clear_if_in_selection();
      X_process_key(&e.xkey);
		    break;
		    
    /* A keyboard mapping has been changed (e.g., with xmodmap). */
    case MappingNotify:  
      X_printf("X: MappingNotify event\n");
      XRefreshKeyboardMapping(&e.xmapping);
		    break;
		       
    /* Mouse events */
		     
#if CONFIG_X_SELECTION
    case ButtonPress:
      if (e.xbutton.button == Button1)
	start_selection(e.xbutton.x, e.xbutton.y);
      else if (e.xbutton.button == Button3)
	start_extend_selection(e.xbutton.x, e.xbutton.y);
      set_mouse_buttons(e.xbutton.state|(0x80<<e.xbutton.button));
                    break;
                    
    case ButtonRelease:
      switch (e.xbutton.button)
	{
	case Button1 :
	case Button3 : 
	  end_selection();
	  break;
	case Button2 :
	  X_printf("X: mouse Button2Release\n");
	  scr_request_selection(dpy,win,e.xbutton.time,
				e.xbutton.x,
				e.xbutton.y);
	  break;
                    }
      set_mouse_buttons(e.xbutton.state&~(0x80<<e.xbutton.button));
      break;
      
    case MotionNotify:
      if (doing_selection)
        extend_selection(e.xmotion.x, e.xmotion.y);
      set_mouse_position(e.xmotion.x, e.xmotion.y);
                    break;

    case EnterNotify:
      X_printf("X: Mouse entering window\n");
      set_mouse_position(e.xcrossing.x, e.xcrossing.y);
      set_mouse_buttons(e.xcrossing.state);
      break;
		                        
    case LeaveNotify:                   /* Should this do anything? */
      X_printf("X: Mouse leaving window\n");
      break;
#endif /* CONFIG_X_SELECTION */
#if CONFIG_X_MOUSE
#endif /* CONFIG_X_MOUSE */
      }
   }
   busy=0;
#if CONFIG_X_MOUSE  
   mouse_event();
#endif  
}


struct video_system Video_X = {
   0,                /* is_mapped */
   X_init,         
   X_close,      
   X_setmode,      
   X_update_screen,
   X_update_cursor
};
