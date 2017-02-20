#ifndef VIDEO_H
#define VIDEO_H

#include "port.h"

extern void gettermcap(int,int *, int *);

/* if you set this to 1, then you will be able to use your MDA + monitor
   as second display. This currently is possible together with
   Video_term, Video_console and Video_graphics.
   You also must set the keyword "dualmon" in the video section
   of your /etc/dosemu.conf.
   With dualmonitor support you can run CAD-programs, debuggers, or
   simply change your PC-console with "mode mono"
*/
#define USE_DUALMON 1
#define CURSOR_START(c) ((c).b.start)
#define CURSOR_END(c)   ((c).b.end)
#define NO_CURSOR 0x2000

/*
 * xterm allows to set virtually any screen size, but DOS applications
 * and the BIOS really can't handle more than these numbers of rows and
 * columns (128*255*2 < 65535)
 */
#define MAX_COLUMNS 255
#define MAX_LINES 128

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

struct vid_mode_params {
    int mode_class;
    int x_res;
    int y_res;
    int w_x_res;
    int w_y_res;
    int text_width;
    int text_height;
};

struct video_system {
   int (*priv_init)(void);     /* does setup which needs root privileges */
   int (*init)(void);              /* does all frontend-specific setup,
                                  like mapping video memory, opening XWindow,
                                  etc. */
   int (*late_init)(void);     /* init with vm86() available (vbe) */
   void (*early_close)(void);  /* close with vm86() available (vbe) */
   void (*close)(void);

   int (*setmode)(struct vid_mode_params);   /* type=0 currently (text mode) */

   int (*update_screen)(void);     /* (partially) update screen and cursor from the
                                  video memory. called from sigalrm handler */
   int (*change_config)(unsigned item, void *buf); /* change configuration
                                  e.g. window title (optional) */
   void (*handle_events)(void);    /* event handler (not strictly video but
                                  related, optional) */
   const char *name;
   void (*vt_activate)(int num);
};

extern struct video_system *Video;
extern int video_mode;
extern int video_combo;

extern int video_initialized;
extern boolean set_video_mode(int);
extern unsigned screen_adr(int page);

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

/* Defines for Color Graphics Adapter */

#define CGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define CGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA/EGA/VGA */
#define CGA_VIDEO_COMBO        4 /* 4=EGA (ok), 8=VGA (not ok?) */

/* Defines for Enhanched Graphics Adapter, same as CGA */

#define EGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define EGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA/EGA/VGA */
#define EGA_VIDEO_COMBO        4 /* 4=EGA (ok), 8=VGA (not ok?) */

/* Defines for Video Graphic Array, same as CGA */

#define VGA_INIT_SCREEN_MODE   3 /* 80x25 VGA color */
#define VGA_CONF_SCREEN_MODE   (2<<4)	/* (2<<4)=80x25 color CGA/EGA/VGA */
#define VGA_VIDEO_COMBO        8 /* 4=EGA (ok), 8=VGA (not ok?) */


enum {
    PLAINVGA,
    SVGALIB,
    VESA,
};
#define MAX_CARDTYPE	VESA

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
#define CHG_FULLSCREEN	11

extern int load_file(char *name, int foffset, unsigned char *mstart, int msize);
extern void register_video_client(struct video_system *vid);
extern struct video_system *video_get(const char *name);

#endif
