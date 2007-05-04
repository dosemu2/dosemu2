/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#include "config.h"
#include <string.h>
#include <stdlib.h>
#include <SDL.h>
#include "emu.h"
#include "video.h"
#include "vgaemu.h"
#include "mouse.h"
#include "sdl.h"

static SDL_Cursor* mouse_TEXT_cursor = NULL;
static SDL_Cursor* mouse_GFX_cursor = NULL;

void SDL_set_mouse_move(int x, int y, int w_x_res, int w_y_res)
{
  /* In text modes, as our cursor's shape is a box, 
   * we transmit the  coordinate of his center
   * rather than the coordinate of the upper left corner */
  if(vga.mode_class == TEXT) {
    mouse_move_absolute(x + (vga.char_width / 2) , y + (vga.char_height / 2), w_x_res, w_y_res);
  } else {
    mouse_move_absolute(x, y, w_x_res, w_y_res);
  }
}

void SDL_set_mouse_text_cursor(void)
{
  Uint8* data, *mask;
  int i,j;

  if (mouse_TEXT_cursor != NULL) {
    SDL_FreeCursor(mouse_TEXT_cursor);  
  }
  i = vga.char_width / 8;
  data = malloc(sizeof(*data) * i * (vga.char_height) );
  mask = malloc(sizeof(*data) * i * (vga.char_height) );
  j = vga.char_height / 3;
  memset(data, 0, i * vga.char_height);      
  memset(mask , 0, i* j);
  memset(mask + i * j, 255, i* (vga.char_height - 2 * j));
  memset(mask + i * (vga.char_height - j), 0, i*j  );

  mouse_TEXT_cursor = SDL_CreateCursor(data, mask, 8, vga.char_height, 0, 0);
  free(data); free(mask);
  SDL_SetCursor(mouse_TEXT_cursor);
}

static void SDL_show_mouse_cursor(int yes)
{
  if(vga.mode_class != GRAPH) {
    return;
  }
  SDL_SetCursor(mouse_GFX_cursor);
  SDL_ShowCursor((yes && !grab_active) ? SDL_ENABLE : SDL_DISABLE);
}

static void SDL_set_mouse_cursor(int action, int mx, int my, int x_range, int y_range)
{
  if (action & 2)
    SDL_show_mouse_cursor(action >> 1);
}

static int SDL_mouse_init(void)
{
  mouse_t *mice = &config.mouse;
  if (Video != &Video_SDL || !mice->intdrv)
    return FALSE;

  mouse_GFX_cursor = SDL_GetCursor();
  mice->type = MOUSE_SDL;
  mice->use_absolute = 1;
  mice->native_cursor = config.X_fullscreen;
  /* we have the X cursor, but if we start fullscreen, grab by default */
  m_printf("MOUSE: SDL Mouse being set\n");
  return TRUE;
}

struct mouse_client Mouse_SDL =  {
  "SDL",          /* name */
  SDL_mouse_init, /* init */
  NULL,         /* close */
  NULL,         /* run */
  SDL_set_mouse_cursor /* set_cursor */
};
