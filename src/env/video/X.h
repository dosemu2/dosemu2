/* 
 * (C) Copyright 1992, ..., 2002 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#define X_CHG_TITLE	1
#define X_CHG_FONT	2
#define X_CHG_MAP	3
#define X_CHG_UNMAP	4
#define X_CHG_WINSIZE	5
#define X_CHG_TITLE_EMUNAME	6
#define X_CHG_TITLE_APPNAME	7
#define X_CHG_TITLE_SHOW_APPNAME	8

#define X_TITLE_EMUNAME_MAXLEN 128
#define X_TITLE_APPNAME_MAXLEN 9

int X_change_config(unsigned, void *);		/* modify X config data from DOS */

void           get_vga_colors (void);
void           X_handler      (void);
void X_draw_cursor(int x,int y);
void X_restore_cell(int x,int y);
void X_move_cursor(int from_x,int from_y,int to_x,int to_y);
void X_setcursorshape(unsigned short shape);
void X_set_textsize(int, int);
