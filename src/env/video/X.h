/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <X11/X.h>
#include <X11/Xlib.h>

#define X_CHG_TITLE	1
#define X_CHG_FONT	2
#define X_CHG_MAP	3
#define X_CHG_UNMAP	4
#define X_CHG_WINSIZE	5
#define X_CHG_TITLE_EMUNAME	6
#define X_CHG_TITLE_APPNAME	7
#define X_CHG_TITLE_SHOW_APPNAME	8
#define X_CHG_BACKGROUND_PAUSE	9

#define X_TITLE_EMUNAME_MAXLEN 128
#define X_TITLE_APPNAME_MAXLEN 9

extern int grab_active;
extern Display *display;
extern char X_title_appname [X_TITLE_APPNAME_MAXLEN];		/* used in plugin/commands/comcom.c */

int X_change_config(unsigned, void *);		/* modify X config data from DOS */

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


