
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
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
GC gc;
Atom proto_atom = None, delete_atom = None;

static int font_width=8, font_height=16, font_shift=12;
int prev_cursor_row=-1, prev_cursor_col=-1;
ushort prev_cursor_shape=-1;

boolean have_focus=1, is_mapped=1;

/* from Xkeyb.c */
void X_process_key(XKeyEvent *);

struct {
   unsigned char r,g,b;
} crgb[16]={
{0,0,0},{10,10,185},{10,195,10},{20,160,160},
{167,10,10},{167,0,167},{165,165,40},{197,197,197},
{100,100,100},{10,10,255},{10,255,10},{10,255,255},
{255,10,10},{255,10,255},{255,255,0},{255,255,255}};

/* initialization is used just in case XAllocColor fails */
static int vga_colors[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

inline void get_vga_colors()
{
	int i;
        int cmap=DefaultColormap(dpy,scr);
        XColor xcol;

        X_printf("X: getting VGA colors\n");
        
	xcol.flags = DoRed | DoGreen | DoBlue;
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


int X_init() {
   XGCValues gcv;
   XSetWindowAttributes attr;
#if 0 /* Not used ... yet */
   XWMHints wmhints;
#endif
   

   X_printf("X_init()\n");
   
   dpy=XOpenDisplay(config.X_display);
   if (!dpy) {
      fprintf(stderr,"Can't open display %s\n",config.X_display);
      leavedos(99);
   }
   scr=DefaultScreen(dpy);
   root=RootWindow(dpy,scr);
   get_vga_colors();
   
   W = XCreateSimpleWindow(dpy,root,
                           0,0,                            /* position */
                           co*font_width,li*font_height,   /* size */
                           0,0,                            /* border */
                           vga_colors[0]                   /* background */
                           );
   
   attr.event_mask=KeyPressMask|KeyReleaseMask|
                   ButtonPressMask|ButtonReleaseMask|
                   EnterWindowMask|LeaveWindowMask|PointerMotionMask|
                   ExposureMask|StructureNotifyMask|FocusChangeMask;

   XChangeWindowAttributes(dpy,W,CWEventMask,&attr);

/*
   wmhints.icon_pixmap=...
   wmhints.icon_mask=...
   wmhints.flags = IconPixmapHint|IconMaskHint;
   XSetWMHints(dpy,W,&wmhints);
*/
   XStoreName(dpy,W,config.X_title);
   XSetIconName(dpy,W,config.X_icon_name);

/* Delete-Window-Message black magic copied from xloadimage */
   proto_atom  = XInternAtom(dpy,"WM_PROTOCOLS", False);
   delete_atom = XInternAtom(dpy,"WM_DELETE_WINDOW", False);
   if ((proto_atom != None) && (delete_atom != None))
      XChangeProperty(dpy, W, proto_atom, XA_ATOM, 32,
                      PropModePrepend, (char*)&delete_atom, 1);
                                    
   
   XMapWindow(dpy,W);

   gcv.foreground=1;
   gcv.background=0;
   gcv.font=XLoadFont(dpy,"vga");
   if (!gcv.font) {
      printf("ERROR: Could not find the vga font - did you run `xinstallvgafont' ?\n"
	     "Please read QuickStart and DOSEMU-HOWTO.* for more information.\n");
      leavedos(99);
   }

   gc=XCreateGC(dpy,W,GCForeground|GCBackground|GCFont,&gcv);

/*
   prev_cursor_row=prev_cursor_col=-1;
   prev_cursor_shape=NO_CURSOR;
*/

   X_printf("X_init() ok, dpy=%x scr=%d root=%d W=%d gc=%x\n",
            (int)dpy,(int)scr,(int)root,(int)W,(int)gc);
            
   return 0;                        
}

void X_close() {
   X_printf("X_close()\n");
   XDestroyWindow(dpy,W);
   XFreeGC(dpy,gc);
   XCloseDisplay(dpy);
}

int X_setmode(int type, int xsize, int ysize) {
   /* unimplemented... */
   return 0;
}

inline void X_setattr(byte attr) {
   XGCValues gcv;
   gcv.foreground=vga_colors[ATTR_FG(attr)];
   gcv.background=vga_colors[ATTR_BG(attr)];
   XChangeGC(dpy,gc,GCForeground|GCBackground,&gcv);
}

inline void X_draw_cursor(int x,int y) {

   X_setattr(ATTR(screen_adr+y*co+x));
   if (have_focus) {
      XFillRectangle(dpy,W,gc,
                     x*font_width, 
                     y*font_height+CURSOR_START(cursor_shape),
                     font_width, 
                     CURSOR_END(cursor_shape)-CURSOR_START(cursor_shape)+1);
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
      X_redraw_cursor();
   }
}


void X_scroll(int x,int y,int width,int height,int n,byte attr) {
  
  x*=font_width;
  y*=font_height;
  width*=font_width;
  height*=font_height;
  n*=font_height;
  
  X_setattr(attr);
  
  if (n>0) {       /* scroll up */
     height-=n;
     XCopyArea(dpy,W,W,gc,x,y+n,width,height,x,y);
     XClearArea(dpy,W,x,y+height,width,n,FALSE);
  }
  else if (n<0) {  /* scroll down */
     height+=n;
     XCopyArea(dpy,W,W,gc,x,y,width,height,x,y-n);
     XClearArea(dpy,W,x,y,width,-n,FALSE);
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
   
   X_printf("X_redraw_screen entered; CO=%d LI=%d screen_adr=%x\n",
            CO,LI,(int)screen_adr);

   sp=screen_adr;

   for(y=0;y<co;y++) {
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

   memcpy(prev_screen,screen_adr,co*li*2);
   
   X_printf("X_redraw_screen done\n");
}


#define Y_INC(y) if (++yloop==li) yloop=0

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
    if (!memcmp(srow, oldsrow, co * 2)) goto chk_cursor;
#endif

    x=0;
    do {
        /* find a non-matching character position */
        while (*sp==*oldsp) {
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
           if (*sp==*oldsp) {
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
        memcpy(oldsrow+start_x,srow+start_x,len*2);
#endif

    } while(x<co);
    
line_done:

    /* Increment the number of successfully updated lines counter */
    numdone++;
#if COPY_WHOLE_LINE
    memcpy(oldsrow, srow, co * 2);  /* Copy screen mem line to buffer */
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
#if 0
                     /*if (running<0) break; ??*/
                     running=-1;
#endif
                     if (e.xexpose.count==0)   /* pcemu does this check... what's it good for ?? */
                        X_redraw_screen();
#if 0
                     running=config.X_updatefreq;
#endif
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
#if 0
                    X_printf("X: mouse button %d pressed, state=%04x\n",
                             e.xbutton.button, e.xbutton.state);
#endif
                    m_setbuttons(e.xbutton.state|(0x80<<e.xbutton.button));
                    break;
                    
       case ButtonRelease:
#if 0
                    X_printf("X: mouse button %d released, state=%04x\n",
                             e.xbutton.button, e.xbutton.state);
#endif
                    m_setbuttons(e.xbutton.state&~(0x80<<e.xbutton.button));
		    break;
		    
       case MotionNotify:
#if 0
                    X_printf("X pointer motion\n");
#endif
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
