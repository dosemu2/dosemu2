
#include <stdio.h>
#include <malloc.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include "emu.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#include "mouse.h"

/* "fine tuning" options for X_update_screen */

#define MAX_UNCHANGED	3
#define COPY_WHOLE_LINE	1
#define PRECHECK_LINE   1

#if 0
#define XCHAR(w) CHAR(w)
#else
/* kludge for incorrect ASCII 0 char in vga font */
#define XCHAR(w) (CHAR(w)?CHAR(w):' ')
#endif

/********************************************/

Display *dpy;
int scr;
Window root,W;
static Cursor X_stnd_cursor, X_mouse_cursor;
GC gc;
Font vga_font;
Atom proto_atom = None, delete_atom = None;

static int font_width=8, font_height=16, font_shift=12;
int prev_cursor_row=-1, prev_cursor_col=-1;
ushort prev_cursor_shape=NO_CURSOR;
int blink_state = 1;
int blink_count = 8;

boolean have_focus=0, is_mapped=0;

/* from Xkeyb.c */
extern void X_process_key(XKeyEvent *);

/* The following are almost IBM standard color codes, with some slight
 *gamma correction for the dim colors to compensate for bright Xwindow screens.
 */
struct {
   unsigned char r,g,b;
} crgb[16]={
{0x00,0x00,0x00},{0x18,0x18,0xB2},{0x18,0xB2,0x18},{0x18,0xB2,0xB2},
{0xB2,0x18,0x18},{0xB2,0x18,0xB2},{0xB2,0x68,0x18},{0xB2,0xB2,0xB2},
{0x68,0x68,0x68},{0x54,0x54,0xFF},{0x54,0xFF,0x54},{0x54,0xFF,0xFF},
{0xFF,0x54,0x54},{0xFF,0x54,0xFF},{0xFF,0xFF,0x54},{0xFF,0xFF,0xFF}};

/* initialization is used just in case XAllocColor fails */
static int vga_colors[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

inline void get_vga_colors()
{
	int i;
        int cmap=DefaultColormap(dpy,scr);
        XColor xcol;

        X_printf("X: getting VGA colors\n");
        
	/*xcol.flags = DoRed | DoGreen | DoBlue;
	*/
	for(i=0;i<16;i++) {
	   xcol.red   = crgb[i].r<<8;
	   xcol.green = crgb[i].g<<8;
	   xcol.blue  = crgb[i].b<<8;
	   if (!XAllocColor(dpy, cmap, &xcol)) {
	      error("X: couldn't allocate all VGA colors!\n");
	      return;
	   }
	   vga_colors[i] = xcol.pixel;
	}
}

  
void set_sizehints(int xsize,int ysize) {
    XSizeHints sh;
	 X_printf("set_sizehints(): xsize=%d, ysize=%d\n",xsize,ysize);
    sh.max_width=xsize*font_width;
    sh.max_height=ysize*font_height;
    sh.width_inc=font_width;
    sh.height_inc=font_height;
    sh.flags = PMaxSize|PResizeInc;
    XSetNormalHints(dpy,W,&sh);
}

int X_init(void) {
   XGCValues gcv;
   XSetWindowAttributes attr;
   XColor fg, bg;
   Font cfont, decfont;
   XFontStruct *font;
   int cmap;
   
   X_printf("X_init()\n");
   
   dpy=XOpenDisplay(config.X_display);
   if (!dpy) {
      fprintf(stderr,"Can't open display %s\n",config.X_display);
      leavedos(99);
   }
   scr=DefaultScreen(dpy);
   root=RootWindow(dpy,scr);
   get_vga_colors();
      
   if ((font=XLoadQueryFont(dpy, config.X_font))==NULL 
       || font->min_bounds.width != font->max_bounds.width) {
      printf("ERROR: \"%s\" is not a monospaced font! Falling back to \"vga\".\n");
      if ((font=XLoadQueryFont(dpy, "vga"))==NULL
	  || font->min_bounds.width!=font->max_bounds.width ) {
	   printf("ERROR: Could not find the vga font - did you run `xinstallvgafont' ?\n"
		"Please read QuickStart and DOSEMU-HOWTO.* for more information.\n"
		"Will attempt to load 9x16\n");
        if ((font=XLoadQueryFont(dpy, "9x16"))==NULL
	  || font->min_bounds.width!=font->max_bounds.width ) {
	   printf("ERROR: Could not find the vga font - did you run `xinstallvgafont' ?\n"
		"Please read QuickStart and DOSEMU-HOWTO.* for more information.\n");
	   leavedos(99);
        }
      }
   }
   font_width = font->max_bounds.width;
   font_height = font->max_bounds.ascent + font->max_bounds.descent;
   font_shift = font->max_bounds.ascent;
   vga_font = font->fid;
   
   W = XCreateSimpleWindow(dpy,root,
                           0,0,                            /* position */
                           co*font_width,li*font_height,   /* size */
                           0,0,                            /* border */
                           vga_colors[0]                   /* background */
                           );
   
   gcv.font=vga_font;
   gc=XCreateGC(dpy,W,GCFont,&gcv);

   set_sizehints(co,li);

   /* Create the mouse cursor shapes */
   /* Use a white on black cursor as the background is normally dark */

   cmap=DefaultColormap(dpy,scr);
   /* Use a white on black cursor as the background is normally dark */
   XParseColor(dpy,cmap,"white",&fg);
   XParseColor(dpy,cmap,"black",&bg);

   cfont=XLoadFont(dpy, "cursor");
   X_stnd_cursor = XCreateGlyphCursor(dpy,cfont,cfont,XC_top_left_arrow,
                                      XC_top_left_arrow+1,&fg,&bg);
   decfont=XLoadFont(dpy,"decw$cursor");
   if (!decfont) {
      /* IMHO, the DEC cursor font looks nicer, but if it is not there, 
      use the standard cursor font */
      X_mouse_cursor = XCreateGlyphCursor(dpy,cfont,cfont,XC_hand2,
                                          XC_hand2+1,&bg,&fg);
   } else {
      X_mouse_cursor = XCreateGlyphCursor(dpy,decfont,decfont,2,3,&fg,&bg);
   }
   XUnloadFont(dpy,cfont);
   XUnloadFont(dpy,decfont);
   /*XDefineCursor(dpy,W,X_stnd_cursor);*/

   attr.event_mask=KeyPressMask|KeyReleaseMask|
                   ButtonPressMask|ButtonReleaseMask|
                   EnterWindowMask|LeaveWindowMask|PointerMotionMask|
                   ExposureMask|StructureNotifyMask|FocusChangeMask;
   attr.cursor=X_stnd_cursor;
      
   XChangeWindowAttributes(dpy,W,CWEventMask|CWCursor,&attr);

   XStoreName(dpy,W,config.X_title);
   XSetIconName(dpy,W,config.X_icon_name);

   /* Delete-Window-Message black magic copied from xloadimage */
   proto_atom  = XInternAtom(dpy,"WM_PROTOCOLS", False);
   delete_atom = XInternAtom(dpy,"WM_DELETE_WINDOW", False);
   if ((proto_atom != None) && (delete_atom != None))
      XChangeProperty(dpy, W, proto_atom, XA_ATOM, 32,
                      PropModePrepend, (char*)&delete_atom, 1);
                                    
   XMapWindow(dpy,W);

   X_printf("X_init() ok, dpy=%x scr=%d root=%d W=%d gc=%x\n",
            (int)dpy,(int)scr,(int)root,(int)W,(int)gc);
            
   return 0;                        
}

void X_close() {
   X_printf("X_close()\n");
   if (dpy==NULL) return;
   XUnloadFont(dpy,vga_font);
   XDestroyWindow(dpy,W);
   XFreeGC(dpy,gc);
   XCloseDisplay(dpy);
}

int X_setmode(int type, int xsize, int ysize) {
	X_printf("X_setmode() type=%d, xsize=%d, ysize=%d\n",type,xsize,ysize);
   if (type==0) { /* text mode */
      XResizeWindow(dpy,W,xsize*font_width,ysize*font_height);
      set_sizehints(xsize,ysize);
   }
   return 0;
}

inline void X_setattr(byte attr) {
   XGCValues gcv;
   gcv.foreground=vga_colors[ATTR_FG(attr)];
   gcv.background=vga_colors[ATTR_BG(attr)];
   XChangeGC(dpy,gc,GCForeground|GCBackground,&gcv);
}

void X_change_mouse_cursor(int flag) {
   XDefineCursor(dpy, W, flag ? X_mouse_cursor : X_stnd_cursor);
}

inline void X_draw_cursor(int x,int y)
{
   int cstart, cend;

   if (!blink_state)
      return;
   X_setattr(ATTR(screen_adr+y*co+x));
   if (have_focus) {
      cstart = CURSOR_START(cursor_shape) * font_height / 16;
      cend = CURSOR_END(cursor_shape) * font_height / 16;
      XFillRectangle(dpy,W,gc,
                     x*font_width, y*font_height+cstart,
                     font_width, cend-cstart+1);
   }
   else {
      XDrawRectangle(dpy,W,gc,
                     x*font_width, y*font_height,
                     font_width-1, font_height-1);
   }
}

inline void X_restore_cell(int x,int y) {
   us *sp = screen_adr+y*co+x;
   char c = XCHAR(sp);
   X_setattr(ATTR(sp));
   XDrawImageString(dpy,W,gc,font_width*x,font_height*y+font_shift,
                    &c,1);
}

void X_redraw_cursor() {
  
  if (!is_mapped) return;
#if 0  
  if (prev_cursor_shape!=NO_CURSOR)
#endif
     X_restore_cell(prev_cursor_col,prev_cursor_row);
   if (cursor_shape!=NO_CURSOR)
     X_draw_cursor(cursor_col,cursor_row);
   XFlush(dpy);

   prev_cursor_row=cursor_row;
   prev_cursor_col=cursor_col;
   prev_cursor_shape=cursor_shape;
}

void X_update_cursor() {

   if (cursor_row!=prev_cursor_row || cursor_col!=prev_cursor_col ||
       cursor_shape!=prev_cursor_shape)
   {
      blink_state = 1;
      blink_count = config.X_blinkrate;
      X_redraw_cursor();
   }
}

void X_blink_cursor()
{
   if (!have_focus)
      return;
   if (--blink_count)
      return;
   blink_count = config.X_blinkrate;
   blink_state = !blink_state;
   if (cursor_shape!=NO_CURSOR) {
      if (blink_state)
	 X_draw_cursor(cursor_col,cursor_row);
      else
	 X_restore_cell(cursor_col, cursor_row);
   }
}

/* redraw the entire screen. Used for expose events etc. */
 
void X_redraw_screen() 
{
   int x,y,start_x,attr;
   us *sp;
   char charbuff[MAX_COLUMNS];
   char *bp;

   if (!is_mapped) return;
   
   X_printf("X_redraw_screen entered; co=%d li=%d screen_adr=%x\n",
            co,li,(int)screen_adr);

   sp=screen_adr;

   for(y=0;y<li;y++) {
      x=0;
      do {
         /* scan in a string of chars of the same attribute. */

         bp=charbuff;
         start_x=x;
         attr=ATTR(sp);

         do {
            *bp++=XCHAR(sp);
            sp++; x++;
         } while(ATTR(sp)==attr && x<co);

         /* ok, we've got the string now send it to the X server */

         X_setattr(attr);
         XDrawImageString(dpy,W,gc,
                          font_width*start_x,font_height*y+font_shift,
                          charbuff,x-start_x);

      } while(x<co);
   }

   prev_cursor_shape = NO_CURSOR;   /* was overwritten */
   X_redraw_cursor();

   XFlush(dpy);

   /* memcpy(prev_screen,screen_adr,co*li*2); */
   MEMCPY_2UNIX(prev_screen,screen_adr,co*li*2);
   clear_scroll_queue();
   
   X_printf("X_redraw_screen done\n");
}

#if USE_SCROLL_QUEUE


void X_scroll(int x,int y,int width,int height,int n,byte attr) {
  
  x*=font_width;
  y*=font_height;
  width*=font_width;
  height*=font_height;
  n*=font_height;
  
  XSetForeground(dpy,gc,vga_colors[attr>>4]);
  
  if (n>0) {       /* scroll up */
     if (n>=height) {
        n=height;
     }
     else {
        height-=n;
        XCopyArea(dpy,W,W,gc,x,y+n,width,height,x,y);
     }
     /*
     XFillRectangle(dpy,W,gc,x,y+height,width,n);
     */
  }
  else if (n<0) {  /* scroll down */
     if (-n>=height) {
        n=-height;
     }
     else {
        height+=n;
        XCopyArea(dpy,W,W,gc,x,y,width,height,x,y-n);
     }
     /*
     XFillRectangle(dpy,W,gc,x,y,width,-n);
     */
  }
}


/* process the scroll queue */
void do_scroll() {
   struct scroll_entry *s;

   while((s=get_scroll_queue())) {
      if (s->n!=0) {
         X_scroll(s->x0,s->y0,s->x1-s->x0+1,s->y1-s->y0+1,s->n,s->attr);
         Scroll(prev_screen,s->x0,s->y0,s->x1,s->y1,s->n,0xff);
         if (prev_cursor_col>=s->x0 && prev_cursor_col<=s->x1 &&
            prev_cursor_row>=s->y0 && prev_cursor_row<=s->y1)
         {
	    prev_cursor_shape=NO_CURSOR;  /* cursor was overwritten */
         }
      }
   }
}
#endif   /* USE_SCROLL_QUEUE */


#define Y_INC(y) if (++y>=li) y=0

/* this is derived from ansi_update but heavily modified 
*/

int X_update_screen()
{
  us *srow, *oldsrow, *sp, *oldsp;
  char *bp;
  char charbuff[MAX_COLUMNS];

  int x, y;	/* X and Y position of character being updated */
  int start_x, len, unchanged;
  byte attr;

  static int yloop = -1;
  int lines;    /* Number of lines to redraw */
  int numscan;	/* counter for number of lines scanned */
  int numdone;  /* counter for number of lines actually updated */

  if (!is_mapped) return 0;       /* no need to do anything... */

#if USE_SCROLL_QUEUE
  do_scroll();
#endif
  
  /* The following determines how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  This is set by the "updatelines" keyword
   * in /etc/dosemu.conf 
   */
  lines = config.X_updatelines;
  if (lines < 2) 
    lines = 2;
  else if (lines > li)
    lines = li;

  numscan = 0;         /* Number of lines that have been scanned */
  numdone = 0;         /* Number of lines that needed to be updated */

  /* The highest priority is given to the current screen row for the
   * first iteration of the loop, for maximum typing response.  
   * If y is out of bounds, then give it an invalid value so that it
   * can be given a different value during the loop.
   */
  y = cursor_row;
  if ((y < 0) || (y > li)) y = -1; 

#if 0  
  X_printf("X_update_screen: co=%d li=%d yloop=%d y=%d lines=%d\n",
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
      Y_INC(yloop);
      if (yloop == cursor_row) Y_INC(yloop);
      y = yloop;
    }
    numscan++;

    sp = srow = screen_adr + y*co;
    oldsp = oldsrow = prev_screen + y*co;

#if PRECHECK_LINE
    /* Only update if the line has changed. 
       If the line matches, then no updated is needed, and skip the line. */
    /*if (!memcmp(srow, oldsrow, co * 2)) goto chk_cursor;*/
    if (!MEMCMP_DOS_VS_UNIX(srow, oldsrow, co * 2)) goto chk_cursor;
#endif

    x=0;
    do {
        /* find a non-matching character position */
/*        while (*sp==*oldsp) {*/
        while (READ_WORD(sp)==*oldsp) {
           sp++; oldsp++; x++;
           if (x==co) goto line_done;    /* sort of `double break' */
        }

        /* now scan in a string of changed chars of the same attribute.
           To keep the number of X calls (and thus the overhead) low,
           we tolerate a few unchanged characters (up to MAX_UNCHANGED in 
           a row) in the 'changed' string. 
        */

        bp=charbuff;
        start_x=x;
        attr=ATTR(sp);
        unchanged=0;         /* counter for unchanged chars */

        while(1) {
           *bp++=XCHAR(sp);
           sp++; oldsp++; x++;

           if (ATTR(sp)!=attr || x==co) break;
/*           if (*sp==*oldsp) {*/
           if (READ_WORD(sp)==*oldsp) {
              if (unchanged>MAX_UNCHANGED) break;
              unchanged++;
           }
           else unchanged=0;
        } 

        len=x-start_x-unchanged;
        /* ok, we've got the string now send it to the X server */

        X_setattr(attr);
        XDrawImageString(dpy,W,gc,
                         font_width*start_x,font_height*y+font_shift,
                         charbuff,len);

        if (y==prev_cursor_row && 
            start_x<=prev_cursor_col && start_x+len>prev_cursor_row)
        {
           prev_cursor_shape=NO_CURSOR;  /* old cursor was overwritten */
        }
	
#if !COPY_WHOLE_LINE
        /* memcpy(oldsrow+start_x,srow+start_x,len*2); */
        MEMCPY_2UNIX(oldsrow+start_x,srow+start_x,len*2);
#endif

    } while(x<co);
    
line_done:

    /* Increment the number of successfully updated lines counter */
    numdone++;
#if COPY_WHOLE_LINE
    /*memcpy(oldsrow, srow, co * 2);*/  /* Copy screen mem line to buffer */
    MEMCPY_2UNIX(oldsrow, srow, co * 2);  /* Copy screen mem line to buffer */
#endif

chk_cursor:
    /* update the cursor. We do this here to avoid the cursor 'running behind'
       when using a fast key-repeat.
    */
    if (y==cursor_row) X_update_cursor();
  }
  XFlush(dpy);

#if 0
  X_printf("X_update_screen: %d lines updated\n",numdone);
#endif

  if (numdone) {
     if (numscan==li)
        return 1;     /* changed, entire screen updated */
     else
        return 2;     /* changed, part of screen updated */
  }
  else {
     /* The updates look a bit cleaner when reset to top of the screen
      * if nothing had changed on the screen in this call to screen_restore
      */
     yloop = -1;
     return 0;
  }
}

void m_setpos(int x,int y) {
   /* XXX - this works in text mode only */
   x = x*8/font_width;
   y = y*8/font_height;
   if (x!=mouse.x || y!=mouse.y) {
      mouse.x=x; 
      mouse.y=y;
      mouse_move();
		mouse_event();
   }
}   

void m_setbuttons(int state) {
   mouse.oldlbutton=mouse.lbutton;
   mouse.oldmbutton=mouse.mbutton;
   mouse.oldrbutton=mouse.rbutton;
   mouse.lbutton = ((state & Button1Mask) != 0);
   mouse.mbutton = ((state & Button2Mask) != 0);
   mouse.rbutton = ((state & Button3Mask) != 0);
   if (mouse.lbutton!=mouse.oldlbutton) mouse_lb();
   if (mouse.mbutton!=mouse.oldmbutton) mouse_mb();
	if (mouse.rbutton!=mouse.oldrbutton) mouse_rb();
	mouse_event();
}

void X_handle_events()
{
   static int busy = 0;
   XEvent e;

   if (busy) return;
   busy=1;

   while (XPending(dpy) > 0) {
      XNextEvent(dpy,&e);

      switch(e.type) {
       case Expose:  
                     X_printf("X: expose event\n");
                     if (e.xexpose.count==0)   /* avoid redundant redraws */
                        X_redraw_screen();
                     break;

       case UnmapNotify:
                     X_printf("X: window unmapped\n");
                     is_mapped = 0;
                     break;

       case MapNotify:
                     X_printf("X: window mapped\n");
                     is_mapped = 1;
                     break;

       case FocusIn:
                     X_printf("X: focus in\n");
                     have_focus = 1;
                     X_redraw_cursor();
                     break;

       case FocusOut:
                     X_printf("X: focus out\n");
                     have_focus = 0;
		     blink_state = 1;
		     blink_count = config.X_blinkrate;
                     X_redraw_cursor();
                     break;

/* keyboard */

       case KeyPress:
       case KeyRelease:
                     X_process_key(&e.xkey);
                     break;

/* mouse... */

/* ok, the bit shifting below is not very clean, but it's somewhat simpler
   this way. It would only break if the button masks in X.h change, 
   which I don't expect to happen.
*/
       case ButtonPress:
                    m_setbuttons(e.xbutton.state|(0x80<<e.xbutton.button));
                    break;
                    
       case ButtonRelease:
                    m_setbuttons(e.xbutton.state&~(0x80<<e.xbutton.button));
		    break;
		    
       case MotionNotify:
	            m_setpos(e.xmotion.x,e.xmotion.y);
		    break;
		   
       case EnterNotify:
                    X_printf("X: mouse entering window\n");
	            m_setpos(e.xcrossing.x,e.xcrossing.y);
		    m_setbuttons(e.xcrossing.state);
		    break;
		    
       case LeaveNotify:      /* should this do anything? */
                    X_printf("X: mouse leaving window\n");
		    break;
		       
/* some weirder things... */
		     
       case DestroyNotify:
                    error("Oh, NO! My beautiful window got destroyed! :-(\n");
                    leavedos(99);
                    break;
                    
       case ClientMessage:
       /* if we get a client message which has the value of the delete 
        * atom, it means the window manager wants us to die.
        */

	            if (e.xclient.data.l[0] == delete_atom) {
      		        error("Got delete message! Goodbye, World...\n");
      		        leavedos(0);    /* XXX - is it ok to call this from a sigalrm handler ? */
                    }
                    break;

      }
   }
   busy=0;
}


struct video_system Video_X = {
   0,                /* is_mapped */
   X_init,         
   X_close,      
   X_setmode,      
   X_update_screen,
   X_update_cursor
};
