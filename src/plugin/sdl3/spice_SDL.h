/*
 *  Copyright (C) 2026 the dosemu2 project
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef SPICE_SDL_H
#define SPICE_SDL_H

#include <SDL3/SDL.h>

#ifdef HAVE_SPICE

/* Bring the remote display up, if $_spice says so.  Returns 1 if it is
 * running, 0 if it is switched off, -1 on failure. */
int spice_sdl_init(void);
void spice_sdl_done(void);

/* Whether anything below will do anything.  Cheap. */
int spice_sdl_on(void);

/* Follow whichever texture the plugin is currently rendering into.  Cheap
 * and idempotent when nothing changed. */
void spice_sdl_set_size(int width, int height, SDL_PixelFormat format);

/* One rendered rectangle, straight out of do_rend_rects(). */
void spice_sdl_blit(int x, int y, SDL_Surface *s);

/* End of frame; nothing reaches clients before this. */
void spice_sdl_flush(void);

/* Wipe the remote display, for the redraws that clear a texture. */
void spice_sdl_clear(void);

/* Push the guest's lock state so remote keyboards' LEDs agree. */
void spice_sdl_update_leds(void);

#else

static inline int spice_sdl_init(void) { return 0; }
static inline void spice_sdl_done(void) { }
static inline int spice_sdl_on(void) { return 0; }
static inline void spice_sdl_set_size(int w, int h, SDL_PixelFormat f) { }
static inline void spice_sdl_blit(int x, int y, SDL_Surface *s) { }
static inline void spice_sdl_flush(void) { }
static inline void spice_sdl_clear(void) { }
static inline void spice_sdl_update_leds(void) { }

#endif /* HAVE_SPICE */

#endif /* SPICE_SDL_H */
