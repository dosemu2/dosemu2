/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#include <stdlib.h>
#include <stdio.h>
#include <slang.h>

#include "config.h"
#include "emu.h"
#include "video.h"
#include "env_term.h"
#include "mouse.h"
#include "utilities.h"
#include "vgaemu.h"
#include "vc.h"

/* XTERM MOUSE suport by M.Laak */
void xtermmouse_get_event (Bit8u **kbp, int *kbcount)
{
	int btn;
	static int last_btn = 0;
	int x_pos, y_pos;

	/* Decode Xterm mouse information to a GPM style event */

	if (*kbcount >= 3) {
		x_pos = (*kbp)[1] - 33;
		y_pos = (*kbp)[2] - 33;
		mouse_move_absolute(x_pos, y_pos,
			READ_WORD(BIOS_SCREEN_COLUMNS),
			READ_BYTE(BIOS_ROWS_ON_SCREEN_MINUS_1) + 1);
		m_printf("XTERM MOUSE: movement detected to pos x=%d: y=%d\n", x_pos, y_pos);

		/* Variable btn has following meaning: */
		/* 0 = btn1 dn, 1 = btn2 dn, 2 = btn3 dn, 3 = btn up,
		 * 32 = no button state change */
		btn = (*kbp)[0] - 32;

		switch (btn) {
		case 0:
			mouse_move_buttons(1, 0, 0);
			m_printf("XTERM MOUSE: left button click detected\n");
			last_btn = 1;
			break;
		case 1:
			mouse_move_buttons(0, 1, 0);
			m_printf("XTERM MOUSE: middle button click detected\n");
			last_btn = 2;
			break;
		case 2:
			mouse_move_buttons(0, 0, 1);
			m_printf("XTERM MOUSE: right button click detected\n");
			last_btn = 3;
			break;
		case 3:
			/* There seems to be no way of knowing which button was released */
			/* So we assume all the buttons were released */
			if (last_btn) {
				mouse_move_buttons(0, 0, 0);
				m_printf("XTERM MOUSE: button release detected\n");
				last_btn = 0;
			}
			break;
		}
		*kbcount -= 3;	/* update count */
		*kbp += 3;
	}
}

static int has_xterm_mouse_support(void)
{
	if (config.vga || on_console())
		return 0;

	term_init();
	return SLtt_tgetstr ("Km") || using_xterm();
}

static int xterm_mouse_init(void)
{
	mouse_t *mice = &config.mouse;

	if (!has_xterm_mouse_support())
		return FALSE;

	mice->type = MOUSE_XTERM;
	mice->native_cursor = 0;      /* we have the xterm cursor */
#if 0
	SLtt_set_mouse_mode(0, 0);
#else
	/* save old highlight mouse tracking */
	printf("\033[?1001s");
	/* enable mouse tracking */
	printf("\033[?9h\033[?1000h\033[?1002h\033[?1003h");
	fflush(stdout);
	m_printf("XTERM MOUSE: Remote terminal mouse tracking enabled\n");
#endif

	return TRUE;
}

static void xterm_mouse_close(void)
{
	/* disable mouse tracking */
	printf("\033[?1003l\033[?1002l\033[?1000l\033[9l");
	/* restore old highlight mouse tracking */
	printf("\033[?1001r\033[?1001l");
	fflush(stdout);

	m_printf("XTERM MOUSE: Mouse tracking deinitialized\n");
}

struct mouse_client Mouse_xterm =  {
	"xterm",		/* name */
	xterm_mouse_init,	/* init */
	xterm_mouse_close,	/* close */
	NULL,
};
