/* 
 * (C) Copyright 1992, ..., 2003 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef VIDEO_H
#define VIDEO_H

#include "extern.h"
#include "port.h"
typedef unsigned char byte;

typedef struct { byte end, start; } cshape;

extern void gettermcap(int);

/* if you set this to 1, the video memory dirty bit will be checked 
   before updating the screen.
   (this affects emu.c and video/int10.c)
*/
#define VIDEO_CHECK_DIRTY 0

/* if you set this to 1, then you will be able to use your MDA + monitor
   as second display. This currently is possible together with
   Video_term, Video_console and Video_graphics.
   You also must set the keyword "dualmon" in the video section
   of your /etc/dosemu.conf.
   With dualmonitor support you can run CAD-programs, debuggers, or
   simply change your PC-console with "mode mono"
   NOTE: 
     Currently this can't be used together with VIDEO_CHECK_DIRTY,
     because the kernel (vm86.c) would remap all video pages
     from 0xa0000 to 0xbffff.
*/
#define USE_DUALMON 1
void init_dualmon(void);

#if USE_DUALMON && VIDEO_CHECK_DIRTY
  #error "Currently USE_DUALMON can't be used together with VIDEO_CHECK_DIRTY"
#endif

#define CURSOR_START(c) ((int)((cshape*)&c)->start)
#define CURSOR_END(c)   ((int)((cshape*)&c)->end)
#define NO_CURSOR 0x0100

/* 
 * xterm allows to set virtually any screen size, but DOS applications
 * and the BIOS really can't handle more than these numbers of rows and
 * columns (128*255*2 < 65535)
 */
#define MAX_COLUMNS 255
#define MAX_LINES 128

#if 0
#define SCREEN_ADR(s)	((ushort *)(virt_text_base + (s*TEXT_SIZE)))
#endif

/********************************************/

/* macros for accessing video memory. w is an ushort* 
   to a character cell, attr is a byte.
*/

#if 0
#define CHAR(w) (((char*)(w))[0])
#define ATTR(w) (((byte*)(w))[1])
#else
#define CHAR(w) ((char)READ_BYTE(w))
#define ATTR(w) ((byte)READ_BYTE(((Bit8u *)(w))+1))
#endif
#define ATTR_FG(attr) (attr & 0x0F)
#define ATTR_BG(attr) (attr >> 4)

/***********************************************************************/

/* Here's an idea to clean up the video code: Build a 'virtual' video
   support, a bit like the VFS in the linux kernel.
   The following structure defines the currently active video routines.
   There should be one such structure for each 'video front end',
   i.e. terminal, ncurses, console, X.
   The pointer 'Video' will be initialized at startup to point to
   the video_system struct of the active frontend and Video->init
   will then be called. The other contents don't have to be valid
   before Video->init() but the recommended method is to define
   them statically.

   Most of the function pointers defined here can be set to NULL for
   no special handling.
   functions returning int should return zero for normal completion,
   non-zero on error (unless specified otherwise).

   The contents of this struct should only be changed by init, cleanup
   and (maybe) setmode.
*/


struct video_system {
   boolean is_mapped;          /* if true, video ram is directly mapped and
                                  update_screen is not needed. */

   int (*init)(void);              /* does all frontend-specific setup,
                                  like mapping video memory, opening XWindow,
                                  etc. */
   void (*close)(void);
   
   int (*setmode)(int type, int xsize,int ysize);   /* type=0 currently (text mode) */

   int (*update_screen)(void);     /* (partially) update screen and cursor from the 
                                  video memory. called from sigalrm handler */

   void (*update_cursor)(void);    /* update cursor position&shape. Called by sigalrm
                                  handler *only* if update_screen does not exist
                                  or is not done because the video mem is clean */
   int (*change_config)(unsigned item, void *buf); /* change configuration
                                  e.g. window title (optional) */
};

extern struct video_system *Video;
#if USE_DUALMON
extern struct video_system *Video_default;
#endif
extern struct video_system Video_graphics,  Video_X, Video_console, Video_hgc, Video_term;

EXTERN ushort *screen_adr;   /* pointer to video memory of current page */
/* currently displayed page (was scrbuf) */
EXTERN ushort *prev_screen;  /* pointer to currently displayed screen   */
                             /* used&updated by Video->update_screen    */

EXTERN int video_mode INIT(0);
EXTERN int video_page INIT(0); 
EXTERN int char_blink INIT(0);
EXTERN int co INIT(80);
EXTERN int li INIT(25);

EXTERN int cursor_col INIT(0);
EXTERN int cursor_row INIT(0);
extern int cursor_blink;
EXTERN ushort cursor_shape INIT(0xe0f);
/* bit mask for testing vm86s.screen_bitmap */
EXTERN unsigned int screen_mask;

EXTERN int font_height INIT(16);

EXTERN int vga_font_height INIT(16);  /* current EMULATED setting for vga font height */

EXTERN int std_font_height INIT(16);  /* font height set by int10,0 mode 3 */
EXTERN int text_scanlines INIT(400);   /* # of scan lines in textmodes */
                             /* these have effect only on video mode sets! */

EXTERN unsigned char video_initialized INIT(0);
extern void install_int_10_handler(void);
extern void clear_screen(int s,int att);
extern boolean set_video_mode(int);

extern void Scroll(us *sadr,int x0,int y0,int x1,int y1,int n,int attr);
#define scrollup(x0,y0,x1,y1,l,att) Scroll(x0,y0,x1,y1,l,att)
#define scrolldn(x0,y0,x1,y1,l,att) Scroll(x0,y0,x1,y1,-(l),att)

/* Values are set by video_config_init depending on video-card defined in config */
/* Values are set from emu.c depending on video-config */


EXTERN int virt_text_base INIT(0);
EXTERN int phys_text_base INIT(0);
EXTERN int video_combo INIT(0);
EXTERN int video_subsys;
EXTERN int v_8514_base INIT(0);

/* The following defines are for terminal (curses) mode */

/* Character set defines */
#define CHARSET_LATIN	1
#define CHARSET_IBM	2
#define CHARSET_FULLIBM	3
#define CHARSET_LATIN1  4
#define CHARSET_LATIN2  5
#define CHARSET_KOI8    6


/* Color set defines */
/* #define COLOR_NORMAL	1 */
/* #define COLOR_XTERM	2 */

/* Terminal update defines. For using direct ANSI sequences, or NCURSES */
/* #define METHOD_FAST	1 */
/* #define METHOD_NCURSES	2 */

/* Various defines for all common video adapters */

#define CARD_VGA	1
#define CARD_EGA	2
#define CARD_CGA	3
#define CARD_MDA	4
#define CARD_NONE	5

/* Defines for Monochrome Display Adapter */

#define MDA_INIT_SCREEN_MODE   7 /* 80x25 MDA monochrome */
#define MDA_CONF_SCREEN_MODE   (3<<4)	/* for int 11h info     */
#define MDA_VIDEO_COMBO        1
#define MDA_VIDEO_SUBSYS       1 /* 1=mono               */
/* #define BASE_CRTC               0x3b4  currently not used */

/* Defines for Color Graphics Adapter */

#define CGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define CGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA/EGA/VGA */
#define CGA_VIDEO_COMBO        4 /* 4=EGA (ok), 8=VGA (not ok?) */
#define CGA_VIDEO_SUBSYS       0 /* 0=color */
/* #define BASE_CRTC               0x3d4  currently not used */

/* Defines for Enhanched Graphics Adapter, same as CGA */

#define EGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define EGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA/EGA/VGA */
#define EGA_VIDEO_COMBO        4 /* 4=EGA (ok), 8=VGA (not ok?) */
#define EGA_VIDEO_SUBSYS       0 /* 0=color */
/* #define BASE_CRTC               0x3d4  currently not used */

/* Defines for Video Graphic Array, same as CGA */

#define VGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define VGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA/EGA/VGA */
#define VGA_VIDEO_COMBO        8 /* 4=EGA (ok), 8=VGA (not ok?) */
#define VGA_VIDEO_SUBSYS       0 /* 0=color */
/* #define BASE_CRTC               0x3d4  currently not used */


#define PLAINVGA	0
#define TRIDENT		1
#define ET4000		2
#define DIAMOND		3
#define S3		4
#define AVANCE		5
#define ATI		6
#define CIRRUS		7
#define MATROX		8
#define WDVGA		9
#define SIS		10
#define SVGALIB	11
#define MAX_CARDTYPE	SVGALIB

/* title and change config definitions */
#define TITLE_EMUNAME_MAXLEN 128
#define TITLE_APPNAME_MAXLEN 25

#define CHG_TITLE	1
#define CHG_FONT	2
#define CHG_MAP		3
#define CHG_UNMAP	4
#define CHG_WINSIZE	5
#define CHG_TITLE_EMUNAME	6
#define CHG_TITLE_APPNAME	7
#define CHG_TITLE_SHOW_APPNAME	8
#define CHG_BACKGROUND_PAUSE	9
#define GET_TITLE_APPNAME	10

EXTERN void get_screen_size (void);
EXTERN void set_video_bios_size(void);
EXTERN void X_handle_events(void);
EXTERN void X_blink_cursor(void);
EXTERN void init_vga_card(void);
EXTERN void scr_state_init(void);

extern void set_console_video(void);
extern void clear_console_video(void);
extern inline void console_update_cursor (int, int, int, int);
extern int load_file(char *name, int foffset, char *mstart, int msize);


/* moved here from s3.c --AV  */
#define BASE_8514_1	0x2e8
#define BASE_8514_2	0x148

static __inline__ int emu_video_retrace_on(void)
{
  return (config.emuretrace>1? set_ioperm(0x3da,1,0):0);
}

static __inline__ int emu_video_retrace_off(void)
{
  return (config.emuretrace>1? set_ioperm(0x3c0,1,1),set_ioperm(0x3da,1,1):0);
}

#endif
