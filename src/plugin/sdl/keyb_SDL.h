/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

void SDL_process_key_press(SDL_KeyboardEvent keyevent);
extern struct keyboard_client Keyboard_SDL;
void SDL_process_key_text(SDL_KeyboardEvent keyevent,
	SDL_TextInputEvent textevent);
void SDL_process_key_release(SDL_KeyboardEvent keyevent);
