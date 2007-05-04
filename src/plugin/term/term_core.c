/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */
/* 
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Put slang specific code here to avoid initialization & cleanup races.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 18 October 1998: Created
 * -- EB
 *
 * DANG_END_CHANGELOG
 */

#include <slang.h>
#include "dosemu_debug.h"
#include "emu.h"
#include "init.h"
#include "video.h"
#include "vc.h"
#include "keyb_clients.h"
#include "utilities.h"

#include "env_term.h"

#ifndef SLANG_VERSION
# define SLANG_VERSION 1
#endif

/* unsigned char DOSemu_Slang_Escape_Character = 30; */

const char *DOSemu_Keyboard_Keymap_Prompt = NULL;
int DOSemu_Terminal_Scroll = 0;
int DOSemu_Slang_Show_Help = 0;

#if SLANG_VERSION < 10000
static void sl_exit_error (char *err)
{
	error ("%s\n", err);
	leavedos (32);
}
#else
#include <stdarg.h>
static void sl_exit_error (char *fmt, va_list args)
{
	verror (fmt, args);
	leavedos (32);
}
#endif

static void sl_print_error(char *str)
{
	dbug_printf("Slang:%s\n", str);
}

static int term_initialized = 0;
int term_init(void)
{
	if (term_initialized++)
		return 0;
	SLang_Exit_Error_Hook = sl_exit_error;
	SLtt_get_terminfo ();
	/* FIXME USE_OLD_SLANG_KBD */
#if SLANG_VERSION < 10000
	SLang_Error_Routine = sl_print_error;
#else
	SLang_Error_Hook = sl_print_error;
#endif
	return 0;
}

void term_close(void)
{
	if (--term_initialized != 0) 
		return;
}

CONSTRUCTOR(static void init(void))
{
	if (!Video) {
		Video = &Video_term;
		v_printf("VID: Video set to Video_term\n");
	}
	register_keyboard_client(&Keyboard_raw);
	register_keyboard_client(&Keyboard_slang);
#ifdef USE_GPM
	if (!config.vga && config.mouse.intdrv && on_console())
		load_plugin("gpm");
#endif
	register_mouse_client(&Mouse_xterm);
}
