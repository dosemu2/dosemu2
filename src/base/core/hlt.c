/*
 * DANG_BEGIN_MODULE
 *
 * Description: HLT handling code for DOSEMU
 *
 * Maintainers:
 *
 * In the BIOS there is a 2K 'hlt block'.  When a HLT in this block is
 * reached, the corresponding handler function is called.  If a HLT is
 * called outside this block, an error is flagged.
 *
 * DANG_END_MODULE
 *
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
static struct hlt_handler hlt_handler[MAX_HLT_HANDLERS];

#define MAX_HLT_BLK_SIZE 4096
static int        hlt_handler_id[MAX_HLT_BLK_SIZE];
static int        hlt_handler_count;
static int        hlt_block_size;

/*
 * This is the default HLT handler for the HLT block -- assume that
 * someone did a CALLF to get to us.
 */
static void hlt_default(Bit16u addr, void *arg)
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
void hlt_init(int size)
{
  int i;

  hlt_handler[0].h.func = hlt_default;
  hlt_handler[0].h.name = "Unmapped HLT instruction";

  hlt_handler_count   = 1;
  assert(size <= MAX_HLT_BLK_SIZE);
  for (i=0; i < size; i++)
    hlt_handler_id[i] = 0;  /* unmapped HLT handler */
  hlt_block_size = size;
}

/*
 * DANG_BEGIN_FUNCTION hlt_handle()
 *
 * description:
 * Handles a HLT instruction reached inside the dos emulator.
 *
 * DANG_END_FUNCTION
 */
int hlt_handle(Bit16u offs)
{
  struct hlt_handler *hlt = &hlt_handler[hlt_handler_id[offs]];
#if CONFIG_HLT_TRACE > 0
  h_printf("HLT: fcn 0x%04x called in HLT block, handler: %s +%#x\n", offs,
	     hlt->h.name, offs - hlt->start_addr);
#endif
  hlt->h.func(offs - hlt->start_addr, hlt->h.arg);
  return hlt->h.ret;
}

/*
 * Register a HLT handler.
 */
Bit16u hlt_register_handler(emu_hlt_t handler)
{
  int handle, i, j;
  Bit16u start_addr = -1;

  /* initialization check */
  assert(hlt_handler_count);

  if (hlt_handler_count >= MAX_HLT_HANDLERS) {
    error("HLT: too many HLT handlers, increase MAX_HLT_HANDLERS\n");
    config.exitearly = 1;
    return -1;
  }

  for (i = 0; i + handler.len <= hlt_block_size; i++) {
      for (j = 0; j < handler.len; j++) {
        if (hlt_handler_id[i + j]) {
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

  handle = hlt_handler_count++;

  hlt_handler[handle].h = handler;
  hlt_handler[handle].start_addr = start_addr;

  /* change table to reflect new handler id for that address */
  for (j = 0; j < handler.len; j++)
    hlt_handler_id[start_addr + j] = handle;

  h_printf("HLT: registered %s at %#x,%i\n",
      handler.name, start_addr, handler.len);

  return start_addr;
}

int hlt_unregister_handler(Bit16u start_addr)
{
  int handle, i;
  emu_hlt_t *h;

  assert(start_addr < hlt_block_size);
  handle = hlt_handler_id[start_addr];
  if (!handle)
    return -1;
  h = &hlt_handler[handle].h;
  for (i = 0; i < h->len; i++)
    hlt_handler_id[start_addr + i] = 0;
  h->func = hlt_default;
  while (hlt_handler_count &&
	hlt_handler[hlt_handler_count - 1].h.func == hlt_default)
    hlt_handler_count--;
  return 0;
}
