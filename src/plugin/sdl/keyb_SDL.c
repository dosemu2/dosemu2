/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Authors: Emmanuel Jeandel, Bart Oldeman
 * SDL2 and XKB support by Stas Sergeev
 */

#include "dosemu_config.h"

#include <SDL.h>
#include <langinfo.h>
#include <string.h>
#ifdef X_SUPPORT
#include <X11/Xlib.h>
#include <dlfcn.h>
#endif

#include "emu.h"
#include "keyb_clients.h"
#include "keyboard.h"
#include "keysym_attributes.h"
#ifdef X_SUPPORT
#include "keyb_X.h"
#include "keynum.h"
#include "sdl2-keymap.h"
#endif
#include "video.h"
#include "sdl.h"

#ifndef USE_DL_PLUGINS
#undef X_SUPPORT
#endif

/*
 * at startup SDL does not detect numlock and capslock status
 * so we get them from xkb.
 *
 * The fix is targeted for 2.0.4:
 * https://bugzilla.libsdl.org/show_bug.cgi?id=2736
 */
#define SDL_BROKEN_MODS 1

#ifdef X_SUPPORT
#ifdef USE_DL_PLUGINS
#define X_get_modifier_info pX_get_modifier_info
static struct modifier_info (*X_get_modifier_info)(void);
#define Xkb_lookup_key pXkb_lookup_key
static t_unicode (*Xkb_lookup_key)(Display *display, KeyCode keycode,
	unsigned int state);
#define X_keycode_initialize pX_keycode_initialize
static void (*X_keycode_initialize)(Display *display);
#define keyb_X_init pkeyb_X_init
static void (*keyb_X_init)(Display *display);
#define keynum_to_keycode pkeynum_to_keycode
static KeyCode (*keynum_to_keycode)(t_keynum keynum);
#define Xkb_get_group pXkb_get_group
static int (*Xkb_get_group)(Display *display, unsigned int *mods);
#define X_sync_shiftstate pX_sync_shiftstate
static void (*X_sync_shiftstate)(Boolean make, KeyCode kc, unsigned int e_state);
#endif
#endif

static int use_move_key(t_keysym key)
{
	int result = FALSE;
        /* If it's some kind of function key move it
         * otherwise just make sure it gets pressed
         */
        if (is_keysym_function(key) ||
            is_keysym_dosemu_key(key) ||
            is_keypad_keysym(key) ||
            (key == DKY_TAB) ||
            (key == DKY_RETURN) ||
            (key == DKY_BKSP)) {
                result = TRUE;
        }
	return result;
}

static t_modifiers map_SDL_modifiers(SDL_Keymod e_state)
{
        t_modifiers modifiers = 0;
        if (e_state & KMOD_SHIFT) {
                modifiers |= MODIFIER_SHIFT;
        }
        if (e_state & KMOD_CTRL) {
                modifiers |= MODIFIER_CTRL;
        }
        if (e_state & KMOD_LALT) {
                modifiers |= MODIFIER_ALT;
        }
        if (e_state & (KMOD_RALT|KMOD_MODE)) {
                modifiers |= MODIFIER_ALTGR;
        }
        if (e_state & KMOD_CAPS) {
                modifiers |= MODIFIER_CAPS;
        }
        if (e_state & KMOD_NUM) {
                modifiers |= MODIFIER_NUM;
        }
#if 0
        if (e_state & X_mi.ScrollLockMask) {
                modifiers |= MODIFIER_SCR;
        }
        if (e_state & X_mi.InsLockMask) {
                modifiers |= MODIFIER_INS;
        }
#endif
        return modifiers;
}

static void SDL_sync_shiftstate(Boolean make, SDL_Keycode kc, SDL_Keymod e_state)
{
	t_modifiers shiftstate = get_shiftstate();

	/* Check for modifiers released outside this window */
	if (!!(shiftstate & MODIFIER_SHIFT) != !!(e_state & KMOD_SHIFT)) {
		shiftstate ^= MODIFIER_SHIFT;
	}
	if (!!(shiftstate & MODIFIER_CTRL) != !!(e_state & KMOD_CTRL)) {
		shiftstate ^= MODIFIER_CTRL;
	}
	if (!!(shiftstate & MODIFIER_ALT) != !!(e_state & KMOD_LALT)) {
		shiftstate ^= MODIFIER_ALT;
	}
	if (!!(shiftstate & MODIFIER_ALTGR) != !!(e_state & (KMOD_RALT|KMOD_MODE))) {
		shiftstate ^= MODIFIER_ALTGR;
	}

	if (!!(shiftstate & MODIFIER_CAPS) != !!(e_state & KMOD_CAPS)
		&& (make || (kc != SDLK_CAPSLOCK))) {
		shiftstate ^= MODIFIER_CAPS;
	}
	if (!!(shiftstate & MODIFIER_NUM) != !!(e_state & KMOD_NUM)
		&& (make || (kc != SDLK_NUMLOCKCLEAR))) {
		shiftstate ^= MODIFIER_NUM;
	}
#if 0
	if (!!(shiftstate & MODIFIER_SCR) != !!(e_state & X_mi.ScrollLockMask)) {
		&& (make || (kc != SDLK_SCROLLOCK))) {
		shiftstate ^= MODIFIER_SCR;
	}
	if (!!(shiftstate & MODIFIER_INS) != !!(e_state & X_mi.InsLockMask)) {
		shiftstate ^= MODIFIER_INS;
	}
#endif
	set_shiftstate(shiftstate);
}

#ifdef X_SUPPORT
static unsigned int SDL_to_X_mod(SDL_Keymod smod, int grp)
{
	/* try to reconstruct X event keyboard state */
	unsigned int xmod = 0;
	struct modifier_info X_mi = X_get_modifier_info();
	if (smod & KMOD_SHIFT)
		xmod |= ShiftMask;
	if (smod & KMOD_CTRL)
		xmod |= ControlMask;
	if (smod & KMOD_LALT)
		xmod |= X_mi.AltMask;
	if (smod & KMOD_RALT)
		xmod |= X_mi.AltGrMask;
#if !SDL_BROKEN_MODS
	if (smod & KMOD_CAPS)
		xmod |= X_mi.CapsLockMask;
	if (smod & KMOD_NUM)
		xmod |= X_mi.NumLockMask;
#endif
	if (grp)
		xmod |= 1 << (grp + 12);
	return xmod;
}

void SDL_process_key_xkb(Display *display, SDL_KeyboardEvent keyevent)
{
	SDL_Keysym keysym = keyevent.keysym;
	SDL_Scancode scan = keysym.scancode;
	t_keynum keynum = sdl2_scancode_to_keynum[scan];
	KeyCode keycode = keynum_to_keycode(keynum);
	unsigned int xm;
	int grp = Xkb_get_group(display, &xm);
	unsigned int xmod = SDL_to_X_mod(keysym.mod, grp);
	t_unicode key;
	int make;
#if SDL_BROKEN_MODS
	xmod |= xm;
#endif
	key = Xkb_lookup_key(display, keycode, xmod);
	make = (keyevent.state == SDL_PRESSED);
	X_sync_shiftstate(make, keycode, xmod);
	move_keynum(make, keynum, key);
}
#endif

void SDL_process_key(SDL_KeyboardEvent keyevent)
{
	SDL_Keysym keysym = keyevent.keysym;
	t_unicode key = DKY_VOID;
	t_modifiers modifiers = map_SDL_modifiers(keysym.mod);
	switch (keysym.sym) {
	  case SDLK_UNKNOWN:
		/* workaround for X11+SDL bug with AltGR (from QEMU) */
		if (keysym.scancode == 113)
			key = DKY_R_ALT;
		break;

	  case SDLK_SPACE ... SDLK_z: /* ASCII range 32..122 */
		key = keysym.sym;
		break;
#if 0
	  case SDLK_WORLD_0 ... SDLK_WORLD_95:
		/* workaround for older SDLs; harmless for newer;
		   this just fixes the iso-8859-1 subset of utf-8; other
		   characters are almost impossible with SDL < 1.2.10
		 */
		if (key < 0x100 && strcmp(nl_langinfo(CODESET), "UTF-8") == 0)
			key = keysym.sym;
		break;
#endif
	  case SDLK_CAPSLOCK:
		key = DKY_CAPS;
		break;

	  case SDLK_NUMLOCKCLEAR:
		key = DKY_NUM;
		break;

	  case SDLK_SCROLLLOCK:
		key = DKY_SCROLL;
		break;

#define DOKP(x) case SDLK_KP_##x: key = DKY_PAD_##x; break;
	  DOKP(0);
	  DOKP(1);
	  DOKP(2);
	  DOKP(3);
	  DOKP(4);
	  DOKP(5);
	  DOKP(6);
	  DOKP(7);
	  DOKP(8);
	  DOKP(9);

	  case SDLK_KP_PERIOD:
		key = DKY_PAD_DECIMAL;
		break;

	  case SDLK_KP_DIVIDE:
		key = DKY_PAD_SLASH;
		break;

	  case SDLK_KP_MULTIPLY:
		key = DKY_PAD_AST;
		break;

	  case SDLK_KP_MINUS:
		key = DKY_PAD_MINUS;
		break;

	  case SDLK_KP_PLUS:
		key = DKY_PAD_PLUS;
		break;

	  case SDLK_KP_ENTER:
		key = DKY_PAD_ENTER;
		break;

	  case SDLK_KP_EQUALS:
		key = DKY_PAD_EQUAL;
		break;

	  case SDLK_F1 ... SDLK_F10:
		key = (keysym.sym - SDLK_F1) + DKY_F1 ;
		break;
#define DOKEY(x) case SDLK_##x: key = DKY_##x; break;
	  DOKEY(F11)
	  DOKEY(F12)
	  DOKEY(F13)
	  DOKEY(F14)
	  DOKEY(F15)
	  DOKEY(RETURN)
	  DOKEY(TAB)
	  DOKEY(PAUSE)
//	  DOKEY(BREAK)
	  DOKEY(HOME)
	  DOKEY(LEFT)
	  DOKEY(UP)
	  DOKEY(RIGHT)
	  DOKEY(DOWN)
	  DOKEY(END)
	  case SDLK_PAGEUP:
		key = DKY_PGUP;
		break;
	  case SDLK_PAGEDOWN:
		key = DKY_PGDN;
		break;
	  case SDLK_DELETE:
		key = DKY_DEL;
		break;

	  case SDLK_INSERT:
		key = DKY_INS;
		break;

	  case SDLK_BACKSPACE:
		key = DKY_BKSP;
		break;

	  case SDLK_ESCAPE:
		key = DKY_ESC;
		break;

	  case SDLK_SYSREQ:
		key = DKY_SYSRQ;
		break;

	  case SDLK_CLEAR:
		key = DKY_DOSEMU_CLEAR;
		break;
#if 0
	  case SDLK_COMPOSE:
		key = DKY_MULTI_KEY;
		break;
#endif
	  case SDLK_PRINTSCREEN:
		key = DKY_PRTSCR;
		break;

	  case SDLK_MENU:
		key = DKY_DOSEMU_UNDO;
		break;

	  case SDLK_HELP:
		key = DKY_DOSEMU_HELP;
		break;
#if 0
	  case SDLK_EURO:
		key = U_EURO_SIGN;
		break;
#endif
	  case SDLK_UNDO:
		key = DKY_DOSEMU_UNDO;
		break;

	  case SDLK_RSHIFT: key = DKY_R_SHIFT ; break;
	  case SDLK_LSHIFT: key = DKY_L_SHIFT ; break;
	  case SDLK_RCTRL: key = DKY_R_CTRL ; break;
	  case SDLK_LCTRL: key = DKY_L_CTRL ; break;
//	  case SDLK_RMETA: key = DKY_R_META ; break;
	  case SDLK_MODE: key = DKY_MODE_SWITCH ; break;
	  case SDLK_RALT: key = DKY_R_ALT ; break;
//	  case SDLK_RSUPER: key = DKY_R_SUPER ; break;
//	  case SDLK_LMETA: key = DKY_L_META ; break;
	  case SDLK_LALT: key = DKY_L_ALT ; break;
//	  case SDLK_LSUPER: key = DKY_L_SUPER ; break;

	  /* case SDLK_POWER: */
	  default:
		if (keysym.sym > 255)
			key = DKY_VOID;
		break;
	}
	SDL_sync_shiftstate(keyevent.state==SDL_PRESSED, keysym.sym, keysym.mod);
        if (!use_move_key(key) || (move_key(keyevent.state == SDL_PRESSED, key) < 0)) {
                put_modified_symbol(keyevent.state==SDL_PRESSED, modifiers, key);
        }
}

#ifdef X_SUPPORT
int init_SDL_keyb(void *handle, Display *display)
{
	if (!config.X_keycode)
		return 0;
	X_get_modifier_info = dlsym(handle, "X_get_modifier_info");
	Xkb_lookup_key = dlsym(handle, "Xkb_lookup_key");
	X_keycode_initialize = dlsym(handle, "X_keycode_initialize");
	keyb_X_init = dlsym(handle, "keyb_X_init");
	keynum_to_keycode = dlsym(handle, "keynum_to_keycode");
	Xkb_get_group = dlsym(handle, "Xkb_get_group");
	X_sync_shiftstate = dlsym(handle, "X_sync_shiftstate");
	X_keycode_initialize(display);
	keyb_X_init(display);
	return 0;
}
#endif

static int sdl_kbd_probe(void)
{
	return (config.sdl == 1);
}

struct keyboard_client Keyboard_SDL =  {
	"SDL",                  /* name */
	sdl_kbd_probe,          /* probe */
	NULL,                   /* init */
	NULL,                   /* reset */
	NULL,                   /* close */
	NULL                    /* set_leds */
};
