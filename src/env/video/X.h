/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <X11/X.h>
#include <X11/Xlib.h>

#define X_CHG_TITLE	CHG_TITLE
#define X_CHG_FONT	CHG_FONT
#define X_CHG_MAP	CHG_MAP
#define X_CHG_UNMAP	CHG_UNMAP
#define X_CHG_WINSIZE	CHG_WINSIZE
#define X_CHG_TITLE_EMUNAME	CHG_TITLE_EMUNAME
#define X_CHG_TITLE_APPNAME	CHG_TITLE_APPNAME
#define X_CHG_TITLE_SHOW_APPNAME	CHG_TITLE_SHOW_APPNAME
#define X_CHG_BACKGROUND_PAUSE	CHG_BACKGROUND_PAUSE
#define X_GET_TITLE_APPNAME	GET_TITLE_APPNAME

#define X_TITLE_EMUNAME_MAXLEN TITLE_EMUNAME_MAXLEN
#define X_TITLE_APPNAME_MAXLEN TITLE_APPNAME_MAXLEN

extern int grab_active;
extern Display *display;

void           get_vga_colors (void);
void           X_handler      (void);
void X_draw_cursor(int x,int y);
void X_restore_cell(int x,int y);
void X_set_textsize(int, int);
void X_init_videomode(void);
void X_show_mouse_cursor(int yes);
void X_set_mouse_cursor(int yes, int mx, int my, int x_range, int y_range);

void X_process_key(XKeyEvent *); 
#ifdef HAVE_UNICODE_KEYB
void X_process_keys(XKeymapEvent *);
#else
static inline void X_process_keys(XKeymapEvent *event) { return; }
#endif

