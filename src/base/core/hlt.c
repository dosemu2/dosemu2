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

#include <string.h>
#include <assert.h>

#include "emu.h"
#include "int.h"
#include "bios.h"
#include "memory.h"
#include "xms.h"
#include "dpmi.h"
#include "timers.h"
#include "hlt.h"

#define CONFIG_HLT_TRACE 1

/*
 * maximum number of halt handlers.
 * you can increase this to anything below 256 since an 8-bit handle
 * is used for each device
 */
#define MAX_HLT_HANDLERS 50

struct hlt_handler {
  emu_hlt_t	h;
  Bit16u	start_addr;
};

#define MAX_HLT_BLK_SIZE 4096
struct hlt_struct {
  struct hlt_handler hlt_handler[MAX_HLT_HANDLERS];
  int hlt_handler_id[MAX_HLT_BLK_SIZE];
  int hlt_handler_count;
  int hlt_block_size;
};

/*
 * This is the default HLT handler for the HLT block -- assume that
 * someone did a CALLF to get to us.
 */
static void hlt_default(Bit16u addr, HLT_ARG(arg))
{
  error("HLT: hlt_default(0x%04x) called, attemping a retf\n", addr);
  leavedos(2);
}

/*
 * DANG_BEGIN_FUNCTION hlt_init(void)
 *
 * description:
 *   Resets all the HLT handlers
 *
 * DANG_END_FUNCTION
 */
void *hlt_init(int size)
{
  struct hlt_struct *state;
  int i;

  state = malloc(sizeof(*state));
  state->hlt_handler[0].h.func = hlt_default;
  state->hlt_handler[0].h.name = "Unmapped HLT instruction";

  state->hlt_handler_count   = 1;
  assert(size <= MAX_HLT_BLK_SIZE);
  for (i = 0; i < size; i++)
    state->hlt_handler_id[i] = 0;  /* unmapped HLT handler */
  state->hlt_block_size = size;
  return state;
}

/*
 * DANG_BEGIN_FUNCTION hlt_handle()
 *
 * description:
 * Handles a HLT instruction reached inside the dos emulator.
 *
 * DANG_END_FUNCTION
 */
int hlt_handle(void *arg, Bit16u offs, void *arg2)
{
  struct hlt_struct *state = arg;
  struct hlt_handler *hlt = &state->hlt_handler[state->hlt_handler_id[offs]];
#if CONFIG_HLT_TRACE > 0
  h_printf("HLT: fcn 0x%04x called in HLT block, handler: %s +%#x\n", offs,
	     hlt->h.name, offs - hlt->start_addr);
#endif
  hlt->h.func(offs - hlt->start_addr, arg2, hlt->h.arg);
  return hlt->h.ret;
}

/*
 * Register a HLT handler.
 */
Bit16u hlt_register_handler(void *arg, emu_hlt_t handler)
{
  struct hlt_struct *state = arg;
  int handle, i, j;
  Bit16u start_addr = -1;

  /* initialization check */
  assert(state->hlt_handler_count);

  if (state->hlt_handler_count >= MAX_HLT_HANDLERS) {
    error("HLT: too many HLT handlers, increase MAX_HLT_HANDLERS\n");
    config.exitearly = 1;
    return -1;
  }

  for (i = 0; i + handler.len <= state->hlt_block_size; i++) {
      for (j = 0; j < handler.len; j++) {
        if (state->hlt_handler_id[i + j]) {
          i += j;
          break;
        }
      }
      /* see if found free block */
      if (j == handler.len) {
        start_addr = i;
        break;
      }
  }
  if (start_addr == (Bit16u)-1) {
      error("HLT: Cannot find free block of len %i\n", handler.len);
      config.exitearly = 1;
      return -1;
  }

  handle = state->hlt_handler_count++;

  state->hlt_handler[handle].h = handler;
  state->hlt_handler[handle].start_addr = start_addr;

  /* change table to reflect new handler id for that address */
  for (j = 0; j < handler.len; j++)
    state->hlt_handler_id[start_addr + j] = handle;

  h_printf("HLT: registered %s at %#x,%i\n",
      handler.name, start_addr, handler.len);

  return start_addr;
}

int hlt_unregister_handler(void *arg, Bit16u start_addr)
{
  struct hlt_struct *state = arg;
  int handle, i;
  emu_hlt_t *h;

  assert(start_addr < state->hlt_block_size);
  handle = state->hlt_handler_id[start_addr];
  if (!handle)
    return -1;
  h = &state->hlt_handler[handle].h;
  for (i = 0; i < h->len; i++)
    state->hlt_handler_id[start_addr + i] = 0;
  h->func = hlt_default;
  while (state->hlt_handler_count &&
	state->hlt_handler[state->hlt_handler_count - 1].h.func == hlt_default)
    state->hlt_handler_count--;
  return 0;
}
