/* mouse.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1995/05/06 16:27:42 $
 * $Source: /usr/src/dosemu0.60/mouse/RCS/mouse.c,v $
 * $Revision: 2.20 $
 * $State: Exp $
 *
 * $Log: mouse.c,v $
 * Revision 2.20  1995/05/06  16:27:42  root
 * Prep for 0.60.2.
 *
 * Revision 2.19  1995/04/08  22:34:58  root
 * Release dosemu0.60.0
 *
 * Revision 2.18  1995/02/25  21:54:24  root
 * *** empty log message ***
 *
 * Revision 2.18  1995/02/25  21:54:24  root
 * *** empty log message ***
 *
 * Revision 2.17  1995/02/05  16:54:02  root
 * Prep for Scotts patches.
 *
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

#include <stdio.h>
#include <termios.h>
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

#include "dpmi.h"

#include "vc.h"
#include "port.h"
extern int get_perm ();
extern int release_perm ();

#ifdef X_SUPPORT
extern void X_change_mouse_cursor(void);
#endif

/* This is included for video mode support. Please DO NOT remove !
 * mousevid.h will become part of VGA emulator package */
#include "mousevid.h"
#include "gcursor.h"

void DOSEMUSetupMouse(void);

static
void mouse_cursor(int), mouse_pos(void), mouse_setpos(void),
 mouse_setxminmax(void), mouse_setyminmax(void), mouse_set_tcur(void),
 mouse_set_gcur(void), mouse_setsub(void), mouse_bpressinfo(void), mouse_brelinfo(void),
 mouse_mickeys(void), mouse_version(void), mouse_enable_internaldriver(void),
 mouse_disable_internaldriver(void), mouse_software_reset(void), 
 mouse_getgeninfo(void), mouse_exclusionarea(void), mouse_setcurspeed(void),
 mouse_undoc1(void), mouse_storestate(void), mouse_restorestate(void),
 mouse_getmaxcoord(void), mouse_getstorereq(void), mouse_setsensitivity(void),
 mouse_detsensitivity(void), mouse_detstatbuf(void), mouse_excevhand(void),
 mouse_largecursor(void), mouse_doublespeed(void), mouse_alternate(void),
 mouse_detalternate(void), mouse_hardintrate(void), mouse_disppage(void),
 mouse_detpage(void);

/* mouse movement functions */
void mouse_reset(int);
void mouse_move(void), mouse_lb(void), mouse_rb(void);
static void mouse_do_cur(void), mouse_update_cursor(void);
static void mouse_reset_to_current_video_mode(void);

/* graphics cursor */
void graph_cursor(void), text_cursor(void);
void graph_plane(int);

/* called when mouse changes */
static void mouse_delta(int);

static int mouse_events = 0;
static mouse_erase_t mouse_erase;

#if 0
mouse_t mice[MAX_MOUSE] ;
struct mouse_struct mouse;
#endif

static inline int mouse_roundx(int x) 
{ 
	return x & ~((1 << mouse.xshift)-1); 
}

static inline int mouse_roundy(int y) 
{
	return y & ~((1 << mouse.yshift)-1); 
}

static short default_graphcursormask[HEIGHT] =  {
  0x0000,  /*0000000000000000*/
  0x4000,  /*0100000000000000*/
  0x6000,  /*0110000000000000*/
  0x7000,  /*0111000000000000*/
  0x7800,  /*0111100000000000*/
  0x7c00,  /*0111110000000000*/
  0x7e00,  /*0111111000000000*/
  0x7f00,  /*0111111100000000*/
  0x7f80,  /*0111111110000000*/
  0x7f00,  /*0111111100000000*/
  0x7c00,  /*0111110000000000*/
  0x4600,  /*0100011000000000*/
  0x0600,  /*0000011000000000*/
  0x0300,  /*0000001100000000*/
  0x0300,  /*0000001100000000*/
  0x0000   /*0000000000000000*/
};

static short default_graphscreenmask[HEIGHT] =  {
  0x3fff,  /*0011111111111111*/
  0x1fff,  /*0001111111111111*/
  0x0fff,  /*0000111111111111*/
  0x07ff,  /*0000011111111111*/
  0x03ff,  /*0000001111111111*/
  0x01ff,  /*0000000111111111*/
  0x00ff,  /*0000000011111111*/
  0x007f,  /*0000000001111111*/
  0x003f,  /*0000000000111111*/
  0x007f,  /*0000000001111111*/
  0x01ff,  /*0000000111111111*/
  0x10ff,  /*0001000011111111*/
  0xb0ff,  /*1011000011111111*/
  0xf87f,  /*1111100001111111*/
  0xf87f,  /*1111100001111111*/
  0xfcff   /*1111110011111111*/
};

void
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
    m_printf("MOUSE Microsoft Mouse (two buttons) selected.\n");
    mouse.threebuttons = FALSE;
    break;
  case 2:				/* Select PC Mouse (3 button) */
    /* enabling this if you only have a two button mouse and didn't
    	set emulate3buttons is silly.  however, if your mouse really
    	does have three buttons, you want to be able to use them. */
    m_printf("MOUSE PC Mouse (three buttons) selected.\n");
    mouse.threebuttons = TRUE;
    break;
  case 3:				/* Tell me what mode we are in ? */
    if (!mouse.threebuttons)
      HI(bx) = 0x10;		/* We are currently in Microsoft Mode */
    else
      HI(bx) = 0x20;		/* We are currently in PC Mouse Mode */
    LO(cx) = mouse.speed_x;
    HI(cx) = mouse.speed_y;
    LO(dx) = mouse.ignorexy;
    break;
  case 4:				/* Set vertical speed */
    if ((LO(cx) < 1) || (LO(cx) > 255)) {
      m_printf("MOUSE Vertical speed out of range. ERROR!\n");
      LWORD(eax) = 1;
    } else 
      mouse.speed_y = LO(cx);
    break;
  case 5:				/* Set horizontal speed */
    if ((LO(cx) < 1) || (LO(cx) > 255)) {
      m_printf("MOUSE Horizontal speed out of range. ERROR!\n");
      LWORD(eax) = 1;
    } else
      mouse.speed_x = LO(cx);
    break;
  case 6:				/* Ignore horz/vert selection */
    if (LO(cx) == 1) 
      mouse.ignorexy = TRUE;
    else
      mouse.ignorexy = FALSE;
    break;
  case 0xf0:
    m_printf("MOUSE Start video mode set\n");
    /* make sure cursor gets turned off */
    mouse_cursor(-1);
    break;
  case 0xf1:
    m_printf("MOUSE End video mode set\n");
    /* redetermine the video mode */
    mouse_reset_to_current_video_mode();
    /* replace cursor if necessary */
    mouse_cursor(1);
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
  case 0x00:			/* Mouse Reset/Get Mouse Installed Flag */
    mouse_reset(0);
    break;

  case 0x01:			/* Show Mouse Cursor */
    mouse_cursor(1);
    break;

  case 0x02:			/* Hide Mouse Cursor */
    mouse_cursor(-1);
    break;

  case 0x03:			/* Get Mouse Position and Button Status */
    mouse_pos();
    break;

  case 0x04:			/* Set Cursor Position */
    mouse_setpos();
    break;

  case 0x05:			/* Get mouse button press info */
    mouse_bpressinfo();
    break;

  case 0x06:			/* Get mouse button release info */
    mouse_brelinfo();
    break;

  case 0x07:			/* Set Mouse Horizontal Min/Max Position */
    mouse_setxminmax();
    break;

  case 0x08:			/* Set Mouse Vertical Min/Max Position */
    mouse_setyminmax();
    break;

  case 0x09:			/* Set mouse gfx cursor */
    mouse_set_gcur();
    break;

  case 0x0A:			/* Set mouse text cursor */
    mouse_set_tcur();
    break;

  case 0x0B:			/* Read mouse motion counters */
    mouse_mickeys();
    break;

  case 0x0C:			/* Set mouse subroutine */
    mouse_setsub();
    break;

  case 0x0D:			/* Enable Light Pen */
    break;

  case 0x0E:			/* Disable Light Pen */
    break;

  case 0x0F:			/* Set cursor speed */
    mouse_setcurspeed();
    break;

  case 0x10:			/* Define mouse exclusion zone */
    mouse_exclusionarea();	
    break;

  case 0x11: 			/* Undocumented */
    mouse_undoc1();		
    break;

  case 0x12:			/* Set Large Graphics Cursor Block */
    mouse_largecursor();
    break;

  case 0x13:			/* Set Maximum for mouse speed doubling */
    mouse_doublespeed();	
    break;

  case 0x14:			/* Exchange Event Handler */
    mouse_excevhand();	
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

  case 0x18:			/* Install alternate event handler */
    mouse_alternate();
    break;

  case 0x19:			/* Determine address of alternate */
    mouse_detalternate();
    break;

  case 0x1A:			/* Set mouse sensitivity */
    mouse_setsensitivity();
    break;

  case 0x1B:			/* Determine mouse sensitivity */
    mouse_detsensitivity();
    break;

  case 0x1C:			/* Set mouse hardware interrupt rate */
    mouse_hardintrate();
    break;

  case 0x1D:			/* Set display page */
    mouse_disppage();
    break;

  case 0x1E:			/* Determine display page */
    mouse_detpage();
    break;

  case 0x1F:			/* Disable Mouse Driver */
    mouse_disable_internaldriver();
    break;

  case 0x20:			/* Enable Mouse Driver */
    mouse_enable_internaldriver();
    break;

  case 0x21:			
#if 0
    mouse_software_reset();	/* Perform Software reset on mouse */
#else
    mouse_reset(0);		/* Should Perform reset on mouse */
#endif
    break;

  case 0x22:			/* Set language for messages */
    m_printf("MOUSE: Set LANGUAGE, ignoring.\n");
    break;			/* Ignore as we are US version only */

  case 0x23:			/* Get language for messages */
    m_printf("MOUSE: Get LANGUAGE\n");
    LWORD(ebx) = 0x0000;	/* Return US/ENGLISH */
    break;

  case 0x24:			/* Get driver version/mouse type/IRQ */
    mouse_version();
    break;

  case 0x25:			/* Get general driver information */
    mouse_getgeninfo();	
    break;

  case 0x26:			/* Return Maximal Co-ordinates */
    mouse_getmaxcoord();
    break;

#if 0
  case 0x27:			/* Get masks and mickey counts */
    mouse_getmasksmicks();	/* TO DO ! when > 7.01 */
    break;

  case 0x28:			/* Set video mode */
    mouse_setvidmode();		/* TO DO ! now ! */
    break;
 
  case 0x29:			/* Count video modes */
    mouse_countvids();		/* TO DO ! now ! */
    break;
 
  case 0x2A:			/* Get cursor hotspot */
    mouse_gethotspot();		/* TO DO ! when > 7.02 */
    break;

  case 0x2B:			/* Set acceleration curves */
    mouse_setacccurv();		/* TO DO ! now */
    break;

  case 0x2C:			/* Read acceleration curves */
    mouse_getacccurv();		/* TO DO ! now */
    break;

  case 0x2D:			/* Set/Get active acceleration curves */
    mouse_setgetactcurv();	/* TO DO ! now */
    break;

  case 0x2E:			/* Set acceleration profile names */
    mouse_setaccprofile();	/* TO DO ! when > 8.01 */
    break;

  case 0x2F:			/* Hardware reset */
    mouse_hardreset();		/* TO DO ! when > 7.02 */
    break;

  case 0x30:			/* Set/Get ballpoint information */
    mouse_getsetballpoint();	/* TO DO ! when > 7.04 */
    break;

  case 0x31:			/* Get minimum/maximum virtual coords */
    mouse_getmaxminvirt();	/* TO DO ! when > 7.05 */
    break;

  case 0x32:			/* Get active advanced functions */
    mouse_getadvfunc();		/* TO DO ! when > 7.05 */
    break;

  case 0x33:			/* Get switch settings */
    mouse_getswitchset();	/* TO DO ! when > 7.05 */ 
    break;

  case 0x34:			/* Get MOUSE.INI ! Location */
    mouse_getinilocation();	/* TO DO ! when > 8.00 */
    break;
 
  case 0x35:			/* LCD Large Screen support */
    mouse_lcd();		/* TO DO ! when > 8.10 */
    break;
#endif

/*
 * Functions after 0x35 are not Microsoft calls, Implemented for PC Mouse
 * and Mouse Systems 3 button emulation
 */

  case 0x42:
    mouse_getstorereq();	/* Get storage requirements */
    break;

  default:
    error("MOUSE: function 0x%04x not implemented\n", LWORD(eax));
    break;
  }

  if (!config.X && mouse.cursor_on == 0)
     mouse_update_cursor();
}

void
mouse_detpage(void)
{
  LWORD(ebx) = mouse.display_page;
}

void
mouse_disppage(void)
{
  /* turn off (ie erase) the mouse cursor before changing the
  	display page so that it doesn't erase the wrong page */
  mouse_cursor(-1);
  mouse.display_page = LWORD(ebx);
  mouse_cursor(1);
}

void
mouse_hardintrate(void)
{
  /* Only used by the Inport Mouse */
}

void
mouse_detalternate(void)
{
  error("MOUSE: RETURN ALTERNATE MOUSE USER HANDLER unimplemented.\n");
  LWORD(ecx) = 0;			/* Error ! */
}

void
mouse_alternate(void)
{
  error("MOUSE: SET ALTERNATE MOUSE USER HANDLER unimplemented.\n");
  LWORD(eax) = 0xffff;			/* Failed to install alternate */
}

void
mouse_doublespeed(void)
{
  mouse.threshold = LWORD(edx);		/* Double speed at mickeys per second */
}

void
mouse_largecursor(void)
{
  LWORD(eax) = 0;			/* Failed to set large cursor */
}

static void
mouse_exclusionarea(void)
{
  mouse.exc_ux = LWORD(ecx);
  mouse.exc_uy = LWORD(edx);
  mouse.exc_lx = LWORD(esi);
  mouse.exc_ly = LWORD(edi);
}

static void
mouse_getstorereq(void)
{
  if (!mouse.threebuttons) 
    LWORD(eax) = 0x42;                /* Configures mouse for Microsoft Mode */
  else {
    LWORD(eax) = 0xffff;              /* Configures mouse for PC Mouse Mode */
    LWORD(ebx) = sizeof(mouse);       /* applies to Genius Mouse too */
  }
}

static void
mouse_getmaxcoord(void)
{
  LWORD(ebx) = (mice->intdrv ? 1 : 0);
  LWORD(ecx) = mouse.maxx;
  LWORD(edx) = mouse.maxy;
  m_printf("MOUSE: COORDINATES: x: %d, y: %d\n",mouse.maxx,mouse.maxy);
}

static void
mouse_getgeninfo(void)
{
  /* Set AX to 0 */
  LWORD(eax) = 0x0000;

  /* Bits 0-7 are count of currently-active Mouse Display Drivers (MDD) */
  LWORD(eax) |= 0x0000;			/* For now none ! */

  /* Bits 8-11 are the interrupt rate */
  LWORD(eax) |= 0x0000;			/* Don't allow interrupts */

  /* Bits 12-13 are Mouse cursor information */
  if (mouse.gfx_cursor)
    LWORD(eax) |= 0x2000;		/* Set graphic cursor */
  else
    LWORD(eax) |= 0x0000;		/* Set software text cursor */

  /* Bit 14 sets driver is newer integrated type */
  LWORD(eax) |= 0x0000;			/* Not the newer integrated type */

  /* Bit 15, 0=COM file, 1=SYS file */
  LWORD(eax) |= 0x8000;			/* Set SYS file */
}

static void
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
#ifndef X_SLOW_CHANGE_CURSOR
  if (config.X)
    X_change_mouse_cursor();
#endif
#endif
  
  /* Return 0xffff on success, 0x21 on failure */
  LWORD(eax) = 0xffff;  

  /* Return Number of mouse buttons */
  if (mouse.threebuttons)
    LWORD(ebx) = 3;
  else
    LWORD(ebx) = 2;
}

static void
mouse_detsensitivity(void)
{
  LWORD(ebx) = mouse.speed_x;         /* horizontal speed */
  LWORD(ecx) = mouse.speed_y;         /* vertical speed */
  LWORD(edx) = mouse.threshold;       /* double speed threshold */
}

static void
mouse_setsensitivity(void)
{
  if (!mouse.ignorexy) {
    if (mouse.speed_x != 0)	/* We don't set if speed_x = 0 */
    mouse.speed_x = LWORD(ebx);
    if (mouse.speed_y != 0)	/* We don't set if speed_y = 0 */
    mouse.speed_y = LWORD(ecx);
  }
  mouse.threshold = LWORD(edx);
}

static void
mouse_restorestate(void)
{
  /* turn cursor off before restore */
  mouse_cursor(-1);

  memcpy(&mouse, (u_char *)(LWORD(es) << 4)+LWORD(edx), sizeof(mouse));

  /* regenerate mouse graphics cursor from masks; they take less
  	space than the "compiled" version. */
  define_graphics_cursor(mouse.graphscreenmask,mouse.graphcursormask);

  /* we turned off the mouse cursor prior to saving, so turn it
  	back on again at the restore. */
  mouse_cursor(1);

  m_printf("MOUSE: Restore mouse state\n");
}


static void
mouse_storestate(void)
{
  /* always turn off mouse cursor prior to saving, that way we don't
  	have to remember the erase information */
  mouse_cursor(-1);

  memcpy((u_char *)(LWORD(es) << 4)+LWORD(edx), &mouse, sizeof(mouse));

  /* now turn it back on */
  mouse_cursor(1);
  m_printf("MOUSE: Save mouse state\n");
}


static void
mouse_detstatbuf(void)
{
  LWORD(ebx) = sizeof(mouse);
  m_printf("MOUSE: get size of state buffer 0x%04x\n", LWORD(ebx));
}

static void
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

  if (mouse.threebuttons && (MOUSE_VERSION > 0x0906)) {
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
  if (!mouse.ignorexy) {
    if (LWORD(ecx) >= 1)
	    mouse.speed_x = LWORD(ecx);
    if (LWORD(edx) >= 1)
	    mouse.speed_y = LWORD(edx);
  }
}


static void
mouse_reset_to_current_video_mode(void)
{
  /* Setup MAX / MIN co-ordinates */
  mouse.minx = mouse.miny = 0;
 /*
  * Here we make sure text modes are resolved properly, according to the
  * standard vga/ega/cga/mda specs for int10. If we don't know that we are
  * in text mode, then we return pixel resolution and assume graphic mode.
  */
  get_current_video_mode();

  if (current_video.textgraph == 'T') {
    mouse.gfx_cursor = FALSE;
  } else {
    mouse.gfx_cursor = TRUE;
    define_graphics_cursor(default_graphscreenmask,default_graphcursormask);
  }

  /* virtual resolution is always at least 640x200 */
  mouse.maxx = 640;
  mouse.maxy = current_video.height;
  if (mouse.maxy < 200) {
  	mouse.maxy = 200;
  	mouse.yshift = 3;
  }
  else
  	mouse.yshift = 0;

  mouse.x = (mouse.minx + mouse.maxx) / 2;
  mouse.y = (mouse.miny + mouse.maxy) / 2;

  /* force update first time after reset */
  mouse.oldrx = -1;

  /* determine shift compensation factors between physical and
  	virtual resolution */
  switch (current_video.width) {
  	case 40: mouse.xshift = 4; break;
  	case 80: mouse.xshift = 3; break;
  	case 320: mouse.xshift = 1; break;
  	default: mouse.xshift = 0; break;
  }

  mouse.maxx = mouse_roundx(mouse.maxx - 1);
  mouse.maxy = mouse_roundy(mouse.maxy - 1);
}


void 
mouse_reset(int flag)
{
  m_printf("MOUSE: reset mouse/installed!\n");

  SETIVEC(0x33, Mouse_SEG, Mouse_OFF + 4);
  SETIVEC(0x74, Mouse_SEG, Mouse_OFF + 4);

  /* Return 0xffff on mouse installed, 0x0000 - no mouse driver installed */
  /* Return number of mouse buttons */
  if (flag == 0) {
    LWORD(eax) = 0xffff;
    if (mouse.threebuttons) 
      LWORD(ebx)=3; 
    else 
      LWORD(ebx)=2; 
  }

  mouse_reset_to_current_video_mode();

  /* turn cursor off if reset requested by software and it was on. */
  if (flag == 0 && mouse.cursor_on == 0)
  	mouse_cursor(-1);

  mouse.cursor_on = -1;
#ifdef X_SUPPORT
#ifndef X_SLOW_CHANGE_CURSOR
  if (config.X)
     X_change_mouse_cursor();
#endif
#endif
  mouse.lbutton = mouse.mbutton = mouse.rbutton = 0;
  mouse.oldlbutton = mouse.oldmbutton = mouse.oldrbutton = 1;
  mouse.lpcount = mouse.mpcount = mouse.rpcount = 0;
  mouse.lrcount = mouse.mrcount = mouse.rrcount = 0;

  if (!mouse.ignorexy) {
    mouse.speed_x = 8;
    mouse.speed_y = 16;
  }

  mouse.exc_lx = mouse.exc_ux = 0;
  mouse.exc_ly = mouse.exc_uy = 0;

  mouse.ip = mouse.cs = 0;
  mouse.mask = 0;		/* no interrupts */

  mouse.threshold = 64;

  mouse.lpx = mouse.lpy = mouse.lrx = mouse.lry = 0;
  mouse.rpx = mouse.rpy = mouse.rrx = mouse.rry = 0;

  mouse.mickeyx = mouse.mickeyy = 0;

  mouse.textscreenmask = 0xffff;
  mouse.textcursormask = 0x7f00;
  memcpy(mouse.graphscreenmask,default_graphscreenmask,32);
  memcpy(mouse.graphcursormask,default_graphcursormask,32);
  mouse.hotx = mouse.hoty = -1;
}

void 
mouse_cursor(int flag)	/* 1=show, -1=hide */
/* bon@elektron.ikp.physik.th-darmstadt.de 951207
   Under X, we can't really hide the mouse and we don't need to.
   So let's skip this function, if nobody objects
*/
{

  if (config.X) return; /* bon@elektron 951207 */

  /* Delete exclusion zone, if show cursor applied */
  if (flag == 1) {
    mouse.exc_lx = mouse.exc_ux = 0;
    mouse.exc_ly = mouse.exc_uy = 0;
  }

  /* already on, don't do anything */
  if (flag == 1 && mouse.cursor_on == 0)
  	return;

  /* adjust hide count (always negative or zero) */
  mouse.cursor_on = mouse.cursor_on + flag;

  /* update the cursor if we just turned it off or on */
  if (!config.X) {
    if ((flag == -1 && mouse.cursor_on == -1) ||
  		(flag == 1 && mouse.cursor_on == 0))
  	mouse_do_cur();
  }
 
#ifdef X_SUPPORT
#ifndef X_SLOW_CHANGE_CURSOR
  if (config.X)
	  X_change_mouse_cursor();
#endif
#endif

  m_printf("MOUSE: %s mouse cursor %d\n", mouse.cursor_on ? "hide" : "show", mouse.cursor_on);
}

void 
mouse_pos(void)
{
  m_printf("MOUSE: get mouse position x:%d, y:%d, b(l%d m%d r%d)\n", mouse.x,
	   mouse.y, mouse.lbutton, mouse.mbutton, mouse.rbutton);
  LWORD(ecx) = mouse.rx;
  LWORD(edx) = mouse.ry;
  LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
  if (mouse.threebuttons)
     LWORD(ebx) |= (mouse.mbutton ? 4 : 0);
}

/* Set mouse position */
void 
mouse_setpos(void)
{
  if (!config.X) {
     mouse.x = LWORD(ecx);
     mouse.y = LWORD(edx);
     mouse_move();
     m_printf("MOUSE: set cursor pos x:%d, y:%d\n", mouse.x, mouse.y);
  }
  else
     m_printf("MOUSE: ignoring 'set cursor pos' in X\n");
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
    if (mouse.threebuttons) {
      LWORD(ebx) = mouse.mpcount;
      mouse.mpcount = 0;
      LWORD(ecx) = mouse.mpx;
      LWORD(edx) = mouse.mpy;
    }
    break;
  }
  
  LWORD(eax) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
  if (mouse.threebuttons)
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
    if (mouse.threebuttons) {
      LWORD(ebx) = mouse.mrcount;
      mouse.mrcount = 0;
      LWORD(ecx) = mouse.mrx;
      LWORD(edx) = mouse.mry;
    }
    break;
  }
  
  LWORD(eax) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
  if (mouse.threebuttons)
     LWORD(eax) |= (mouse.mbutton ? 4 : 0);
}

void 
mouse_setxminmax(void)
{
  m_printf("MOUSE: set horz. min: %d, max: %d\n", LWORD(ecx), LWORD(edx));

  /* if the min > max, they are swapped */
  mouse.minx = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.maxx = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
  mouse.minx = mouse_roundx(mouse.minx);
  mouse.maxx = mouse_roundx(mouse.maxx);
}

void 
mouse_setyminmax(void)
{
  m_printf("MOUSE: set vert. min: %d, max: %d\n", LWORD(ecx), LWORD(edx));

  /* if the min > max, they are swapped */
  mouse.miny = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.maxy = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
  mouse.miny = mouse_roundy(mouse.miny);
  mouse.maxy = mouse_roundy(mouse.maxy);
}

void 
mouse_set_gcur(void)
{
  unsigned short *ptr = (unsigned short*) ((LWORD(es) << 4) + LWORD(edx));

  /* Ignore if in X */
  if (config.usesX) return;

  m_printf("MOUSE: set gfx cursor...hspot: %d, vspot: %d, masks: %04x:%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(es), LWORD(edx));

  /* remember hotspot location */
  mouse.hotx = LWORD(ebx);
  mouse.hoty = LWORD(ecx);

  /* remember mask definition */
  memcpy(mouse.graphscreenmask,ptr,32);
  memcpy(mouse.graphcursormask,ptr+16,32);

  /* compile it so that it can acutally be drawn. */
  define_graphics_cursor(mouse.graphscreenmask,mouse.graphcursormask);
}

void 
mouse_set_tcur(void)
{
  /* Ignore if in X */
  if (config.X) return;

  m_printf("MOUSE: set text cursor...type: %d, start: 0x%04x, end: 0x%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(edx));

  if (LWORD(ebx)==0) {				/* Software cursor */
	  mouse.textscreenmask = LWORD(ecx);
	  mouse.textcursormask = LWORD(edx);
	  mouse_do_cur();
  } else {					/* Hardware cursor */
  /* CX - should be starting line of hardware cursor 
   * DX - should be ending line of hardware cursor
   * FIXME ! -- then again, who ever actually uses the hw cursor? */
   /* (the only advantage of the hw cursor is that you don't have to
   	erase it before hitting vram */
	  mouse.textscreenmask = 0x7fff;
	  mouse.textcursormask = 0xff00;
	  mouse_do_cur();
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
  /* I'm pretty sure the raw motion counters don't take the speed
  	compensation into account; at least DeluxePaint agrees with me */
  /* That doesn't mean that some "advanced" mouse driver with acceleration
  	profiles, etc., wouldn't want to tweak these values; then again,
  	this function is probably used most often by games, and they'd
  	probably just rather have the raw counts */
  LWORD(ecx) = mouse.mickeyx;
  LWORD(edx) = mouse.mickeyy;

  /* counters get reset after read */
  mouse.mickeyx = mouse.mickeyy = 0;
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
  
  SETIVEC(0x33, Mouse_SEG, Mouse_OFF + 4);
  SETIVEC(0x74, Mouse_SEG, Mouse_OFF + 4);

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


static void
mouse_round_coords()
{
	/* we round these down depending on the granularity of the
		screen mode; text mode has all coordinates multiplied
		by eight; 320 pixel-wide graphics modes have x coordinates
		multiplied by two */
	/* (the mouse driver simulates a resolution of at least 640x200
		if the underlying screen mode isn't that wide.) */
	mouse.rx = mouse_roundx(mouse.x);
	mouse.ry = mouse_roundy(mouse.y);
	m_printf("MOUSE coords (%d,%d) rounded to (%d,%d) (%d,%d-%d,%d)\n",
		mouse.x,mouse.y,mouse.rx,mouse.ry,
		mouse.minx,mouse.miny,mouse.maxx,mouse.maxy);
}

void 
mouse_move(void)
{
  if (mouse.x <= mouse.minx) mouse.x = mouse.minx;
  if (mouse.y <= mouse.miny) mouse.y = mouse.miny;
  if (mouse.x >= mouse.maxx) mouse.x = mouse.maxx;
  if (mouse.y >= mouse.maxy) mouse.y = mouse.maxy;

  mouse_round_coords();

  /* Check exclusion zone, then turn off cursor if necessary */
  /* !!! doesn't get graphics cursor or hotspot right !!! */
  if (mouse.exc_lx || mouse.exc_uy) {
    if ((mouse.x > mouse.exc_ux) && 
        (mouse.x < mouse.exc_lx) &&
        (mouse.y > mouse.exc_uy) &&
        (mouse.y < mouse.exc_ly))
	mouse_cursor(-1);
  }

  m_printf("MOUSE: move. x=%x,y=%x\n", mouse.x, mouse.y);
   
  mouse_delta(DELTA_CURSOR);
}

void 
mouse_lb(void)
{
  m_printf("MOUSE: left button %s\n", mouse.lbutton ? "pressed" : "released");
  if (!mouse.lbutton) {
    mouse.lrcount++;
    mouse.lrx = mouse.rx;
    mouse.lry = mouse.ry;
    mouse_delta(DELTA_LEFTBUP);
  }
  else {
    mouse.lpcount++;
    mouse.lpx = mouse.rx;
    mouse.lpy = mouse.ry;
    mouse_delta(DELTA_LEFTBDOWN);
  }
}

void 
mouse_mb(void)
{
  m_printf("MOUSE: middle button %s\n", mouse.mbutton ? "pressed" : "released");
  if (!mouse.mbutton) {
    mouse.mrcount++;
    mouse.mrx = mouse.rx;
    mouse.mry = mouse.ry;
    mouse_delta(DELTA_MIDDLEBUP);
  }
  else {
    mouse.mpcount++;
    mouse.mpx = mouse.rx;
    mouse.mpy = mouse.ry;
    mouse_delta(DELTA_MIDDLEBDOWN);
  }
}

void 
mouse_rb(void)
{
  m_printf("MOUSE: right button %s\n", mouse.rbutton ? "pressed" : "released");
  if (!mouse.rbutton) {
    mouse.rrcount++;
    mouse.rrx = mouse.rx;
    mouse.rry = mouse.ry;
    mouse_delta(DELTA_RIGHTBUP);
  }
  else {
    mouse.rpcount++;
    mouse.rpx = mouse.rx;
    mouse.rpy = mouse.ry;
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
    if (mouse.threebuttons)
      LWORD(ebx) |= (mouse.mbutton ? 4 : 0);

    fake_call(Mouse_SEG, Mouse_OFF + 4);	/* skip to popa */

    REG(cs) = mouse.cs;
    REG(eip) = mouse.ip;

    REG(ds) = *mouse.csp;	/* put DS in user routine */

    if (in_dpmi && REG(cs) == DPMI_SEG && REG(eip) == DPMI_mouse_callback)
	run_pm_mouse();
    
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


/* unconditional mouse cursor update */
static void
mouse_do_cur(void)
{
  if (!scr_state.current) 
  	return;

  if (mouse.gfx_cursor) {
    if (scr_state.mapped)
	    graph_cursor();
  }
  else {
    text_cursor();
  }
}

/* conditionally update the mouse cursor only if it's changed position. */
static void
mouse_update_cursor(void)
{
	/* sigh, too many programs seem to expect the mouse cursor
		to magically redraw itself when in text mode, so we'll
		bend to their will... */
	if ((mouse.rx != mouse.oldrx || mouse.ry != mouse.oldry) &&
			!mouse.gfx_cursor) {
		mouse_do_cur();
		mouse.oldrx = mouse.rx;
		mouse.oldry = mouse.ry;
	}
}

void
text_cursor(void)
{
  unsigned short *p;
  int offset;
  int cx, cy;
  co = READ_WORD(BIOS_SCREEN_COLUMNS);
  p = SCREEN_ADR(mouse.display_page);
  offset = mouse_erase.x;
  cx = mouse.rx >> mouse.xshift;
  cy = mouse.ry >> mouse.yshift;

  if (mouse_erase.drawn) {
  	/* only erase the mouse cursor if it's the same thing we
  		drew; some applications seem to reset the mouse
  		*after* clearing the screen and we end up leaving
  		glitches behind. */
  	if (p[offset] == mouse_erase.backingstore.text[1])
		p[offset] = mouse_erase.backingstore.text[0];
  	mouse_erase.drawn = FALSE;
  }

  if (mouse.cursor_on != 0)
  	return;

  /* remember where we drew this. */
  mouse_erase.x = offset = cx + cy * co;
  mouse_erase.backingstore.text[0] = p[offset];

  /* draw it. */
  mouse_erase.backingstore.text[1] = p[offset] = 
  	(p[offset] & mouse.textscreenmask) ^ mouse.textcursormask;

  mouse_erase.drawn = TRUE;
}

void 
graph_cursor(void)
{
  get_perm();
  
  erase_graphics_cursor(&mouse_erase);

  /* draw_graphics_cursor wants screen coordinates, we have coordinates
  	based on width of 640; hotspot is always in screen coordinates. */
  if (mouse.cursor_on == 0)
	  draw_graphics_cursor(mouse.rx >> mouse.xshift,mouse.ry,
		mouse.hotx,mouse.hoty,16,16,&mouse_erase);

  release_perm();
}


void
mouse_curtick(void)
{
#ifdef X_SUPPORT
#ifdef X_SLOW_CHANGE_CURSOR
  static short last_cursor_on = -1;

  if (config.X && mouse.cursor_on != last_cursor_on) {
    X_change_mouse_cursor();
    last_cursor_on = mouse.cursor_on;
  }
#endif
#endif

  if ((mouse.cursor_on != 0) || config.X)
    return;

  m_printf("MOUSE: curtick x:%d  y:%d\n", mouse.rx, mouse.ry);

  /* we used to do an unconditional update here, but that causes a
  	distracting flicker in the mouse cursor. */
  mouse_update_cursor();
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
  serial_t *sptr=NULL;
  char mouse_ver[]={2,3,4,5,0x14,0x7,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f};
#if 1 /* BUG CATCHER */
  char p[32];
#else
  char *p=(char *)0xefe00;
#endif
#if 0 /* Not sure why she's here? 94/09/19 */
  int old_mice_flags = -1;
#endif
 
  mouse.ignorexy = FALSE;

  /* we'll admit we have three buttons if the user asked us to emulate
  	one or else we think the mouse actually has a third button. */
  if (mice->emulate3buttons || mice->has3buttons)
  	mouse.threebuttons = TRUE;
  else
  	mouse.threebuttons = FALSE;

  mouse_reset(1);		/* Let's set defaults now ! */

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
 	mice->add_to_io_select = 0;
 	return;
      }
      /* want_sigio causes problems with internal mouse driver */
      add_to_io_select(mice->fd, mice->add_to_io_select);
      DOSEMUSetupMouse();
      memcpy(p,mouse_ver,sizeof(mouse_ver));
      return;
    }

    if ((mice->type == MOUSE_PS2) || (mice->type == MOUSE_BUSMOUSE)) {
      mice->fd = DOS_SYSCALL(open(mice->dev, O_RDWR | O_NONBLOCK));
      mice->add_to_io_select = 0;
      if (mice->fd == -1) {
	  mice->intdrv = FALSE;
	  mice->type = MOUSE_NONE;
	  return;
      }
      add_to_io_select(mice->fd, mice->add_to_io_select); 
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
      else {
        m_printf("MOUSE: Mouse configured in serial config! num_ser=%d\n",config.num_ser);
      }
    }
  }
  else {
    mice->fd = mousepipe;
    if (mice->fd == -1) {
      mice->intdrv = FALSE;
      mice->type = MOUSE_NONE;
      mice->add_to_io_select = 1;
      return;
    }
    add_to_io_select(mice->fd, mice->add_to_io_select);
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
  
  if (mice->intdrv && mice->fd != -1 ) {
    tcsetattr(mice->fd, TCSANOW, &mice->oldset);
    DOS_SYSCALL(close(mice->fd));
    return;
  }

  if ((mice->type == MOUSE_PS2) || (mice->type == MOUSE_BUSMOUSE))
    DOS_SYSCALL(close(mice->fd));
}

/* TO DO LIST: (in no particular order)
	- exclusion area doesn't take size of cursor
		into account, also ignores hotspot
	- play with acceleration profiles more
*/
