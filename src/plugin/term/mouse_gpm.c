/* 
 * (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */


/* mouse support for GPM */

#include <gpm.h>
#include <fcntl.h>

#include "config.h"
#include "emu.h"
#include "video.h"
#include "mouse.h"
#include "utilities.h"

static void gpm_getevent(void)
{
	static unsigned char buttons;
	Gpm_Event ev;
	Gpm_GetEvent(&ev);
	m_printf("MOUSE: Get GPM Event, %d\n", ev.type);
	switch (GPM_BARE_EVENTS(ev.type)) {
	case GPM_MOVE:
	case GPM_DRAG:
		mouse_move_absolute(ev.x - 1, ev.y - 1, co, li);
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

	if (config.vga || !mice->intdrv || !is_console( 0 ))
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

struct mouse_client Mouse_gpm =	 {
	"gpm",		/* name */
	gpm_init,	/* init */
	gpm_close,	/* close */
	gpm_getevent,	/* run */
	NULL		/* set_cursor */
};
