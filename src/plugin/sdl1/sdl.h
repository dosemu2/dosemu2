/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

void SDL_process_key(SDL_KeyboardEvent keyevent);
void SDL_set_mouse_text_cursor(void);
void SDL_mouse_move(int x, int y, int w_x_res, int w_y_res);
extern struct keyboard_client Keyboard_SDL;
extern struct mouse_client Mouse_SDL;
extern int grab_active;
extern int no_mouse;
extern struct video_system Video_SDL;
