/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * Authors: Emmanuel Jeandel, Bart Oldeman
 * SDL2 and XKB support by Stas Sergeev
 */

#include <SDL.h>
#include <string.h>

#include "emu.h"
#include "keyboard/keyb_clients.h"
#include "keyboard/keyboard.h"
#include "keyboard/keynum.h"
#include "keyb_SDL.h"
#include "sdl2-keymap.h"

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

void SDL_process_key_text(SDL_KeyboardEvent keyevent,
		SDL_TextInputEvent textevent)
{
	const char *p = textevent.text;
	struct char_set_state state;
	int src_len;
	t_unicode key;
	SDL_Keysym keysym = keyevent.keysym;
	SDL_Scancode scan = keysym.scancode;
	t_keynum keynum = sdl2_scancode_to_keynum[scan];

	k_printf("SDL: text key pressed: %s\n", p);
	init_charset_state(&state, lookup_charset("utf8"));
	src_len = strlen(p);
	charset_to_unicode_string(&state, &key, &p, src_len);
	cleanup_charset_state(&state);

	assert(keyevent.state == SDL_PRESSED);
	SDL_sync_shiftstate(1, keysym.sym, keysym.mod);
	move_keynum(1, keynum, key);
}

void SDL_process_key_press(SDL_KeyboardEvent keyevent)
{
	SDL_Keysym keysym = keyevent.keysym;
	SDL_Scancode scan = keysym.scancode;
	t_keynum keynum = sdl2_scancode_to_keynum[scan];

	k_printf("SDL: non-text key pressed: %c\n", keysym.sym);
	assert(keyevent.state == SDL_PRESSED);
	SDL_sync_shiftstate(1, keysym.sym, keysym.mod);
	move_keynum(1, keynum, DKY_VOID);
}

void SDL_process_key_release(SDL_KeyboardEvent keyevent)
{
	SDL_Keysym keysym = keyevent.keysym;
	SDL_Scancode scan = keysym.scancode;
	t_keynum keynum = sdl2_scancode_to_keynum[scan];

	k_printf("SDL: key released: %c\n", keysym.sym);
	assert(keyevent.state == SDL_RELEASED);
	SDL_sync_shiftstate(0, keysym.sym, keysym.mod);
	move_keynum(0, keynum, DKY_VOID);
}

static int sdl_kbd_probe(void)
{
	return (config.sdl == 1);
}

static int sdl_kbd_init(void)
{
	return 1;
}

struct keyboard_client Keyboard_SDL =  {
	"SDL",                  /* name */
	sdl_kbd_probe,          /* probe */
	sdl_kbd_init,           /* init */
	NULL,                   /* reset */
	NULL,                   /* close */
	NULL                    /* set_leds */
};
