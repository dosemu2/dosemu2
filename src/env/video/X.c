/* 
 * DANG_BEGIN_MODULE
 *
 * This module contains a text-mode video interface for the X Window 
 * System. It has mouse and selection 'cut' support.
 *
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * $Id: X.c,v 1.13 1995/05/06 16:25:24 root Exp root $
 *
 * 951205: bon@elektron.ikp.physik.th-darmstadt.de
 *   Merged in again the selection stuff. Hope introduce hidden bugs 
 *        are removed!  
 *
 *         TODO: highlighting should be smarter and either discard area
 *               or follow ir when scrolling (text-mode
 *
 * 5/24/95, Started to get the graphics modes to work
 * Erik Mouw (J.A.K.Mouw@et.tudelft.nl) 
 * and Arjan Filius (I.A.Filius@et.tudelft.nl)
 *
 * changed:
 *  X_setmode()
 *  X_redraw_screen()
 *  X_update_screen()
 *  set_mouse_position()
 * in video/X.c
 *
 * and:
 *  set_video_mode()
 * in video/int10.c
 * int10()
 *
 * added:
 *  get_vga256_colors()
 *
 * Now dosemu can work in graphics mode with X, but at the moment only in mode
 * 0x13 and very slow.
 *
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
 * 1996/04/29: Added the ability to use MIT-SHM X extensions to speed
 * up graphics handling... compile-time option.  Typically halves system
 * overhead for local graphics. -- Adam D. Moss (aspirin@tigerden.com) (adm)
 *
 * 1996/05/03: Graphics are MUCH faster now due to changes in vgaemu.c. --adm
 *
 * 1996/05/05: Re-architected the process of delivering changes from
 * vgaemu.c to the client (X.c).  This will allow further optimization
 * in the future (I hope). --adm
 *
 * 1996/05/06: Began work on shared-colourmap and mismatched-colourdepth
 * graphics support. --adm
 *
 * 1996/05/20: More work on colourmap, bugfixes, speedups, and better
 * mouse support in graphics modes. --adm
 *
 * DANG_END_CHANGELOG
 */

/* The different methods you can use to remap to a shared colourmap */
#define REMAP_TYPE_FAST   0  /* Fastest; simple remap, often looks poor */
#define REMAP_TYPE_MEDIUM 1  /* Slower, does simple dithering */
#define REMAP_TYPE_SLOW   2  /* Slower, does slightly better dithering */
#define REMAP_TYPE_SLOWER 3  /* Slowest, does pretty good dithering */

/* So, what sort of shared colourmap support would you like? */
#define REMAP_TYPE      REMAP_TYPE_MEDIUM     /* Default: REMAP_TYPE_MEDIUM    --adm */

/*#define OLD_X_TEXT*/		/* do you want to use the old X_textmode's */
#define CONFIG_X_SELECTION 1
#define CONFIG_X_MOUSE 1
#define DOSEMU060 1
#define DOSEMU061 0

/* Comment out if you wish to have a visual indication of the area
   being updated */
/* #define DEBUG_SHOW_UPDATE_AREA yeah */

#include <config.h>
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <sys/mman.h>           /* root@sjoerd:for mprotect*/


#ifdef HAVE_MITSHM
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif


#include <sys/time.h>
#include <unistd.h>


#if DOSEMU060
#include "emu.h"
#include "bios.h"
#include "video.h"
#include "memory.h"
#if CONFIG_X_MOUSE
#include "mouse.h"
#include <time.h>
#endif
#if CONFIG_X_SELECTION
#include "screen.h"
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

#include "vgaemu.h"
#include "vgaemu_inside.h"

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

/* "Fine tuning" options for X_update_screen */
#define MAX_UNCHANGED	3

/* Kludge for incorrect ASCII 0 char in vga font. */
#define XCHAR(w) ((u_char)CHAR(w)?(u_char)CHAR(w):(u_char)' ')


#if CONFIG_X_SELECTION
#define SEL_ACTIVE(w) (visible_selection && ((w) >= sel_start) && ((w) <= sel_end))
#define SEL_ATTR(attr) (((attr) >> 4) ? (attr) :(((attr) & 0x0f) << 4))
#define XATTR(w) (SEL_ACTIVE(w) ? SEL_ATTR(ATTR(w)) : ATTR(w))
#else
#define XATTR(w) (ATTR(w))
#endif
#define XREAD_WORD(w) ((XATTR(w)<<8)|XCHAR(w))

/********************************************/

#ifdef HAVE_MITSHM
static XShmSegmentInfo shminfo;
#endif

static Display *display;
static int screen;
static Window rootwindow,mainwindow;
static XImage *ximage_p;		/*Used as a buffer for the X-server*/

static XColor xcol;

unsigned char* image_data_p=NULL; 	/* the data in the XImage*/

static Cursor X_standard_cursor;
#if CONFIG_X_MOUSE
static Cursor X_mouse_cursor;
static time_t X_mouse_change_time;
static int X_mouse_last_on;
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

static Boolean have_shmap = FALSE;

static unsigned long vga_colors[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};

static void set_sizehints(int xsize, int ysize);
static Colormap text_cmap=0;
static Colormap vga256_cmap=0;

static int X_setmode(int type, int xsize, int ysize) ;

static unsigned char lut[256]; /* lookup table for remapping colours */
static unsigned char lut2[256]; /* another one... I like them. */
static unsigned long pixelval[256]; /* ...and another one.  Trust me. --adm */
unsigned char redshades,blueshades,greenshades;

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
static struct
  {
   unsigned char r,g,b;
  } crgb[16]=
    {
{0x00,0x00,0x00},{0x18,0x18,0xB2},{0x18,0xB2,0x18},{0x18,0xB2,0xB2},
{0xB2,0x18,0x18},{0xB2,0x18,0xB2},{0xB2,0x68,0x18},{0xB2,0xB2,0xB2},
{0x68,0x68,0x68},{0x54,0x54,0xFF},{0x54,0xFF,0x54},{0x54,0xFF,0xFF},
    {0xFF,0x54,0x54},{0xFF,0x54,0xFF},{0xFF,0xFF,0x54},{0xFF,0xFF,0xFF}
  };

	int i;
        XColor xcol2;
Colormap text_cmap = DefaultColormap(display, screen);

X_printf("X: getting VGA colors\n");

xcol.flags=DoRed | DoGreen | DoBlue;  /* Since we're here, we may
				       as well set this up since it
				       never changes... */

for(i=0;i<16;i++) 
  {
	   xcol2.red   = (crgb[i].r*65535)/255;
	   xcol2.green = (crgb[i].g*65535)/255;
	   xcol2.blue  = (crgb[i].b*65535)/255;
    if (!XAllocColor(display, text_cmap, &xcol2)) 
      {
	error("X: couldn't allocate all VGA colors!\n");
	      return;
	   }
	   vga_colors[i] = xcol2.pixel;
	}
}


static int try_cube(unsigned char redsh,
		     unsigned char greensh,
		     unsigned char bluesh)
{
  int ir,ig,ib,ii;
  int got_okay[256];

  for (ii=0;ii<256;ii++)
    {
      got_okay[ii]=0;
    }

  for (ir=0;ir<redsh;ir++)
    {
      for (ig=0;ig<greensh;ig++)
	{
	  for (ib=0;ib<bluesh;ib++)
	    {
	      xcol.red=(ir*65535)/(redsh-1);
	      xcol.green=(ig*65535)/(greensh-1);
	      xcol.blue=(ib*65535)/(bluesh-1);
	      if (XAllocColor(display, vga256_cmap, &xcol))
		{
		  got_okay[ii=(
			       (ir) +
			       (ig)*redsh +
			       (ib)*redsh*greensh
			       )]=1;
		  pixelval[ii] = xcol.pixel;
		}
	      else
		{
		  for (ii=0;ii<256;ii++)
		    {
		      if (got_okay[ii])
			XFreeColors(display, vga256_cmap, &pixelval[ii], 1, 0);
		    }
		  return(0);
		}
	    }
	}
    }

  redshades=redsh;
  greenshades=greensh;
  blueshades=bluesh;

  return(1); /* got cube successfully */
}

  
/* DANG_BEGIN_FUNCTION get_vga256_colors
 *
 * Allocates a colormap for 256 color modes and initializes it.
 *
 * DANG_END_FUNCTION
 */
static void get_vga256_colors(void)
{
  int i;
  Visual *visual;
  DAC_entry color;

  DAC_init();

  /* Note: should really deallocate and reallocate our colours every time
   we switch graphic modes... --adm */

  if(vga256_cmap==0) /* only the first time! */
    {
      visual=DefaultVisual(display, screen);

      v_printf(config.X_sharecmap==0?"Private Colourmap.\n":"Shared Colourmap.\n");
      
      have_shmap=FALSE;
      if (config.X_sharecmap)
	{
	  vga256_cmap=DefaultColormap(display,screen);
	  
	  if (!try_cube(6,8,5))
	    if (!try_cube(6,7,5))
	      if (!try_cube(6,6,5))
		if (!try_cube(5,7,5))
		  if (!try_cube(5,6,5))
		    /* XV-3's normal colour-cube: */
		    if (!try_cube(4,8,4))
		      if (!try_cube(5,6,4))
			if (!try_cube(5,5,4))
			  if (!try_cube(4,5,4))
			    {
			      printf("VGAEMU: Warning: Couldn't get many free colours.  May look bad.\n");
			      if (!try_cube(4,5,3))
				if (!try_cube(4,4,3))
				  if (!try_cube(3,4,3))
				    {
				      printf("VGAEMU: Warning: Couldn't get a semi-decent number of free colours\n         - using private colourmap.\n");
				      have_shmap=FALSE;
				      greenshades=blueshades=redshades=0;
				    }
			    }
	  if (blueshades>0)
	    {
	      have_shmap=TRUE;
	      v_printf("VGAEMU: Using %dx%dx%d colour-cube.\n",redshades,greenshades,blueshades);
	    }
	}
      if (!have_shmap)
	{
	  vga256_cmap=XCreateColormap(display, rootwindow, visual, AllocAll);
	  for(i=0; i<256; i++)
	    {
	      DAC_get_entry(&color, (unsigned char)i);
	      xcol.pixel=i;
	      xcol.red  =(color.r*65535)/63;
	      xcol.green=(color.g*65535)/63;
	      xcol.blue =(color.b*65535)/63;
	      
	      XStoreColor(display, vga256_cmap, &xcol);
	    }
	}
    }
  else
    {  /* should not be needed, root@sjoerd*/
      /* seems to be called on a ctrl-alt-del  :(  --adm */
      printf("VGAEMU: Palette change should not be needed!!\n");
      for(i=0; i<256; i++)
	{
	  DAC_get_entry(&color, (unsigned char)i);
	  xcol.pixel=i;
	  xcol.red  =color.r<<10;
	  xcol.green=color.g<<10;
	  xcol.blue =color.b<<10;

	  XStoreColor(display, vga256_cmap, &xcol);
	}
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
    if ((font=XLoadQueryFont(display, *p)) == NULL) {
      error("X: Unable to open font \"%s\"", *p);
    }
    else if (font->min_bounds.width != font->max_bounds.width) {
      error("X: Font \"%s\" isn't monospaced", *p);
      XFreeFont(display, font);
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
  cmap = DefaultColormap(display, screen);
  XParseColor(display, cmap, "white", &fg);
  XParseColor(display, cmap, "black", &bg);

  cfont = XLoadFont(display, "cursor");
  if (!cfont) {
    error("X: Unable to open font \"cursor\", aborting!\n");
    leavedos(99);
  }
#if CONFIG_X_SELECTION
/*  X_standard_cursor = XCreateGlyphCursor(display, cfont, cfont, 
    XC_xterm, XC_xterm+1, &fg, &bg);
*/
#endif
#if CONFIG_X_MOUSE
  X_standard_cursor = XCreateGlyphCursor(display, cfont, cfont, 
    XC_top_left_arrow, XC_top_left_arrow+1, &fg, &bg);
  /* IMHO, the DEC cursor font looks nicer, but if it's not there, 
   * use the standard cursor font. 
   */
  decfont = XLoadFont(display, "decw$cursor");
  if (!decfont) {
    X_mouse_cursor = XCreateGlyphCursor(display, cfont, cfont, 
      XC_hand2, XC_hand2+1, &bg, &fg);
  } 
  else {
    X_mouse_cursor = XCreateGlyphCursor(display, decfont, decfont, 2, 3, &fg, &bg);
    XUnloadFont(display, decfont);  
  }
  X_mouse_change_time = 0;
  X_mouse_last_on = -1;
#endif /* CONFIG_X_MOUSE */
  XUnloadFont(display, cfont);
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
  set_video_bios_size();		/* make it stick */
#endif  
  
  /* Open X connection. */
  display=XOpenDisplay(config.X_display);
  if (!display) 
    {
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
   
  screen=DefaultScreen(display);
  rootwindow=RootWindow(display,screen);
  get_vga_colors();
  get_vga256_colors();
  load_text_font();

  mainwindow = XCreateSimpleWindow(display, rootwindow,
    0, 0,                               /* Position */
    co*font_width, li*font_height,      /* Size */
    0, 0,                               /* Border */
    vga_colors[0]);                     /* Background */
   set_sizehints(co,li);
  load_cursor_shapes();
  gcv.font = vga_font;
  gc = XCreateGC(display, mainwindow, GCFont, &gcv);
  attr.event_mask=KeyPressMask|KeyReleaseMask|KeymapStateMask|
                   ButtonPressMask|ButtonReleaseMask|
                   EnterWindowMask|LeaveWindowMask|PointerMotionMask|
	          ExposureMask|StructureNotifyMask|FocusChangeMask|
	          ColormapChangeMask;
  attr.cursor = X_standard_cursor;
      
  XChangeWindowAttributes(display,mainwindow,CWEventMask|CWCursor,&attr);

  XStoreName(display,mainwindow,config.X_title);
  XSetIconName(display,mainwindow,config.X_icon_name);
  xch.res_name  = "XDosEmu";
  xch.res_class = "XDosEmu";
  XSetClassHint(display,mainwindow,&xch);

#if CONFIG_X_SELECTION
  /* Get atom for COMPOUND_TEXT type. */
  compound_text_atom = XInternAtom(display, "COMPOUND_TEXT", False);
#endif
  /* Delete-Window-Message black magic copied from xloadimage. */
  proto_atom  = XInternAtom(display,"WM_PROTOCOLS", False);
  delete_atom = XInternAtom(display,"WM_DELETE_WINDOW", False);
  if ((proto_atom != None) && (delete_atom != None)) {
    XChangeProperty(display, mainwindow, proto_atom, XA_ATOM, 32,
                      PropModePrepend, (char*)&delete_atom, 1);
  }
                                    
  XMapWindow(display,mainwindow);
  X_printf("X: X_init done, screen=%d, root=0x%lx, mainwindow=0x%lx\n",
    screen, (unsigned long) rootwindow, (unsigned long) mainwindow);
        
  /* allocate video memory */
    if(vga_emu_init()==NULL)
      {
	error("X: couldn't allocate video memory!\n");
	leavedos(99);
      }
  
  /* HACK!!! Don't konw is video_mode is already set */
  X_setmode(TEXT, co, li);
            
  return(0);                        
}


/*
 * DANG_BEGIN_FUNCTION X_close
 *
 * description:
 *  Destroys the window, unloads font, pixmap and colormap.
 *
 * DANG_END_FUNCTION
 */
static void X_close(void) 
{
  X_printf("X_close()\n");
  if (display==NULL)
    return;

  XUnloadFont(display,vga_font);
  XDestroyWindow(display,mainwindow);

  if(ximage_p!=NULL)
    {
#ifdef HAVE_MITSHM
      XShmDetach(display, &shminfo);
#endif
      XDestroyImage(ximage_p);	/* calls ximage_p->destroy_image() */
      ximage_p=NULL;
#ifdef HAVE_MITSHM
      shmdt(shminfo.shmaddr);
#endif
    }
  /*free(vga_video_mem);*/
  /* Uninstall vgaemu!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
  
  
  
  
  if(vga256_cmap!=0)
    {
      if(video_mode==0x13)
        XUninstallColormap(display, vga256_cmap);

      XFreeColormap(display, vga256_cmap);
    }

  XFreeGC(display,gc);
  XCloseDisplay(display);
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
  XSetWMNormalHints(display, mainwindow, &sh);
}

/* 
 * DANG_BEGIN_FUNCTION X_setmode
 *
 * description:
 *  Resizes the window, also the graphical sizes/video modes.
 *  remember the dos videomodi
 *
 * DANG_END_FUNCTION
 */
static int X_setmode(int type, int xsize, int ysize) 
{
  static int previous_video_type=TEXT;
  XSetWindowAttributes win_attr;
  XSizeHints sh;
  Window r;
  int x, y;
  unsigned int width, heigth, border_width, depth;
  
  /* tell vgaemu we're going to another mode */
  if(!set_vgaemu_mode(video_mode, xsize, ysize))
    return 0;  /* if it can't fail! */
                                
  X_printf("X_setmode() type=%d, xsize=%d, ysize=%d info: x=%d y=%d\n",
         type,xsize,ysize,get_vgaemu_width(),get_vgaemu_heigth());
  
  X_printf("X_setmode(), video_mode = 0x%02x\n",video_mode);
  
  switch(previous_video_type)
    {
    case GRAPH:
      win_attr.colormap=text_cmap;
      XChangeWindowAttributes(display, mainwindow, CWColormap, &win_attr);
      if(vga256_cmap!=0)
        XUninstallColormap(display, vga256_cmap);
      
      if(ximage_p!=NULL)
	{
#ifdef HAVE_MITSHM
	  XShmDetach(display, &shminfo);
#endif
	  XDestroyImage(ximage_p);	/* just make a new one all time */
	  /* image_data_p is also >/dev/0 */	
	  ximage_p=NULL;
#ifdef HAVE_MITSHM
	  shmdt(shminfo.shmaddr);
#endif
	}
      break;

    case TEXT:
      if(ximage_p!=NULL)
	{
#ifdef HAVE_MITSHM
	  XShmDetach(display, &shminfo);
#endif
	  XDestroyImage(ximage_p);        /* just make a new one all time */
	  /* image_data_p is also >/dev/0 */
	  ximage_p=NULL;
#ifdef HAVE_MITSHM
	  shmdt(shminfo.shmaddr);
#endif
	}
      break;
                                                     
    default:
      break;
   }

  previous_video_type=type;

  switch(type)
    {
    case TEXT:
      XSetWindowColormap(display, mainwindow, text_cmap);
          
      X_printf("X: Going to textmode...\n");
      XResizeWindow(display,mainwindow,
	            get_vgaemu_tekens_x()*font_width,
	            get_vgaemu_tekens_y()*font_height);

      /*setsizehints()*/
      /* We use the X font! Own font in future? */
      sh.max_width=get_vgaemu_tekens_x()*font_width;
      sh.max_height=get_vgaemu_tekens_y()*font_height;
      sh.width_inc=1;		/* ??? */
      sh.height_inc=1;
      sh.flags = PMaxSize|PResizeInc;
      XSetNormalHints(display, mainwindow, &sh);

      XGetGeometry(display, mainwindow, &r, &x, &y, &width, &heigth,
		   &border_width, &depth);

      /* I don't think this is OK for monochrome X servers */
      /* It should be OK for 256 color X servers (depth=8bits) */

#ifdef HAVE_MITSHM
      ximage_p=XShmCreateImage(display,
			       DefaultVisual(display, DefaultScreen(display)),
			       depth, ZPixmap, NULL, 
			       &shminfo,
			       get_vgaemu_tekens_x()*font_width,
			       get_vgaemu_tekens_y()*font_height);
      if (ximage_p==NULL)
	{
	  fprintf(stderr, "Warning: XShmCreateImage() failed\n");
	}
      shminfo.shmid = shmget(IPC_PRIVATE, 
			     get_vgaemu_tekens_x()*get_vgaemu_tekens_y()*font_width*font_height,
			     IPC_CREAT | 0777);
      if (shminfo.shmid<0)
	{
	  perror("shmget");
	  XDestroyImage(ximage_p);
	  fprintf(stderr, "Warning: shmget() failed\n");
	}
      shminfo.shmaddr = (char *) shmat(shminfo.shmid, 0, 0);
      if (shminfo.shmaddr==((char *) -1))
	{
	  XDestroyImage(ximage_p);
	  fprintf(stderr, "Warning: shmat() failed\n");
	}
      shminfo.readOnly = False;
      XShmAttach(display, &shminfo);
      shmctl(shminfo.shmid, IPC_RMID, 0 );
      ximage_p->data = shminfo.shmaddr;
      XSync(display, False);
#else
      ximage_p=XCreateImage(display,
                            DefaultVisual(display, DefaultScreen(display)),
                            depth, ZPixmap, 0, 
                            (unsigned char*)malloc(get_vgaemu_tekens_x()*get_vgaemu_tekens_y()*font_width*font_height),
                                get_vgaemu_tekens_x()*font_width,
                                get_vgaemu_tekens_y()*font_height, 8, 0);
#endif
      /*      memset((void *)ximage_p->data, 0, get_vgaemu_tekens_x()*get_vgaemu_tekens_y()*font_width*font_height);*/
      break;
                  

    case GRAPH:
      XSetWindowColormap(display, mainwindow, vga256_cmap);

      X_printf("X: Going to graphics mode with fingers crossed...\n");
      XResizeWindow(display,mainwindow,
	            get_vgaemu_width(), get_vgaemu_heigth());

      /*setsizehints()*/
      sh.max_width=get_vgaemu_width();
      sh.max_height=get_vgaemu_heigth();
      sh.width_inc=1;
      sh.height_inc=1;
      sh.flags = PMaxSize|PResizeInc;
      XSetNormalHints(display,mainwindow,&sh);

      XGetGeometry(display,mainwindow, &r, &x, &y, &width,& heigth,
		   &border_width, &depth);
      
#ifdef HAVE_MITSHM
      ximage_p=XShmCreateImage(display,
			       DefaultVisual(display,DefaultScreen(display)),
			       depth,ZPixmap,NULL,
			       &shminfo,
			       get_vgaemu_width(),
			       get_vgaemu_heigth());
      if (ximage_p==NULL)
	{
	  fprintf(stderr, "Warning: XShmCreateImage() failed\n");
	}
      shminfo.shmid = shmget(IPC_PRIVATE, 
			     get_vgaemu_width()*get_vgaemu_heigth(),
			     IPC_CREAT | 0777);
      if (shminfo.shmid<0)
	{
	  perror("shmget");
	  XDestroyImage(ximage_p);
	  fprintf(stderr, "Warning: shmget() failed\n");
	}
      shminfo.shmaddr = (char *) shmat(shminfo.shmid, 0, 0);
      if (shminfo.shmaddr==((char *) -1))
	{
	  XDestroyImage(ximage_p);
	  fprintf(stderr, "Warning: shmat() failed\n");
	}
      shminfo.readOnly = False;
      XShmAttach(display, &shminfo);
      shmctl(shminfo.shmid, IPC_RMID, 0 );
      ximage_p->data = shminfo.shmaddr;
      XSync(display, False);
#else
      ximage_p=XCreateImage(display,DefaultVisual(display,DefaultScreen(display)),
                            depth,ZPixmap,0,(unsigned char*)malloc(get_vgaemu_width()*get_vgaemu_heigth()),
                            get_vgaemu_width(),get_vgaemu_heigth(),8,0  );
#endif
      /*      memset((void *)ximage_p->data, 0, get_vgaemu_width()*get_vgaemu_heigth());*/
      /* set colormap */
      /*get_vga256_colors();*/
      break;
  
    default:
      X_printf("X_setmode: No valid mode type (TEXT or GRAPH)\n");
      return 0;
      break;
    }

  return 1;
}



/* Change color values in our graphics context 'gc'. */
static inline void set_gc_attr(Bit8u attr)

{
   XGCValues gcv;
   gcv.foreground=vga_colors[ATTR_FG(attr)];
   gcv.background=vga_colors[ATTR_BG(attr)];
  XChangeGC(display,gc,GCForeground|GCBackground,&gcv);
}

/*
 * DANG_BEGIN_FUNCTION X_change_mouse_cursor(void)
 *   
 * description:
 * This function seems to be called each screen_update :(
 * It is called in base/mouse/mouse.c:mouse_cursor(int) a lot for show and 
 * hide.
 * DANG_END_FUNCTION
 *
 * DANG_FIXME Use a delayline, or something smarter in X_change_mouse_cursor
 *  
 */  

/**************************************************************************/
/*                              CURSOR                                    */
/**************************************************************************/
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
/* Draw the cursor (normal if we have focus, rectangle otherwise). */
static void draw_cursor(int x, int y)
{  
  if(video_mode==0x13)
    return;

  set_gc_attr(XATTR(screen_adr+y*co+x));
  if (!have_focus) {
    XDrawRectangle(display, mainwindow, gc,
      x*font_width, y*font_height,
      font_width-1, font_height-1);
  }
  else if (blink_state) {
    int cstart = CURSOR_START(cursor_shape) * font_height / 16,
      cend = CURSOR_END(cursor_shape) * font_height / 16;
      XFillRectangle(display,mainwindow,gc,
                     x*font_width, y*font_height+cstart,
                     font_width, cend-cstart+1);
   }
}



/* Restore a character cell (i.e., remove the cursor). */
static inline void restore_cell(int x, int y)
{
  Bit16u *sp = screen_adr+y*co+x, *oldsp = prev_screen+y*co+x;
   u_char c = XCHAR(sp);

  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(video_mode==0x13)
    return;

  set_gc_attr(XATTR(sp));
  *oldsp = XREAD_WORD(sp);
  XDrawImageString(display, mainwindow, gc, x*font_width, 
		   y*font_height+font_shift, &c, 1);
}

  
/* Move cursor to a new position (and erase the old cursor). */
static void redraw_cursor(void) 
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(video_mode==0x13)
    return;

  if (!is_mapped) 
    return;

  if (prev_cursor_shape!=NO_CURSOR)
    restore_cell(prev_cursor_col, prev_cursor_row);
   if (cursor_shape!=NO_CURSOR)
    draw_cursor(cursor_col, cursor_row);

  XFlush(display);

   prev_cursor_row=cursor_row;
   prev_cursor_col=cursor_col;
   prev_cursor_shape=cursor_shape;
}


/* Redraw the cursor if it's necessary. */
static void X_update_cursor(void) 
   {
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(video_mode==0x13)
    return;
   
  if ((cursor_row != prev_cursor_row) || (cursor_col != prev_cursor_col) ||
    (cursor_shape != prev_cursor_shape))
  {
    redraw_cursor();
   }
}

/* Blink the cursor. This is called from SIGALRM handler. */
void X_blink_cursor(void)
{
  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(video_mode==0x13)
    return;

  if (!have_focus || --blink_count)
      return;

   blink_count = config.X_blinkrate;
   blink_state = !blink_state;
  if (cursor_shape!=NO_CURSOR) 
    {
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

 
/* 
 * DANG_BEGIN_FUNCTION X_redraw_screen
 *
 * description:
 *  Redraws the entire screen, also in graphics mode
 *  Used for expose events etc.
 *
 * returns: 
 *  nothing
 *
 * arguments: 
 *  none
 *
 * DANG_END_FUNCTION
 */ 

/*
 * DANG_FIXME Graphics in X has to be improved a lot
 */

/**************************************************************************/
/*                          SCREEN UPDATES                                */
/**************************************************************************/

/* Redraw the entire screen. Used for expose events. */
static void X_redraw_screen(void)
{
  Bit16u *sp, *oldsp;
  u_char charbuff[MAX_COLUMNS], *bp;
  int x, y, start_x;
  Bit8u attr;

  if (!is_mapped)
    return;
  X_printf("X_redraw_screen entered; co=%d li=%d screen_adr=%x mode=%d \n",
            co,li,(int)screen_adr,video_mode);
   sp=screen_adr;
  oldsp = prev_screen;

  switch(video_mode)
    {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
      for(y = 0; (y < li); y++) 
	{
	  x=0;
	  do 
	    {
	      /* scan in a string of chars of the same attribute. */
	      
	      bp=charbuff;
	      start_x=x;
	      attr=XATTR(sp);
	      
	      do 
		{			/*Conversion of string to X*/
		  *oldsp++ = XREAD_WORD(sp);
		  *bp++=XCHAR(sp);
		  sp++; 
		  x++;
		} 
	      while((XATTR(sp)==attr) && (x<co));
	      
	      /* ok, we've got the string now send it to the X server */
	      
	      set_gc_attr(attr);
	      XDrawImageString(display,mainwindow,gc,
			       font_width*start_x,font_height*y+font_shift,
			       charbuff,x-start_x);
	      
	    } 
	  while(x<co);
	}
      break;

#ifdef 0 /* wordt eigenlijk niet meer gebruikt */
    case 0x13:	/* graphics 320x200 256*/
      {
        unsigned int counter;
	DAC_entry color;
	int i;
        X_printf("In total redraw!\n");

	/* first update the colormap */
	while ((i=DAC_get_dirty_entry(&color))!=-1)
	  {
	    X_printf("X: X_redraw_screen(): DAC_get_dirty_entry()=%i\n", i);
	    xcol.pixel=i;
	    xcol.red  =color.r<<10;
	    xcol.green=color.g<<10;
	    xcol.blue =color.b<<10;

	    XStoreColor(display, vga256_cmap, &xcol);
	  }
	while(i!=-1);

    	  {
            int x, y, left=320, top=200, right=0, bottom=0;
            unsigned int changed=0;
            unsigned char *real_vid_mem_p;
            unsigned char *image_data_p=NULL;
            
            for(y=0, counter=0; y<200; y++)
              {
              real_vid_mem_p=(unsigned char*)(0xA0000+(0+320*(y)));
              image_data_p=(unsigned char*)(ximage_p->data+(0+320*(y)));
              for(x=0; x<320; x++)
                {
		  XPutPixel(ximage_p,x,y,*real_vid_mem_p++);
                }
   }
  	  XPutImage(display,mainwindow,gc,ximage_p,0,0,0,0,320,200);
          }
      }
      break;
#endif
     
    default:
      X_printf("X_redraw_screen:No valid video mode\n");
      break;
    }

  prev_cursor_shape = NO_CURSOR;   /* was overwritten */
  redraw_cursor();
  clear_scroll_queue();
   
  XFlush(display);

  /* memcpy(prev_screen,screen_adr,co*li*2); */
  MEMCPY_2UNIX(prev_screen,screen_adr,co*li*2); /* what?????????????????????*/
  clear_scroll_queue();

  X_printf("X_redraw_screen done\n");
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
  
  XSetForeground(display,gc,vga_colors[attr>>4]);
  
  if (n > 0) 
    {       /* scroll up */
      if (n>=height) 
	{
        n=height;
     }
      else 
	{
        height-=n;
	  XCopyArea(display,mainwindow,mainwindow,gc,x,y+n,width,height,x,y);
     }
     /*
      XFillRectangle(display,mainwindow,gc,x,y+height,width,n);
     */
  }
  else if (n < 0) 
    {  /* scroll down */
      if (-n>=height) 
	{
        n=-height;
     }
      else 
	{
        height+=n;
	  XCopyArea(display,mainwindow,mainwindow,gc,x,y,width,height,x,y-n);
     }
     /*
      XFillRectangle(display,mainwindow,gc,x,y,width,-n);
     */
  }
}


/* Process the scroll queue */
static void do_scroll(void) 
{
   struct scroll_entry *s;

   X_printf("X: do_scroll\n");
  while(s=get_scroll_queue()) 
    {
      if (s->n!=0) 
	{
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

/* 
 * DANG_BEGIN_FUNCTION X_update_screen
 *
 * description:
 *  Updates, also in graphics mode
 *  Graphics in X has to be smarter and improved
 *  
 * returns: 
 *  0 - nothing updated 
 *  2 - partly updated 
 *  1 - whole update
 *
 * arguments: 
 *  none
 *
 * DANG_END_FUNCTION
 */ 

static int X_update_screen(void)
/*called from ../dosemu/signal.c:SIGALRM_call() */
{
  switch(get_vgaemu_type())
    {
    case TEXT:
      /* Do it the OLD way! */
      {

	Bit16u *sp, *oldsp;
	u_char charbuff[MAX_COLUMNS], *bp;
	int x, y;	/* X and Y position of character being updated */
	int start_x, len, unchanged;
	Bit8u attr;

	static int yloop = -1;
	int lines;               /* Number of lines to redraw. */
	int numscan = 0;         /* Number of lines scanned. */
	int numdone = 0;         /* Number of lines actually updated. */

	if (!is_mapped) 
	  return 0;       /* no need to do anything... */

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

  /* The highest priority is given to the current screen row for the
   * first iteration of the loop, for maximum typing response.  
   * If y is out of bounds, then give it an invalid value so that it
   * can be given a different value during the loop.
   */
	y = cursor_row;
	if ((y < 0) || (y >= li)) 
	  y = -1;

/*  X_printf("X: X_update_screen, co=%d, li=%d, yloop=%d, y=%d, lines=%d\n",
           co,li,yloop,y,lines);*/

  /* The following loop scans lines on the screen until the maximum number
   * of lines have been updated, or the entire screen has been scanned.
   */
	while ((numdone < lines) && (numscan < li)) 
	  {
    /* The following sets the row to be scanned and updated, if it is not
     * the first iteration of the loop, or y has an invalid value from
     * loop pre-initialization.
     */
	    if ((numscan > 0) || (y < 0)) 
	      {
		yloop = (yloop+1) % li;
		if (yloop == cursor_row)
		  yloop = (yloop+1) % li;
		y = yloop;
	      }
	    numscan++;
	    
	    sp = screen_adr + y*co;
	    oldsp = prev_screen + y*co;

	    x=0;
	    do 
	      {
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
/* now scan in a string of changed chars of the same attribute.
   To keep the number of X calls (and thus the overhead) low,
   we tolerate a few unchanged characters (up to MAX_UNCHANGED in 
   a row) in the 'changed' string. 
*/
		bp = charbuff;
		start_x=x;
		attr=XATTR(sp);
		unchanged=0;         /* counter for unchanged chars */
		
		while(1) 
		  {
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

/* ok, we've got the string now send it to the X server */

		set_gc_attr(attr);
		XDrawImageString(display,mainwindow,gc,
				 font_width*start_x,font_height*y+font_shift,
				 charbuff,len);

		if ((prev_cursor_row == y) && 
		    (prev_cursor_col >= start_x) && 
		    (prev_cursor_col < start_x+len))
		  {
		    prev_cursor_shape=NO_CURSOR;  
/* old cursor was overwritten */
		  }
	      } 
	    while(x<co);
line_done:
/* Increment the number of successfully updated lines counter */
	    numdone++;

chk_cursor:
/* update the cursor. We do this here to avoid the cursor 'running behind'
       when using a fast key-repeat.
*/
	    if (y == cursor_row)
	      X_update_cursor();
	  }
	XFlush(display);

/*	X_printf("X: X_update_screen: %d lines updated\n",numdone);*/
	
	if (numdone) 
	  {
	    if (numscan==li)
	      return 1;     /* changed, entire screen updated */
	    else
	      return 2;     /* changed, part of screen updated */
	  }
	else 
	  {
/* The updates look a bit cleaner when reset to top of the screen
 * if nothing had changed on the screen in this call to screen_restore
 */
	    yloop = -1;
	    return(1);
	  }
      }
      break;
      
    case GRAPH:
      {
	DAC_entry color;
	int i,colchanged=0,rerr,gerr,berr,tmppix;

	if (!is_mapped) 
	  return 0;       /* no need to do anything... */

	/* first update the colormap */
	do
	  {
	    i=DAC_get_dirty_entry(&color);
	    X_printf("X: X_redraw_screen(): DAC_get_dirty_entry()=%i\n", i);
	    if(i!=-1)
	      {
		if (have_shmap)
		  {
		    tmppix=
		      pixelval[
			       (rerr=((color.r*redshades) >>6)) +
			       (gerr=((color.g*greenshades) >>6))*redshades +
			       (berr=((color.b*blueshades) >>6))*redshades*greenshades
		      ];
		    /* The virtual-colourmap may have changed, but that
		       doesn't necessarily mean that the colourmap entry
		       corresponds to a different physical pixel value. */
		    if (tmppix!=lut[i])
		      {
			lut[i]=tmppix;
			colchanged=1;
		      }
#if REMAP_TYPE!=REMAP_TYPE_FAST
		    /* Use the error from the first lookup table to help
		       calculate a better value for the second lookup
		       table, for dithering */
		    rerr=(((color.r + (color.r - ((rerr<<6) /redshades)))*redshades)>>6);
		    gerr=(((color.g + (color.g - ((gerr<<6) /greenshades)))*greenshades)>>6);
		    berr=(((color.b + (color.b - ((berr<<6) /blueshades)))*blueshades)>>6);
		    if (rerr>=redshades) rerr=redshades-1;
		    if (gerr>=greenshades) gerr=greenshades-1;
		    if (berr>=blueshades) berr=blueshades-1;
		    tmppix=
		      pixelval[
			       rerr +
			       gerr*redshades +
			       berr*redshades*greenshades
		      ];
		    if (tmppix!=lut2[i])
		      {
			lut2[i]=tmppix;
			colchanged=1;
		      }
#endif /* REMAP_TYPE!=REMAP_TYPE_FAST */
		  }
		else
		  {
		    xcol.pixel=i;
		    xcol.red  =(color.r*65535)/63;
		    xcol.green=(color.g*65535)/63;
		    xcol.blue =(color.b*65535)/63;
		    XStoreColor(display, vga256_cmap, &xcol);
		  }
	      }
	  }
	while(i!=-1);

	if (have_shmap)
	  {
	    if (colchanged==1) /* should remap ALL VGA pages */
	      dirty_all_video_pages();
	  }

	/* ! */
    	 {
#ifdef DEBUG_SHOW_UPDATE_AREA
	   static  unsigned char xxxx;
#endif		
	   unsigned char *base;
	   unsigned long int offset, len;
	   unsigned int loop,xx,yy,ww,hh, modewidth;
	   unsigned int changed=0;
	   unsigned char *baseplusoffset;
	   unsigned char *dataplusoffset;
	   
	   while(vgaemu_update(&base, &offset, &len,
			       VGAEMU_UPDATE_METHOD_GET_CHANGES,
			       &modewidth)==True)
	     {
	       /*	       printf("Dirty: %7ld %7ld\t",offset,len);*/

	       if (have_shmap)
		 {
#if REMAP_TYPE==REMAP_TYPE_SLOWXXXXXXXXXXX
		   int linectr = len/modewidth;
		   int lineoff;
		   /* Quick remap & copy of all changed pixels */
		   dataplusoffset = &ximage_p->data[offset];
		   baseplusoffset = &base[offset];
		   /* Assuming that the number of pixels to be remapped is an
		      even number... this is a good assumption at the
		      moment... */
		   loop=len;
		   while (linectr--)
		     {
		       lineoff=loop<modewidth?loop:modewidth;
		       while (lineoff--)
			 {
			   dataplusoffset[loop]=lut[baseplusoffset[loop--]];
			   dataplusoffset[loop]=lut2[baseplusoffset[loop--]];
			 }
		       if (linectr--)
			 {
			   lineoff=modewidth;
			   while (lineoff--)
			     {
			       dataplusoffset[loop]=lut2[baseplusoffset[loop--]];
			       dataplusoffset[loop]=lut[baseplusoffset[loop--]];
			     }
			 }
		       else break;
		     }
#else
#if REMAP_TYPE==REMAP_TYPE_MEDIUM
		   /* Quick remap & copy of all changed pixels */
		   dataplusoffset = &ximage_p->data[offset];
		   baseplusoffset = &base[offset];
		   loop=len;
		   /* Assuming that the number of pixels to be remapped is an
		      even number... this is a good assumption at the moment... */
		   while (loop--)
		     {
		       dataplusoffset[loop]=lut[baseplusoffset[loop--]];
		       dataplusoffset[loop]=lut2[baseplusoffset[loop]];
		     }
#else
#if REMAP_TYPE==REMAP_TYPE_FAST
		   /* Quick remap & copy of all changed pixels */
		   dataplusoffset = &ximage_p->data[offset];
		   baseplusoffset = &base[offset];
		   loop=len;
		   /* Simple remap, no dither */
		   while (loop--)
		     {
		       dataplusoffset[loop]=lut[baseplusoffset[loop]];
		     }
#else
#error REMAP_TYPE specified is not implemented yet.		   
#endif /* REMAP_TYPE==REMAP_TYPE_FAST */	   
#endif /* REMAP_TYPE==REMAP_TYPE_MEDIUM */   
#endif /* REMAP_TYPE==REMAP_TYPE_SLOW */
		 }
	       else
		 {
		   memcpy(&ximage_p->data[offset],&base[offset],len);
		 }

	       xx = 0;
	       ww = modewidth;
	       yy = offset / modewidth;
	       hh = 1+((offset+len)/modewidth) - yy;

	       if (hh+yy > ximage_p->height) hh--; /* (bleh..) */

	       changed=1;
#ifdef DEBUG_SHOW_UPDATE_AREA
	       XSetForeground(display,gc,xxxx++);
	       XFillRectangle(display, mainwindow, gc, xx, yy, ww, hh);
	       XSync(display, False);
#endif /* DEBUG_SHOW_UPDATE_AREA */
#ifdef HAVE_MITSHM
	       XShmPutImage(display,mainwindow,gc,ximage_p,xx,yy,xx,yy,ww,hh, True);
#else
	       XPutImage(display,mainwindow,gc,ximage_p,xx,yy,xx,yy,ww,hh);
#endif /* HAVE_MITSHM */
	       X_printf("X_update_screen(): %i pixel changes, redraw "
			"window is (%i,%i),(%i,%i)\n", 
			changed, xx, yy, ww, hh);
	       
	       return changed;
	     }
         }   
      }
      
      return 1;	
      break;
    
    default:
      return(0);
      break;
    }
    
/*  return(0);	*//* compiler catch all */
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
/*
  if (mouse.cursor_on != 0)
    XDefineCursor(display, mainwindow, X_standard_cursor);
  else
    XDefineCursor(display, mainwindow, X_mouse_cursor);
*/
  if(X_mouse_change_time == 0) X_mouse_change_time = time(NULL);

#endif    
}

#if CONFIG_X_MOUSE
/* 
 * DANG_BEGIN_FUNCTION set_mouse_position
 *
 * description:
 *  places the mouse on the right position
 * Not tested in X with graphics
 * 
 * returns: 
 *  nothing
 *
 * arguments: 
 * x,y - coordinates
 *
 * DANG_END_FUNCTION
 */ 
static void set_mouse_position(int x, int y)
{
  switch(video_mode)	/* only a conversion when in text-mode*/
    {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
      /* XXX - this works in text mode only */
      x = x*8/font_width;
      y = y*8/font_height;
      break;
    case 0x13:
    case 0x5c: /* 8bpp SVGA graphics modes */
    case 0x5d:
    case 0x5e:
    case 0x62:
      x*=8;  /* these scalings make DeluxePaintIIe operation perfect - */
      y*=8;  /*  I don't know about anything else... */
      /* Dos expects 640x200 mouse coordinates! only in this videomode */
      /* Some games don't... */
      break;

    default:
      X_printf("set_mouse_position: Invalid video mode 0x%02x\n",video_mode);
    }

  if ((x != mouse.x) || (y!=mouse.y)) 
    {
      mouse.x=x; 
      mouse.y=y;
      mouse_move();
   }
/*
  X_printf("Mouse x = %03i, y = %03i\n",mouse.x,mouse.y);
*/
/*printf("Bios Video_mode= 0x%02x\n",READ_BYTE(BIOS_VIDEO_MODE));*/
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

  if (mouse.threebuttons && mouse.mbutton!=mouse.oldmbutton) 
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
  X_printf("X:clear check selection , cursor at %d %d\n",
	   cursor_col,cursor_row);
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
  XSetSelectionOwner(display, XA_PRIMARY, mainwindow, CurrentTime);
  if (XGetSelectionOwner(display, XA_PRIMARY) != mainwindow)
  {
    X_printf("X: Couldn't get primary selection!\n");
    return;
}
  XChangeProperty(display, rootwindow, XA_CUT_BUFFER0, XA_STRING,
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
    XChangeProperty(display, requestor, property, target, 8, PropModeReplace, 
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
  XSendEvent(display, requestor, False, 0, &e);
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

   /*  struct timeval currenttime;
  struct timezone tz;*/


  /* Can't we turn off the SIGALRMs altogether while in here,
     and, more importantly, reset the interval from the start
     when we leave? */

   /*   gettimeofday(&currenttime, &tz);
   printf("ENTER: %10d %10d - ",currenttime.tv_sec,currenttime.tv_usec);
   printf(XPending(display)?"Event.":"ALARM!");*/

   if (busy)
     {
       printf(" - busy.\n");
       return;
     }
     
   busy=1;
   

#if CONFIG_X_MOUSE
   if(X_mouse_change_time != 0 &&
      time(NULL) > X_mouse_change_time) {
     X_mouse_change_time = 0;
     if(mouse.cursor_on != X_mouse_last_on) {
       if (mouse.cursor_on != 0)
       XDefineCursor(display, mainwindow, X_standard_cursor);
       else
       XDefineCursor(display, mainwindow, X_mouse_cursor);

       X_mouse_last_on = mouse.cursor_on;
     }
   }
#endif

  while (XPending(display) > 0) 
    {
      XNextEvent(display,&e);

      switch(e.type) 
	{
       case Expose:  
	  X_printf("X: expose event\n");
/*	  if(video_mode==0x13)
	    {
              XPutImage(display,mainwindow,gc,ximage_p,
                        e.xexpose.x, e.xexpose.y,e.xexpose.x, e.xexpose.y,
                        e.xexpose.width, e.xexpose.height);
	    }
	  else
	    {
	      if(e.xexpose.count == 0)
		X_redraw_screen();
	    }*/
	  switch(get_vgaemu_type())
	    {
	    case GRAPH:
#ifdef HAVE_MITSHM
	      XShmPutImage(display,mainwindow,gc,ximage_p,
	                e.xexpose.x, e.xexpose.y,e.xexpose.x, e.xexpose.y,
	                e.xexpose.width, e.xexpose.height, True);
#else
	      XPutImage(display,mainwindow,gc,ximage_p,
	                e.xexpose.x, e.xexpose.y,e.xexpose.x, e.xexpose.y,
	                e.xexpose.width, e.xexpose.height);
#endif
	      break;
	      
	    default:    /* case TEXT: */
	      {
	        if(e.xexpose.count == 0)
	          X_redraw_screen();
	      }
	      break;
	    }
	  break;

	case UnmapNotify:
	  X_printf("X: window unmapped\n");
	  is_mapped = FALSE;
	  break;
	  
	case MapNotify:
	  X_printf("X: window mapped\n");
	  is_mapped = TRUE;
	  break;
	  
	case FocusIn:
	  X_printf("X: focus in\n");
	  have_focus = TRUE;
	  redraw_cursor();
	  break;

	case FocusOut:
	  X_printf("X: focus out\n");
	  have_focus = FALSE;
	  blink_state = TRUE;
	  blink_count = config.X_blinkrate;
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
	  scr_paste_primary(display,e.xselection.requestor,
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
/* 
      Clears the visible selection if the cursor is inside the selection
*/
#if CONFIG_X_SELECTION
	  if (visible_selection)
	    clear_if_in_selection();
#endif
	  X_process_key(&e.xkey);
	  break;
    /* A keyboard mapping has been changed (e.g., with xmodmap). */
	case MappingNotify:  
	  X_printf("X: MappingNotify event\n");
	  XRefreshKeyboardMapping(&e.xmapping);
	  break;

/* Mouse events */
#if CONFIG_X_MOUSE  
	case ButtonPress:
#if CONFIG_X_SELECTION
	  if (e.xbutton.button == Button1)
	    start_selection(e.xbutton.x, e.xbutton.y);
	  else if (e.xbutton.button == Button3)
	    start_extend_selection(e.xbutton.x, e.xbutton.y);
#endif /* CONFIG_X_SELECTION */
	  set_mouse_position(e.xmotion.x,e.xmotion.y); /*root@sjoerd*/
	  set_mouse_buttons(e.xbutton.state|(0x80<<e.xbutton.button));
	  break;
	  
	case ButtonRelease:
	  set_mouse_position(e.xmotion.x,e.xmotion.y);  /*root@sjoerd*/
#if CONFIG_X_SELECTION
	  switch (e.xbutton.button)
	    {
	    case Button1 :
	    case Button3 : 
	      end_selection();
	      break;
	    case Button2 :
	      X_printf("X: mouse Button2Release\n");
	      scr_request_selection(display,mainwindow,e.xbutton.time,
				    e.xbutton.x,
				    e.xbutton.y);
	      break;
	    }
#endif /* CONFIG_X_SELECTION */
	  set_mouse_buttons(e.xbutton.state&~(0x80<<e.xbutton.button));
	  break;
	  
	case MotionNotify:
#if CONFIG_X_SELECTION
	  if (doing_selection)
	    extend_selection(e.xmotion.x, e.xmotion.y);
#endif /* CONFIG_X_SELECTION */
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
/* some weirder things... */
		     
	}
    }
   busy=0;
#if CONFIG_X_MOUSE  
   mouse_event();
#endif  

   /*   gettimeofday(&currenttime, &tz);
   printf("\nLEAVE: %10d %10d\n",currenttime.tv_sec,currenttime.tv_usec);*/

 }


struct video_system Video_X = 
{
   0,                /* is_mapped */
   X_init,         
   X_close,      
   X_setmode,      
   X_update_screen,
   X_update_cursor
};
