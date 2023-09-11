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
 * Purpose: terminal clipboard support (OSC-52)
 *
 * Note: only copy support for now (no paste)
 *
 * Author: @stsp
 */

#include <stdio.h>
#include <alloca.h>
#include <b64/cencode.h>
#include "init.h"
#include "types.h"
#include "emu.h"
#include "clipboard.h"

static int cterm_clear(void)
{
  fprintf(stdout, "\033]52;c;-\a");
  fflush(stdout);
  return TRUE;
}

static int base64_encoded_size(int size)
{
  return (size + 2) / 3 * 4 + 1;
}

static int cterm_write(int type, const char *p, int size)
{
  int cnt;
  base64_encodestate state;
  char *buf, *str;

  str = clipboard_make_str_utf8(type, p, size);
  if (!str)
    return FALSE;
  size = strlen(str) + 1;
  buf = alloca(base64_encoded_size(size));
  fprintf(stdout, "\033]52;c;");
  base64_init_encodestate(&state);
  cnt = base64_encode_block(str, size, buf, &state);
  free(str);
  if (cnt > 0)
    fwrite(buf, 1, cnt, stdout);
  cnt = base64_encode_blockend(buf, &state);
  if (cnt > 0)
    fwrite(buf, 1, cnt, stdout);
  fprintf(stdout, "\a");
  fflush(stdout);
  return TRUE;
}

static int cterm_getsize(int type)
{
  return 0;
}

static int cterm_getdata(int type, char *p, int size)
{
  return FALSE;
}

static struct clipboard_system clip_term =
{
  cterm_clear,
  cterm_write,
  cterm_getsize,
  cterm_getdata,
  "terminal clipboard",
};

CONSTRUCTOR(static void clipterm_initialize(void))
{
  if (config.clip_term)
    register_clipboard_system(&clip_term);
}
