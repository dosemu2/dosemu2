/*
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

/*
 * Purpose: SDL clipboard support
 *
 * Author: @andrewbird @stsp
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#endif
#include <SDL_clipboard.h>
#include <SDL_error.h>
#include <SDL_video.h>
#include "dosemu_debug.h"
#include "types.h"
#include "init.h"
#include "clipboard.h"
#include "clip_SDL.h"

static int cl_type;
#define _clipboard_grabbed_data clip_str

static int clipboard_clear(void)
{
  cnn_clear();
  // SDL3 TODO: SDL_ClearClipboardData()
  if (SDL_SetClipboardText("")) {
    v_printf("SDL_clipboard: Clear failed, error '%s'\n", SDL_GetError());
    return FALSE;
  }
  return TRUE;
}

static int clipboard_write(int type, const char *p, int size)
{
  int ret;

  if (!cnn_write(type, p, size))
    return FALSE;
  ret = SDL_SetClipboardText(clip_str);
  return ret == 0 ? TRUE : FALSE;
}

static int clipboard_getsize(int type)
{
  cnn_clear();

  if (SDL_HasClipboardText() == FALSE)
    return 0;

  _clipboard_grabbed_data = SDL_GetClipboardText();
  if (!_clipboard_grabbed_data) {
    v_printf("SDL_clipboard: GetSize failed (grabbed data is NULL\n");
    return 0;
  }

  return cnn_getsize(type);
}

static int clipboard_getdata(int type, char *p, int size)
{
  return cnn_getdata(type, p, size);
}

static struct clipboard_system cnative_SDL =
{
  NULL,
  NULL,
  clipboard_clear,
  clipboard_write,
  clipboard_getsize,
  clipboard_getdata,
  "sdl native clipboard",
};

static struct clipboard_system cnonnative_SDL =
{
  NULL,
  NULL,
  cnn_clear,
  cnn_write,
  cnn_getsize,
  cnn_getdata,
  "sdl nonnative clipboard",
};

static struct clipboard_system *cl_ops[] = {
  &cnonnative_SDL,
  &cnative_SDL,
};

static int cwrp_clear(void)
{
  return cl_ops[cl_type]->clear();
}

static int cwrp_write(int type, const char *p, int size)
{
  return cl_ops[cl_type]->write(type, p, size);
}

static int cwrp_getsize(int type)
{
  return cl_ops[cl_type]->getsize(type);
}

static int cwrp_getdata(int type, char *p, int size)
{
  return cl_ops[cl_type]->getdata(type, p, size);
}

static struct clipboard_system cwrapped_SDL =
{
  cnn_open,
  cnn_close,
  cwrp_clear,
  cwrp_write,
  cwrp_getsize,
  cwrp_getdata,
  "sdl clipboard",
};

void sdlclip_setnative(int on)
{
  assert(on <= 1);
  cl_type = on;
}

int sdlclip_copy(SDL_Window *window)
{
  int ret;
  if (!clip_str)
    return FALSE;
  // SDL3 TODO: SDL_ClearWindowNotification()
  ret = SDL_SetClipboardText(clip_str);
  return ret == 0 ? TRUE : FALSE;
}

int sdlclip_paste(SDL_Window *window)
{
  cnn_clear();

  if (SDL_HasClipboardText() == FALSE)
    return FALSE;

  _clipboard_grabbed_data = SDL_GetClipboardText();
  if (!_clipboard_grabbed_data) {
    v_printf("SDL_clipboard: SDL_GetClipboardText() failed\n");
    return FALSE;
  }
  return TRUE;
}

int sdlclip_clear(SDL_Window *window)
{
  // SDL3 TODO: SDL_ClearWindowNotification()
  cnn_clear();
  return TRUE;
}

CONSTRUCTOR(void sdlclip_initialize(void))
{
  register_clipboard_system(&cwrapped_SDL);
}
