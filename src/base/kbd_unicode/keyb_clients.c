/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/* This file contains code on the keyboard client side which is common to all frontends.
 * In particular, the client initialisation and the paste routine.
 */
#include <string.h>
#include <stdlib.h>
#include "emu.h"
#include "sig.h"
#include "keyboard.h"
#include "keyb_clients.h"
#include "keymaps.h"
#include "video.h"
#include "translate/translate.h"

static t_unicode *paste_buffer = NULL;
static int paste_len = 0, paste_idx = 0;
struct keyboard_client *Keyboard;
static struct keyboard_client *Kbd_root;

static int paste_unicode_text(const t_unicode *text, int len)
{
	int i;
	/* if previous paste in progress, ignore current request */
	/* XXX - maybe this should append ? */
	k_printf("KBD: paste_text called, len=%d\n",len);
	if (paste_buffer!=NULL) {
		k_printf("KBD: paste in progress, ignoring request\n");
		return 0;
	}
	paste_buffer = malloc(sizeof(t_unicode) * len);
	memcpy(paste_buffer, text, sizeof(t_unicode) * len);
	/* translate all 0xa's (\n) to ENTERs */
	for (i = 0; i < len; i++)
		if (paste_buffer[i] == U_LINE_FEED)
			paste_buffer[i] = U_CARRIAGE_RETURN;
	paste_len = len;
	paste_idx = 0;
	return 1;
}

/* paste a string of (almost) arbitrary length through the DOS keyboard,
 * without danger of overrunning the keyboard queue/buffer.
 * pasting in X causes either utf8, iso2022, or iso8859-1, all with
 * unix ('\n') line end convention.
 */
int paste_text(const char *text, int len, const char *charset)
{
	struct char_set *paste_charset = lookup_charset(charset);
	struct char_set_state state;
	t_unicode *str;
	int characters;
	int result;

	init_charset_state(&state, paste_charset);
	characters = character_count(&state, text, len);
	if (characters == -1) {
		k_printf("KBD: invalid paste\n");
		return 0;
	}
	str = malloc(sizeof(t_unicode) * (characters +1));

	charset_to_unicode_string(&state, str, &text, len, characters + 1);
	cleanup_charset_state(&state);

	result = paste_unicode_text(str, characters);
	free(str);

	return result;
}

static void paste_run(void)
{
	int count=0;

	if (paste_buffer) {    /* paste in progress */
		k_printf("KBD: paste_run running\n");
		t_unicode keysym;
		keysym = paste_buffer[paste_idx];

		put_symbol(PRESS, keysym);
		put_symbol(RELEASE, keysym);

		count++;
		if (++paste_idx == paste_len) {   /* paste finished */
			free(paste_buffer);
			paste_buffer=NULL;
			paste_len=paste_idx=0;
			k_printf("KBD: paste finished\n");
		}
		k_printf("KBD: paste_run() pasted %d chars\n",count);
	}
}

/* register keyboard at the back of the linked list */
void register_keyboard_client(struct keyboard_client *keyboard)
{
	struct keyboard_client *k;

	keyboard->next = NULL;
	if (Kbd_root == NULL)
		Kbd_root = keyboard;
	else {
		for (k = Kbd_root; k->next; k = k->next);
		k->next = keyboard;
	}
}

/* DANG_BEGIN_FUNCTION keyb_client_init
 *
 * Figures out which keyboard client to use and initialises it.
 *
 * First it calls the probe method to see if it should use the client,
 * Then it call init to set that client up.
 *
 * If probe or init fails it tries another client.
 *
 * Eventually falling back to Keyboard_none a dummy client, which does nothing.
 *
 * DANG_END_FUNCTION
 */
int keyb_client_init(void)
{
	int ok;

	register_keyboard_client(&Keyboard_raw);
	register_keyboard_client(&Keyboard_stdio);
	register_keyboard_client(&Keyboard_none);
	for (Keyboard = Kbd_root; Keyboard; Keyboard = Keyboard->next) {
		k_printf("KBD: probing '%s' mode keyboard client\n",
			Keyboard->name);
		ok = Keyboard->probe();

		if (ok) {
			k_printf("KBD: initialising '%s' mode keyboard client\n",
				Keyboard->name);

			ok = Keyboard->init?Keyboard->init():TRUE;
			if (ok) {
				k_printf("KBD: Keyboard init ok, '%s' mode\n",
					Keyboard->name);
				break;
			}
			else {
				k_printf("KBD: Keyboard init ***failed***, '%s' mode\n",
					Keyboard->name);
			}
		}
	}

	sigalrm_register_handler(paste_run);

	return TRUE;
}

void keyb_client_reset(void)
{
	if ((Keyboard!=NULL) && (Keyboard->reset!=NULL)) {
		Keyboard->reset();
	}
}

void keyb_client_close(void)
{
	if ((Keyboard!=NULL) && (Keyboard->close!=NULL)) {
		Keyboard->close();
	}
}

void keyb_client_set_leds(t_modifiers modifiers)
{
	/* FIXME static variable. . . */
	static t_modifiers prev_modifiers = -1;

	if (modifiers == prev_modifiers)
		return;
	prev_modifiers = modifiers;

	if (Keyboard->set_leds) {
		Keyboard->set_leds(modifiers);
	}
}
