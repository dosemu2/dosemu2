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
#include "init.h"
#include "bios.h"
#include "int.h"
#include "memory.h"
#include "video.h"		/* video base address */
#include "mouse.h"
#include "serial.h"
#include "port.h"
#include "utilities.h"

#include "dpmi.h"

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
#define MICKEYX() (mouse.mickeyx >> 3)
#define MICKEYY() (mouse.mickeyy >> 3)

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
static void mouse_reset(void);
static void mouse_do_cur(int callback), mouse_update_cursor(int clipped);
static void mouse_reset_to_current_video_mode(int mode);

static void int33_mouse_move_buttons(int lbutton, int mbutton, int rbutton, void *udata);
static void int33_mouse_move_relative(int dx, int dy, int x_range, int y_range, void *udata);
static void int33_mouse_move_mickeys(int dx, int dy, void *udata);
static void int33_mouse_move_absolute(int x, int y, int x_range, int y_range, void *udata);
static void int33_mouse_drag_to_corner(int x_range, int y_range, void *udata);
static void int33_mouse_sync_coords(int x, int y, int x_range, int y_range, void *udata);
static void int33_mouse_enable_native_cursor(int flag, void *udata);

/* graphics cursor */
void graph_cursor(void), text_cursor(void);
void graph_plane(int);

/* called when mouse changes */
static void mouse_delta(int);

/* Internal mouse helper functions */
static int mouse_round_coords(void);
static void mouse_hide_on_exclusion(void);

static void call_mouse_event_handler(void);
static int mouse_events = 0;
static int dragged;
static mouse_erase_t mouse_erase;
static int sent_mouse_esc = FALSE;

#define mice (&config.mouse)
struct mouse_struct mouse;

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
      unsigned int ssp = SEGOFF2LINEAR(regs->ss, 0);
      unsigned int sp = WORD(regs->esp + 2 + 6);
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
    mouse_reset();
    break;

  case 0x01:			/* Show Mouse Cursor */
    mouse_cursor(1);
    mouse_client_set_cursor(3, -1, -1, -1, -1);
    break;

  case 0x02:			/* Hide Mouse Cursor */
    mouse_cursor(-1);
    mouse_client_set_cursor(2, -1, -1, -1, -1);
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
    mouse_reset();		/* Should Perform reset on mouse */
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

static void reset_unscaled(void)
{
	mouse.unsc_x = mouse.unsc_y = 0;
	mouse.unscm_x = mouse.unscm_y = 0;
}

static int get_unsc_x(int dx)
{
	int mx_range;
	mx_range = mouse.maxx - mouse.minx +1;
	return dx * mouse.speed_x * mx_range;
}

static int get_unsc_y(int dy)
{
	int my_range;
	my_range = mouse.maxy - mouse.miny +1;
	return dy * mouse.speed_y * my_range;
}

static void recalc_coords(int udx, int udy, int x_range, int y_range)
{
	int dx, dy, dmx, dmy;
#define ROUND_MK 0
#if ROUND_MK
	if (!(mouse.ps2.cs || mouse.ps2.ip)) {
	    if (udx > 0) {
		int max_dx = mouse.virtual_maxx - mouse.x;
		int max_udx = max_dx * (x_range * mouse.speed_x);
		if (udx + mouse.unsc_x > max_udx)
			udx = max_udx - mouse.unsc_x;
	    }
	    if (udx < 0) {
		int min_dx = mouse.virtual_minx - mouse.x;	// negative
		int min_udx = min_dx * (x_range * mouse.speed_x);
		if (udx + mouse.unsc_x < min_udx)
			udx = min_udx - mouse.unsc_x;
	    }
	    if (udy > 0) {
		int max_dy = mouse.virtual_maxy - mouse.y;
		int max_udy = max_dy * (y_range * mouse.speed_y);
		if (udy + mouse.unsc_y > max_udy)
			udy = max_udy - mouse.unsc_y;
	    }
	    if (udy < 0) {
		int min_dy = mouse.virtual_miny - mouse.y;	// negative
		int min_udy = min_dy * (y_range * mouse.speed_y);
		if (udy + mouse.unsc_y < min_udy)
			udy = min_udy - mouse.unsc_y;
	    }
	}
#endif
	mouse.unsc_x += udx;
	mouse.unsc_y += udy;
	mouse.unscm_x += udx;
	mouse.unscm_y += udy;
	dx = mouse.unsc_x / (x_range * mouse.speed_x);
	dy = mouse.unsc_y / (y_range * mouse.speed_y);
	dmx = mouse.unscm_x / x_range;
	dmy = mouse.unscm_y / y_range;
	mouse.x += dx;
	mouse.y += dy;
#if !ROUND_MK
	mouse_round_coords();
#endif
	mouse.mickeyx += dmx;
	mouse.mickeyy += dmy;
	mouse.unsc_x -= dx * x_range * mouse.speed_x;
	mouse.unsc_y -= dy * y_range * mouse.speed_y;
	mouse.unscm_x -= dmx * x_range;
	mouse.unscm_y -= dmy * y_range;
}

static void add_mk(int dx, int dy)
{
	int mx_range = mouse.maxx - mouse.minx +1;
	int my_range = mouse.maxy - mouse.miny +1;
	int udx = dx * 8 * mx_range;
	int udy = dy * 8 * my_range;
	recalc_coords(udx, udy, mx_range, my_range);
}

static void add_px(int dx, int dy, int x_range, int y_range)
{
	int udx = get_unsc_x(dx);
	int udy = get_unsc_y(dy);
	recalc_coords(udx, udy, x_range, y_range);
}

/*
 * Because the mouse hook finally works all video sets that pass
 * through the video bios eventually pass through here.
 * Win31 does not reset the mouse in the normal way, and
 * the only clue we have is the video mode (0x62 and 0x06 for 1024x768)
 */
static void
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
}

static void int33_mouse_enable_native_cursor(int flag, void *udata)
{
  mice->native_cursor = flag;
  mouse_do_cur(1);
}

static void mouse_reset(void)
{
  m_printf("MOUSE: reset mouse/installed!\n");

  mouse.ps2.state = 0;

  mouse_enable_internaldriver();

  /* Return 0xffff on mouse installed, 0x0000 - no mouse driver installed */
  /* Return number of mouse buttons */
  LWORD(eax) = 0xffff;
  if (mouse.threebuttons)
      LWORD(ebx)=3;
  else
      LWORD(ebx)=2;
  mouse_reset_to_current_video_mode(-1);

  /* turn cursor off if reset requested by software and it was on. */
  if (mouse.cursor_on >= 0) {
  	mouse.cursor_on = 0;
  	mouse_cursor(-1);
  }

  mouse.cursor_on = -1;
  mouse.lbutton = mouse.mbutton = mouse.rbutton = 0;
  mouse.oldlbutton = mouse.oldmbutton = mouse.oldrbutton = 1;
  mouse.lpcount = mouse.mpcount = mouse.rpcount = 0;
  mouse.lrcount = mouse.mrcount = mouse.rrcount = 0;


  mouse.exc_lx = mouse.exc_ux = -1;
  mouse.exc_ly = mouse.exc_uy = -1;

  mouse.ip = mouse.cs = 0;
  mouse.mask = 0;		/* no interrupts */

  mouse.threshold = 64;

  mouse.lpx = mouse.lpy = mouse.lrx = mouse.lry = 0;
  mouse.rpx = mouse.rpy = mouse.rrx = mouse.rry = 0;

  mouse.mickeyx = mouse.mickeyy = 0;
  mouse.x_delta = mouse.y_delta = 0;
  mouse.abs_x = mouse.abs_y = 0;

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
    mouse.exc_lx = mouse.exc_ux = -1;
    mouse.exc_ly = mouse.exc_uy = -1;
    mouse.x_delta = mouse.y_delta = 0;
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
}

/* Set mouse position */
void
mouse_setpos(void)
{
  /* disable the hack below, it is likely no longer needed. -stsp */
#if 0
  /* WP reads mickeys, and then sets the cursor position to make certain
   * it doesn't loose any mickeys.  This will catch that case, and keeps
   * us from breaking all apps under X that set the mouse position.
   */
  if (last_mouse_call_read_mickeys && mice->use_absolute) {
    m_printf("MOUSE: ignoring 'set cursor pos' in X with no grab active\n");
    return;
  }
#endif
  mouse.x = LWORD(ecx);
  mouse.y = LWORD(edx);
  reset_unscaled();
  mouse_round_coords();
  mouse_hide_on_exclusion();
  if (mouse.cursor_on >= 0) {
    mouse.x_delta = mouse.y_delta = 0;
    mouse_do_cur(1);
  } else {
    mouse.x_delta = mouse.x - mouse.abs_x;
    mouse.y_delta = mouse.y - mouse.abs_y;
  }
  m_printf("MOUSE: set cursor pos x:%d, y:%d delta x:%d, y:%d\n",
	mouse.x, mouse.y, mouse.x_delta, mouse.y_delta);
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
    break;

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
  m_printf("MOUSE: read mickeys %d %d\n", MICKEYX(), MICKEYY());
  /* I'm pretty sure the raw motion counters don't take the speed
  	compensation into account; at least DeluxePaint agrees with me */
  /* That doesn't mean that some "advanced" mouse driver with acceleration
  	profiles, etc., wouldn't want to tweak these values; then again,
  	this function is probably used most often by games, and they'd
  	probably just rather have the raw counts */
  LWORD(ecx) = MICKEYX();
  LWORD(edx) = MICKEYY();

  /* counters get reset after read */
  mouse.mickeyx = mouse.mickeyy = 0;
  dragged = 0;
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
	mouse_move_mickeys(dx, dy);
	mouse_move_buttons(state.lbutton, state.mbutton, state.rbutton);
}

static int mouse_round_coords(void)
{
	/* Make certain we have the correct screen boundaries */

	int clipped = 0;
	/* in ps2 mode ignore clipping */
	if (mouse.ps2.cs || mouse.ps2.ip)
		return 0;
	/* put the mouse coordinate in bounds */
	if (mouse.x < mouse.virtual_minx) {
		mouse.x = mouse.virtual_minx;
		clipped = 1;
	}
	if (mouse.y < mouse.virtual_miny) {
		mouse.y = mouse.virtual_miny;
		clipped = 1;
	}
	if (mouse.x > mouse.virtual_maxx) {
		mouse.x = mouse.virtual_maxx;
		clipped = 1;
	}
	if (mouse.y > mouse.virtual_maxy) {
		mouse.y = mouse.virtual_maxy;
		clipped = 1;
	}
#if 0
	if (mouse.abs_x > mouse.maxx)
		mouse.abs_x = mouse.maxx;
	if (mouse.abs_x < mouse.minx)
		mouse.abs_x = mouse.minx;
	if (mouse.abs_y > mouse.maxy)
		mouse.abs_y = mouse.maxy;
	if (mouse.abs_y < mouse.miny)
		mouse.abs_y = mouse.miny;
#endif
	if (clipped)
		reset_unscaled();
	return clipped;
}

static void mouse_hide_on_exclusion(void)
{
  /* Check exclusion zone, then turn off cursor if necessary */
  if (mouse.exc_ux != -1) {
    short xl, xr, yt, yb;

    /* find the cursor box's edges coords (in mouse coords not screen) */
    if (mouse.gfx_cursor) { /* graphics cursor */
      xl = mouse.x - (mouse.hotx << mouse.xshift);
      xr = mouse.x + ((HEIGHT-1 - mouse.hotx) << mouse.xshift);
      yt = mouse.y - (mouse.hoty << mouse.yshift);
      yb = mouse.y + ((HEIGHT-1 - mouse.hoty) << mouse.yshift);
    }
    else { /* text cursor */
      xl = MOUSE_RX;
      xr = MOUSE_RX + (1 << mouse.xshift) - 1;
      yt = MOUSE_RY;
      yb = MOUSE_RY + (1 << mouse.yshift) - 1;

    }
    /* check zone boundary */
    if ((xr >= mouse.exc_ux) &&
        (xl <= mouse.exc_lx) &&
        (yb >= mouse.exc_uy) &&
        (yt <= mouse.exc_ly))
	mouse_cursor(-1);
  }
}

static void mouse_move(int clipped)
{
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

static void int33_mouse_move_buttons(int lbutton, int mbutton, int rbutton, void *udata)
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

static void int33_mouse_move_relative(int dx, int dy, int x_range, int y_range,
	void *udata)
{
	add_px(dx, dy, x_range, y_range);
	mouse.x_delta = mouse.y_delta = 0;

	m_printf("mouse_move_relative(%d, %d) -> %d %d \n",
		 dx, dy, mouse.x, mouse.y);

	/*
	 * update the event mask
	 */
	if (dx || dy)
	   mouse_move(0);
}

static void int33_mouse_move_mickeys(int dx, int dy, void *udata)
{
	add_mk(dx, dy);

	m_printf("mouse_move_mickeys(%d, %d) -> %d %d \n",
		 dx, dy, mouse.x, mouse.y);

	/*
	 * update the event mask
	 */
	if (dx || dy)
	   mouse_move(0);
}

static void int33_mouse_move_absolute(int x, int y, int x_range, int y_range,
	void *udata)
{
	int dx, dy, new_x, new_y, mx_range, my_range, clipped;
	mx_range = mouse.maxx - mouse.minx +1;
	my_range = mouse.maxy - mouse.miny +1;
	new_x = (x*mx_range)/x_range + mouse.minx;
	new_y = (y*my_range)/y_range + mouse.miny;
	dx = (new_x - mouse.abs_x) * mouse.speed_x;
	dy = (new_y - mouse.abs_y) * mouse.speed_y;
	mouse.mickeyx += dx;
	mouse.mickeyy += dy;
	mouse.x = new_x + mouse.x_delta;
	mouse.y = new_y + mouse.y_delta;
	reset_unscaled();
	mouse.abs_x = new_x;
	mouse.abs_y = new_y;
	clipped = mouse_round_coords();
	/* we dont allow DOS prog to grab mouse pointer by locking it
	 * inside a clipping region. So just update deltas instead if
	 * it is invisible, and do nothing if visible. */
	if (clipped && mouse.cursor_on < 0) {
	    mouse.x_delta = mouse.x - mouse.abs_x;
	    mouse.y_delta = mouse.y - mouse.abs_y;
	}

	m_printf("mouse_move_absolute(%d, %d, %d, %d) -> %d %d \n",
		 x, y, x_range, y_range, mouse.x, mouse.y);
	m_printf("mouse_move_absolute dx:%d dy:%d mickeyx%d mickeyy%d\n",
		 dx, dy, mouse.mickeyx, mouse.mickeyy);
	/*
	 * update the event mask
	 */
	if (dx || dy)
	   mouse_move(0);
}

static void int33_mouse_sync_coords(int x, int y, int x_range, int y_range,
	void *udata)
{
	int mx_range, my_range;
	mx_range = mouse.maxx - mouse.minx +1;
	my_range = mouse.maxy - mouse.miny +1;
	mouse.x = (x*mx_range)/x_range + mouse.minx;
	mouse.y = (y*my_range)/y_range + mouse.miny;
	mouse.abs_x = mouse.x;
	mouse.abs_y = mouse.y;
	mouse.x_delta = mouse.y_delta = 0;
	mouse.mickeyx = mouse.x * mouse.speed_x;
	mouse.mickeyy = mouse.y * mouse.speed_y;
	m_printf("MOUSE: synced coords, x:%i->%i y:%i->%i\n",
		mouse.x, mouse.mickeyx, mouse.y, mouse.mickeyy);
}

static void int33_mouse_drag_to_corner(int x_range, int y_range, void *udata)
{
	m_printf("MOUSE: drag to corner\n");
	int33_mouse_move_relative(-3 * x_range, -3 * y_range, x_range, y_range,
		udata);
	dragged = 1;
	mouse.abs_x = mouse.x;
	mouse.abs_y = mouse.y;
	mouse.x_delta = mouse.y_delta = 0;
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
  struct vm86_regs saved_regs;
  int status;
  unsigned int ssp, sp;
  int dx, dy, cnt = 0;

  ssp = SEGOFF2LINEAR(LWORD(ss), 0);
  sp = LWORD(esp);
  saved_regs = REGS;

  do {
    cnt++;
    dx = MICKEYX();
    /* PS/2 wants the y direction reversed */
    dy = -MICKEYY();

    status = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0) | 8;
    if (mouse.threebuttons)
      status |= (mouse.mbutton ? 4 : 0);
    /* overflow; many DOS drivers do a simple cbw on the value... */
    if (dx < -128 || dx > 127)
      dx = dx < -128 ? -128 : 127;
    if (dy < -128 || dy > 127)
      dy = dy < -128 ? -128 : 127;
    mouse.mickeyx -= dx << 3;
    mouse.mickeyy += dy << 3;
    /* we'll call the handler again */
    if (dx < 0)
      status |= 0x10;
    if (dy < 0)
      status |= 0x20;

    m_printf("PS2MOUSE data: dx=%hhx, dy=%hhx, status=%hx\n", (Bit8u)dx, (Bit8u)dy, (unsigned short)status);
    pushw(ssp, sp, status);
    pushw(ssp, sp, dx & 0xff);
    pushw(ssp, sp, dy & 0xff);
    pushw(ssp, sp, 0);
    LWORD(esp) -= 8;

    /* jump to mouse cs:ip */
    m_printf("PS2MOUSE: .........jumping to %04x:%04x %i times\n",
	    mouse.ps2.cs, mouse.ps2.ip, cnt);
    do_call_back(mouse.ps2.cs, mouse.ps2.ip);
    LWORD(esp) += 8;
  } while (MICKEYX() || MICKEYY());

  REGS = saved_regs;
}

static void call_int33_mouse_event_handler(void)
{
    struct vm86_regs saved_regs;

    saved_regs = REGS;
    LWORD(eax) = mouse_events;
    LWORD(ecx) = mouse.x;
    LWORD(edx) = mouse.y;
    LWORD(esi) = MICKEYX();
    LWORD(edi) = MICKEYY();
    LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
    if (mouse.threebuttons)
      LWORD(ebx) |= (mouse.mbutton ? 4 : 0);

    /* jump to mouse cs:ip */
    m_printf("MOUSE: event %d, x %d, y %d, mx %d, my %d, b %x\n",
	     mouse_events, mouse.x, mouse.y, mouse.mickeyx, mouse.mickeyy,
	     LWORD(ebx));
    m_printf("MOUSE: .........jumping to %04x:%04x\n", LWORD(cs), LWORD(eip));
    REG(ds) = mouse.cs;		/* put DS in user routine */
    do_call_back(mouse.cs, mouse.ip);
    REGS = saved_regs;

    /* When mouse is dragged to the corner with mouse_drag_to_corner(),
     * the mickey counters are going crazy. This is not a problem when
     * an app _reads_ the counters - they are then reset. However, if it
     * doesn't read them but instead monitors them via callback (Carmageddon),
     * then we need to sync them with coords eventually, or, otherwise, drop
     * the dragging hack entirely. Since the dragging hack is pretty
     * useful and the idea of monitoring mickeys is outright silly (and
     * almost no progs do that), the dragging hack stays, and the hack
     * below compensates its effect on mickeys. - stsp */
    if (dragged) {
      /* syncing mickey counters with coords is silly, I know, but
       * Carmageddon needs this after dragging mouse to the corner. */
      mouse.mickeyx = mouse.x * mouse.speed_x;
      mouse.mickeyy = mouse.y * mouse.speed_y;
      dragged = 0;
      m_printf("MOUSE: mickey synced with coords, x:%i->%i y:%i->%i\n",
          mouse.x, mouse.mickeyx, mouse.y, mouse.mickeyy);
    }
}

/* this function is called from int74 via inte6 */
static void call_mouse_event_handler(void)
{
  if (mouse_events && mouse.ps2.state && (mouse.ps2.cs || mouse.ps2.ip)) {
    call_int15_mouse_event_handler();
  } else {
    mouse.old_mickeyx = MICKEYX();
    mouse.old_mickeyy = MICKEYY();
    if (mouse.mask & mouse_events && (mouse.cs || mouse.ip))
      call_int33_mouse_event_handler();
    else
      m_printf("MOUSE: Skipping event handler, "
	       "mask=0x%x, ev=0x%x, cs=0x%x, ip=0x%x\n",
	       mouse.mask, mouse_events, mouse.cs, mouse.ip);
  }
  mouse_events = 0;
}

/* unconditional mouse cursor update */
static void mouse_do_cur(int callback)
{
  if (mouse.gfx_cursor) {
    graph_cursor();
  }
  else {
    text_cursor();
  }

  if (mice->native_cursor || !callback)
    return;

  /* this callback is used to e.g. warp the X cursor if int33/ax=4
     requested it to be moved */
  mouse_client_set_cursor(mouse.cursor_on == 0?1: 0,
		    mouse.x - mouse.x_delta - mouse.minx,
		    mouse.y - mouse.y_delta - mouse.miny,
		    mouse.maxx - mouse.minx +1, mouse.maxy - mouse.miny +1);
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
  unsigned int p;
  int offset;
  int cx, cy;
  int co = READ_WORD(BIOS_SCREEN_COLUMNS);
  p = screen_adr(mouse.display_page);
  offset = mouse_erase.x*2;
  cx = MOUSE_RX >> mouse.xshift;
  cy = MOUSE_RY >> mouse.yshift;

  if (mouse_erase.drawn) {
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
  mouse_erase.x = cx + cy * co;
  offset = mouse_erase.x*2;
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
  if (!mice->intdrv || mouse.cursor_on != 0)
    return;

  m_printf("MOUSE: curtick x:%d  y:%d\n", MOUSE_RX, MOUSE_RY);

  /* we used to do an unconditional update here, but that causes a
  	distracting flicker in the mouse cursor. */
  mouse_update_cursor(0);
}

void dosemu_mouse_reset(void)
{
  if (mice->intdrv)
    SETIVEC(0x74, Mouse_SEG, Mouse_ROUTINE_OFF);
}

/*
 * DANG_BEGIN_FUNCTION mouse_init
 *
 * description:
 *  Initialize internal mouse.
 *
 * DANG_END_FUNCTION
 */
static int int33_mouse_init(void)
{
  char mouse_ver[]={2,3,4,5,0x14,0x7,0x38,0x39,0x3a,0x3b,0x3c,0x3d,0x3e,0x3f};
#if 1 /* BUG CATCHER */
  char p[32];
#else
  char *p=(char *)0xefe00;
#endif

  if (!mice->intdrv)
    return 0;

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

  /* Set to disabled, will be enabled in post_boot() */
  mouse.enabled = FALSE;

  mice->native_cursor = 1;
  mice->init_speed_x = 8;
  mice->init_speed_y = 16;
  mouse.speed_x = mice->init_speed_x;
  mouse.speed_y = mice->init_speed_y;

  memcpy(p,mouse_ver,sizeof(mouse_ver));
  pic_seti(PIC_IMOUSE, NULL, 0, NULL);

  m_printf("MOUSE: INIT complete\n");
  return 1;
}

static int int33_mouse_accepts(void *udata)
{
  /* if sermouse.c accepts events, we only accept events
   * that are explicitly sent to int33 (they ignore .accepts member) */
  if (mice->com != -1 && mousedrv_accepts("serial mouse"))
    return 0;
  return mice->intdrv;
}

void mouse_post_boot(void)
{
  unsigned int ptr;

  if (!mice->intdrv) return;

  /* This is needed here to revectoring the interrupt, after dos
   * has revectored it. --EB 1 Nov 1997 */

  mouse_reset_to_current_video_mode(-1);
  mouse_enable_internaldriver();
  SETIVEC(0x33, Mouse_SEG, Mouse_INT_OFF);

  /* grab int10 back from video card for mouse */
  ptr = SEGOFF2LINEAR(BIOSSEG, ((long)bios_f000_int10_old - (long)bios_f000));
  m_printf("ptr is at %x; ptr[0] = %x, ptr[1] = %x\n",ptr,READ_WORD(ptr),READ_WORD(ptr+2));
  WRITE_WORD(ptr, IOFF(0x10));
  WRITE_WORD(ptr + 2, ISEG(0x10));
  m_printf("after store, ptr[0] = %x, ptr[1] = %x\n",READ_WORD(ptr),READ_WORD(ptr+2));
  /* Otherwise this isn't safe */
  SETIVEC(0x10, INT10_WATCHER_SEG, INT10_WATCHER_OFF);

  mouse_client_post_init();
}

void mouse_io_callback(void *arg)
{
  m_printf("MOUSE: We have data\n");
  mouse_client_run();
}

void
dosemu_mouse_close(void)
{
  if (mice->type == MOUSE_XTERM && !sent_mouse_esc)
    return;
  mouse_client_close();
  sent_mouse_esc = FALSE;
}

/* TO DO LIST: (in no particular order)
	- play with acceleration profiles more
*/

struct mouse_drv int33_mouse = {
  int33_mouse_init,
  int33_mouse_accepts,
  int33_mouse_move_buttons,
  int33_mouse_move_relative,
  int33_mouse_move_mickeys,
  int33_mouse_move_absolute,
  int33_mouse_drag_to_corner,
  int33_mouse_sync_coords,
  int33_mouse_enable_native_cursor,
  "int33 mouse"
};

CONSTRUCTOR(static void int33_mouse_register(void))
{
  register_mouse_driver(&int33_mouse);
}
