/* Copyright (c) 1998 John E. Davis
 * This file is part of the S-Lang library.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Perl Artistic License.
 */
#include <stdio.h>

#ifndef SLANG_VERSION
# include <slang.h>
#endif

/* This is a temporary hack until lynx is fixed to not include this file. */
#ifndef LYCURSES_H

typedef struct
{
   unsigned int _begy, _begx, _maxy, _maxx;
   unsigned int _curx, _cury;
   unsigned int nrows, ncols;
   unsigned int scroll_min, scroll_max;
   unsigned short **lines;
   unsigned short color;
   int is_subwin;
   SLtt_Char_Type attr;
   int delay_off;
   int scroll_ok;
   int modified;
   int has_box;
   int use_keypad;
}
SLcurses_Window_Type;

extern int SLcurses_wclrtobot (SLcurses_Window_Type *);
extern int SLcurses_wscrl (SLcurses_Window_Type *, int);
extern int SLcurses_wrefresh (SLcurses_Window_Type *);
extern int SLcurses_delwin (SLcurses_Window_Type *);
extern int SLcurses_wprintw (SLcurses_Window_Type *, char *, ...);
extern SLcurses_Window_Type *SLcurses_newwin (unsigned int, unsigned int,
					      unsigned int, unsigned int);

extern SLcurses_Window_Type *SLcurses_subwin (SLcurses_Window_Type *,
					      unsigned int, unsigned int,
					      unsigned int, unsigned int);

extern int SLcurses_wnoutrefresh (SLcurses_Window_Type *);
extern int SLcurses_wclrtoeol (SLcurses_Window_Type *);

extern int SLcurses_wmove (SLcurses_Window_Type *, unsigned int, unsigned int);
extern int SLcurses_waddch (SLcurses_Window_Type *, SLtt_Char_Type);
extern int SLcurses_waddnstr (SLcurses_Window_Type *, char *, int);

#define waddnstr		SLcurses_waddnstr
#define waddch			SLcurses_waddch
#define waddstr(w,s)		waddnstr((w),(s),-1)
#define addstr(x)		waddstr(stdscr, (x))
#define addnstr(s,n)		waddnstr(stdscr,(s),(n))
#define addch(ch)      		waddch(stdscr,(ch))

#define mvwaddnstr(w,y,x,s,n) \
  (-1 == wmove((w),(y),(x)) ? -1 : waddnstr((w),(s),(n)))
#define mvwaddstr(w,y,x,s) \
  (-1 == wmove((w),(y),(x)) ? -1 : waddnstr((w),(s), -1))
#define mvaddnstr(y,x,s,n)	mvwaddnstr(stdscr,(y),(x),(s),(n))
#define mvaddstr(y,x,s)       	mvwaddstr(stdscr,(y),(x),(s))
#define mvwaddch(w,y,x,c) \
  ((-1 == wmove((w),(y),(x))) ? -1 : waddch((w),(c)))
#define mvaddch(y,x,c)         	mvwaddch(stdscr,(y),(x),(c))

extern int SLcurses_wclear (SLcurses_Window_Type *w);
extern int SLcurses_printw (char *, ...);

#if 0
/* Why are these functions part of curses??? */
extern int SLcurses_mvwscanw (SLcurses_Window_Type *, unsigned int, unsigned int,
			      char *, ...);
extern int SLcurses_wscanw (SLcurses_Window_Type *, char *, ...);
extern int SLcurses_scanw (char *, ...);
#define mvwscanw SLcurses_mvwscanw
#define wscanw SLcurses_wscanw
#define scanw SLcurses_scanw
#endif

extern SLcurses_Window_Type *SLcurses_Stdscr;
#define WINDOW SLcurses_Window_Type
#define stdscr SLcurses_Stdscr

#define subwin		SLcurses_subwin
#define wclrtobot	SLcurses_wclrtobot
#define wscrl		SLcurses_wscrl
#define scrl(n)		wscrl(stdscr,(n))
#define scroll(w)	wscrl((w),1)
#define wrefresh	SLcurses_wrefresh
#define delwin		SLcurses_delwin
#define wmove		SLcurses_wmove
#define newwin		SLcurses_newwin
#define wnoutrefresh	SLcurses_wnoutrefresh
#define werase(w)	SLcurses_wmove((w),0,0); SLcurses_wclrtobot(w)
#define wclear(w)	SLcurses_wmove((w),0,0); SLcurses_wclrtobot(w)
#define wprintw		SLcurses_wprintw
#define mvwprintw	SLcurses_mvwprintw

#define winch(w) \
    ((((w)->_cury < (w)->nrows) && ((w)->_curx < (w)->ncols)) \
       ? ((w)->lines[(w)->_cury][(w)->_curx]) : 0)

#define inch() winch(stdscr)
#define mvwinch(w,x,y) \
    ((-1 != wmove((w),(x),(y))) ? winch(w) : (-1))
#define doupdate SLsmg_refresh

#define mvwin(w,a,b) ((w)->_begy = (a), (w)->_begx = (b))

extern int SLcurses_mvprintw (int, int, char *, ...);
extern int SLcurses_mvwprintw (SLcurses_Window_Type *, int, int, char *, ...);
extern int SLcurses_has_colors(void);
extern int SLcurses_nil (void);
extern int SLcurses_wgetch (SLcurses_Window_Type *);
extern int SLcurses_getch (void);

extern int SLcurses_wattrset (SLcurses_Window_Type *, SLtt_Char_Type);
extern int SLcurses_wattron (SLcurses_Window_Type *, SLtt_Char_Type);
extern int SLcurses_wattroff (SLcurses_Window_Type *, SLtt_Char_Type);
#define attrset(x) SLcurses_wattrset(stdscr, (x))
#define attron(x) SLcurses_wattron(stdscr, (x))
#define attroff(x) SLcurses_wattroff(stdscr, (x))
#define wattrset(w, x) SLcurses_wattrset((w), (x))
#define wattron(w, x) SLcurses_wattron((w), (x))
#define wattroff(w, x) SLcurses_wattroff((w), (x))
#define wattr_get(w) ((w)->color << 8)
#define attr_get() wattr_get(stdscr)

#define COLOR_PAIR(x) ((x) << 8)

extern int SLcurses_start_color (void);
#define start_color SLcurses_start_color

#define ERR 0xFFFF
#define wgetch SLcurses_wgetch
#define getch SLcurses_getch

extern int SLcurses_nodelay (SLcurses_Window_Type *, int);
extern SLcurses_Window_Type *SLcurses_initscr (void);
#define initscr SLcurses_initscr

extern int SLcurses_cbreak (void);
extern int SLcurses_raw (void);
#define cbreak SLcurses_cbreak
#define crmode SLcurses_cbreak
#define raw SLcurses_raw
#define noraw SLang_reset_tty
#define nocbreak SLang_reset_tty

#define mvprintw SLcurses_mvprintw
#define has_colors SLcurses_has_colors
#define nodelay SLcurses_nodelay

#define ungetch SLang_ungetkey

#define COLS SLtt_Screen_Cols
#define LINES SLtt_Screen_Rows

#define move(x,y) SLcurses_wmove(stdscr, (x), (y))
#define wclrtoeol SLcurses_wclrtoeol
#define clrtoeol() SLcurses_wclrtoeol(stdscr)
#define clrtobot() SLcurses_wclrtobot(stdscr)

#define printw SLcurses_printw
#define mvprintw SLcurses_mvprintw
#define wstandout(w) SLcurses_wattrset((w),A_STANDOUT)
#define wstandend(w) SLcurses_wattrset((w),A_NORMAL)
#define standout() SLcurses_wattrset(stdscr,A_STANDOUT)
#define standend() SLcurses_wattrset(stdscr,A_NORMAL)

#define refresh() SLcurses_wrefresh(stdscr)
#define clear() SLcurses_wclear(stdscr)
#define erase() werase(stdscr)
#define touchline SLsmg_touch_lines
#define resetterm SLang_reset_tty

extern int SLcurses_endwin (void);
#define endwin SLcurses_endwin
extern int SLcurses_Is_Endwin;
#define isendwin() SLcurses_Is_Endwin

#define keypad(w,x) ((w)->use_keypad = (x))

#define KEY_MIN		SL_KEY_UP
#define KEY_DOWN	SL_KEY_DOWN
#define KEY_UP		SL_KEY_UP
#define KEY_LEFT	SL_KEY_LEFT
#define KEY_RIGHT	SL_KEY_RIGHT
#define KEY_A1		SL_KEY_A1
#define KEY_B1		SL_KEY_B1
#define KEY_C1		SL_KEY_C1
#define KEY_A2		SL_KEY_A2
#define KEY_B2		SL_KEY_B2
#define KEY_C2		SL_KEY_C2
#define KEY_A3		SL_KEY_A3
#define KEY_B3		SL_KEY_B3
#define KEY_C3		SL_KEY_C3
#define KEY_REDO	SL_KEY_REDO
#define KEY_UNDO	SL_KEY_UNDO
#define KEY_BACKSPACE	SL_KEY_BACKSPACE
#define KEY_PPAGE	SL_KEY_PPAGE
#define KEY_NPAGE	SL_KEY_NPAGE
#define KEY_HOME	SL_KEY_HOME
#define KEY_END		SL_KEY_END
#define KEY_F0		SL_KEY_F0
#define KEY_F		SL_KEY_F
#define KEY_ENTER	SL_KEY_ENTER
#define KEY_MAX		0xFFFF

/* Ugly Hacks that may not work */
#define flushinp SLcurses_nil
#define winsertln(w) \
  ((w)->scroll_min=(w)->_cury, \
   (w)->scroll_max=(w)->nrows, \
   wscrl((w), -1))

extern SLtt_Char_Type SLcurses_Acs_Map [128];
#define acs_map SLcurses_Acs_Map

#define ACS_ULCORNER (acs_map[SLSMG_ULCORN_CHAR])
#define ACS_URCORNER (acs_map[SLSMG_URCORN_CHAR])
#define ACS_LRCORNER (acs_map[SLSMG_LRCORN_CHAR])
#define ACS_LLCORNER (acs_map[SLSMG_LLCORN_CHAR])
#define ACS_TTEE (acs_map[SLSMG_UTEE_CHAR])
#define ACS_LTEE (acs_map[SLSMG_LTEE_CHAR])
#define ACS_RTEE (acs_map[SLSMG_RTEE_CHAR])
#define ACS_BTEE (acs_map[SLSMG_DTEE_CHAR])
#define ACS_PLUS (acs_map[SLSMG_PLUS_CHAR])
#define ACS_VLINE (acs_map[SLSMG_VLINE_CHAR])
#define ACS_HLINE (acs_map[SLSMG_HLINE_CHAR])
#define ACS_S1		'-'
#define ACS_S9		'-'
#define ACS_DIAMOND		'&'
#define ACS_CKBOARD		(acs_map[SLSMG_CKBRD_CHAR])
#define ACS_DEGREE		'o'
#define ACS_PLMINUS		'+'
#define ACS_BULLET		'*'
#define ACS_LARROW		'<'
#define ACS_RARROW		'>'
#define ACS_DARROW		'v'
#define ACS_UARROW		'^'
#define ACS_BOARD		'#'
#define ACS_LANTERN		'#'
#define ACS_BLOCK		'#'

#if 1
#define hline(x,y) SLcurses_nil ()
#define vline(x,y) SLcurses_nil ()
#endif

#define A_CHARTEXT	0x00FF
#define A_NORMAL 0
#define A_BOLD		0x1000
#define A_REVERSE	0x2000
#define A_STANDOUT	A_REVERSE
#define A_UNDERLINE	0x4000
#define A_BLINK		0
#define A_COLOR		0x0700
#define A_ALTCHARSET	0x8000
#define A_DIM		0
#define A_PROTECT	0
#define A_INVIS		0

#define COLOR_BLACK	SLSMG_COLOR_BLACK
#define COLOR_RED	SLSMG_COLOR_RED
#define COLOR_GREEN	SLSMG_COLOR_GREEN
#define COLOR_YELLOW	SLSMG_COLOR_BROWN
#define COLOR_BLUE	SLSMG_COLOR_BLUE
#define COLOR_MAGENTA	SLSMG_COLOR_MAGENTA
#define COLOR_CYAN	SLSMG_COLOR_CYAN
#define COLOR_WHITE	SLSMG_COLOR_LGRAY

#define COLORS		8
#define COLOR_PAIRS	(8 * 8)

#define init_pair(_x,_f,_b) \
 SLtt_set_color_object((_x), ((_f) == (_b) ? 0x0700 : ((_f) | ((_b) << 8)) << 8))

#define scrollok(a,b) ((a)->scroll_ok = (b))
#define getyx(a,y,x)  (y=(a)->_cury, x=(a)->_curx)
#define getmaxyx(a,y,x)  (y=(a)->nrows, x=(a)->ncols)
#define napms(x) usleep(1000 * (x))
typedef SLtt_Char_Type chtype;
#define beep SLtt_beep
#define curs_set(x) SLtt_set_cursor_visibility(x)
#define touchwin(x) SLsmg_touch_lines((x)->_begy, (x)->nrows)
#define flash SLtt_beep

#define wsetscrreg(w,a,b)	((w)->scroll_min = (a), (w)->scroll_max = (b))

#define wtimeout(a,b) (a)->delay_off = ((b >= 0) ? (b) / 100 : -1)
#define timeout(a) wtimeout(stdscr, a)
extern int SLcurses_wdelch (SLcurses_Window_Type *);
#define wdelch SLcurses_wdelch
#define delch() wdelch(stdscr)

extern int SLcurses_winsch (SLcurses_Window_Type *, int);
#define winsch SLcurses_winsch

extern int SLcurses_Esc_Delay;/* ESC expire time in milliseconds (ncurses compatible) */
#define ESCDELAY SLcurses_Esc_Delay

extern int SLcurses_clearok (SLcurses_Window_Type *, int);
#define clearok SLcurses_clearok

/* Functions that have not been implemented. */
#define copywin(w,v,a,b,c,d,e,f,g) SLcurses_nil()
#define wdeleteln(win) SLcurses_nil()
#define resetty SLcurses_nil
#define savetty SLcurses_nil
#define overlay(u,v) SLcurses_nil()

/* These functions do nothing */
#define savetty SLcurses_nil
#define nonl    SLcurses_nil
#define echo SLcurses_nil
#define noecho SLcurses_nil
#define saveterm SLcurses_nil
#define box(w,y,z) ((w)->has_box = 1, (w)->modified = 1)
#define leaveok(a,b) SLcurses_nil()
#define nl() SLcurses_nil()
#define trace(x) SLcurses_nil()
#define tigetstr(x) NULL

/* These have no place in C */
#define TRUE 1
#define FALSE 0
#define bool int

/* Lynx compatability */
#else

#define stdscr NULL
#define COLS SLtt_Screen_Cols
#define LINES SLtt_Screen_Rows
#define move SLsmg_gotorc
#define addstr SLsmg_write_string
#define clear SLsmg_cls
#define standout SLsmg_reverse_video
#define standend  SLsmg_normal_video
#define clrtoeol SLsmg_erase_eol
#define scrollok(a,b) SLsmg_Newline_Moves = ((b) ? 1 : -1)
#define addch SLsmg_write_char
#define echo()
#define printw SLsmg_printf
#define endwin SLsmg_reset_smg(),SLang_reset_tty

#endif
