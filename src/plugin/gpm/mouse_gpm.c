/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */


/* mouse support for GPM */

#include <gpm.h>
#include <fcntl.h>

#include "config.h"
#include "emu.h"
#include "video.h"
#include "mouse.h"
#include "utilities.h"
#include "init.h"
#include "vc.h"

/* there exist several binary incompatible Gpm_Event structures,
   depending on the GPM version and distribution. 
   This makes it impossible to use the structure from gpm.h while
   still having cross-distribution binaries */
typedef struct {
	unsigned char buttons, modifiers;
	unsigned short vc;
	short dx, dy, x, y;
	union {
		/* official GPM >= 1.20.1 */
		struct {
			int     type, clicks, margin;
			/* old GPMs did not have wdx and wdy */
			short   wdx, wdy;
		} gpm_w1;
		/* patched Debian GPM */
		struct {
			short   wdx, wdy;
			int     type, clicks, margin;
		} gpm_w2;
	} tail;
} dosemu_Gpm_Event;

static void gpm_getevent(void)
{
	static unsigned char buttons;
	static int variety = 1;
	dosemu_Gpm_Event ev;
	fd_set mfds;
	int type;

	FD_ZERO(&mfds);
	FD_SET(config.mouse.fd, &mfds);
	if (select(config.mouse.fd + 1, &mfds, NULL, NULL, NULL) <= 0)
		return;
	Gpm_GetEvent((Gpm_Event*)&ev);
	type = GPM_BARE_EVENTS(ev.tail.gpm_w1.type);
	if( variety == 1 && type != GPM_DRAG && type != GPM_DOWN &&
	    type != GPM_UP && type != GPM_MOVE )
		variety = 2;
	if( variety == 2 )
		type = GPM_BARE_EVENTS(ev.tail.gpm_w2.type);
	m_printf("MOUSE: Get GPM Event, %d\n", type);
	switch (type) {
	case GPM_MOVE:
	case GPM_DRAG:
		mouse_move_absolute(ev.x - 1, ev.y - 1, gpm_mx, gpm_my);
		break;
	case GPM_UP:
		if (ev.buttons & GPM_B_LEFT) buttons &= ~GPM_B_LEFT;
		if (ev.buttons & GPM_B_MIDDLE) buttons &= ~GPM_B_MIDDLE;
		if (ev.buttons & GPM_B_RIGHT) buttons &= ~GPM_B_RIGHT;
		ev.buttons = buttons;
		/* fall through */
	case GPM_DOWN:
		mouse_move_buttons(ev.buttons & GPM_B_LEFT,
				   ev.buttons & GPM_B_MIDDLE,
				   ev.buttons & GPM_B_RIGHT);
		buttons = ev.buttons;
		/* fall through */
	default:
		break;
	}
}

static int gpm_init(void)
{
	int fd;
	mouse_t *mice = &config.mouse;
	Gpm_Connect conn;

	if (config.vga || !mice->intdrv || !on_console())
		return FALSE;
	
	conn.eventMask	 = ~0;
	conn.defaultMask = GPM_MOVE;
	conn.minMod	 = 0;
	conn.maxMod	 = 0;

	fd = Gpm_Open(&conn, 0);
	if (fd < 0)
		return FALSE;

	mice->fd = fd;
	mice->type = MOUSE_GPM;
	mice->use_absolute = 1;
	fcntl(mice->fd, F_SETFL, fcntl(mice->fd, F_GETFL) | O_NONBLOCK);
	add_to_io_select(mice->fd, 1, mouse_io_callback);
	m_printf("GPM MOUSE: Using GPM Mouse\n");
	return TRUE;
}

static void gpm_close(void)
{
	Gpm_Close();
	m_printf("GPM MOUSE: Mouse tracking deinitialized\n");
}

static struct mouse_client Mouse_gpm = {
	"gpm",		/* name */
	gpm_init,	/* init */
	gpm_close,	/* close */
	gpm_getevent,	/* run */
	NULL		/* set_cursor */
};

CONSTRUCTOR(static void init(void))
{
	register_mouse_client(&Mouse_gpm);
}
