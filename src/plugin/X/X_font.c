/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* X font handling. Generally X fonts are faster than bitmapped fonts
   but they can't be scaled or their images changed by DOS software */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "emu.h"
#include "translate/translate.h"
#include "vgaemu.h"
#include "vgatext.h"
#include "video.h"
#include "X.h"
#include "dosemu_config.h"

static Display *text_display;
static Window text_window;
static XFontStruct *font = NULL;
static int font_width, font_height, font_shift;
static Colormap text_cmap;
static int text_col_stats[16] = {0};
static unsigned long text_colors[16];
static GC text_gc;
static int text_cmap_colors;

static void X_text_lock(void *opaque)
{
  XLockDisplay(text_display);
}

static void X_text_unlock(void *opaque)
{
  XUnlockDisplay(text_display);
}

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
static void X_draw_string(void *opaque, int x, int y, const char *text,
    int len, Bit8u attr)
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

static void X_draw_string16(void *opaque, int x, int y, const char *text,
    int len, Bit8u attr)
{
  XChar2b buff[len];
  t_unicode uni;
  struct char_set_state state;
  size_t i, d;

  set_gc_attr(attr);
  init_charset_state(&state, trconfig.video_mem_charset);
  d = font->max_char_or_byte2 - font->min_char_or_byte2 + 1;
  for (i = 0; i < len; i++) {
    if (charset_to_unicode(&state, &uni, (const unsigned char *)&text[i], 1) != 1)
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
static void X_draw_line(void *opaque, int x, int y, float ul, int len,
    Bit8u attr)
{
  set_gc_attr(attr);
  XDrawLine(
      text_display, text_window, text_gc,
      font_width * x,
      font_height * y + font_shift * ul,
      font_width * (x + len) - 1,
      font_height * y + font_shift * ul
    );
}


/*
 * Draw the cursor (nothing in graphics modes, normal if we have focus,
 * rectangle otherwise).
 */
static void X_draw_text_cursor(void *opaque, int x, int y, Bit8u attr,
                               int start, int end, Boolean focus)
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
static void X_set_text_palette(void *opaque, DAC_entry *col, int i)
{
  int read_cmap = 1;
  int shift = 16 - vga.dac.bits;
  XColor xc;

  xc.flags = DoRed | DoGreen | DoBlue;
  xc.pixel = text_colors[i];
  xc.red   = col->r << shift;
  xc.green = col->g << shift;
  xc.blue  = col->b << shift;

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
   X_text_lock,
   X_text_unlock,
   NULL,
   "X_font",
};

/* Runs xset to load X fonts */
static int run_xset(const char *path)
{
  char *command;
  int status, ret;
  struct stat buf;

  ret = stat(path, &buf);
  if (ret == -1 || !S_ISDIR(buf.st_mode)) {
    X_printf("X: xset stat fail '%s'\n", path);
    return 0;
  }

  ret = asprintf(&command, "xset +fp %s 2>/dev/null", path);
  assert(ret != -1);

  X_printf("X: running %s\n", command);
  status = system(command);
  if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    /* messed up font path -- last resort */
    X_printf("X: running xset fp default\n");
    if(system("xset fp default")) {
      X_printf("X: 'xset fp default' failed\n");
    }
    if(system(command)) {
      X_printf("X: command '%s' failed\n", command);
    }
  }
  free(command);

  if(system("xset fp rehash")) {
    X_printf("X: 'xset fp rehash' failed\n");
  }
  return 1;
}

/*
 * Load the main text font. Try first the user specified font, then
 * vga, then 9x15 and finally fixed. If none of these exists and
 * is monospaced, give up (shouldn't happen).
 */
static int _X_load_text_font(Display *dpy, int private_dpy, Window w,
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
      if (run_xset(SYSTEM_XFONTS_PATH))
	xfont = XLoadQueryFont(text_display, p);
    }
    if (xfont == NULL) {
      char *path = strdup(dosemu_proc_self_exe);
      if (path) {
	size_t len = strlen(path);
	if (len > 15) {
	  char *d = path + len - 15;
	  if (strcmp(d, "/bin/dosemu.bin") == 0) {
	    strcpy(d, "/Xfonts");
	    if (run_xset(path))
	      xfont = XLoadQueryFont(text_display, p);
	  }
	}
	free(path);
      }
    }
    if (xfont == NULL) {
      fprintf(stderr,
      "You do not have the %s %s font installed and are running\n"
      "remote X. You need to install the %s font on your _local_ Xserver.\n"
      "Look at the readme for details. For now we start with the bitmapped\n"
      "built-in font instead, which may be slower.\n",
      ((strncmp(p, "vga", 3) == 0) ? "DOSEMU" : ""), p, p);
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
  if (!font) {
    X_printf("X: X_change_config: font \"%s\" not found, "
	     "using builtin\n", p);
    X_printf("X: NOT loading a font. Using EGA/VGA builtin/RAM fonts.\n");
    X_printf("X: EGA/VGA font size is %d x %d\n",
	     vga.char_width, vga.char_height);
    if (width)
      *width = vga.char_width;
    if (height)
      *height = vga.char_height;
    return 0;
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

  return 1;
}

int X_load_text_font(Display *dpy, int private_dpy, Window w,
		      const char *p, int *width, int *height)
{
  int ret;
  XLockDisplay(dpy);
  ret = _X_load_text_font(dpy, private_dpy, w, p, width, height);
  XUnlockDisplay(dpy);
  return ret;
}

void X_close_text_display(void)
{
  if (text_display)
    XCloseDisplay(text_display);
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
