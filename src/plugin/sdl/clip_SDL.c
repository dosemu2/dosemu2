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
#include <assert.h>
#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#endif
#include <SDL_clipboard.h>
#include <SDL_error.h>
#include <SDL_video.h>
#include "dosemu_debug.h"
#include "types.h"
#include "translate/translate.h"
#include "clipboard.h"
#include "clip_SDL.h"

static int cl_type;
#define _clipboard_grabbed_data clip_str
static char *clip_str;

static void do_clear(void)
{
  free(clip_str);
  clip_str = NULL;
}

static int clipboard_clear(void)
{
  do_clear();
  // SDL3 TODO: SDL_ClearClipboardData()
  if (SDL_SetClipboardText("")) {
    v_printf("SDL_clipboard: Clear failed, error '%s'\n", SDL_GetError());
    return FALSE;
  }
  return TRUE;
}

static char *clipboard_make_str(int type, const char *p, int size)
{
  char *q;

  if (type == CF_TEXT) {
    q = strndup(p, size);
  } else { // CF_OEMTEXT
    struct char_set_state state;
    int characters;
    t_unicode *str;

    init_charset_state(&state, trconfig.dos_charset);

    characters = character_count(&state, p, size);
    if (characters == -1) {
      v_printf("SDL_clipboard: Write invalid char count\n");
      return NULL;
    }

    str = malloc(sizeof(t_unicode) * (characters + 1));
    charset_to_unicode_string(&state, str, &p, size, characters + 1);
    cleanup_charset_state(&state);
    q = unicode_string_to_charset((wchar_t *)str, "utf8");
    free(str);
  }
  return q;
}

static void add_clip_str(char *q)
{
  if (clip_str) {
    clip_str = realloc(clip_str, strlen(clip_str) + strlen(q) + 1);
    strcat(clip_str, q);
    free(q);
  } else {
    clip_str = q;
  }
}

static int clipboard_write(int type, const char *p, int size)
{
  int ret;
  char *q;

  /* Note:
   *   1/ size includes the terminating nul
   *   2/ SDL_SetClipboardText converts CR/LF -> LF/LF/LF
   */
  if (type != CF_TEXT && type != CF_OEMTEXT) {
    v_printf("SDL_clipboard: Write failed, type (0x%02x) unsupported\n", type);
    return FALSE;
  }
  q = clipboard_make_str(type, p, size);
  if (!q)
    return FALSE;
  add_clip_str(q);
  ret = SDL_SetClipboardText(clip_str);
  return ret == 0 ? TRUE : FALSE;
}

static int clipboard_getsize(int type)
{
  char *q;
  int ret;

  if (type == CF_TEXT)
    v_printf("SDL_clipboard: GetSize of type CF_TEXT\n");
  else if (type == CF_OEMTEXT)
    v_printf("SDL_clipboard: GetSize of type CF_OEMTEXT\n");
  else {
    v_printf("SDL_clipboard: GetSize failed (type 0x%02x unsupported\n", type);
    return 0;
  }

  free(_clipboard_grabbed_data);
  _clipboard_grabbed_data = NULL;

  if (SDL_HasClipboardText() == FALSE)
    return 0;

  _clipboard_grabbed_data = SDL_GetClipboardText();
  if (!_clipboard_grabbed_data) {
    v_printf("SDL_clipboard: GetSize failed (grabbed data is NULL\n");
    return 0;
  }

  q = clipboard_make_str(type, _clipboard_grabbed_data,
      strlen(_clipboard_grabbed_data));
  if (!q)
    return 0;
  ret = strlen(_clipboard_grabbed_data) + 1;
  free(q);
  return ret;
}

static int clipboard_getdata(int type, char *p, int size)
{
  char *q;

  if (type == CF_TEXT)
    v_printf("SDL_clipboard: GetData of type CF_TEXT\n");
  else if (type == CF_OEMTEXT)
    v_printf("SDL_clipboard: GetData of type CF_OEMTEXT\n");
  else {
    v_printf("SDL_clipboard: GetData failed (type 0x%02x unsupported\n", type);
    return FALSE;
  }

  if (!_clipboard_grabbed_data) {
    v_printf("SDL_clipboard: GetData failed (grabbed_data is NULL)\n");
    return FALSE;
  }
  q = clipboard_make_str(type, _clipboard_grabbed_data,
      strlen(_clipboard_grabbed_data));
  if (!q)
    return FALSE;

  /* Since the GetSize function must have been called to set the grabbed
   * data we can be fairly certain that the caller has allocated a correctly
   * sized buffer to receive this */
  strlcpy(p, q, size);
  free(q);
  return TRUE;
}

static struct clipboard_system cnative_SDL =
{
  clipboard_clear,
  clipboard_write,
  clipboard_getsize,
  clipboard_getdata,
  "sdl native clipboard",
};


static int cnn_clear(void)
{
  // SDL3 TODO: SDL_ClearWindowNotification()
  do_clear();
  return TRUE;
}

static int cnn_write(int type, const char *p, int size)
{
  char *q;

  if (type != CF_TEXT && type != CF_OEMTEXT) {
    v_printf("SDL_clipboard: Write failed, type (0x%02x) unsupported\n", type);
    return FALSE;
  }
  q = clipboard_make_str(type, p, size);
  if (!q)
    return FALSE;
  // SDL3 TODO: SDL_SetWindowNotification()
  add_clip_str(q);
  return TRUE;
}

static int cnn_getsize(int type)
{
  char *q;
  int ret;

  if (type == CF_TEXT)
    v_printf("SDL_clipboard: GetSize of type CF_TEXT\n");
  else if (type == CF_OEMTEXT)
    v_printf("SDL_clipboard: GetSize of type CF_OEMTEXT\n");
  else {
    v_printf("SDL_clipboard: GetSize failed (type 0x%02x unsupported\n", type);
    return 0;
  }

  if (!_clipboard_grabbed_data) {
    v_printf("SDL_clipboard: GetSize failed (grabbed data is NULL\n");
    return 0;
  }
  q = clipboard_make_str(type, _clipboard_grabbed_data,
      strlen(_clipboard_grabbed_data));
  if (!q)
    return 0;
  ret = strlen(_clipboard_grabbed_data) + 1;
  free(q);
  return ret;
}

static int cnn_getdata(int type, char *p, int size)
{
  char *q;

  if (!_clipboard_grabbed_data)
    return FALSE;
  q = clipboard_make_str(type, _clipboard_grabbed_data,
      strlen(_clipboard_grabbed_data));
  if (!q)
    return FALSE;
  strlcpy(p, q, size);
  free(q);
  return TRUE;
}

static struct clipboard_system cnonnative_SDL =
{
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
  free(_clipboard_grabbed_data);
  _clipboard_grabbed_data = NULL;

  if (SDL_HasClipboardText() == FALSE)
    return FALSE;

  _clipboard_grabbed_data = SDL_GetClipboardText();
  if (!_clipboard_grabbed_data) {
    v_printf("SDL_clipboard: GetSize failed (grabbed data is NULL\n");
    return FALSE;
  }
  return TRUE;
}

int sdlclip_clear(SDL_Window *window)
{
  // SDL3 TODO: SDL_ClearWindowNotification()
  do_clear();
  return TRUE;
}

CONSTRUCTOR(void sdlclip_initialize(void))
{
  register_clipboard_system(&cwrapped_SDL);
}
