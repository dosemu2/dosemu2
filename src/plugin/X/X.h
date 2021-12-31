/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <X11/X.h>
#include <X11/Xlib.h>

#ifdef __cplusplus
extern "C" {
#endif

extern Display *display;
extern struct keyboard_client Keyboard_X;
extern struct video_system Video_X;

void           get_vga_colors (void);
void           X_handler      (void);
void X_draw_cursor(int x,int y);
void X_restore_cell(int x,int y);
void X_init_videomode(void);
void X_pre_init(void);
void X_register_speaker(Display *display);

void X_process_key(Display *display, XKeyEvent *);
void X_keycode_process_key(Display *display, XKeyEvent *);
//void X_process_keys(XKeymapEvent *);

int X_load_text_font(Display *dpy, int private_dpy,
		      Window, const char *p, int *w, int *h);
void X_close_text_display(void);
int X_handle_text_expose(void);
void X_set_resizable(Display *display, Window window, int on,
	int x_res, int y_res);
void X_force_mouse_cursor(int yes);

#ifdef __cplusplus
};
#endif
