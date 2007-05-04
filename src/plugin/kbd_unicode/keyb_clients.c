/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* This file contains code on the keyboard client side which is common to all frontends.
 * In particular, the client initialisation and the paste routine.
 */
#include <string.h>
#include <stdlib.h>
#include "emu.h"
#include "termio.h"
#include "keyboard.h"
#include "keyb_clients.h"
#include "keymaps.h"
#include "video.h"
#include "translate.h"

static t_unicode *paste_buffer = NULL;
static int paste_len = 0, paste_idx = 0;

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
 * 'text' is expected to be in latin1 charset, with unix ('\n')
 * line end convention.
 *
 * 'text' is actually expected to be in trconfig.paste_charset
 * which defaults to iso8859-1 with unix ('\n') line end convetion.
 */
int paste_text(const char *text, int len) 
{
	struct char_set *paste_charset = trconfig.paste_charset;
	struct char_set_state state;
	t_unicode *str;
	int characters;
	int result;

	init_charset_state(&state, paste_charset);
	characters = character_count(&state, text, len);
	str = malloc(sizeof(t_unicode) * (characters +1));

	charset_to_unicode_string(&state, str, &text, len);
	cleanup_charset_state(&state);
	
	result = paste_unicode_text(str, characters);
	free(str);
	
	return result;
}

static void paste_run(void) 
{
	int count=0;
   
	k_printf("KBD: paste_run running\n");
	if (paste_buffer) {    /* paste in progress */
		while (1) {
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
				break;
			}
		}
		k_printf("KBD: paste_run() pasted %d chars\n",count);
	}
}

/* register keyboard at the back of the linked list */
void register_keyboard_client(struct keyboard_client *keyboard)
{
	struct keyboard_client *k;

	keyboard->next = NULL;
	if (Keyboard == NULL)
		Keyboard = keyboard;
	else {
		for (k = Keyboard; k->next; k = k->next);
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
 * If probe or init fails it trys another client.
 *
 * Eventually falling back to Keyboard_none a dummy client, which does nothing.
 *
 * DANG_END_FUNCTION
 */
int keyb_client_init(void)
{
	int ok;

	if(Keyboard == NULL)
		register_keyboard_client(&Keyboard_raw);
	register_keyboard_client(&Keyboard_none);
	while(Keyboard) {
		k_printf("KBD: probing '%s' mode keyboard client\n",
			Keyboard->name);
		ok = Keyboard->probe && Keyboard->probe();

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
		Keyboard = Keyboard->next;
	}
	
	/* Rationalize the keyboard config.
	 * This should probably be done elsewhere . . .
	 */
	config.console_keyb = (Keyboard == &Keyboard_raw);

	/* We always have a least Keyboard_none to fall back too */
	if(Keyboard == NULL)
		Keyboard = &Keyboard_none;
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

void keyb_client_run(void) 
{
	/* if a paste operation is currently running, give it priority over the keyboard
	 * frontend, in case the user continues typing before pasting is finished.
	 */
	if (paste_buffer!=NULL) {
		paste_run();
	}
	else if ((Keyboard!=NULL) && (Keyboard->run!=NULL)) {
		Keyboard->run();
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
