
#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include "emu.h"
#include "bios.h"
#include "memory.h"
#include "video.h"

/* "fine tuning" options for X_update_screen */

#define MAX_UNCHANGED	3
#define COPY_WHOLE_LINE	1
#define MOD_Y_INC 	0
#define PRECHECK_LINE   1

#define MY_WINDOW_TITLE "DOS in a BOX"
#define MY_ICON_NAME    MY_WINDOW_TITLE

/********************************************/

typedef unsigned char byte;

typedef struct { byte end, start; } cshape;

#define CHAR(w) (((char*)(w))[0])
#define ATTR(w) (((byte*)(w))[1])
#define ATTR_FG(attr) (attr & 0x0F)
#define ATTR_BG(attr) (attr >> 4)

#define CURSOR_START(c) ((int)((cshape*)&c)->start)
#define CURSOR_END(c)   ((int)((cshape*)&c)->end)
#define NO_CURSOR 0x0100

#define MAX_COLUMNS 132

/********************************************/

Display *dpy;
int scr;
Window root,W;
GC gc;

int font_height=16, font_width=8, font_shift=12;
int prev_cursor_row, prev_cursor_col;
us cursor_shape, prev_cursor_shape;

boolean have_focus=1;


struct {
   unsigned char r,g,b;
} crgb[16]={
{0,0,0},{10,10,185},{10,195,10},{20,160,160},
{167,10,10},{167,0,167},{165,165,40},{197,197,197},
{100,100,100},{10,10,255},{10,255,10},{10,255,255},
{255,10,10},{255,10,255},{255,255,0},{255,255,255}};

static int vga_colors[16];

void get_vga_colors()
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
	   XAllocColor(dpy, cmap, &xcol);
	   vga_colors[i] = xcol.pixel;
	}
}


int X_init() {
   XGCValues gcv;
   XSetWindowAttributes attr;
   
   X_printf("X_init()\n");
   
   dpy=XOpenDisplay(":0");
   if (!dpy) {
      fprintf(stderr,"Can't open display\n");
      return 1;
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
   
   attr.event_mask=KeyPressMask|ButtonPressMask|ExposureMask;
   XChangeWindowAttributes(dpy,W,CWEventMask,&attr);

   XStoreName(dpy,W,MY_WINDOW_TITLE);
   XSetIconName(dpy,W,MY_ICON_NAME);

   XMapWindow(dpy,W);

   gcv.foreground=1;
   gcv.background=0;
   gcv.font=XLoadFont(dpy,"vga");
   gc=XCreateGC(dpy,W,GCForeground|GCBackground|GCFont,&gcv);

   prev_cursor_row=prev_cursor_col=-1;
   prev_cursor_shape=NO_CURSOR;

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

inline void X_setattr(byte attr) {
   XGCValues gcv;
   gcv.foreground=vga_colors[ATTR_FG(attr)];
   gcv.background=vga_colors[ATTR_BG(attr)];
   XChangeGC(dpy,gc,GCForeground|GCBackground,&gcv);
}

inline void X_draw_cursor(us *sadr,int x,int y) {
   X_setattr(ATTR(sadr+y*co+x));

   if (CURSOR_END(cursor_shape)>=CURSOR_START(cursor_shape)) {
      (have_focus?XFillRectangle:XDrawRectangle)(dpy,W,gc,
              x*font_width, 
              y*font_height+CURSOR_START(cursor_shape),
              font_width, 
              CURSOR_END(cursor_shape)-CURSOR_START(cursor_shape)+1);
   }

}

inline void X_restore_cell(us *sadr,int x,int y) {
   us *sp = sadr+y*co+x;
   X_setattr(ATTR(sp));
   XDrawImageString(dpy,W,gc,font_width*x,font_height*y+font_shift,
                    (char*)sp,1);
}

inline void X_move_cursor(us *sadr,int from_x,int from_y,int to_x,int to_y) {

   X_printf("X_move_cursor: (%d,%d) -> (%d,%d)\n",from_x,from_y,to_x,to_y);

   X_restore_cell(sadr,from_x,from_y);
   X_draw_cursor(sadr,to_x,to_y);
}


void X_setcursorshape(us shape) {
   int cs,ce;
   cs=CURSOR_START(shape) & 0x0F;
   ce=CURSOR_END(shape) & 0x0F;

   if (shape & 0x2000 || cs>ce) {
      X_printf("X: no cursor\n");
      cursor_shape=NO_CURSOR;
      return;
   }

   if (ce>3 && ce<12) {
      if (cs>ce-3) cs+=font_height-ce-1;
      else if (cs>3) cs=font_height/2;
      ce=font_height-1;
   }
   X_printf("X: mapped cursor start=%d end=%d\n",(int)cs,(int)ce);
   CURSOR_START(cursor_shape)=cs;
   CURSOR_END(cursor_shape)=ce;
}

#undef cs
#undef ce


/* redraw the entire screen. Used for expose events etc. */
 
void X_redraw_screen(Window W,GC gc) 
{
   int x,y,start_x,curattr;
   us *screen=(us*)SCREEN_ADR(bios_current_screen_page);
   us *sp;
   char buff[MAX_COLUMNS];
   char *bp;

   X_printf("X_redraw_screen entered; CO=%d LI=%d screen=%x\n",
            CO,LI,(int)screen);

   for(y=0;y<LI;y++) {
      sp=&screen[y*CO];
      bp=buff;
      curattr=-1;
      for(x=0;x<=CO;x++) {
         *bp++=CHAR(sp);
         if (ATTR(sp)!=curattr || x==CO) {
            if (x!=0) {
               X_setattr(ATTR(sp));
               XDrawImageString(dpy,W,gc,
                        font_width*start_x,font_height*y,
                        buff,x-start_x);
            }
            curattr=ATTR(sp);
            bp=buff;
            start_x=x;
         }
         sp++;
      }
   }

   X_printf("X_redraw_screen done\n");
}


#if MOD_Y_INC
#define Y_INC(y) yloop = (yloop + 1) % li
#else
#define Y_INC(y) if (++yloop==li) yloop=0
#endif

int yloop, prev_cursor_row, prev_cursor_col;
us prev_cursor_shape;


/* this is derived from ansi_update but heavily modified 
*/

int X_update_screen()
{
  us *sadr;	/* Ptr to start of screen memory of curr page */
  us *srow, *oldsrow, *sp, *oldsp;
  char *bp;
  char charbuff[MAX_COLUMNS];

  int x, y;	/* X and Y position of character being updated */
  int start_x, len, unchanged;
  byte attr;

  int lines;    /* Number of lines to redraw */
  int numscan;	/* counter for number of lines scanned */
  int numdone;  /* counter for number of lines actually updated */

  sadr = SCREEN_ADR(bios_current_screen_page);

  /* The following determines how many lines it should scan at once,
   * since this routine is being called by sig_alrm.  If the entire
   * screen changes, it often incurs considerable delay when this
   * routine updates the entire screen.  So the variable "lines"
   * contains the maximum number of lines to update at once in one
   * call to this routine.  This is set by the "updatelines" keyword
   * in /etc/dosemu.conf 
   */
  lines = config.term_updatelines;
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
  
  X_printf("X_update_screen: co=%d li=%d yloop=%d y=%d lines=%d\n",
           co,li,yloop,y,lines);

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

    sp = srow = sadr + y*co;
    oldsp = oldsrow = (us*)scrbuf + y*co;

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
#if 0
           *bp++=CHAR(sp);
#else
	   { char c = CHAR(sp);
             *bp++ = c?c:' ';     /* kludge for incorrect ASCII 0 char in vga font */
           }
#endif
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
   if (y==cursor_row) {
      if (cursor_row!=prev_cursor_row || cursor_col!=prev_cursor_col ||
          cursor_shape!=prev_cursor_shape)
      {
#if 1
         X_move_cursor(sadr,prev_cursor_col,prev_cursor_row,
                            cursor_col,cursor_row);
#else
         if (prev_cursor_shape!=NO_CURSOR)
            X_restore_cell(sadr,prev_cursor_col,prev_cursor_row);
         if (cursor_shape!=NO_CURSOR)
            X_draw_cursor(sadr,cursor_col,cursor_row);
#endif
         prev_cursor_row=cursor_row;
         prev_cursor_col=cursor_col;
         prev_cursor_shape=cursor_shape;
      }
    }
  }
  XFlush(dpy);

  X_printf("X_update_screen: %d lines updated\n",numdone);

  if (!numdone) {
    /* The updates look a bit cleaner when reset to top of the screen
     * if nothing had changed on the screen in this call to screen_restore
     */
    yloop = -1;
  }

  return numdone;
}

#if 0
void X_handler() 
{
   XEvent e;

   XNextEvent(dpy,&e);

   switch(e.type) {
    case Expose:  X_redraw_screen();
                  break;

    case ButtonPress: 
                  break;
   }
}
#endif

