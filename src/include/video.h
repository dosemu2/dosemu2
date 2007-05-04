/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef VIDEO_H
#define VIDEO_H

#include "extern.h"
#include "port.h"
typedef unsigned char byte;

typedef struct { byte end, start; } cshape;

extern void gettermcap(int,int *, int *);

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

#define CURSOR_START(c) (((cshape*)&c)->start)
#define CURSOR_END(c)   (((cshape*)&c)->end)
#define NO_CURSOR 0x2000

/* 
 * xterm allows to set virtually any screen size, but DOS applications
 * and the BIOS really can't handle more than these numbers of rows and
 * columns (128*255*2 < 65535)
 */
#define MAX_COLUMNS 255
#define MAX_LINES 128

/********************************************/

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
   int (*priv_init)(void);     /* does setup which needs root privileges */
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
   void (*handle_events)(void);    /* event handler (not strictly video but
                                  related, optional) */
};

extern struct video_system *Video;
#if USE_DUALMON
extern struct video_system *Video_default;
#endif
extern struct video_system Video_none, Video_graphics, Video_X,
  Video_console, Video_hgc, Video_term, Video_SDL;

EXTERN ushort *prev_screen;  /* pointer to currently displayed screen   */
                             /* used&updated by Video->update_screen    */

EXTERN int video_mode INIT(0);
EXTERN int video_combo INIT(0);

/* bit mask for testing vm86s.screen_bitmap */
EXTERN unsigned int screen_mask;

EXTERN unsigned char video_initialized INIT(0);
extern boolean set_video_mode(int);
extern unsigned short *screen_adr(int page);

/* Values are set by video_config_init depending on video-card defined in config */
/* Values are set from emu.c depending on video-config */


EXTERN void *virt_text_base INIT(0);
EXTERN int phys_text_base INIT(0);
EXTERN int v_8514_base INIT(0);

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
    TRIDENT,
    ET4000,
    DIAMOND,
    S3,
    AVANCE,
    ATI,
    CIRRUS,
    MATROX,
    WDVGA,
    SIS,
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

extern void clear_console_video(void);
extern int console_size(void);
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
