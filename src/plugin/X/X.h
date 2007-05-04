/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include <X11/X.h>
#include <X11/Xlib.h>

extern Display *display;
extern struct keyboard_client Keyboard_X;

void           get_vga_colors (void);
void           X_handler      (void);
void X_draw_cursor(int x,int y);
void X_restore_cell(int x,int y);
void X_init_videomode(void);

void X_process_key(XKeyEvent *); 
void X_process_keys(XKeymapEvent *);

void load_text_font(void);
void X_load_text_font(Display *dpy, int private_dpy,
		      Window, const char *p, int *w, int *h);
int X_handle_text_expose(void);
