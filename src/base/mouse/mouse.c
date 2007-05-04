/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* mouse.c for the DOS emulator
 *       Robert Sanders, gt8134b@prism.gatech.edu
 *
 */

#include "config.h"
#include <stdio.h>
#include <termios.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "emu.h"
#include "bios.h"
#include "int.h"
#include "memory.h"
#include "video.h"		/* video base address */
#include "mouse.h"
#include "serial.h"
#include "port.h"
#include "termio.h"
#include "utilities.h"

#include "dpmi.h"

#include "vc.h"
#include "port.h"

#include "keyboard.h"

#include "iodev.h"
#include "bitops.h"

/* This is included for video mode support. Please DO NOT remove !
 * mousevid.h will become part of VGA emulator package */
#include "mousevid.h"
#include "gcursor.h"
#include "vgaemu.h"

/* DANG_BEGIN_REMARK
 * I have not properly tested this INT74 - JES 96/10/20 
 * I have removed it.  INT74 is irq 12.  Which I suppose is the proper
 * irq for a ps2 mouse.  It appears initial support was planned to
 * support irq 12 and at Mouse_ROUTINE_OFF is a routine that
 * acknowledges an irq.  That routine is probably what should be
 * acknowledging irq12, and what int 0x74 should point to.  
 * I have disabled int0x74 support for now. --EB 29 Oct 1997
 * Got it working --BO 4 Nov 2004
 * DANG_END_REMARK
 */

#define MOUSE_RX mouse_roundx(mouse.x)
#define MOUSE_RY mouse_roundy(mouse.y)

static
void mouse_cursor(int), mouse_pos(void), mouse_setpos(void),
 mouse_setxminmax(void), mouse_setyminmax(void), mouse_set_tcur(void),
 mouse_set_gcur(void), mouse_setsub(void), mouse_bpressinfo(void), mouse_brelinfo(void),
 mouse_mickeys(void), mouse_version(void), mouse_enable_internaldriver(void),
 mouse_disable_internaldriver(void),
#if 0
 mouse_software_reset(void), 
#endif
 mouse_getgeninfo(void), mouse_exclusionarea(void), mouse_setcurspeed(void),
 mouse_undoc1(void), mouse_storestate(void), mouse_restorestate(void),
 mouse_getmaxcoord(void), mouse_getstorereq(void), mouse_setsensitivity(void),
 mouse_detsensitivity(void), mouse_detstatbuf(void), mouse_excevhand(void),
 mouse_largecursor(void), mouse_doublespeed(void), mouse_alternate(void),
 mouse_detalternate(void), mouse_hardintrate(void), mouse_disppage(void),
 mouse_detpage(void), mouse_getmaxminvirt(void);

/* mouse movement functions */
static void mouse_reset(int);
static void mouse_do_cur(int callback), mouse_update_cursor(int clipped);

/* graphics cursor */
void graph_cursor(void), text_cursor(void);
void graph_plane(int);

/* called when mouse changes */
static void mouse_delta(int);

/* Internal mouse helper functions */
static int mouse_round_coords(void);
static void mouse_hide_on_exclusion(void);
static int last_mouse_call_read_mickeys = 0;

static void call_mouse_event_handler(void);
static int mouse_events = 0;
static mouse_erase_t mouse_erase;
static int sent_mouse_esc = FALSE;

static struct mouse_client Mouse_serial, Mouse_none;
static mouse_t *mice = &config.mouse;
/* the 'volatile' is there to cover some bug in gcc -O -g3 */
volatile struct mouse_struct mouse;

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


struct mouse_client *Mouse = NULL;

/* register mouse at the back of the linked list */
void register_mouse_client(struct mouse_client *mouse)
{
	struct mouse_client *m;

	mouse->next = NULL;
	if (Mouse == NULL)
		Mouse = mouse;
	else {
		for (m = Mouse; m->next; m = m->next);
		m->next = mouse;
	}
}

void
mouse_helper(struct vm86_regs *regs) 
{
  if (!mice->intdrv) {
    m_printf("MOUSE No Internaldriver set, exiting mouse_helper()\n");
    SETWORD(&regs->eax, 0xffff);
    return;
  }
 
  SETWORD(&regs->eax, 0);		/* Set successful completion */
    
  switch (LOW(regs->ebx)) {
  case 0:				/* Reset iret for mouse */
    m_printf("MOUSE move iret !\n");
    mouse_enable_internaldriver();
    SETIVEC(0x33, Mouse_SEG, Mouse_INT_OFF);
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
      SETHIGH(&regs->ebx, 0x10);		/* We are currently in Microsoft Mode */
    else
      SETHIGH(&regs->ebx, 0x20);		/* We are currently in PC Mouse Mode */
    SETLOW(&regs->ecx, mouse.speed_x);
    SETHIGH(&regs->ecx, mouse.speed_y);
    SETLOW(&regs->edx, mouse.ignorexy);
    break;
  case 4:				/* Set vertical speed */
    if (LOW(regs->ecx) < 1) {
      m_printf("MOUSE Vertical speed out of range. ERROR!\n");
      SETWORD(&regs->eax, 1);
    } else 
      mice->init_speed_y = mouse.speed_y = LOW(regs->ecx);
    break;
  case 5:				/* Set horizontal speed */
    if (LOW(regs->ecx) < 1) {
      m_printf("MOUSE Horizontal speed out of range. ERROR!\n");
      SETWORD(&regs->eax, 1);
    } else
      mice->init_speed_x = mouse.speed_x = LOW(regs->ecx);
    break;
  case 6:				/* Ignore horz/vert selection */
    if (LOW(regs->ecx) == 1) 
      mouse.ignorexy = TRUE;
    else
      mouse.ignorexy = FALSE;
    break;
  case 7:				/* get minimum internal resolution */
    SETWORD(&regs->ecx, mouse.min_max_x);
    SETWORD(&regs->edx, mouse.min_max_y);
    break;
  case 8:				/* set minimum internal resolution */
    mouse.min_max_x = WORD(regs->ecx);
    mouse.min_max_y = WORD(regs->edx);
    break;
  case 0xf0:
    m_printf("MOUSE Start video mode set\n");
    /* make sure cursor gets turned off */
    mouse_cursor(-1);
    break;
  case 0xf1:
    m_printf("MOUSE End video mode set\n");
    {
      /* redetermine the video mode:
         the stack contains: mode, saved ax, saved bx */
      int video_mode = -1;
      unsigned char *ssp = SEG2LINEAR(regs->ss);
      unsigned long sp = WORD(regs->esp + 2);
      unsigned ax = popw(ssp, sp);
      int mode = popw(ssp, sp);

      if (mode >= 0x100 && (mode & 0xff00) != 0x1100 && ax == 0x4f)
	/* no chargen function; check if vesa mode set successful */
	video_mode = mode;
      mouse_reset_to_current_video_mode(video_mode);
    }
    /* replace cursor if necessary */
    mouse_cursor(1);
    break;
  case 0xf2:
    m_printf("MOUSE int74 helper\n");
    call_mouse_event_handler();
    break;
  case 0xff:
    m_printf("MOUSE Checking InternalDriver presence !!\n");
    break;
  default:
    m_printf("MOUSE Unknown mouse_helper function\n");
    SETWORD(&regs->eax, 1);		/* Set unsuccessful completion */
  }
}

void mouse_ps2bios(void)
{        
  m_printf("PS2MOUSE: Call ax=0x%04x, bx=0x%04x\n", LWORD(eax), LWORD(ebx));
  if (!mice->intdrv) {
    REG(eax) = 0x0500;        /* No ps2 mouse device handler */
    CARRY;
    return;
  }

  switch (REG(eax) &= 0x00FF) {
  case 0x0000:                    
    mouse.ps2.state = HI(bx);
    HI(ax) = 0;
    NOCARRY;
    break;
  case 0x0001:
    mouse.ps2.state = 0;
    HI(ax) = 0;
    LWORD(ebx) = 0xAAAA;    /* we have a ps2 mouse */
    NOCARRY;
    break;
  case 0x0002:
    HI(ax) = 0;
    NOCARRY;
    break;
  case 0x0003:
    if (HI(bx) > 3) {
      CARRY;
      HI(ax) = 1;
    } else {
      NOCARRY;
      HI(ax) = 0;
    }
    break;
  case 0x0004:
    HI(bx) = 0xAA;
    HI(ax) = 0;
    NOCARRY;
    break;
  case 0x0005:			/* Initialize ps2 mouse */
    HI(ax) = 0;
    mouse.ps2.state = 0;
    mouse.ps2.pkg = HI(bx);
    NOCARRY;
    break;
  case 0x0006:
    switch (HI(bx)) {
    case 0x00:
      LO(bx)  = (mouse.rbutton ? 1 : 0);
      LO(bx) |= (mouse.lbutton ? 4 : 0);
      LO(bx) |= 0; 	/* scaling 1:1 */
      LO(bx) |= 0x20;	/* device enabled */
      LO(bx) |= 0;	/* stream mode */
      LO(cx) = 0;		/* resolution, one count */
      LO(dx) = 0;		/* sample rate */
      HI(ax) = 0;
      NOCARRY;
      break;
    case 0x01:
      HI(ax) = 0;
      NOCARRY;
      break;
    case 0x02:
      HI(ax) = 1;
      CARRY;
      break;
    }
    break;
  case 0x0007:
    m_printf("PS2MOUSE: set device handler %04x:%04x\n", REG(es), LWORD(ebx));
    mouse.ps2.cs = LWORD(es);
    mouse.ps2.ip = LWORD(ebx);
    HI(ax) = 0;
    NOCARRY;
    break;
  default:
    HI(ax) = 1;
    g_printf("PS2MOUSE: Unknown call ax=0x%04x\n", LWORD(eax));
    CARRY;
  }
}

int
mouse_int(void)
{
  /* delayed mouse init for xterms; should be done cleaner in 1.3.x */
  if (mice->type == MOUSE_XTERM && !sent_mouse_esc) {
    /* save old highlight mouse tracking */
    printf("\033[?1001s");
    /* enable mouse tracking */
    printf("\033[?9h\033[?1000h\033[?1002h\033[?1003h");	
    fflush (stdout);
    m_printf("XTERM MOUSE: Remote terminal mouse tracking enabled\n");
    sent_mouse_esc = TRUE;
  }

  m_printf("MOUSE: int 33h, ax=%x bx=%x\n", LWORD(eax), LWORD(ebx));

  switch (LWORD(eax)) {
  case 0x00:			/* Mouse Reset/Get Mouse Installed Flag */
    mouse_reset(0);
    break;

  case 0x01:			/* Show Mouse Cursor */
    mouse_cursor(1);
    if (Mouse->set_cursor) Mouse->set_cursor(3, 0, 0, 0, 0);
    break;

  case 0x02:			/* Hide Mouse Cursor */
    mouse_cursor(-1);
    if (Mouse->set_cursor) Mouse->set_cursor(2, 0, 0, 0, 0);
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
#endif

  case 0x31:			/* Get minimum/maximum virtual coords */
    mouse_getmaxminvirt();	/* TO DO ! when > 7.05 */
    break;

#if 0
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
    m_printf("MOUSE: function 0x%04x not implemented\n", LWORD(eax));
    break;
  }

  if (mouse.cursor_on == 0)
     mouse_update_cursor(0);

  return 1;
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
mouse_getmaxminvirt(void)
{
  LWORD(eax) = mouse.virtual_minx;
  LWORD(ebx) = mouse.virtual_miny;
  LWORD(ecx) = mouse.virtual_maxx;
  LWORD(edx) = mouse.virtual_maxy;
  m_printf("MOUSE: VIRTUAL COORDINATES: x: %d-%d, y: %d-%d\n",
    mouse.virtual_minx, mouse.virtual_maxx, 
    mouse.virtual_miny, mouse.virtual_maxy);
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

#if 0
static void
mouse_software_reset(void)
{
  m_printf("MOUSE: software reset on mouse\n");

  /* Disable cursor, and de-install current event handler */
  mouse.cursor_on = -1;      
  mouse.cs=0;
  mouse.ip=0;
  mouse_enable_internaldriver();

  /* Return 0xffff on success, 0x21 on failure */
  LWORD(eax) = 0xffff;  

  /* Return Number of mouse buttons */
  if (mouse.threebuttons)
    LWORD(ebx) = 3;
  else
    LWORD(ebx) = 2;
}
#endif

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
  int current_state;
  current_state = mouse.cursor_on;
  /* turn cursor off before restore */
  if (current_state >= 0) {
	mouse.cursor_on = 0;
  	mouse_cursor(-1);
  }

  memcpy((void *)&mouse, MK_FP32(LWORD(es),LWORD(edx)), sizeof(mouse));

  /* regenerate mouse graphics cursor from masks; they take less
  	space than the "compiled" version. */
  define_graphics_cursor((short *)mouse.graphscreenmask,(short *)mouse.graphcursormask);

  /* we turned off the mouse cursor prior to saving, so turn it
  	back on again at the restore. */
  if (mouse.cursor_on >= 0) {
	mouse.cursor_on = -1;
  	mouse_cursor(1);
  }

  m_printf("MOUSE: Restore mouse state\n");
}


static void
mouse_storestate(void)
{
  int current_state;
  current_state = mouse.cursor_on;
  /* always turn off mouse cursor prior to saving, that way we don't
  	have to remember the erase information */
  if (current_state >= 0) {
	mouse.cursor_on = 0;
  	mouse_cursor(-1);
  }
  mouse.cursor_on = current_state;

  memcpy(MK_FP32(LWORD(es),LWORD(edx)), (void *)&mouse, sizeof(mouse));

  /* now turn it back on */
  if (mouse.cursor_on >= 0) {
	mouse.cursor_on = -1;
  	mouse_cursor(1);
  }
  mouse.cursor_on = current_state;
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

/*
 * Because the mouse hook finally works all video sets that pass
 * through the video bios eventually pass through here.
 * Win31 does not reset the mouse in the normal way, and
 * the only clue we have is the video mode (0x62 and 0x06 for 1024x768)
 */
void
mouse_reset_to_current_video_mode(int mode)
{
  /* Setup MAX / MIN co-ordinates */
  mouse.minx = mouse.miny = 0;

  /* This looks like a generally safer place to reset scaling factors
   * then in mouse_reset, as it gets called more often.
   * -- Eric Biederman 29 May 2000
   */
   mouse.speed_x = mice->init_speed_x;
   mouse.speed_y = mice->init_speed_y;

 /*
  * Here we make sure text modes are resolved properly, according to the
  * standard vga/ega/cga/mda specs for int10. If we don't know that we are
  * in text mode, then we return pixel resolution and assume graphic mode.
  */
  get_current_video_mode(mode);

  /* 
   * Actually what happens is: if a Text mode is found, height and width
   * are in characters, else they are in pixels. This was clearly done to
   * confuse people. Besides, they ignore the ACTUAL maxx,maxy values as
   * stored in the BIOS variables.
   */
  /* To get quicken to run correctly I no longer ignore maxx and maxy
   * values as stored in the BIOS variables for text mode.
   * Does this violate a spec?
   * -- EB 25 May 1998
   */

/* DANG_BEGIN_REMARK
 * Whoever wrote the dos mouse driver spec was brain dead...
 * For some video modes the mouse driver appears to randomly
 * pick a shift factor, possibly to keep at least a 640x200 resolution.
 *
 * The general programming documentation doesn't make this clear.
 * And says that in text modes it is safe to divide the resolution by 
 * 8 to get the coordinates in characters.
 *
 * The only safe way to handle the mouse driver is to call function
 * 0x26 Get max x & max y coordinates and scale whatever the driver 
 * returns yourself.
 *
 * To handle programs written by programmers who weren't so cautious a
 * doctrine of least suprise has been implemented.
 *
 * As much as possible do the same as a standard dos mouse driver in the
 * original vga modes 0,1,2,3,4,5,6,7,13,14,15,16,17,18,19. 
 *
 * For other text modes allow the divide by 8 technique to work.
 * For other graphics modes return x & y in screen coordinates.
 * Except when those modes are either 40x?? or 320x???
 * and then handle the x resolution as in 40x25 and 320x200 modes.
 *
 * 320x200 modes are slightly controversial as I have indications that
 * not all mouse drivers do the same thing. So I have taken the
 * simplest, and most common route, which is also long standing dosemu
 * practice of always shifting the xaxis by 1.  When I researched this
 * I could find no examples that did otherwise.
 * 
 *
 * -- Eric Biederman 19 August 1998
 *
 * This code has now been updated so it defaults as above but allows
 * work arounds if necessary.  Because tweaking dosemu is easier
 * than fixing applications without source.
 *
 * -- Eric Biederman 29 May 2000
 *
 * DANG_END_REMARK
 */

  if (mouse_current_video.textgraph == 'T') {
    mouse.xshift = 3;
    mouse.yshift = 3;
    if (mouse_current_video.width == 40) {
	    mouse.xshift = 4;
    }
    mouse.gfx_cursor = FALSE;
  } else {
    mouse.gfx_cursor = TRUE;
    mouse.xshift = 0;    
    mouse.yshift = 0;
    while ((mouse_current_video.width << mouse.xshift) < mouse.min_max_x) {
      mouse.xshift++;
    }
    while ((mouse_current_video.height << mouse.yshift) < mouse.min_max_y) {
      mouse.yshift++;
    }
    define_graphics_cursor(default_graphscreenmask,default_graphcursormask);
  }

  /* Set the maximum sizes */
  mouse.maxx = mouse_current_video.width << mouse.xshift;
  mouse.maxy = mouse_current_video.height << mouse.yshift;

  /* force update first time after reset */
  mouse.oldrx = -1;
  
  /* Change mouse.maxx & mouse.maxy from ranges to maximums */
  mouse.maxx = mouse_roundx(mouse.maxx - 1);
  mouse.maxy = mouse_roundy(mouse.maxy - 1);
  mouse.maxx += (1 << mouse.xshift) -1;
  mouse.maxy += (1 << mouse.yshift) -1;

  /* Setup up the virtual coordinates */
  mouse.virtual_minx = mouse.minx;
  mouse.virtual_maxx = mouse.maxx;
  mouse.virtual_miny = mouse.miny;
  mouse.virtual_maxy = mouse.maxy;

  m_printf("maxx=%i, maxy=%i speed_x=%i speed_y=%i ignorexy=%i type=%d\n", 
	   mouse.maxx, mouse.maxy, mouse.speed_x, mouse.speed_y, mouse.ignorexy,
	   mice->type);
  last_mouse_call_read_mickeys = 0;
}

void mouse_enable_native_cursor(int flag)
{
  mice->native_cursor = flag;
  mouse_do_cur(1);
}

static void mouse_reset(int flag)
{
  m_printf("MOUSE: reset mouse/installed!\n");

  mouse.ps2.state = 0;

  if (flag == 0) mouse_enable_internaldriver();

  /* Return 0xffff on mouse installed, 0x0000 - no mouse driver installed */
  /* Return number of mouse buttons */
  if (flag == 0) {
    LWORD(eax) = 0xffff;
    if (mouse.threebuttons) 
      LWORD(ebx)=3; 
    else 
      LWORD(ebx)=2; 
    mouse_reset_to_current_video_mode(-1);
  }

  /* turn cursor off if reset requested by software and it was on. */
  if ((flag == 0) && (mouse.cursor_on >= 0)) {
  	mouse.cursor_on = 0;
  	mouse_cursor(-1);
  }

  mouse.cursor_on = -1;
  mouse.lbutton = mouse.mbutton = mouse.rbutton = 0;
  mouse.oldlbutton = mouse.oldmbutton = mouse.oldrbutton = 1;
  mouse.lpcount = mouse.mpcount = mouse.rpcount = 0;
  mouse.lrcount = mouse.mrcount = mouse.rrcount = 0;


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
  memcpy((void *)mouse.graphscreenmask,default_graphscreenmask,32);
  memcpy((void *)mouse.graphcursormask,default_graphcursormask,32);
  mouse.hotx = mouse.hoty = -1;

  mouse_do_cur(1);
}

void 
mouse_cursor(int flag)	/* 1=show, -1=hide */
{
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
  if ((flag == -1 && mouse.cursor_on == -1) ||
  		(flag == 1 && mouse.cursor_on == 0)){
	  mouse_do_cur(1);
  }
 
  m_printf("MOUSE: %s mouse cursor %d\n", mouse.cursor_on ? "hide" : "show", mouse.cursor_on);
}

void 
mouse_pos(void)
{
  m_printf("MOUSE: get mouse position x:%d, y:%d, b(l%d m%d r%d)\n", mouse.x,
	   mouse.y, mouse.lbutton, mouse.mbutton, mouse.rbutton);
  LWORD(ecx) = MOUSE_RX;
  LWORD(edx) = MOUSE_RY;
  LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
  if (mouse.threebuttons)
     LWORD(ebx) |= (mouse.mbutton ? 4 : 0);
  last_mouse_call_read_mickeys = 0;
}

/* Set mouse position */
void 
mouse_setpos(void)
{
  /* WP reads mickeys, and then sets the cursor position to make certain
   * it doesn't loose any mickeys.  This will catch that case, and keeps
   * us from breaking all apps under X that set the mouse position.
   */
  if (last_mouse_call_read_mickeys && mice->use_absolute) {
    m_printf("MOUSE: ignoring 'set cursor pos' in X with no grab active\n");
    return;
  }
  mouse.x = LWORD(ecx);
  mouse.unsc_x = mouse.x * mouse.speed_x;
  mouse.y = LWORD(edx);
  mouse.unsc_y = mouse.y * mouse.speed_y;
  mouse_round_coords();
  mouse_hide_on_exclusion();
  mouse_do_cur(1);
  m_printf("MOUSE: set cursor pos x:%d, y:%d\n", mouse.x, mouse.y);
}

void 
mouse_bpressinfo(void)
{
  m_printf("MOUSE: Button press info\n");

  if (LWORD(ebx) > 2)
    LWORD(ebx) = 0;

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
    LWORD(ebx) = mouse.mpcount;
    mouse.mpcount = 0;
    LWORD(ecx) = mouse.mpx;
    LWORD(edx) = mouse.mpy;
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

  if (LWORD(ebx) > 2)
    LWORD(ebx) = 0;

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
    LWORD(ebx) = mouse.mrcount;
    mouse.mrcount = 0;
    LWORD(ecx) = mouse.mrx;
    LWORD(edx) = mouse.mry;
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
  mouse.virtual_minx = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.virtual_maxx = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
  mouse.virtual_minx = mouse_roundx(mouse.virtual_minx);
  mouse.virtual_maxx = mouse_roundx(mouse.virtual_maxx);
  mouse.virtual_maxx += (1 << mouse.xshift) -1;
}

void 
mouse_setyminmax(void)
{
  m_printf("MOUSE: set vert. min: %d, max: %d\n", LWORD(ecx), LWORD(edx));

  /* if the min > max, they are swapped */
  mouse.virtual_miny = (LWORD(ecx) > LWORD(edx)) ? LWORD(edx) : LWORD(ecx);
  mouse.virtual_maxy = (LWORD(ecx) > LWORD(edx)) ? LWORD(ecx) : LWORD(edx);
  mouse.virtual_miny = mouse_roundy(mouse.virtual_miny);
  mouse.virtual_maxy = mouse_roundy(mouse.virtual_maxy);
  mouse.virtual_maxy += (1 << mouse.yshift) -1;
}

void 
mouse_set_gcur(void)
{
  unsigned short *ptr = MK_FP32(LWORD(es),LWORD(edx));

  m_printf("MOUSE: set gfx cursor...hspot: %d, vspot: %d, masks: %04x:%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(es), LWORD(edx));

  /* remember hotspot location */
  mouse.hotx = LWORD(ebx);
  mouse.hoty = LWORD(ecx);

  /* remember mask definition */
  memcpy((void *)mouse.graphscreenmask,ptr,32);
  memcpy((void *)mouse.graphcursormask,ptr+16,32);

  /* compile it so that it can acutally be drawn. */
  define_graphics_cursor((short *)mouse.graphscreenmask,(short *)mouse.graphcursormask);
}

void 
mouse_set_tcur(void)
{
  m_printf("MOUSE: set text cursor...type: %d, start: 0x%04x, end: 0x%04x\n",
	   LWORD(ebx), LWORD(ecx), LWORD(edx));

  if (LWORD(ebx)==0) {				/* Software cursor */
	  mouse.textscreenmask = LWORD(ecx);
	  mouse.textcursormask = LWORD(edx);
	  mouse_do_cur(1);
  } else {					/* Hardware cursor */
  /* CX - should be starting line of hardware cursor 
   * DX - should be ending line of hardware cursor
   * FIXME ! -- then again, who ever actually uses the hw cursor? */
   /* (the only advantage of the hw cursor is that you don't have to
   	erase it before hitting vram */
	  mouse.textscreenmask = 0x7fff;
	  mouse.textcursormask = 0xff00;
	  mouse_do_cur(1);
  }
}

void 
mouse_setsub(void)
{
  mouse.cs = LWORD(es);
  mouse.ip = LWORD(edx);
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
  last_mouse_call_read_mickeys = 1;
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

  mouse.enabled = FALSE;

  m_printf("MOUSE: Disable InternalDriver\n");
}

void
mouse_enable_internaldriver()
{
  mouse.enabled = TRUE;
  m_printf("MOUSE: Enable InternalDriver\n");
}

/* Keyboard mouse control :)  */
void mouse_keyboard(Boolean make, t_keysym key)
{
	static struct keyboard_mouse_state {
		int l, r, u, d;
		int ul, ur, dl, dr;
		int lbutton, mbutton, rbutton; 
	} state;
	int dx, dy;
	switch (key) {
	case KEY_MOUSE_DOWN:
		state.d = !!make;
		break;
	case KEY_MOUSE_LEFT:
		state.l = !!make;
		break;
	case KEY_MOUSE_RIGHT:
		state.r = !!make;
		break;
	case KEY_MOUSE_UP:
		state.u = !!make;
		break;
	case KEY_MOUSE_UP_AND_LEFT:
		state.ul = make;
		break;
	case KEY_MOUSE_UP_AND_RIGHT:
		state.ur = make;
		break;
	case KEY_MOUSE_DOWN_AND_LEFT:
		state.dl = make;
		break;
	case KEY_MOUSE_DOWN_AND_RIGHT:
		state.dr = make;
		break;
	case KEY_MOUSE_BUTTON_LEFT:
		state.lbutton = make;
		break;
	case KEY_MOUSE_BUTTON_RIGHT:
		state.rbutton = make;
		break;
	case KEY_MOUSE_BUTTON_MIDDLE:
		state.mbutton = make;
		break;
	default:
		m_printf("MOUSE: keyboard_mouse(), key 0x%02x unknown!\n", key);
		break;
	}
	dx = dy = 0;
	if (state.d) {
		dy += 1;
	}
	if (state.dl) {
		dy += 1;
		dx -= 1;
	}
	if (state.dr) {
		dy += 1;
		dx += 1;
	}
	if (state.u) {
		dy -= 1;
	}
	if (state.ul) {
		dy -= 1;
		dx -= 1;
	}
	if (state.ur) {
		dy -= 1;
		dx += 1;
	}
	if (state.r) {
		dx += 1;
	}
	if (state.l) {
		dx -= 1;
	}
	mouse_move_relative(dx, dy);
	mouse_move_buttons(state.lbutton, state.mbutton, state.rbutton);
}

static int mouse_round_coords(void)
{
	/* Make certain we have the correct screen boundaries */

	int clipped = 0;

	/* put the mouse coordinate in bounds */
	if (mouse.x < mouse.virtual_minx) {
		mouse.x = mouse.virtual_minx;
		mouse.unsc_x = mouse.x * mouse.speed_x;
		clipped = 1;
	}
	if (mouse.y < mouse.virtual_miny) {
		mouse.y = mouse.virtual_miny;
		mouse.unsc_y = mouse.y * mouse.speed_y;
		clipped = 1;
	}
	if (mouse.x > mouse.virtual_maxx) {
		mouse.x = mouse.virtual_maxx;
		mouse.unsc_x = mouse.x * mouse.speed_x;
		clipped = 1;
	}
	if (mouse.y > mouse.virtual_maxy) {
		mouse.y = mouse.virtual_maxy;
		mouse.unsc_y = mouse.y * mouse.speed_y;
		clipped = 1;
	}
	return clipped;
}

static void mouse_hide_on_exclusion(void)
{
  /* Check exclusion zone, then turn off cursor if necessary */
  /* !!! doesn't get graphics cursor or hotspot right !!! */
  if (mouse.exc_lx || mouse.exc_uy) {
    if ((mouse.x > mouse.exc_ux) && 
        (mouse.x < mouse.exc_lx) &&
        (mouse.y > mouse.exc_uy) &&
        (mouse.y < mouse.exc_ly))
	mouse_cursor(-1);
  }
}

static void mouse_move(int clipped)
{
  mouse_round_coords();
  mouse_hide_on_exclusion();
  mouse_update_cursor(clipped);

  m_printf("MOUSE: move: x=%d,y=%d\n", mouse.x, mouse.y);
   
  mouse_delta(DELTA_CURSOR);
}

static void mouse_lb(void)
{
  m_printf("MOUSE: left button %s\n", mouse.lbutton ? "pressed" : "released");
  if (!mouse.lbutton) {
    mouse.lrcount++;
    mouse.lrx = MOUSE_RX;
    mouse.lry = MOUSE_RY;
    mouse_delta(DELTA_LEFTBUP);
  }
  else {
    mouse.lpcount++;
    mouse.lpx = MOUSE_RX;
    mouse.lpy = MOUSE_RY;
    mouse_delta(DELTA_LEFTBDOWN);
  }
}

static void mouse_mb(void)
{
  m_printf("MOUSE: middle button %s\n", mouse.mbutton ? "pressed" : "released");
  if (!mouse.mbutton) {
    mouse.mrcount++;
    mouse.mrx = MOUSE_RX;
    mouse.mry = MOUSE_RY;
    mouse_delta(DELTA_MIDDLEBUP);
  }
  else {
    mouse.mpcount++;
    mouse.mpx = MOUSE_RX;
    mouse.mpy = MOUSE_RY;
    mouse_delta(DELTA_MIDDLEBDOWN);
  }
}

static void mouse_rb(void)
{
  m_printf("MOUSE: right button %s\n", mouse.rbutton ? "pressed" : "released");
  if (!mouse.rbutton) {
    mouse.rrcount++;
    mouse.rrx = MOUSE_RX;
    mouse.rry = MOUSE_RY;
    mouse_delta(DELTA_RIGHTBUP);
  }
  else {
    mouse.rpcount++;
    mouse.rpx = MOUSE_RX;
    mouse.rpy = MOUSE_RY;
    mouse_delta(DELTA_RIGHTBDOWN);
  }
}

void mouse_move_buttons(int lbutton, int mbutton, int rbutton)
{
	/* Provide 3 button emulation on 2 button mice,
	   But only when PC Mouse Mode is set, otherwise
	   Microsoft Mode = 2 buttons.
	   Middle press if left/right pressed together. 
	   Middle release done for us by the above routines. 
	   This bit also resets the left/right button, because
	   we can't have all three buttons pressed at once. 
	   Well, we could, but not under emulation on 2 button mice. 
	   Alan Hourihane */

	if (mice->emulate3buttons && mouse.threebuttons) {
		if (lbutton && rbutton) {
			mbutton = 1;  /* Set middle button */
		}
	}
	mouse.oldlbutton = mouse.lbutton;
	mouse.oldmbutton = mouse.mbutton;
	mouse.oldrbutton = mouse.rbutton;
	mouse.lbutton = !!lbutton;
	mouse.mbutton = !!mbutton;
	mouse.rbutton = !!rbutton;
	/*
	 * update the event mask
	 */
	if (mouse.oldlbutton != mouse.lbutton)
	   mouse_lb();
        if (mouse.threebuttons && mouse.oldmbutton != mouse.mbutton)
	   mouse_mb();
	if (mouse.oldrbutton != mouse.rbutton)
	   mouse_rb();
}

void mouse_move_relative(int dx, int dy)
{
	/* according to the interrupt list, the speed setting is in
		mickeys per eight pixels. */
	/* IDEA: running dx and dy through a filter which dampens
		values near zero and amplifies larger values might
		give us a cheap acceleration profile. */
	mouse.unsc_x += dx << 3;
	mouse.unsc_y += dy << 3;
	mouse.x = mouse.unsc_x / mouse.speed_x;
	mouse.y = mouse.unsc_y / mouse.speed_y;
	mouse.mickeyx += dx;
	mouse.mickeyy += dy;

	m_printf("mouse_move_relative(%d, %d) -> %d %d \n",
		 dx, dy, mouse.x, mouse.y);

	/*
	 * update the event mask
	 */
	if (dx || dy)
	   mouse_move(0);
}

void mouse_move_absolute(int x, int y, int x_range, int y_range)
{
	int dx, dy, new_x, new_y, mx_range, my_range,
            minx, maxx, miny, maxy, clipped;
        
	minx = mouse.minx<mouse.virtual_minx ? mouse.minx : mouse.virtual_minx;
	maxx = mouse.maxx>mouse.virtual_maxx ? mouse.maxx : mouse.virtual_maxx;
	miny = mouse.miny<mouse.virtual_miny ? mouse.miny : mouse.virtual_miny;
	maxy = mouse.maxy>mouse.virtual_maxy ? mouse.maxy : mouse.virtual_maxy;
	mx_range = maxx - minx +1;
	my_range = maxy - miny +1;
	new_x = (x*mx_range)/x_range + minx;
	new_y = (y*my_range)/y_range + miny;
	dx = (((new_x - mouse.x) * mouse.speed_x) >> 3);
	dy = (((new_y - mouse.y) * mouse.speed_y) >> 3);
	mouse.mickeyx += dx;
	mouse.mickeyy += dy;
	mouse.x = new_x;
	mouse.y = new_y;
	clipped = mouse_round_coords();

	m_printf("mouse_move_absolute(%d, %d, %d, %d) -> %d %d \n",
		 x, y, x_range, y_range, mouse.x, mouse.y);
	m_printf("mouse_move_absolute dx:%d dy:%d mickeyx%d mickeyy%d\n", 
		 dx, dy, mouse.mickeyx, mouse.mickeyy);
	/*
	 * update the event mask
	 */
	if (dx || dy || mouse.x != new_x || mouse.y != new_y) 
	   mouse_move(clipped);
}

/*
 * add the event to the current event mask
 */
void
mouse_delta(int event)
{
	mouse_events |= event;
	pic_request(PIC_IMOUSE);
	reset_idle(0);
}

/*
 * call PS/2 (int15) event handler
 */
static void call_int15_mouse_event_handler(void)
{
    int status;
    unsigned char *ssp;
    unsigned long sp;
    int dx, dy;

    ssp = SEG2LINEAR(LWORD(ss));
    sp = (unsigned long) LWORD(esp);

    dx = mouse.mickeyx - mouse.old_mickeyx;
    /* PS/2 wants the y direction reversed */
    dy = mouse.old_mickeyy - mouse.mickeyy;

    status = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0) | 8;
    if (mouse.threebuttons)
      status |= (mouse.mbutton ? 4 : 0);
    /* overflow; many DOS drivers do a simple cbw on the value... */
    if (dx < -128 || dx > 127)
      dx = dx < -128 ? -128 : 127;
    if (dy < -128 || dy > 127)
      dy = dy < -128 ? -128 : 127;
    /* we'll call the handler again */
    if (dx < 0)
      status |= 0x10;
    if (dy < 0)
      status |= 0x20;
    mouse.old_mickeyx += dx;
    mouse.old_mickeyy -= dy;

    m_printf("PS2MOUSE data: dx=%hhx, dy=%hhx, status=%hx\n", (Bit8u)dx, (Bit8u)dy, status);
    pushw(ssp, sp, status | 8);
    pushw(ssp, sp, dx & 0xff);
    pushw(ssp, sp, dy & 0xff);
    pushw(ssp, sp, 0);
    LWORD(esp) -= 8;

    /* push return address F000:2100 */
    fake_call(Mouse_SEG, Mouse_PS2_OFF);

    /* jump to mouse cs:ip */
    REG(cs) = mouse.ps2.cs;
    REG(eip) = mouse.ps2.ip;

    m_printf("PS2MOUSE: .........jumping to %04x:%04x\n", LWORD(cs), LWORD(eip));
}

static void call_int33_mouse_event_handler(void)
{
    /* push all 16-bit regs plus _DS,_ES. At F000:20F0 (bios.S) we find
     * 'pop es; pop ds; popa; eoi' */
    fake_pusha();

    LWORD(eax) = mouse_events;
    LWORD(ecx) = mouse.x;
    LWORD(edx) = mouse.y;
    LWORD(esi) = mouse.mickeyx;
    LWORD(edi) = mouse.mickeyy;
    LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
    if (mouse.threebuttons)
      LWORD(ebx) |= (mouse.mbutton ? 4 : 0);

    /* push return address F000:20F0 */
    fake_call(Mouse_SEG, Mouse_OFF);

    /* jump to mouse cs:ip */
    REG(cs) = mouse.cs;
    REG(eip) = mouse.ip;

    REG(ds) = mouse.cs;		/* put DS in user routine */

    m_printf("MOUSE: event %d, x %d ,y %d, mx %d, my %d, b %x\n",
	     mouse_events, mouse.x, mouse.y, mouse.maxx, mouse.maxy, LWORD(ebx));
    m_printf("MOUSE: .........jumping to %04x:%04x\n", LWORD(cs), LWORD(eip));
}

/* this function is called from int74 via inte6 */
static void call_mouse_event_handler(void)
{
  unsigned char *ssp;
  unsigned long sp;

  ssp = SEG2LINEAR(LWORD(ss));
  sp = (unsigned long) LWORD(esp);

  /* first pop bx and ax, which were changed to
     call this function */
  LWORD(ebx) = popw(ssp, sp);
  LWORD(eax) = popw(ssp, sp);
  LWORD(esp) += 4;

  if (mouse_events && mouse.ps2.state && (mouse.ps2.cs || mouse.ps2.ip)) {
    call_int15_mouse_event_handler();
  } else {
    mouse.old_mickeyx = mouse.mickeyx;
    mouse.old_mickeyy = mouse.mickeyy;
    if (mouse.mask & mouse_events && (mouse.cs || mouse.ip))
      call_int33_mouse_event_handler();
    else
      m_printf("MOUSE: Skipping event handler, "
	       "mask=0x%x, ev=0x%x, cs=0x%x, ip=0x%x\n",
	       mouse.mask, mouse_events, mouse.cs, mouse.ip);
  }
  if (mouse.old_mickeyx != mouse.mickeyx ||
      mouse.old_mickeyy != mouse.mickeyy)
    /* keep requesting until the mouse driver caught up */
    pic_request(PIC_IMOUSE);
  else
    mouse_events = 0;
}

void
do_mouse_irq()
{
  /* function called for xterm, X and SDL. Others do it directly */
  if (mouse_events)
    pic_request(PIC_IMOUSE);
}

/* unconditional mouse cursor update */
static void mouse_do_cur(int callback)
{
  int minx, maxx, miny, maxy;

  if (mouse.gfx_cursor) {
    graph_cursor();
  }
  else {
    text_cursor();
  }

  if (mice->native_cursor || !Mouse->set_cursor || !callback)
    return;

  /* this callback is used to e.g. warp the X cursor if int33/ax=4
     requested it to be moved */

  minx = mouse.minx<mouse.virtual_minx ? mouse.minx : mouse.virtual_minx;
  maxx = mouse.maxx>mouse.virtual_maxx ? mouse.maxx : mouse.virtual_maxx;
  miny = mouse.miny<mouse.virtual_miny ? mouse.miny : mouse.virtual_miny;
  maxy = mouse.maxy>mouse.virtual_maxy ? mouse.maxy : mouse.virtual_maxy;

  Mouse->set_cursor(mouse.cursor_on == 0?1: 0, 
		    mouse.x - minx, mouse.y - miny,
		    maxx - minx +1, maxy - miny +1);
}

/* conditionally update the mouse cursor only if it's changed position. */
static void
mouse_update_cursor(int clipped)
{
	/* sigh, too many programs seem to expect the mouse cursor
		to magically redraw itself, so we'll bend to their will... */
	if (MOUSE_RX != mouse.oldrx || MOUSE_RY != mouse.oldry) {
		mouse_do_cur(clipped);
		mouse.oldrx = MOUSE_RX;
		mouse.oldry = MOUSE_RY;
	}
}

void
text_cursor(void)
{
  unsigned short *p;
  int offset;
  int cx, cy;
  int co = READ_WORD(BIOS_SCREEN_COLUMNS);
  p = screen_adr(mouse.display_page);
  offset = mouse_erase.x;
  cx = MOUSE_RX >> mouse.xshift;
  cy = MOUSE_RY >> mouse.yshift;

  if (mouse_erase.drawn && mice->native_cursor) {
  	/* only erase the mouse cursor if it's the same thing we
  		drew; some applications seem to reset the mouse
  		*after* clearing the screen and we end up leaving
  		glitches behind. */
  	if (vga_read_word(p + offset) == mouse_erase.backingstore.text[1])
	  vga_write_word(p + offset, mouse_erase.backingstore.text[0]);
  	mouse_erase.drawn = FALSE;
  }

  if (mouse.cursor_on != 0 || !mice->native_cursor)
  	return;

  /* remember where we drew this. */
  mouse_erase.x = offset = cx + cy * co;
  mouse_erase.backingstore.text[0] = vga_read_word(p + offset);

  /* draw it. */
  mouse_erase.backingstore.text[1] = 
    (vga_read_word(p + offset) & mouse.textscreenmask) ^ mouse.textcursormask;
  vga_write_word(p + offset, mouse_erase.backingstore.text[1]);

  mouse_erase.drawn = TRUE;
}

void 
graph_cursor(void)
{
  erase_graphics_cursor(&mouse_erase);

  /* draw_graphics_cursor wants screen coordinates, we have coordinates
  	based on width of 640; hotspot is always in screen coordinates. */
  if (mouse.cursor_on == 0 && mice->native_cursor)
	  draw_graphics_cursor(MOUSE_RX >> mouse.xshift, MOUSE_RY >> mouse.yshift,
		mouse.hotx,mouse.hoty,16,16,&mouse_erase);
}


void
mouse_curtick(void)
{
  if (!mice->intdrv || mouse.cursor_on != 0 || Mouse->set_cursor)
    return;

  m_printf("MOUSE: curtick x:%d  y:%d\n", MOUSE_RX, MOUSE_RY);

  /* we used to do an unconditional update here, but that causes a
  	distracting flicker in the mouse cursor. */
  mouse_update_cursor(0);
}

static void mouse_client_init(void)
{
  int ok;

#ifdef USE_GPM
  if (Mouse == NULL)
    load_plugin("gpm");
#endif
  register_mouse_client(&Mouse_serial);
  register_mouse_client(&Mouse_raw);
  register_mouse_client(&Mouse_none);
  while(TRUE) {
    m_printf("MOUSE: initialising '%s' mode mouse client\n",
             Mouse->name);
    
    ok = Mouse->init?Mouse->init():TRUE;
    if (ok) {
      m_printf("MOUSE: Mouse init ok, '%s' mode\n", Mouse->name);
      break;
    }
    else {
      m_printf("MOUSE: Mouse init ***failed***, '%s' mode\n",
               Mouse->name);
    }
    Mouse = Mouse->next;
  }
}

void dosemu_mouse_reset(void)
{
  if (mice->intdrv)
    SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);

  /* We set the defaults at the end so that we can test the mouse type */
  mouse_reset(1);		/* Let's set defaults now ! */
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
dosemu_mouse_init(void)
{
  char mouse_ver[]={2,3,4,5,0x14,0x7,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f};
#if 1 /* BUG CATCHER */
  char p[32];
#else
  char *p=(char *)0xefe00;
#endif
 
  mouse.ignorexy = FALSE;

  /* set minimum internal resolution
   * 640x200 is a long standing dosemu default
   * although I accidentally broke it broke it by flipping my X & Y coordiantes.
   * -- Eric Biederman 29 May 2000
   */
  mouse.min_max_x = 640;
  mouse.min_max_y = 200;

  /* we'll admit we have three buttons if the user asked us to emulate
  	one or else we think the mouse actually has a third button. */
  if (mice->emulate3buttons || mice->has3buttons)
  	mouse.threebuttons = TRUE;
  else
  	mouse.threebuttons = FALSE;

  mice->fd = -1;
  /* Set to disabled, will be enabled in post_boot() */
  mouse.enabled = FALSE;

  mice->native_cursor = 1;
  mice->init_speed_x = 8;
  mice->init_speed_y = 16;
  mouse.speed_x = mice->init_speed_x;
  mouse.speed_y = mice->init_speed_y;

  mouse_client_init();

  if (mice->intdrv) {
    memcpy(p,mouse_ver,sizeof(mouse_ver));
    pic_seti(PIC_IMOUSE, DOSEMUMouseEvents, 0, NULL);
  }

  m_printf("MOUSE: INIT complete\n");
}

void mouse_post_boot(void)
{
  us *ptr;
        
  if (!mice->intdrv) return;
  
  /* This is needed here to revectoring the interrupt, after dos
   * has revectored it. --EB 1 Nov 1997 */
  
  mouse_reset_to_current_video_mode(-1);
  mouse_enable_internaldriver();
  SETIVEC(0x33, Mouse_SEG, Mouse_INT_OFF);
  
  /* grab int10 back from video card for mouse */
  ptr = (us*)((BIOSSEG << 4) +
              ((long)bios_f000_int10_old - (long)bios_f000));
  m_printf("ptr is at %p; ptr[0] = %x, ptr[1] = %x\n",ptr,ptr[0],ptr[1]);
  WRITE_WORD(ptr, IOFF(0x10));
  WRITE_WORD(ptr + 1, ISEG(0x10));
  m_printf("after store, ptr[0] = %x, ptr[1] = %x\n",ptr[0],ptr[1]);
  /* Otherwise this isn't safe */
  SETIVEC(0x10, INT10_WATCHER_SEG, INT10_WATCHER_OFF);
}

void mouse_io_callback(void)
{
  if (mice->intdrv && mice->fd >= 0) {
    m_printf("MOUSE: We have data\n");
    pic_request(PIC_IMOUSE);
  }
}

void
dosemu_mouse_close(void)
{
  if (mice->type == MOUSE_XTERM && !sent_mouse_esc)
    return;
  if (Mouse && Mouse->close) Mouse->close(); 
  sent_mouse_esc = FALSE;
}


static int serial_mouse_init(void)
{
  int x;
  serial_t *sptr=NULL;
  for (x=0;x<config.num_ser;x++){
    sptr = &com[x];
    if (sptr->mouse) break;
  }
  if (!sptr || !(sptr->mouse)) {
    m_printf("MOUSE: No mouse configured in serial config! num_ser=%d\n",config.num_ser);
    return FALSE;
  }
  m_printf("MOUSE: Mouse configured in serial config! num_ser=%d\n",config.num_ser);
  mice->intdrv = FALSE;
  return TRUE;
}

static struct mouse_client Mouse_serial =  {
  "No Mouse",   /* name */
  serial_mouse_init,	/* init */
  NULL,		/* close */
  NULL,		/* run */
  NULL
};

static int none_init(void)
{
  return TRUE;
}

static struct mouse_client Mouse_none =  {
  "No Mouse",   /* name */
  none_init,	/* init */
  NULL,		/* close */
  NULL,		/* run */
  NULL
};

int DOSEMUMouseEvents(int ilevel)
{
  if (Mouse->run) Mouse->run();
  return 1;
}

/* TO DO LIST: (in no particular order)
	- exclusion area doesn't take size of cursor
		into account, also ignores hotspot
	- play with acceleration profiles more
*/

