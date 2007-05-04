/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* X font handling. Generally X fonts are faster than bitmapped fonts
   but they can't be scaled or their images changed by DOS software */

#include <string.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "emu.h"
#include "translate.h"
#include "vgaemu.h"
#include "vgatext.h"
#include "video.h"
#include "X.h"

static Display *text_display;
static Window text_window;
static XFontStruct *font = NULL;
static int font_width, font_height, font_shift;
static Colormap text_cmap;
static int text_col_stats[16] = {0};
static unsigned long text_colors[16];
static GC text_gc;
static int text_cmap_colors;

/* 
 * Change color values in our graphics context 'gc'.
 */
static void set_gc_attr(Bit8u attr)
{
  XGCValues gcv;

  gcv.foreground = text_colors[ATTR_FG(attr)];
  gcv.background = text_colors[ATTR_BG(attr)];
  XChangeGC(text_display, text_gc, GCForeground | GCBackground, &gcv);
}


/*
 * Draw a non-bitmap text string.
 * The attribute is the VGA color/mono text attribute.
 */
static void X_draw_string(int x, int y, unsigned char *text, int len, Bit8u attr)
{
  set_gc_attr(attr);
  XDrawImageString(
    text_display, text_window, text_gc,
    font_width * x,
    font_height * y + font_shift,
    text,
    len
    );
}

static void X_draw_string16(int x, int y, unsigned char *text, int len, Bit8u attr)
{
  XChar2b buff[len];
  t_unicode uni;
  struct char_set_state state;
  size_t i, d;

  set_gc_attr(attr);
  init_charset_state(&state, trconfig.video_mem_charset);
  d = font->max_char_or_byte2 - font->min_char_or_byte2 + 1;
  for (i = 0; i < len; i++) {
    if (charset_to_unicode(&state, &uni, &text[i], 1) != 1)
      break;
    buff[i].byte1 = uni / d + font->min_byte1;
    buff[i].byte2 = uni % d + font->min_char_or_byte2;
  }
  cleanup_charset_state(&state);
  XDrawImageString16(
    text_display, text_window, text_gc,
    font_width * x,
    font_height * y + font_shift,
    buff,
    i
    );
}


/*
 * Draw a horizontal line (for text modes)
 * The attribute is the VGA color/mono text attribute.
 */
static void X_draw_line(int x, int y, int len)
{
  XDrawLine(
      text_display, text_window, text_gc,
      font_width * x,
      font_height * y + font_shift,
      font_width * (x + len) - 1,
      font_height * y + font_shift
    );
}


/*
 * Draw the cursor (nothing in graphics modes, normal if we have focus,
 * rectangle otherwise).
 */
static void X_draw_text_cursor(int x, int y, Bit8u attr, int start, int end,
			       Boolean focus)
{
  int cstart, cend;

  /* no hardware cursor emulation in graphics modes (erik@sjoerd) */
  if(vga.mode_class == GRAPH) return;

  set_gc_attr(attr);
  if(!focus) {
    XDrawRectangle(
      text_display, text_window, text_gc,
      x * font_width,
      y * font_height,
      font_width - 1,
      font_height - 1
      );
  }
  else {
    cstart = ((start + 1) * font_height) / vga.char_height - 1;
    if (cstart == -1) cstart = 0;
    cend = ((end + 1) * font_height) / vga.char_height - 1;
    if (cend == -1) cend = 0;
    XFillRectangle(
      text_display, text_window, text_gc,
      x * font_width,
      y * font_height + cstart,
      font_width,
      cend - cstart + 1
    );
  }
}

/*
 * Scans the colormap cmap for the color closest to xc and
 * returns it in xc. The color values used by cmap are queried
 * from the X server only if read_cmap is set.
 */
static void get_approx_color(XColor *xc, Colormap cmap, int read_cmap)
{
  int i, d0, ind;
  unsigned d1, d2;
  static XColor xcols[256];

  if(read_cmap) {
    for(i = 0; i < text_cmap_colors; i++) xcols[i].pixel = i;
    XQueryColors(text_display, cmap, xcols, text_cmap_colors);
  }

  ind = -1; d2 = -1;
  for(i = 0; i < text_cmap_colors; i++) {
    /* Note: X color values are unsigned short, so using int for the sum (d1) should be ok. */
    d0 = (int) xc->red - xcols[i].red; d1 = abs(d0);
    d0 = (int) xc->green - xcols[i].green; d1 += abs(d0);
    d0 = (int) xc->blue - xcols[i].blue; d1 += abs(d0);
    if(d1 < d2) ind = i, d2 = d1;
  }

  if(ind >= 0) *xc = xcols[ind];
}


/*
 * Update the active X colormap for text modes DAC entry col.
 */
static void X_set_text_palette(DAC_entry col)
{
  int read_cmap = 1;
  int i, shift = 16 - vga.dac.bits;
  XColor xc;

  xc.flags = DoRed | DoGreen | DoBlue;
  xc.pixel = text_colors[i = col.index];
  xc.red   = col.r << shift;
  xc.green = col.g << shift;
  xc.blue  = col.b << shift;

  if(text_col_stats[i]) XFreeColors(text_display, text_cmap, &xc.pixel, 1, 0);

  if(!(text_col_stats[i] = XAllocColor(text_display, text_cmap, &xc))) {
    get_approx_color(&xc, text_cmap, read_cmap);
    read_cmap = 0;
    X_printf("X: refresh_text_palette: %d (%d -> app. %d)\n", i, (int) text_colors[i], (int) xc.pixel);
      
  }
  else {
    X_printf("X: refresh_text_palette: %d (%d -> %d)\n", i, (int) text_colors[i], (int) xc.pixel);
  }
  text_colors[i] = xc.pixel;
}


static struct text_system Text_X =
{
   X_draw_string, 
   X_draw_line,
   X_draw_text_cursor,
   X_set_text_palette,
};

/*
 * Load the main text font. Try first the user specified font, then
 * vga, then 9x15 and finally fixed. If none of these exists and
 * is monospaced, give up (shouldn't happen).
 */
void X_load_text_font(Display *dpy, int private_dpy, Window w,
		      const char *p, int *width, int *height)
{
  XFontStruct *xfont;
  XGCValues gcv;
  XWindowAttributes xwa;
  int depth;

  xfont = NULL;
  if (!private_dpy)
    text_display = dpy;

  if (p && *p) {
    /* Open a separate connection to receive expose events without
       SDL eating them. */
    if (private_dpy && text_display == NULL)
      text_display = XOpenDisplay(NULL);
    xfont = XLoadQueryFont(text_display, p);
    if (xfont == NULL) {
      error("X: Unable to open font \"%s\", using builtin\n", p);
    } else if (xfont->min_bounds.width != xfont->max_bounds.width) {
      error("X: Font \"%s\" isn't monospaced, using builtin\n", p);
      XFreeFont(text_display, xfont);
      xfont = NULL;
    }
  }

  if(font != NULL) {
    XFreeFont(text_display, font);
    XFreeGC(text_display, text_gc);
    if (xfont == NULL && private_dpy) {
      /* called from SDL:
	 set expose events back from text_display to the main one */
      XSelectInput(text_display, w, 0);
      XGetWindowAttributes(dpy, w, &xwa);
      XSelectInput(dpy, w, xwa.your_event_mask | ExposureMask);
    }
  }

  font = xfont;
  use_bitmap_font = (font == NULL);
  dirty_all_vga_colors();
  if (use_bitmap_font) {
    if (p == NULL) {
      if (private_dpy && text_display)
	XCloseDisplay(text_display);
      return;
    }
    X_printf("X: X_change_config: font \"%s\" not found, "
	     "using builtin\n", p);
    X_printf("X: NOT loading a font. Using EGA/VGA builtin/RAM fonts.\n");
    X_printf("X: EGA/VGA font size is %d x %d\n",
	     vga.char_width, vga.char_height);
    return;
  }

  depth = DefaultDepth(text_display, DefaultScreen(text_display));
  if(depth > 8) depth = 8;
  text_cmap_colors = 1 << depth;
  text_cmap = DefaultColormap(text_display, DefaultScreen(text_display));
  text_window = w;

  gcv.font = font->fid;
  text_gc = XCreateGC(text_display, w, GCFont, &gcv);
  font_width = font->max_bounds.width;
  font_height = font->max_bounds.ascent + font->max_bounds.descent;
  font_shift = font->max_bounds.ascent;
  X_printf("X: Using font \"%s\", size = %d x %d\n", p, font_width, font_height);
  if (font->min_byte1 || font->max_byte1) {
    Text_X.Draw_string = X_draw_string16;
    X_printf("X: Assuming unicode font\n");
  } else
    Text_X.Draw_string = X_draw_string;
  register_text_system(&Text_X);
  if (width)
    *width = font_width;
  if (height)
    *height = font_height;

  if (private_dpy) { /* called from SDL */
    XSelectInput(text_display, w, ExposureMask);
    XGetWindowAttributes(dpy, w, &xwa);
    XSelectInput(dpy, w, xwa.your_event_mask & ~ExposureMask);
  }
}

/*
 *  handle expose events (called from SDL only)
 */
int X_handle_text_expose(void)
{
  int ret = 0;
  if (!text_display)
    return ret;
  while (XPending(text_display) > 0) {
    XEvent e;
    XNextEvent(text_display, &e);
    switch(e.type) {
    case Expose:
      X_printf("X: text_display expose event\n");
      ret = 1;
      break;
    default:
      v_printf("SDL: some other X event (ignored)\n");
      break;
    }
  }
  return ret;
}
