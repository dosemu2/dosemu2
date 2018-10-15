/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#if defined(__cplusplus) || !defined(HAVE_XKB)
#define _HAVE_XKB 0
#else
#define _HAVE_XKB HAVE_XKB
#endif

void SDL_process_key(SDL_KeyboardEvent keyevent);
extern struct keyboard_client Keyboard_SDL;
#if _HAVE_XKB
void SDL_process_key_xkb(Display *display, SDL_KeyboardEvent keyevent);
#endif
