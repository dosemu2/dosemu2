/* mouse.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/11/13 00:43:00 $
 * $Source: /home/src/dosemu0.60/mouse/RCS/mouse.c,v $
 * $Revision: 2.15 $
 * $State: Exp $
 *
 * $Log: mouse.c,v $
 * Revision 2.15  1994/11/13  00:43:00  root
 * Prep for Hans's latest.
 *
 * Revision 2.14  1994/10/14  18:02:19  root
 * Prep for pre53_27.tgz
 *
 * Revision 2.13  1994/09/22  23:53:01  root
 * Prep for pre53_21.
 *
 * Revision 2.12  1994/09/20  01:55:27  root
 * Prep for pre53_21.
 *
 * Revision 2.11  1994/09/11  01:03:30  root
 * Prep for pre53_19.
 *
 * Revision 2.10  1994/08/25  00:52:24  root
 * Prep for pre53_16.
 *
 * Revision 2.9  1994/08/14  02:54:28  root
 * Rain's CLEANUP and DOS in a X box MOUSE additions.
 *
 * Revision 2.8  1994/08/09  01:51:17  root
 * Prep for pre53_11.
 *
 * Revision 2.7  1994/08/01  14:27:57  root
 * Prep for pre53_7  with Markks latest, EMS patch, and Makefile changes.
 *
 * Revision 2.6  1994/07/09  14:30:38  root
 * prep for pre53_3.
 *
 * Revision 2.5  1994/07/05  00:01:31  root
 * Prep for Markkk's NCURSES patches.
 *
 * Revision 2.4  1994/06/27  02:16:59  root
 * Prep for pre53
 *
 * Revision 2.3  1994/06/17  00:14:37  root
 * Let's wrap it up and call it DOSEMU0.52.
 *
 * Revision 2.2  1994/06/14  23:10:21  root
 * Alan's latest mice patches
 *
 * Revision 2.2  1994/06/14  23:10:21  root
 * Alan's latest mice patches
 *
 * Revision 2.1  1994/06/12  23:17:50  root
 * Wrapping up prior to release of DOSEMU0.52.
 *
 * Revision 1.4  1994/06/10  23:22:02  root
 * prep for pre51_25.
 *
 * Revision 1.3  1994/06/05  21:18:15  root
 * Prep for pre51_24.
 *
 * Revision 1.2  1994/05/24  01:24:18  root
 * Lutz's latest, int_queue_run() update.
 *
 * Revision 1.1  1994/05/04  22:17:27  root
 * Initial revision
 *
 * Revision 1.16  1994/04/30  01:05:16  root
 * Clean up.
 *
 * Revision 1.15  1994/04/27  23:39:57  root
 * Lutz's patches to get dosemu up under 1.1.9.
 *
 * Revision 1.14  1994/04/27  21:34:15  root
 * Jochen's Latest.
 *
 * Revision 1.13  1994/04/23  20:51:40  root
 * Get new stack over/underflow working in VM86 mode.
 *
 * Revision 1.12  1994/04/18  22:52:19  root
 * Ready pre51_7.
 *
 * Revision 1.11  1994/04/16  01:28:47  root
 * Prep for pre51_6.
 *
 * Revision 1.10  1994/04/07  20:50:59  root
 * More updates.
 *
 * Revision 2.9  1994/04/06  00:57:16  root
 * Made serial config more flexible, and up to 4 ports.
 *
 * Revision 1.8  1994/04/04  22:51:55  root
 * Patches for PS/2 mice.
 *
 * Revision 1.7  1994/03/23  23:24:51  root
 * Prepare to split out do_int.
 *
 * Revision 1.6  1994/03/04  15:23:54  root
 * Run through indent.
 *
 * Revision 1.5  1994/03/04  14:46:13  root
 * Jochen patches.
 *
 * Revision 1.4  1994/01/20  21:14:24  root
 * Indent.
 *
 * Revision 1.3  1993/11/30  22:21:03  root
 * Final Freeze for release pl3
 *
 * Revision 1.2  1993/11/30  21:26:44  root
 * Chips First set of patches, WOW!
 *
 * Revision 1.1  1993/11/12  12:32:17  root
 * Initial revision
 *
 * Revision 1.1  1993/07/07  00:49:06  root
 * Initial revision
 *
 * Revision 1.3  1993/05/04  05:29:22  root
 * added console switching, new parse commands, and serial emulation
 *
 * Revision 1.2  1993/04/05  17:25:13  root
 * big pre-49 checkit; EMS, new MFS redirector, etc.
 *
 * Revision 1.1  1993/02/24  11:33:24  root
 * Initial revision
 *
 */
#define MOUSE_C

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>

#include "bios.h"
#include "emu.h"
#include "memory.h"
#include "video.h"		/* video base address */
#include "mouse.h"
#include "serial.h"
#include "port.h"
#include "termio.h"

#include "vc.h"
#include "port.h"
extern int get_perm ();
extern int release_perm ();

extern struct screen_stat scr_state;

#ifdef X_SUPPORT
extern void X_change_mouse_cursor(int);
#endif

extern void open_kmem(void), close_kmem(void);

void DOSEMUSetupMouse(void);

extern struct config_info config;

void mouse_reset(void), mouse_cursor(int), mouse_pos(void), mouse_setpos(void),
 mouse_setxminmax(void), mouse_setyminmax(void), mouse_set_tcur(void),
 mouse_set_gcur(void), mouse_setsub(void), mouse_bpressinfo(void), mouse_brelinfo(void),
 mouse_mickeys(void), mouse_version(void), mouse_enable_internaldriver(void),
 mouse_disable_internaldriver(void), mouse_software_reset(void), 
 mouse_getgeninfo(void), mouse_exclusionarea(void), mouse_setcurspeed(void),
 mouse_undoc1(void), mouse_storestate(void), mouse_restorestate(void),
 mouse_getmaxcoord(void), mouse_getstorereq(void), mouse_setsensitivity(void),
 mouse_detsensitivity(void), mouse_detstatbuf(void), mouse_excevhand(void);

/* mouse movement functions */
void mouse_move(void), mouse_lb(void), mouse_rb(void), mouse_do_cur(void);

/* called when mouse changes */
static void mouse_delta(int);

static int mouse_events = 0;

boolean gfx_cursor;

mouse_t mice[MAX_MOUSE] ;
struct mouse_struct mouse;

static long mousecursormask[HEIGHT] =  {
  0x00000000L,  /*0000000000000000*/
  0x40000000L,  /*0100000000000000*/
  0x60000000L,  /*0110000000000000*/
  0x70000000L,  /*0111000000000000*/
  0x78000000L,  /*0111100000000000*/
  0x7c000000L,  /*0111110000000000*/
  0x7e000000L,  /*0111111000000000*/
  0x7f000000L,  /*0111111100000000*/
  0x7f800000L,  /*0111111110000000*/
  0x7f000000L,  /*0111111100000000*/
  0x7c000000L,  /*0111110000000000*/
  0x46000000L,  /*0100011000000000*/
  0x06000000L,  /*0000011000000000*/
  0x03000000L,  /*0000001100000000*/
  0x03000000L,  /*0000001100000000*/
  0x00000000L   /*0000000000000000*/
};

#if 1
static long mousescreenmask[HEIGHT] =  {
  0x3fffffffL,  /*0011111111111111*/
  0x1fffffffL,  /*0001111111111111*/
  0x0fffffffL,  /*0000111111111111*/
  0x07ffffffL,  /*0000011111111111*/
  0x03ffffffL,  /*0000001111111111*/
  0x01ffffffL,  /*0000000111111111*/
  0x00ffffffL,  /*0000000011111111*/
  0x007fffffL,  /*0000000001111111*/
  0x003fffffL,  /*0000000000111111*/
  0x007fffffL,  /*0000000001111111*/
  0x01ffffffL,  /*0000000111111111*/
  0x10ffffffL,  /*0001000011111111*/
  0xb0ffffffL,  /*1011000011111111*/
  0xf87fffffL,  /*1111100001111111*/
  0xf87fffffL,  /*1111100001111111*/
  0xfcffffffL   /*1111110011111111*/
};
#endif

static ushort mousetextscreen = 0xffff;
static ushort mousetextcursor = 0x7f00;
static ushort mousegraphscreen = 0xffff;
static ushort mousegraphcursor = 0x7f00;

int delta_x, delta_y;

mouse_helper(void) 
{
  if (!mice->intdrv) {
    m_printf("MOUSE No Internaldriver set, exiting mouse_helper()\n");
    LWORD(eax) = 0xffff;
    return;
  }
 
  LWORD(eax) = 0;		/* Set successful completion */
    
  switch (LO(bx)) {
  case 0:				/* Reset iret for mouse */
    m_printf("MOUSE move iret !\n");
    SETIVEC(0x33, Mouse_SEG, Mouse_ROUTINE_OFF);
    SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);
    break;
  case 1:				/* Select Microsoft Mode */
    m_printf("MOUSE Microsoft Mouse selected.\n");
    mouse.mode = TRUE;
    break;
  case 2:				/* Select PC Mouse (3 button) */
    if (mice->emulate3buttons == TRUE) {
      m_printf("MOUSE PC Mouse selected.\n");
      mouse.mode = FALSE;
    } else { 
      m_printf("MOUSE PC Mouse cannot be selected, emulate3buttons not set.\n");
      LWORD(eax) = 0x00ff;
    }
    break;
  case 3:				/* Tell me what mode we are in ? */
    if (mouse.mode)
      HI(bx) = 0x10;		/* We are currently in Microsoft Mode */
    else
      HI(bx) = 0x20;		/* We are currently in PC Mouse Mode */
    break;
  case 0xff:
    m_printf("MOUSE Checking InternalDriver presence !!\n");
    break;
  default:
    m_printf("MOUSE Unknown mouse_helper function\n");
    LWORD(eax) = 1;		/* Set unsuccessful completion */
  }
}

void
mouse_int(void)
{
  m_printf("MOUSEALAN: int 0x%x ebx=%x\n", LWORD(eax), LWORD(ebx));
  switch (LWORD(eax)) {
  case 0:			/* Mouse Reset/Get Mouse Installed Flag */
    mouse_reset();
    break;

  case 1:			/* Show Mouse Cursor */
    mouse_cursor(1);
    break;

  case 2:			/* Hide Mouse Cursor */
    mouse_cursor(-1);
    break;

  case 3:			/* Get Mouse Position and Button Status */
    mouse_pos();
    break;

  case 4:			/* Set Cursor Position */
    mouse_setpos();
    break;

  case 5:			/* Get mouse button press info */
    mouse_bpressinfo();
    break;

  case 6:			/* Get mouse button release info */
    mouse_brelinfo();
    break;

  case 7:			/* Set Mouse Horizontal Min/Max Position */
    mouse_setxminmax();
    break;

  case 8:			/* Set Mouse Vertical Min/Max Position */
    mouse_setyminmax();
    break;

  case 9:			/* Set mouse gfx cursor */
    mouse_set_gcur();
    break;

  case 0xa:			/* Set mouse text cursor */
    mouse_set_tcur();
    break;

  case 0xb:			/* Read mouse motion counters */
    mouse_mickeys();
    break;

  case 0xc:			/* Set mouse subroutine */
    mouse_setsub();
    break;

  case 0xf:
    mouse_setcurspeed();	/* Set cursor speed */
    break;

  case 0x10:
    mouse_exclusionarea();	/* Define mouse exclusion zone */
    break;

  case 0x11: 
    mouse_undoc1();		/* Undocumented */
    break;

  case 0x14:
    mouse_excevhand();		/* Exchange Event Handler */
    break;

  case 0x15:			/* Get size of buffer needed to hold state */
    mouse_detstatbuf();
    break;

  case 0x16:			/* Save mouse driver state */
    mouse_storestate();
    break;

  case 0x17:			/* Restore mouse driver state */
    mouse_restorestate();
    break;

  case 0x1a:			/* Set mouse sensitivity */
    mouse_setsensitivity();
    break;

  case 0x1b:
    mouse_detsensitivity();	/* Determine mouse sensitivity */
    break;

  case 0x1f:
    mouse_disable_internaldriver();		/* Disable Mouse Driver */
    break;

  case 0x20:
    mouse_enable_internaldriver();		/* Enable Mouse Driver */
    break;

  case 0x21:			
    mouse_software_reset();	/* Perform Software reset on mouse */
    break;

  case 0x22:			/* Set language for messages */
    m_printf("MOUSE: Set LANGUAGE, ignoring.\n");
    break;			/* Ignore as we are US version only */

  case 0x23:
    m_printf("MOUSE: Get LANGUAGE\n");
    LWORD(ebx) = 0x0000;	/* Return US/ENGLISH */
    break;

  case 0x24:			/* Get driver version/mouse type/IRQ */
    mouse_version();
    break;

  case 0x25:
    mouse_getgeninfo();		/* Get general driver information */
    break;

  case 0x26:			/* Return Maximal Co-ordinates */
    mouse_getmaxcoord();
    break;

  case 0x30:
    LWORD(eax) = 0xFFFF;	/* We are currently v7.01, need 7.04+ */
    break;
 
  case 0x42:
    mouse_getstorereq();	/* Get storage requirements */
    break;

  default:
    error("MOUSE: function 0x%04x not implemented\n", LWORD(eax));
    break;
  }
}

void
mouse_exclusionarea(void)
{
  mouse.exc_ux = LWORD(ecx);
  mouse.exc_uy = LWORD(edx);
  mouse.exc_lx = LWORD(esi);
  mouse.exc_ly = LWORD(edi);
}

void
mouse_getstorereq(void)
{
  if (mouse.mode) 
    LWORD(eax) = 0x42;                /* Configures mouse for Microsoft Mode */
  else {
    LWORD(eax) = 0xffff;              /* Configures mouse for PC Mouse Mode */
    LWORD(ebx) = sizeof(mouse);       /* applies to Genius Mouse too */
  }
}

void
mouse_getmaxcoord(void)
{
  LWORD(ebx) = (mice->intdrv ? 1 : 0);
  LWORD(ecx) = mouse.maxx;
  LWORD(edx) = mouse.maxy;
  m_printf("MOUSE: COORDINATES: x: %d, y: %d\n",mouse.maxx,mouse.maxy);
}

void
mouse_getgeninfo(void)
{
  /* Set AX to 0 */
  LWORD(eax) = 0x0000;

  /* Bits 0-7 are count of currently-active Mouse Display Drivers (MDD) */
  LWORD(eax) |= 0x0000;			/* For now none ! */

  /* Bits 8-11 are the interrupt rate */
  LWORD(eax) |= 0x0000;			/* Don't allow interrupts */

  /* Bits 12-13 are Mouse cursor information */
  if (gfx_cursor)
    LWORD(eax) |= 0x2000;		/* Set graphic cursor */
  else
    LWORD(eax) |= 0x0000;		/* Set software text cursor */

  /* Bit 14 sets driver is newer integrated type */
  LWORD(eax) |= 0x0000;			/* Not the newer integrated type */

  /* Bit 15, 0=COM file, 1=SYS file */
  LWORD(eax) |= 0x8000;			/* Set SYS file */
}

void
mouse_software_reset(void)
{
  m_printf("MOUSE: software reset on mouse\n");

  /* Disable cursor, and de-install current event handler */
  mouse.cursor_on = -1;      
  mouse.cs=0;
  mouse.ip=0;
  SETIVEC(0x33, Mouse_SEG, Mouse_OFF + 4);
  SETIVEC(0x74, Mouse_SEG, Mouse_OFF + 4);

#ifdef X_SUPPORT
  if (config.X)
    X_change_mouse_cursor(0);
#endif
  
  /* Return 0x21 on success, 0xffff on failure */
  LWORD(eax) = 0x21;  

  /* Return Number of mouse buttons */
  if (!mouse.mode)
    LWORD(ebx) = 3;
  else
    LWORD(ebx) = 2;
}

void
mouse_detsensitivity(void)
{
  LWORD(ebx) = mouse.speed_x;         /* horizontal speed */
  LWORD(ecx) = mouse.speed_y;         /* vertical speed */
  LWORD(edx) = mouse.threshold;       /* double speed threshold */
}

void
mouse_setsensitivity(void)
{
  mouse.speed_x = LWORD(ebx);
  mouse.speed_y = LWORD(ecx);
  mouse.threshold = LWORD(edx);
}

void
mouse_restorestate(void)
{
  struct mouse_struct *mpt;
  mpt=&mouse;
  memcpy(mpt, (u_char *)(LWORD(es) << 4)+LWORD(edx), sizeof(mouse));
  m_printf("MOUSE: Restore mouse state\n");
}


void
mouse_storestate(void)
{
  struct mouse_struct *mpt;
  mpt=&mouse;
  memcpy((u_char *)(LWORD(es) << 4)+LWORD(edx), mpt, sizeof(mouse));
  m_printf("MOUSE: Save mouse state\n");
}


void
mouse_detstatbuf(void)
{
  LWORD(ebx) = sizeof(mouse);
  m_printf("MOUSE: get size of state buffer 0x%04x\n", LWORD(ebx));
}

void
mouse_excevhand(void)
{
  unsigned short tmp1, tmp2, tmp3;

  m_printf("MOUSE: exchange subroutines\n");
  tmp1 = mouse.mask;
  tmp2 = mouse.cs;
  tmp3 = mouse.ip;
  mouse_setsub();
  LWORD(ecx) = tmp1;
  LWORD(es) = tmp2;
  LWORD(edx) = tmp3;
}

void
mouse_undoc1(void)
{
  /* This routine is for GENIUS mice only > version 9.06 
   * This function is not defined by my documentation */

  if (!mouse.mode && (MOUSE_VERSION > 0x0906)) {
    LWORD(eax) = 0xffff;      /* Genius mouse driver, return 2 buttons */
    LWORD(ebx) = 2;
  } else {
    /* My documentation says leave alone */
  }
}

void
mouse_setcurspeed(void)
{
  m_printf("MOUSE: function 0f: cx=%04x, dx=%04x\n",LWORD(ecx),LWORD(edx));
  mouse.speed_x = LWORD(ecx);
  mouse.speed_y = LWORD(edx);
}

void 
mouse_reset(void)
{
  m_printf("MOUSE: reset mouse/installed!\n");

  /* de-install current event handler */
  mouse.cs=0;
  mouse.ip=0;
  SETIVEC(0x33, Mouse_SEG, Mouse_OFF + 4);
  SETIVEC(0x74, Mouse_SEG, Mouse_OFF + 4);

  /* Return 0xffff on mouse installed, 0x0000 - no mouse driver installed */
  LWORD(eax) = 0xffff;

  /* Return number of mouse buttons */
  if (!mouse.mode) 
    LWORD(ebx)=3; 
  else 
    LWORD(ebx)=2; 

  mouse.minx = mouse.miny = 0;

  /* Set up Mouse Maximum co-ordinates, then convert to pixel resolution 
   * Here we assume a maximum text mode of 132x60, reasonable assumption ?
   * Possibly, but could run into problems. FIX ME! But Later.... */
  m_printf("MOUSE: SCANLINES %d\n",text_scanlines);
/*
  mouse.maxx = bios_screen_columns;
  mouse.maxy = bios_rows_on_screen_minus_1;
*/
  mouse.maxx = co * 8;
  mouse.maxy = li * vga_font_height;
/*
  mouse.maxx = ((mouse.maxx <= 132) ? (mouse.maxx * 8) : (mouse.maxx));
  mouse.maxy = ((mouse.maxy <= 60) ? ((mouse.maxy+1) * 8) : (mouse.maxy));
*/
  m_printf("MOUSE: COLUMNS %d, LINES %d\n",mouse.maxx, mouse.maxy);
  mouse.maxx -= 1;
  mouse.maxy -= 1;

  mouse.points = (*(unsigned short *)0x485);
  mouse.ratio = 1;
  mouse.cursor_on = -1;
#ifdef X_SUPPORT
  if (config.X)
     X_change_mouse_cursor(0);
#endif
  mouse.cursor_type = 0;
  mouse.lbutton = mouse.mbutton = mouse.rbutton = 0;
  mouse.oldlbutton = mouse.oldmbutton = mouse.oldrbutton = 1;
  mouse.lpcount = mouse.mpcount = mouse.rpcount = 0;
  mouse.lrcount = mouse.mrcount = mouse.rrcount = 0;

  mouse.x = mouse.y = delta_x = delta_y = 0;
  mouse.cx = mouse.cy = 0;
  mouse.speed_x = 16;
  mouse.speed_y = 8;

  mouse.ip = mouse.cs = 0;
  mouse.mask = 0;		/* no interrupts */

  mouse.threshold = 64;

  mouse.lpx = mouse.lpy = mouse.lrx = mouse.lry = 0;
  mouse.rpx = mouse.rpy = mouse.rrx = mouse.rry = 0;

  mouse.mickeyx = mouse.mickeyy = 0;

  mousetextscreen = mousegraphscreen = 0xffff;
  mousetextcursor = mousegraphcursor = 0x7f00;
}

void 
mouse_cursor(int flag)
{
  mouse.cursor_on = mouse.cursor_on + flag;
#ifdef X_SUPPORT
  if (config.X)
	  X_change_mouse_cursor(flag);
#endif
  m_printf("MOUSE: %s mouse cursor %d\n", mouse.cursor_on ? "hide" : "show", mouse.cursor_on);
}

void 
mouse_pos(void)
{
  m_printf("MOUSE: get mouse position x:%d, y:%d, b(l%d m%d r%d)\n", mouse.x,
	   mouse.y, mouse.lbutton, mouse.mbutton, mouse.rbutton);
  LWORD(ecx) = mouse.x;
  LWORD(edx) = mouse.y;
  LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
  if (!mouse.mode)
     LWORD(ebx) |= (mouse.mbutton ? 4 : 0);
}

/* Set mouse position */
void 
mouse_setpos(void)
{
  mouse.x = LWORD(ecx);
  mouse.y = LWORD(edx) ;
  m_printf("MOUSE: set cursor pos x:%d, y:%d\n", mouse.x, mouse.y);
}

void 
mouse_bpressinfo(void)
{
  m_printf("MOUSE: Button press info\n");

  switch(LWORD(ebx)) {
 
  case 0:				/* left button */
    LWORD(ebx) = mouse.lpcount;
    mouse.lpcount = 0;
    LWORD(ecx) = mouse.lpx;
    LWORD(edx) = mouse.lpy;
    break;
 
  case 1:				/* right button */
    LWORD(ebx) = mouse.rpcount;
    mouse.rpcount = 0;
    LWORD(ecx) = mouse.rpx;
    LWORD(edx) = mouse.rpy;
    break;

  case 2:				/* middle button */
    if (!mouse.mode) {
      LWORD(ebx) = mouse.mpcount;
      mouse.mpcount = 0;
      LWORD(ecx) = mouse.mpx;
      LWORD(edx) = mouse.mpy;
    }
    break;
  }
  
  LWORD(eax) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
  if (!mouse.mode)
     LWORD(eax) |= (mouse.mbutton ? 4 : 0);
}

void 
mouse_brelinfo(void)
{
  m_printf("MOUSE: Button release info\n");

  switch(LWORD(ebx)) {
 
  case 0:				/* left button */
    LWORD(ebx) = mouse.lrcount;
    mouse.lrcount = 0;
    LWORD(ecx) = mouse.lrx;
    LWORD(edx) = mouse.lry;
    break; 
 
  case 1:				/* right button */
    LWORD(ebx) = mouse.rrcount;
    mouse.rrcount = 0;
    LWORD(ecx) = mouse.rrx;
    LWORD(edx) = mouse.rry;

  case 2:				/* middle button */
    if (!mouse.mode) {
      LWORD(ebx) = mouse.mrcount;
      mouse.mrcount = 0;
      LWORD(ecx) = mouse.mrx;
      LWORD(edx) = mouse.mry;
    }
    break;
  }
  
  LWORD(eax) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
  if (!mouse.mode)
     LWORD(eax) |= (mouse.mbutton ? 4 : 0);
}

void 
mouse_setxminmax(void)
{
  m_printf("MOUSE: set horz. min: %d, max: %d\n", LWORD(ecx), LWORD(edx));

  /* if the min > max, they are swapped */
  mouse.minx = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.maxx = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
}

void 
mouse_setyminmax(void)
{
  m_printf("MOUSE: set vert. min: %d, max: %d\n", LWORD(ecx), LWORD(edx));

  /* if the min > max, they are swapped */
  mouse.miny = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.maxy = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
}

void 
mouse_set_gcur(void)
{
  m_printf("MOUSE: set gfx cursor...hspot: %d, vspot: %d, masks: %04x:%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(es), LWORD(edx));
#if 1
  gfx_cursor = TRUE;
#endif
  if (LWORD(ebx)==0) {
	  mousegraphscreen = LWORD(ecx);
	  mousegraphcursor = LWORD(edx);
  } else {
	  mousegraphscreen = 0x7fff;
	  mousegraphcursor = 0xff00;
  }
}

void 
mouse_set_tcur(void)
{
  m_printf("MOUSE: set text cursor...type: %d, start: 0x%04x, end: 0x%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(edx));
#if 1
  gfx_cursor = FALSE;
#endif
  if (LWORD(ebx)==0) {
	  mousetextscreen = LWORD(ecx);
	  mousetextcursor = LWORD(edx);
  } else {
	  mousetextscreen = 0x7fff;
	  mousetextcursor = 0xff00;
  }
}

void 
mouse_setsub(void)
{
  mouse.cs = LWORD(es);
  mouse.ip = LWORD(edx);

  *mouse.csp = mouse.cs;
  *mouse.ipp = mouse.ip;

  mouse.mask = LWORD(ecx);

  m_printf("MOUSE: user defined sub %04x:%04x, mask 0x%02x\n",
	   LWORD(es), LWORD(edx), LWORD(ecx));
}

void 
mouse_mickeys(void)
{
  m_printf("MOUSE: read mickeys %d %d\n", mouse.mickeyx, mouse.mickeyy);
  mouse.mickeyx = (mouse.x - delta_x) * mouse.speed_x;
  mouse.mickeyy = (mouse.y - delta_y) * mouse.speed_y;
  LWORD(ecx) = mouse.mickeyx;
  LWORD(edx) = mouse.mickeyy;
  delta_x = mouse.x;
  delta_y = mouse.y;
}

void 
mouse_version(void)
{
  LWORD(ebx) = MOUSE_VERSION;
  HI(cx) = 4;			/* ps2 mouse */
  LO(cx) = 0;			/* Basically, treat the internal mouse */
				/* driver as an IBM PS/2 mouse */
  m_printf("MOUSE: get version %04x\n", LWORD(ebx));
}

void
mouse_disable_internaldriver()
{
  LWORD(eax) = 0x001F;
  LWORD(es) = mouse.cs;
  LWORD(ebx) = mouse.ip;

  mice->intdrv = FALSE;

  m_printf("MOUSE: Disable InternalDriver\n");
}

void
mouse_enable_internaldriver()
{
  mice->intdrv = TRUE;

  m_printf("MOUSE: Enable InternalDriver\n");
}

void 
mouse_keyboard(int sc)
{
  switch (sc) {
  case 0x50:
    mouse.rbutton = mouse.lbutton = mouse.mbutton = 0;
    mouse_move();
    break;
  case 0x4b:
    mouse.rbutton = mouse.lbutton = mouse.mbutton = 0;
    mouse_move();
    break;
  case 0x4d:
    mouse.rbutton = mouse.lbutton = mouse.mbutton = 0;
    mouse_move();
    break;
  case 0x48:
    mouse.rbutton = mouse.lbutton = mouse.mbutton = 0;
    mouse_move();
    break;
  case 0x47:
    mouse_lb();
    break;
  case 0x4f:
    mouse_rb();
    break;
  default:
    m_printf("MOUSE: keyboard_mouse(), sc 0x%02x unknown!\n", sc);
  }
}

void 
mouse_move(void)
{
  if (mouse.x <= mouse.minx) mouse.x = mouse.minx;
  if (mouse.y <= mouse.miny) mouse.y = mouse.miny;
  if (mouse.x >= mouse.maxx) mouse.x = mouse.maxx;
  if (mouse.y >= mouse.maxy) mouse.y = mouse.maxy;

  /* Check exclusion zone, then turn off cursor if necessary */
  if (mouse.exc_lx || mouse.exc_uy)
    if ((mouse.x > mouse.exc_ux) && 
        (mouse.x < mouse.exc_lx) &&
        (mouse.y > mouse.exc_uy) &&
        (mouse.y < mouse.exc_ly))
      mouse.cursor_on = -1;
  else
    mouse.cursor_on = 0;

  m_printf("MOUSE: move. x=%x,y=%x\n", mouse.x, mouse.y);
   
  mouse.cx = mouse.x / 8;
  mouse.cy = mouse.y / 8;

  mouse_delta(DELTA_CURSOR);
}

void 
mouse_lb(void)
{
  m_printf("MOUSE: left button %s\n", mouse.lbutton ? "pressed" : "released");
  if (!mouse.lbutton) {
    mouse.lrcount++;
    mouse.lrx = mouse.x;
    mouse.lry = mouse.y;
    mouse_delta(DELTA_LEFTBUP);
  }
  else {
    mouse.lpcount++;
    mouse.lpx = mouse.x;
    mouse.lpy = mouse.y;
    mouse_delta(DELTA_LEFTBDOWN);
  }
}

void 
mouse_mb(void)
{
  m_printf("MOUSE: middle button %s\n", mouse.mbutton ? "pressed" : "released");
  if (!mouse.mbutton) {
    mouse.mrcount++;
    mouse.mrx = mouse.x;
    mouse.mry = mouse.y;
    mouse_delta(DELTA_MIDDLEBUP);
  }
  else {
    mouse.mpcount++;
    mouse.mpx = mouse.x;
    mouse.mpy = mouse.y;
    mouse_delta(DELTA_MIDDLEBDOWN);
  }
}

void 
mouse_rb(void)
{
  m_printf("MOUSE: right button %s\n", mouse.rbutton ? "pressed" : "released");
  if (!mouse.rbutton) {
    mouse.rrcount++;
    mouse.rrx = mouse.x;
    mouse.rry = mouse.y;
    mouse_delta(DELTA_RIGHTBUP);
  }
  else {
    mouse.rpcount++;
    mouse.rpx = mouse.x;
    mouse.rpy = mouse.y;
    mouse_delta(DELTA_RIGHTBDOWN);
  }
}

void 
fake_int(void)
{
  unsigned char *ssp;
  unsigned long sp;

  m_printf("MOUSE: fake_int: CS:IP %04x:%04x\n",LWORD(cs),LWORD(eip));
  ssp = (unsigned char *)(LWORD(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, vflags);
  pushw(ssp, sp, LWORD(cs));
  pushw(ssp, sp, LWORD(eip));
  LWORD(esp) -= 6;
#if 0
  do_hard_int(0x74);
#endif
  
}

void 
fake_call(int cs, int ip)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *)(LWORD(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  m_printf("MOUSE: fake_call() shows S: %04x:%04x, CS:IP %04x:%04x\n",
	   LWORD(ss), LWORD(esp), cs, ip);
  pushw(ssp, sp, cs);
  pushw(ssp, sp, ip);
  LWORD(esp) -= 4;
}

void 
fake_pusha(void)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = (unsigned char *)(LWORD(ss)<<4);
  sp = (unsigned long) LWORD(esp);

  pushw(ssp, sp, LWORD(eax));
  pushw(ssp, sp, LWORD(ecx));
  pushw(ssp, sp, LWORD(edx));
  pushw(ssp, sp, LWORD(ebx));
  pushw(ssp, sp, LWORD(esp));
  pushw(ssp, sp, LWORD(ebp));
  pushw(ssp, sp, LWORD(esi));
  pushw(ssp, sp, LWORD(edi));
  LWORD(esp) -= 16;
  pushw(ssp, sp, REG(ds));
  pushw(ssp, sp, REG(es));
  LWORD(esp) -= 4;
}

/*
 * add the event to the current event mask
 */
void
mouse_delta(int event)
{
	mouse_events |= event;
}

/*
 * call user event handler
 */
void
mouse_event()
{
  if (mouse.mask & mouse_events && mouse.cs && mouse.ip) {
    fake_int();
    fake_pusha();

    LWORD(eax) = mouse_events;
    LWORD(ecx) = mouse.x;
    LWORD(edx) = mouse.y;
    LWORD(esi) = mouse.mickeyx;
    LWORD(edi) = mouse.mickeyy;
    LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
    if (!mouse.mode)
      LWORD(ebx) |= (mouse.mbutton ? 4 : 0);

    fake_call(Mouse_SEG, Mouse_OFF + 4);	/* skip to popa */

    REG(cs) = mouse.cs;
    REG(eip) = mouse.ip;

    REG(ds) = *mouse.csp;	/* put DS in user routine */

    m_printf("MOUSE: event %d, x %d ,y %d, mx %d, my %d, b %x\n",
	     mouse_events, mouse.x, mouse.y, mouse.maxx, mouse.maxy, LWORD(ebx));
    m_printf("MOUSE: "
	     "should call %04x:%04x (actually %04x:%04x)\n"
	     ".........jumping to %04x:%04x\n",
	     mouse.cs, mouse.ip, *mouse.csp, *mouse.ipp,
	     LWORD(cs), LWORD(eip));	
  }
  mouse_events = 0;
}


void
mouse_do_cur(void)
{
  char *graph_mem;
  int i;

#if 0
  if (config.vga) {
    if (scr_state.current) {
      get_perm();
      port_out(0x06, GRA_I);
      if ((port_in(GRA_D) & 0x01))
        gfx_cursor=1;
      else
        gfx_cursor=0;
      release_perm();
    } else 
      gfx_cursor=0;
  }
#endif
  if (gfx_cursor)
  {
    if (scr_state.current)
      {
        open_kmem();		/* Open KMEM for graphics screen access */
        graph_mem = (char *) mmap((caddr_t) GRAPH_BASE,
				(size_t) (GRAPH_SIZE),
				PROT_READ | PROT_WRITE,
				MAP_SHARED | MAP_FIXED,
				mem_fd,
				GRAPH_BASE);
        close_kmem();
 
        for (i = 0; i < HEIGHT; i++) {
          graph_mem[mouse.x / 8 +((mouse.y + i - 4) * 80)] = (long)mousescreenmask[i];
        }
       }
  } else {
    unsigned short *p = SCREEN_ADR(READ_BYTE(BIOS_CURRENT_SCREEN_PAGE));

    i=mouse.hidx + mouse.hidy * 80;
    if (p[i] == ((mouse.hidchar & mousetextscreen) ^ mousetextcursor))
       p[i] = mouse.hidchar;
    /* save old char/attr pair */
    mouse.hidx = mouse.cx;
    mouse.hidy = mouse.cy;
    i=mouse.cx + mouse.cy * 80;
    mouse.hidchar = p[i];
    p[i] = (p[i] & mousetextscreen) ^ mousetextcursor;
  }
}

void
mouse_curtick(void)
{
  if ((mouse.cursor_on != 0) || config.X)
    return;

  m_printf("MOUSE: curtick x: %d  y:%d\n", mouse.cx, mouse.cy);
  mouse_do_cur();
}

/* are ip and cs in right order?? */
void
mouse_sethandler(void *f, us * cs, us * ip)
{
  m_printf("MOUSE: sethandler...%p, %p, %p\n",
	   (void *) f, (void *) cs, (void *) ip);
  m_printf("...............CS:%04x.....IP:%04x\n", *cs, *ip);
  mouse.csp = cs;
  mouse.ipp = ip;
}

/*
 * DANG_BEGIN_FUNCTION mouse_init
 *
 * description:
 *  Initialize internal mouse.
 *
 * DANG_END_FUNCTION
 */
void
mouse_init(void)
{
  serial_t *sptr;
  char mouse_ver[]={2,3,4,5,0x14,0x7,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f};
  char *p=(char *)0xefe00;
#if 0 /* Not sure why she's here? 94/09/19 */
  int old_mice_flags = -1;
#endif

  mouse.cursor_on = -1;
  mouse.speed_x = 8;
  mouse.speed_y = 16;
  mouse.mode = TRUE;		/* Microsoft 2 Button Mouse selected */
#ifdef X_SUPPORT
  if (config.X) {
    mice->intdrv = TRUE;
    mice->type = MOUSE_X;
    memcpy(p,mouse_ver,sizeof(mouse_ver));
    m_printf("MOUSE: X Mouse being set\n");
    return;
  }
#endif
  
  if ( ! config.usesX ){
    if (mice->intdrv) {
      m_printf("Opening internal mouse: %s\n", mice->dev);
      mice->fd = DOS_SYSCALL(open(mice->dev, O_RDWR | O_NONBLOCK));
      if (mice->fd == -1) {
 	mice->intdrv = FALSE;
 	mice->type = MOUSE_NONE;
 	return;
      }
      if (mice->type == MOUSE_PS2)            /* want_sigio causes problems */
        add_to_io_select(mice->fd, 0);        /* with my ps2 mouse,set to 0 */
      else                                    /* keithp@its.bt.co.uk        */
        add_to_io_select(mice->fd, 1);
      DOSEMUSetupMouse();
      memcpy(p,mouse_ver,sizeof(mouse_ver));
      return;
    }

    if ((mice->type == MOUSE_PS2) || (mice->type == MOUSE_BUSMOUSE)) {
      mice->fd = DOS_SYSCALL(open(mice->dev, O_RDWR | O_NONBLOCK));
      add_to_io_select(mice->fd, 0); 
      memcpy(p,mouse_ver,sizeof(mouse_ver));
    }
    else {
      int x;
      for (x=0;x<config.num_ser;x++){
        sptr = &com[x];
        if (sptr->mouse) break;
      }
      if (!(sptr->mouse)) {
        m_printf("MOUSE: No mouse configured in serial config! num_ser=%d\n",config.num_ser);
 	mice->intdrv = FALSE;
      }
    }
  }
  else {
    mice->fd = mousepipe;
    if (mice->fd == -1) {
      mice->intdrv = FALSE;
      mice->type = MOUSE_NONE;
      return;
    }
    add_to_io_select(mice->fd, 1);
    DOSEMUSetupMouse();
    memcpy(p,mouse_ver,sizeof(mouse_ver));
    return;
  }

}

void
mouse_close(void)
{
#ifdef X_SUPPORT
  if (config.X || config.usesX) return;
#endif
  
  if (mice->intdrv) {
    DOS_SYSCALL(close(mice->fd));
    return;
  }

  if ((mice->type == MOUSE_PS2) || (mice->type == MOUSE_BUSMOUSE))
    DOS_SYSCALL(close(mice->fd));
}

#undef MOUSE_C
