/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

typedef struct _win_info
{
  /* window width and height in pixels */
  int pwidth,pheight;
  /* window width and height in characters */
  int cwidth, cheight;
  /* font width and height in pixels */
  int fwidth, fheight;
  /* total lines to save in scroll-back buffer */
  int saved_lines;
  /* how far back we are in the scroll back buffer */
  int offset;
  /* high water mark of saved scrolled lines */
  int sline_top;
} WindowInfo;

#define SELECTION_CLEAR 0
#define SELECTION_BEGIN 1
#define SELECTION_INIT 2
#define SELECTION_ONGOING 3
#define SELECTION_COMPLETE 4

void scr_start_selection(int,int);
void scr_extend_selection(int,int);
void scr_clear_selection(void);
void scr_make_selection(Display*,int);
void scr_send_selection(Display*,int,int,int,int);
void scr_delete_selection(void);
void scr_request_selection(Display*,Window,int,int,int);
void scr_paste_primary(Display*,int,int,int);
void send_string(unsigned char *,int);
void process_string (void);

