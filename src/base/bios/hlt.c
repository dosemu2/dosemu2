/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

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

#include "config.h"
#include "emu.h"
#include "hlt.h"
#include "int.h"
#include "bios.h"
#include "memory.h"
#include "xms.h"
#include "dpmi.h"

/*
 * maximum number of halt handlers.
 * you can increase this to anything below 256 since an 8-bit handle
 * is used for each device
 */
#define MAX_HLT_HANDLERS 50

struct hlt_handler {
  emu_hlt_func  func;
  const char   *name;
  Bit16u	start_addr;
  void         *arg;
};
static struct hlt_handler hlt_handler[MAX_HLT_HANDLERS];

static Bit8u         hlt_handler_id[BIOS_HLT_BLK_SIZE];
static Bit32u        hlt_handler_count;

/*
 * This is the default HLT handler for the HLT block -- assume that
 * someone did a CALLF to get to us.
 */
static void hlt_default(Bit32u addr, void *arg)
{
  /* Assume someone callf'd to get here and do a return far */
  h_printf("HLT: hlt_default(0x%04x) called, attemping a retf\n", addr);

  fake_retf(0);
}

/*
 * DANG_BEGIN_FUNCTION hlt_init(void)
 *
 * description:
 *   Resets all the HLT handlers
 *
 * DANG_END_FUNCTION
 */
void hlt_init(void)
{
  int i;

  hlt_handler[0].func = hlt_default;
  hlt_handler[0].name = "Unmapped HLT instruction";

  hlt_handler_count   = 1;

  for (i=0; i < BIOS_HLT_BLK_SIZE; i++)
    hlt_handler_id[i] = 0;  /* unmapped HLT handler */
}

/*
 * DANG_BEGIN_FUNCTION hlt_handle()
 *
 * description:
 * Handles a HLT instruction reached inside the dos emulator.
 *
 * DANG_END_FUNCTION
 */
void hlt_handle(void)
{
  Bit32u  lina = SEGOFF2LINEAR(_CS, _IP);
  int rmcb_client, rmcb_num;

#if defined(X86_EMULATOR) && defined(SKIP_EMU_VBIOS)
  if ((config.cpuemu>1) && (lina == CPUEMUI10_ADD)) {
    e_printf("EMU86: HLT at int10 end\n");
    _IP += 1;	/* simply skip, so that we go back to emu mode */
  }
  else
#endif

  if ((lina >= BIOS_HLT_BLK) && (lina < BIOS_HLT_BLK+BIOS_HLT_BLK_SIZE)) {
    Bit32u offs = lina - BIOS_HLT_BLK;
    struct hlt_handler *hlt = &hlt_handler[hlt_handler_id[offs]];
#if CONFIG_HLT_TRACE > 0
    h_printf("HLT: fcn 0x%04lx called in HLT block, handler: %s\n", offs,
	     hlt->name);
#endif
    hlt->func(offs - hlt->start_addr, hlt->arg);
  }
  else if (lina == Mouse_HLT_ADD) {
    int33_post();
  }
  else if (lina == XMSControl_ADD) {
    xms_control();
  }
  else if (lina == INT42HOOK_ADD) {
    int42_hook();
  }
  else if (lina == POSTHOOK_ADD) {
    post_hook();
  }
  else if (lina == DPMI_ADD + HLT_OFF(DPMI_dpmi_init)) {
    /* The hlt instruction is 6 bytes in from DPMI_ADD */
    _IP += 1;	/* skip halt to point to FAR RET */
    CARRY;
    dpmi_init();
  }
  else if ((lina >= DPMI_ADD) &&
	   (lina < DPMI_ADD + (Bit32u)(uintptr_t)DPMI_dummy_end-
			     (Bit32u)(uintptr_t)DPMI_dummy_start)) {
#if CONFIG_HLT_TRACE > 0
    h_printf("HLT: dpmi_realmode_hlt\n");
#endif
    dpmi_realmode_hlt(lina);
  }
  else if ((rmcb_client = lookup_realmode_callback(lina, &rmcb_num)) != -1) {
    dpmi_realmode_callback(rmcb_client, rmcb_num);
  }
  else {
#if 0
    haltcount++;
    if (haltcount > MAX_HALT_COUNT)
      fatalerr = 0xf4;
#endif
    h_printf("HLT: unknown halt request CS:IP=%04x:%04x!\n", _CS, _IP);
#if 0
    show_regs(__FILE__, __LINE__);
#endif
    _IP += 1;
  }
}

/*
 * Register a HLT handler.
 */
Bit32u hlt_register_handler(emu_hlt_t handler)
{
  int handle, i, j;
  Bit32u start_addr = handler.start_addr;

  /* initialization check */
  assert(hlt_handler_count);

  if (hlt_handler_count >= MAX_HLT_HANDLERS) {
    error("HLT: too many HLT handlers, increase MAX_HLT_HANDLERS\n");
    config.exitearly = 1;
  }

  if (start_addr == (Bit32u)-1) {
    for (i = 0; i + handler.len <= BIOS_HLT_BLK_SIZE; i++) {
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
    if (start_addr == (Bit32u)-1) {
      error("HLT: Cannot find free block of len %i\n", handler.len);
      config.exitearly = 1;
    }
  }

  handle = hlt_handler_count++;

  hlt_handler[handle].name = handler.name;
  hlt_handler[handle].func = handler.func;
  hlt_handler[handle].start_addr = start_addr;
  hlt_handler[handle].arg = handler.arg;

  /* change table to reflect new handler id for that address */
  for (j = 0; j < handler.len; j++) {
    i = j + start_addr;
    if (i > BIOS_HLT_BLK_SIZE) {
      error("HLT: handler %s can not register values more than 0x%04x\n",
	    handler.name, BIOS_HLT_BLK_SIZE);
      config.exitearly = 1;
    }
    if (hlt_handler_id[i] != 0) {
      error("HLT: HLT handler conflict at offset 0x%04x between %s and %s\n",
	    i, handler.name, hlt_handler[hlt_handler_id[i]].name);
      config.exitearly = 1;
    }
    hlt_handler_id[i] = handle;
  }

  h_printf("HLT: registered %s at %#x,%i (%s)\n",
      handler.name, start_addr, handler.len,
      handler.start_addr == (Bit32u)-1 ? "auto" : "manual");

  return start_addr;
}
