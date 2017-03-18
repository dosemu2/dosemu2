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
#include "sig.h"
#include "bios.h"
#include "int.h"
#include "memory.h"
#include "video.h"		/* video base address */
#include "mouse.h"
#include "serial.h"
#include "port.h"
#include "utilities.h"
#include "doshelpers.h"
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

#define SETHIGH(x, v) HI_BYTE(x) = (v)
#define SETLO_WORD(x, v) LO_WORD(x) = (v)
#define SETLO_BYTE(x, v) LO_BYTE(x) = (v)
#define SETWORD(x, v) SETLO_WORD(x, v)

#define MOUSE_RX mouse_roundx(get_mx())
#define MOUSE_RY mouse_roundy(get_my())
#define MOUSE_MINX 0
#define MOUSE_MINY 0

static int mickeyx(void)
{
	return (mouse.unscm_x / (mouse.px_range * 8));
}

static int mickeyy(void)
{
	return (mouse.unscm_y / (mouse.py_range * 8));
}

static int get_mx(void)
{
	return (mouse.unsc_x / mouse.px_range);
}

static int get_my(void)
{
	return (mouse.unsc_y / mouse.py_range);
}

static void set_px_ranges(int x_range, int y_range)
{
	/* need to update unscaled coords */
	mouse.unsc_x = mouse.unsc_x * x_range / mouse.px_range;
	mouse.unsc_y = mouse.unsc_y * y_range / mouse.py_range;
	mouse.unscm_x = mouse.unscm_x * x_range / mouse.px_range;
	mouse.unscm_y = mouse.unscm_y * y_range / mouse.py_range;

	mouse.px_range = x_range;
	mouse.py_range = y_range;
}

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
static void scale_coords3(int x, int y, int x_range, int y_range,
	int speed_x, int speed_y, int *s_x, int *s_y);
static void scale_coords(int x, int y, int x_range, int y_range,
	int *s_x, int *s_y);
static void scale_coords_spd_unsc(int x, int y, int *s_x, int *s_y);
static void scale_coords_spd_unsc_mk(int x, int y, int *s_x, int *s_y);
static void do_move_abs(int x, int y, int x_range, int y_range);

/* mouse movement functions */
static void mouse_reset(void);
static void mouse_do_cur(int callback), mouse_update_cursor(int clipped);
static void mouse_reset_to_current_video_mode(void);

static void int33_mouse_move_buttons(int lbutton, int mbutton, int rbutton, void *udata);
static void int33_mouse_move_relative(int dx, int dy, int x_range, int y_range, void *udata);
static void int33_mouse_move_mickeys(int dx, int dy, void *udata);
static void int33_mouse_move_absolute(int x, int y, int x_range, int y_range, void *udata);
static void int33_mouse_drag_to_corner(int x_range, int y_range, void *udata);
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
struct dragged_hack {
  int cnt, skipped;
  int x, y, x_range, y_range;
} dragged;
static mouse_erase_t mouse_erase;
static struct mousevideoinfo mouse_current_video;

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
    SETWORD(regs->eax, 0xffff);
    return;
  }

  SETWORD(regs->eax, 0);		/* Set successful completion */

  switch (LO_BYTE(regs->ebx)) {
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
      SETHIGH(regs->ebx, 0x10);		/* We are currently in Microsoft Mode */
    else
      SETHIGH(regs->ebx, 0x20);		/* We are currently in PC Mouse Mode */
    SETLO_BYTE(regs->ecx, mouse.speed_x);
    SETHIGH(regs->ecx, mouse.speed_y);
    SETLO_BYTE(regs->edx, mice->ignorevesa);
    break;
  case 4:				/* Set vertical speed */
    if (LO_BYTE(regs->ecx) < 1) {
      m_printf("MOUSE Vertical speed out of range. ERROR!\n");
      SETWORD(regs->eax, 1);
    } else
      mice->init_speed_y = mouse.speed_y = LO_BYTE(regs->ecx);
    break;
  case 5:				/* Set horizontal speed */
    if (LO_BYTE(regs->ecx) < 1) {
      m_printf("MOUSE Horizontal speed out of range. ERROR!\n");
      SETWORD(regs->eax, 1);
    } else
      mice->init_speed_x = mouse.speed_x = LO_BYTE(regs->ecx);
    break;
  case 6:				/* Ignore vesa modes */
    mice->ignorevesa = LO_BYTE(regs->ecx);
    break;
  case 7:				/* get minimum internal resolution */
    SETWORD(regs->ecx, mouse.min_max_x);
    SETWORD(regs->edx, mouse.min_max_y);
    break;
  case 8:				/* set minimum internal resolution */
    mouse.min_max_x = LO_WORD(regs->ecx);
    mouse.min_max_y = LO_WORD(regs->edx);
    break;
  case DOS_SUBHELPER_MOUSE_START_VIDEO_MODE_SET:
    m_printf("MOUSE Start video mode set\n");
    /* make sure cursor gets turned off */
    mouse_cursor(-1);
    break;
  case DOS_SUBHELPER_MOUSE_END_VIDEO_MODE_SET:
    m_printf("MOUSE End video mode set\n");
    {
      /* redetermine the video mode:
         the stack contains: mode, saved ax, saved bx */
      unsigned int ssp = SEGOFF2LINEAR(regs->ss, 0);
      unsigned int sp = (regs->esp + 2 + 6) & 0xffff;
      unsigned ax = popw(ssp, sp);
      int mode = popw(ssp, sp);

      if (!mice->ignorevesa && mode >= 0x100 && ax == 0x4f) {
	/* no chargen function; check if vesa mode set successful */
	vidmouse_set_video_mode(mode);
      } else {
	vidmouse_set_video_mode(-1);
      }
      mouse_reset_to_current_video_mode();
    }
    /* replace cursor if necessary */
    mouse_cursor(1);
    /* reset hide count on mode switches to fix this:
     * https://github.com/stsp/dosemu2/issues/314
     */
    if (mouse.cursor_on < -1) {
	m_printf("MOUSE: normalizing hide count, %i\n", mouse.cursor_on);
	mouse.cursor_on = -1;
    }
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
    SETWORD(regs->eax, 1);		/* Set unsuccessful completion */
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
    m_printf("PS2MOUSE: set device handler %04x:%04x\n", SREG(es), LWORD(ebx));
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
  m_printf("MOUSE: int 33h, ax=%x bx=%x\n", LWORD(eax), LWORD(ebx));

  switch (LWORD(eax)) {
  case 0x00:			/* Mouse Reset/Get Mouse Installed Flag */
    mouse_reset();
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
  if (mouse.speed_x != 0)	/* We don't set if speed_x = 0 */
    mouse.speed_x = LWORD(ebx);
  if (mouse.speed_y != 0)	/* We don't set if speed_y = 0 */
    mouse.speed_y = LWORD(ecx);

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
  int oldx, oldy, newx, newy;

  if (mouse.cursor_on < 0) {
    /* when speed changes, in ungrabbed mode we need to update deltas
     * so that the (invisible) cursor not to move */
    scale_coords(mouse.px_abs, mouse.py_abs, mouse.px_range, mouse.py_range,
	    &oldx, &oldy);
    scale_coords3(mouse.px_abs, mouse.py_abs, mouse.px_range, mouse.py_range,
	    LWORD(ecx), LWORD(edx), &newx, &newy);
    mouse.x_delta -= newx - oldx;
    mouse.y_delta -= newy - oldy;
  }

  m_printf("MOUSE: function 0f: cx=%04x, dx=%04x\n",LWORD(ecx),LWORD(edx));
  if (LWORD(ecx) >= 1)
    mouse.speed_x = LWORD(ecx);
  if (LWORD(edx) >= 1)
    mouse.speed_y = LWORD(edx);
}

static int get_unsc_x(int dx)
{
	return dx * mouse.px_range;
}

static int get_unsc_y(int dy)
{
	return dy * mouse.py_range;
}

static void setxy(int x, int y)
{
	mouse.unsc_x = get_unsc_x(x);
	mouse.unsc_y = get_unsc_y(y);
}

static int get_unsc_mk_x(int dx)
{
	return dx * mouse.px_range;
}

static int get_unsc_mk_y(int dy)
{
	return dy * mouse.py_range;
}

static void setmxy(int x, int y)
{
	mouse.unscm_x = get_unsc_mk_x(x);
	mouse.unscm_y = get_unsc_mk_y(y);
}

static void add_abs_coords(int udx, int udy)
{
	mouse.unsc_x += udx;
	mouse.unsc_y += udy;

	mouse_round_coords();
}

static void add_mickey_coords(int udx, int udy)
{
	mouse.unscm_x += udx;
	mouse.unscm_y += udy;
}

static void get_scale_range(int *mx_range, int *my_range)
{
	*mx_range = mouse.maxx - MOUSE_MINX +1;
	*my_range = mouse.maxy - MOUSE_MINY +1;
}

static void add_mk(int dx, int dy)
{
	int mx_range, my_range, udx, udy;

	/* pixel range set to 1 */
	udx = dx * 8;
	udy = dy * 8;
	get_scale_range(&mx_range, &my_range);

	add_mickey_coords(udx, udy);
	add_abs_coords(udx * mx_range / (mouse.speed_x * mouse.min_max_x),
		    udy * my_range / (mouse.speed_y * mouse.min_max_y));
}

static void add_px(int dx, int dy)
{
	int mdx, mdy, udx, udy;

	scale_coords_spd_unsc(dx, dy, &mdx, &mdy);
	scale_coords_spd_unsc_mk(dx, dy, &udx, &udy);
	add_mickey_coords(udx, udy);
	add_abs_coords(mdx, mdy);
}

static void reset_scale(void)
{
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

  /* Change mouse.maxx & mouse.maxy from ranges to maximums */
  mouse.maxx = mouse_roundx(mouse.maxx - 1);
  mouse.maxy = mouse_roundy(mouse.maxy - 1);
  mouse.maxx += (1 << mouse.xshift) -1;
  mouse.maxy += (1 << mouse.yshift) -1;
}

static void
mouse_reset_to_current_video_mode(void)
{
  int err;
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
  * stsp: use mode 6 as a fall-back.
  */
  err = get_current_video_mode(&mouse_current_video);
  if (err) {
    m_printf("MOUSE: fall-back to mode 6\n");
    vidmouse_set_video_mode(-1);
    vidmouse_get_video_mode(6, &mouse_current_video);
  }

  if (!mouse.win31_mode)
    reset_scale();

  /* force update first time after reset */
  mouse.oldrx = -1;

  /* Setup up the virtual coordinates */
  mouse.virtual_minx = MOUSE_MINX;
  mouse.virtual_maxx = mouse.maxx;
  mouse.virtual_miny = MOUSE_MINY;
  mouse.virtual_maxy = mouse.maxy;

  m_printf("maxx=%i, maxy=%i speed_x=%i speed_y=%i type=%d\n",
	   mouse.maxx, mouse.maxy, mouse.speed_x, mouse.speed_y,
	   mice->type);
}

static void int33_mouse_enable_native_cursor(int flag, void *udata)
{
  mice->native_cursor = flag;
  mouse_do_cur(1);
}

static void scale_coords_spd(int x, int y, int x_range, int y_range,
	int mx_range, int my_range, int speed_x, int speed_y,
	int *s_x, int *s_y)
{
	*s_x = (x * mx_range * mice->init_speed_x) /
	    (x_range * speed_x) + MOUSE_MINX;
	*s_y = (y * my_range * mice->init_speed_y) /
	    (y_range * speed_y) + MOUSE_MINY;
}

static void scale_coords_spd_unsc_mk(int x, int y, int *s_x, int *s_y)
{
	scale_coords_spd(x, y, 1, 1, mouse.min_max_x, mouse.min_max_y,
		    1, 1, s_x, s_y);
}

static void scale_coords_spd_unsc(int x, int y, int *s_x, int *s_y)
{
	int mx_range, my_range;

	get_scale_range(&mx_range, &my_range);
	scale_coords_spd(x, y, 1, 1, mx_range, my_range,
	mouse.speed_x, mouse.speed_y, s_x, s_y);
}

static void scale_coords_basic(int x, int y, int x_range, int y_range,
	int mx_range, int my_range, int *s_x, int *s_y)
{
	*s_x = (x * mx_range) / x_range + MOUSE_MINX;
	*s_y = (y * my_range) / y_range + MOUSE_MINY;
}

static void scale_coords2(int x, int y, int x_range, int y_range,
	int mx_range, int my_range, int speed_x, int speed_y,
	int *s_x, int *s_y)
{
	/* if cursor is not visible we need to take into
	 * account the user's speed. If it is visible, then
	 * in ungrabbed mode we have to ignore user's speed. */
	if (mouse.cursor_on < 0)
		scale_coords_spd(x, y, x_range, y_range,
			mx_range, my_range, speed_x, speed_y,
			s_x, s_y);
	else
		scale_coords_basic(x, y, x_range, y_range,
			mx_range, my_range, s_x, s_y);
}

static void scale_coords3(int x, int y, int x_range, int y_range,
	int speed_x, int speed_y, int *s_x, int *s_y)
{
	int mx_range, my_range;

	get_scale_range(&mx_range, &my_range);
	scale_coords2(x, y, x_range, y_range, mx_range, my_range,
	speed_x, speed_y, s_x, s_y);
}

static void scale_coords(int x, int y, int x_range, int y_range,
	int *s_x, int *s_y)
{
	int mx_range, my_range;

	get_scale_range(&mx_range, &my_range);
	scale_coords2(x, y, x_range, y_range, mx_range, my_range,
	mouse.speed_x, mouse.speed_y, s_x, s_y);
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
  mouse_reset_to_current_video_mode();

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

  mouse.unscm_x = mouse.unscm_y = 0;
  mouse.unsc_x = mouse.unsc_y = 0;
  mouse.x_delta = mouse.y_delta = 0;
  mouse.need_resync = 0;
  dragged.cnt = 0;

  mouse.textscreenmask = 0xffff;
  mouse.textcursormask = 0x7f00;
  memcpy((void *)mouse.graphscreenmask,default_graphscreenmask,32);
  memcpy((void *)mouse.graphcursormask,default_graphcursormask,32);
  mouse.hotx = mouse.hoty = -1;

  mouse.win31_mode = 0;

  mouse_do_cur(1);
}

void
mouse_cursor(int flag)	/* 1=show, -1=hide */
{
  int need_resync = 0;
  /* Delete exclusion zone, if show cursor applied */
  if (flag == 1) {
    mouse.exc_lx = mouse.exc_ux = -1;
    mouse.exc_ly = mouse.exc_uy = -1;
    if (mouse.x_delta || mouse.y_delta) {
      mouse.x_delta = mouse.y_delta = 0;
      need_resync = 1;
    }
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
    if (flag == 1 && need_resync && !dragged.cnt)
      do_move_abs(mouse.px_abs, mouse.py_abs, mouse.px_range, mouse.py_range);
    mouse_client_show_cursor(mouse.cursor_on >= 0);
  }

  m_printf("MOUSE: %s mouse cursor %d\n", mouse.cursor_on ? "hide" : "show", mouse.cursor_on);
}

void
mouse_pos(void)
{
  m_printf("MOUSE: get mouse position x:%d, y:%d, b(l%d m%d r%d)\n", get_mx(),
	   get_my(), mouse.lbutton, mouse.mbutton, mouse.rbutton);
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
  setxy(LWORD(ecx), LWORD(edx));
  mouse_round_coords();
  mouse_hide_on_exclusion();
  if (mouse.cursor_on >= 0) {
    mouse.x_delta = mouse.y_delta = 0;
    mouse_do_cur(1);
  } else {
    int abs_x, abs_y;
    scale_coords(mouse.px_abs, mouse.py_abs, mouse.px_range, mouse.py_range,
	    &abs_x, &abs_y);
    mouse.x_delta = get_mx() - abs_x;
    mouse.y_delta = get_my() - abs_y;
  }
  m_printf("MOUSE: set cursor pos x:%d, y:%d delta x:%d, y:%d\n",
	get_mx(), get_my(), mouse.x_delta, mouse.y_delta);
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
  int mkx = mickeyx();
  int mky = mickeyy();
  m_printf("MOUSE: read mickeys %d %d\n", mkx, mky);
  LWORD(ecx) = mkx;
  LWORD(edx) = mky;

  /* counters get reset after read */
  if (mkx)
    mouse.unscm_x -= get_unsc_mk_x(mkx) * 8;
  if (mky)
    mouse.unscm_y -= get_unsc_mk_y(mky) * 8;

  if (dragged.cnt) {
    dragged.cnt = 0;
    if (dragged.skipped) {
      dragged.skipped = 0;
      do_move_abs(dragged.x, dragged.y, dragged.x_range, dragged.y_range);
    }
  }
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
	case DKY_MOUSE_DOWN:
		state.d = !!make;
		break;
	case DKY_MOUSE_LEFT:
		state.l = !!make;
		break;
	case DKY_MOUSE_RIGHT:
		state.r = !!make;
		break;
	case DKY_MOUSE_UP:
		state.u = !!make;
		break;
	case DKY_MOUSE_UP_AND_LEFT:
		state.ul = make;
		break;
	case DKY_MOUSE_UP_AND_RIGHT:
		state.ur = make;
		break;
	case DKY_MOUSE_DOWN_AND_LEFT:
		state.dl = make;
		break;
	case DKY_MOUSE_DOWN_AND_RIGHT:
		state.dr = make;
		break;
	case DKY_MOUSE_BUTTON_LEFT:
		state.lbutton = make;
		break;
	case DKY_MOUSE_BUTTON_RIGHT:
		state.rbutton = make;
		break;
	case DKY_MOUSE_BUTTON_MIDDLE:
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

static int mouse_round_coords2(int x, int y, int *r_x, int *r_y)
{
	int clipped = 0;

	*r_x = x;
	*r_y = y;
	/* in ps2 mode ignore clipping */
	if (mouse.ps2.cs || mouse.ps2.ip)
		return 0;
	/* put the mouse coordinate in bounds */
	if (x < mouse.virtual_minx) {
		*r_x = mouse.virtual_minx;
		clipped = 1;
	}
	if (y < mouse.virtual_miny) {
		*r_y = mouse.virtual_miny;
		clipped = 1;
	}
	if (x > mouse.virtual_maxx) {
		*r_x = mouse.virtual_maxx;
		clipped = 1;
	}
	if (y > mouse.virtual_maxy) {
		*r_y = mouse.virtual_maxy;
		clipped = 1;
	}
	return clipped;
}

static int mouse_round_coords(void)
{
	int newx, newy, ret;

	ret = mouse_round_coords2(get_mx(), get_my(), &newx, &newy);
	mouse.unsc_x = get_unsc_x(newx);
	mouse.unsc_y = get_unsc_y(newy);
	return ret;
}

static void mouse_hide_on_exclusion(void)
{
  /* Check exclusion zone, then turn off cursor if necessary */
  if (mouse.exc_ux != -1) {
    short xl, xr, yt, yb;

    /* find the cursor box's edges coords (in mouse coords not screen) */
    if (mouse.gfx_cursor) { /* graphics cursor */
      xl = get_mx() - (mouse.hotx << mouse.xshift);
      xr = get_mx() + ((HEIGHT-1 - mouse.hotx) << mouse.xshift);
      yt = get_my() - (mouse.hoty << mouse.yshift);
      yb = get_my() + ((HEIGHT-1 - mouse.hoty) << mouse.yshift);
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

  m_printf("MOUSE: move: x=%d,y=%d\n", get_mx(), get_my());

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
	if (dragged.skipped) {
		dragged.skipped = 0;
		do_move_abs(dragged.x, dragged.y, dragged.x_range,
			    dragged.y_range);
	}

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
	if (mouse.px_range != x_range || mouse.py_range != y_range)
		set_px_ranges(x_range, y_range);
	add_px(dx, dy);
	mouse.x_delta = mouse.y_delta = 0;
	mouse.need_resync = 1;

	m_printf("mouse_move_relative(%d, %d) -> %d %d \n",
		 dx, dy, get_mx(), get_my());

	/*
	 * update the event mask
	 */
	if (dx || dy)
	   mouse_move(0);
}

static void int33_mouse_move_mickeys(int dx, int dy, void *udata)
{
	if (mouse.px_range != 1 || mouse.py_range != 1)
		set_px_ranges(1, 1);
	add_mk(dx, dy);

	m_printf("mouse_move_mickeys(%d, %d) -> %d %d \n",
		 dx, dy, get_mx(), get_my());

	/*
	 * update the event mask
	 */
	if (dx || dy)
	   mouse_move(0);
}

static int move_abs_mickeys(int dx, int dy, int x_range, int y_range)
{

	int ret = 0, mdx, mdy, oldmx = mickeyx(), oldmy = mickeyy();
	scale_coords_spd_unsc_mk(dx, dy, &mdx, &mdy);

	if (mdx || mdy) {
		add_mickey_coords(mdx, mdy);
		ret = 1;
	}
	m_printf("mouse_move_absolute dx:%d dy:%d mickeys: %d %d -> %d %d\n",
			 dx, dy, oldmx, oldmy, mickeyx(), mickeyy());

	return ret;
}

static int move_abs_coords(int x, int y, int x_range, int y_range)
{
	int new_x, new_y, clipped, c_x, c_y, oldx = get_mx(), oldy = get_my();

	/* simcity hack: only handle cursor speed-ups but ignore
	 * slow-downs. */
	scale_coords3(x, y, x_range, y_range,
			min(mouse.speed_x, mice->init_speed_x),
			min(mouse.speed_y, mice->init_speed_y),
			&new_x, &new_y);
	/* for visible cursor always recalc deltas */
	if (mouse.cursor_on >= 0)
		mouse.x_delta = mouse.y_delta = 0;
	clipped = mouse_round_coords2(new_x + mouse.x_delta,
		    new_y + mouse.y_delta, &c_x, &c_y);
	/* we dont allow DOS prog to grab mouse pointer by locking it
	 * inside a clipping region. So just update deltas. If cursor
	 * is visible, we always try to keep them at 0. */
	if (clipped) {
	    mouse.x_delta = c_x - new_x;
	    mouse.y_delta = c_y - new_y;
	}
	mouse.unsc_x = get_unsc_x(c_x);
	mouse.unsc_y = get_unsc_y(c_y);

	m_printf("mouse_move_absolute(%d, %d, %d, %d) %d %d -> %d %d \n",
		 x, y, x_range, y_range, oldx, oldy, get_mx(), get_my());

	return 1;
}

static void do_move_abs(int x, int y, int x_range, int y_range)
{
	int moved = 0;

	if (mouse.px_range != x_range || mouse.py_range != y_range)
		set_px_ranges(x_range, y_range);

	moved |= move_abs_mickeys(x - mouse.px_abs, y - mouse.py_abs,
		    x_range, y_range);
	moved |= move_abs_coords(x, y, x_range, y_range);

	mouse.px_abs = x;
	mouse.py_abs = y;

	/*
	 * update the event mask
	 */
	if (moved)
		mouse_move(0);
}

static void int33_mouse_move_absolute(int x, int y, int x_range, int y_range,
	void *udata)
{
	/* give an app some time to chew dragging */
	if (dragged.cnt > 1) {
		m_printf("MOUSE: deferring abs move\n");
		dragged.skipped++;
		dragged.x = x;
		dragged.y = y;
		dragged.x_range = x_range;
		dragged.y_range = y_range;
		return;
	}
	if (mouse.need_resync) {
		/* this is mainly for Carmageddon. It can't tolerate the
		 * mickeys getting out of sync with coords. So we recalculate
		 * the position when switching from rel to abs mode. */
		int mx_range, my_range;
		mouse.need_resync = 0;
		/* for invisible cursor update deltas and return */
		if (mouse.cursor_on < 0) {
			int new_x, new_y;
			scale_coords(x, y, x_range, y_range, &new_x, &new_y);
			mouse.x_delta = get_mx() - new_x;
			mouse.y_delta = get_my() - new_y;
			mouse.px_abs = x;
			mouse.py_abs = y;
			return;
		}
		get_scale_range(&mx_range, &my_range);
		mouse.px_abs = mouse.unsc_x * mouse.speed_x /
			    (mice->init_speed_x * mx_range);
		mouse.py_abs = mouse.unsc_y * mouse.speed_y /
			    (mice->init_speed_y * my_range);
		m_printf("MOUSE: synced coords, x:%i y:%i\n",
			    mouse.px_abs, mouse.py_abs);
	}
	do_move_abs(x, y, x_range, y_range);
}

/* this is for buggy apps that use the mickey tracking and have
 * the unreachable areas in ungrabbed mode, because of the resolution
 * mismatch (they organize their own resolution via mickey tracking).
 * Transport Tycoon is an example.
 * The idea is that having an unreachable area only at the bottom is
 * better than randomly in different places, like at the top.
 */
static void int33_mouse_drag_to_corner(int x_range, int y_range, void *udata)
{
	m_printf("MOUSE: drag to corner, %i\n", dragged.cnt);
	if (dragged.cnt > 1)
		return;
	int33_mouse_move_relative(-3 * x_range, -3 * y_range, x_range, y_range,
		udata);
	dragged.cnt = 5;
	dragged.skipped = 0;
	mouse.px_abs = 0;
	mouse.py_abs = 0;
	mouse.x_delta = mouse.y_delta = 0;
	mouse.need_resync = 0;
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
    dx = mickeyx();
    /* PS/2 wants the y direction reversed */
    dy = -mickeyy();

    status = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0) | 8;
    if (mouse.threebuttons)
      status |= (mouse.mbutton ? 4 : 0);
    /* overflow; many DOS drivers do a simple cbw on the value... */
    if (dx < -128 || dx > 127)
      dx = dx < -128 ? -128 : 127;
    if (dy < -128 || dy > 127)
      dy = dy < -128 ? -128 : 127;
    mouse.unscm_x -= get_unsc_mk_x(dx << 3);
    mouse.unscm_y += get_unsc_mk_y(dy << 3);
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
  } while (mickeyx() || mickeyy());

  REGS = saved_regs;
}

static void call_int33_mouse_event_handler(void)
{
    struct vm86_regs saved_regs;

    saved_regs = REGS;
    LWORD(eax) = mouse_events;
    LWORD(ecx) = get_mx();
    LWORD(edx) = get_my();
    LWORD(esi) = mickeyx();
    LWORD(edi) = mickeyy();
    LWORD(ebx) = (mouse.rbutton ? 2 : 0) | (mouse.lbutton ? 1 : 0);
    if (mouse.threebuttons)
      LWORD(ebx) |= (mouse.mbutton ? 4 : 0);

    /* jump to mouse cs:ip */
    m_printf("MOUSE: event %d, x %d, y %d, mx %d, my %d, b %x\n",
	     mouse_events, get_mx(), get_my(), mickeyx(), mickeyy(),
	     LWORD(ebx));
    m_printf("MOUSE: .........jumping to %04x:%04x\n", mouse.cs, mouse.ip);
    SREG(ds) = mouse.cs;		/* put DS in user routine */
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
    if (dragged.cnt) {
      /* syncing mickey counters with coords is silly, I know, but
       * Carmageddon needs this after dragging mouse to the corner. */
      setmxy(get_mx() * mouse.speed_x, get_my() * mouse.speed_y);
      dragged.cnt = 0;
      m_printf("MOUSE: mickey synced with coords, x:%i->%i y:%i->%i\n",
          get_mx(), mickeyx(), get_my(), mickeyy());
    }
}

/* this function is called from int74 via inte6 */
static void call_mouse_event_handler(void)
{
  int handled = 0;

  if (mouse_events && mouse.ps2.state && (mouse.ps2.cs || mouse.ps2.ip)) {
    call_int15_mouse_event_handler();
    handled = 1;
  } else {
    if (mouse.mask & mouse_events && (mouse.cs || mouse.ip)) {
      call_int33_mouse_event_handler();
      handled = 1;
    } else {
      m_printf("MOUSE: Skipping event handler, "
	       "mask=0x%x, ev=0x%x, cs=0x%x, ip=0x%x\n",
	       mouse.mask, mouse_events, mouse.cs, mouse.ip);
    }
  }
  mouse_events = 0;

  if (handled && dragged.skipped) {
    dragged.skipped = 0;
    do_move_abs(dragged.x, dragged.y, dragged.x_range, dragged.y_range);
  }
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
#if 0
  /* this callback is used to e.g. warp the X cursor if int33/ax=4
     requested it to be moved */
  mouse_client_set_cursor(mouse.cursor_on == 0?1: 0,
		    get_mx() - mouse.x_delta,
		    get_my() - mouse.y_delta,
		    mouse.maxx - MOUSE_MINX +1, mouse.maxy - MOUSE_MINY +1);
#endif
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


static void mouse_curtick(void)
{
  if (!mice->intdrv)
    return;

  if (debug_level('m') >= 9)
    m_printf("MOUSE: curtick x:%d  y:%d\n", MOUSE_RX, MOUSE_RY);

  /* HACK: we need some time for an app to sense the dragging event */
  if (dragged.cnt > 1) {
    dragged.cnt--;
  } else if (dragged.skipped) {
    dragged.skipped = 0;
    do_move_abs(dragged.x, dragged.y, dragged.x_range, dragged.y_range);
  }
  if (mouse.cursor_on != 0)
    return;

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
  if (!mice->intdrv)
    return 0;

  /* set minimum internal resolution
   * 640x200 is a long standing dosemu default
   * although I accidentally broke it broke it by flipping my X & Y coordiantes.
   * -- Eric Biederman 29 May 2000
   */
  mouse.min_max_x = 640;
  mouse.min_max_y = 200;

  mouse.px_range = mouse.py_range = 1;
  mouse.px_abs = mouse.py_abs = 0;

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

  pic_seti(PIC_IMOUSE, NULL, 0, NULL);
  sigalrm_register_handler(mouse_curtick);

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

  mouse_reset_to_current_video_mode();
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

void mouse_set_win31_mode(void)
{
  if (!mice->intdrv) {
    CARRY;
    return;
  }

  mouse.maxx = 65535;
  mouse.maxy = 65535;
  mouse.win31_mode = 1;
  m_printf("MOUSE: Enabled win31 mode\n");

  LWORD(eax) = 0;
}

void
dosemu_mouse_close(void)
{
  mouse_client_close();
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
  int33_mouse_enable_native_cursor,
  "int33 mouse"
};

CONSTRUCTOR(static void int33_mouse_register(void))
{
  register_mouse_driver(&int33_mouse);
}
