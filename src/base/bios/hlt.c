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

#include "config.h"
#include "emu.h"
#include "hlt.h"
#include "int.h"
#include "bios.h"
#include "memory.h"
#include "dpmi.h"

static struct {
  emu_hlt_func  func;
  const char   *name;
  Bit16u	start_addr;
} hlt_handler[MAX_HLT_HANDLERS];

static Bit8u         hlt_handler_id[BIOS_HLT_BLK_SIZE];       
static Bit32u        hlt_handler_count;

/*
 * This is the default HLT handler for the HLT block -- assume that
 * someone did a CALLF to get to us.
 */
static void hlt_default(Bit32u addr)
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
  Bit8u  *lina = SEG_ADR((Bit8u *), cs, ip);
  Bit32u  offs = (uintptr_t)lina;
  int rmcb_client, rmcb_num;

#if defined(X86_EMULATOR) && defined(SKIP_EMU_VBIOS)
  if ((config.cpuemu>1) && (lina == (Bit8u *) CPUEMUI10_ADD)) {
    e_printf("EMU86: HLT at int10 end\n");
    _IP += 1;	/* simply skip, so that we go back to emu mode */
  }
  else
#endif

  if ((offs >= BIOS_HLT_BLK) && (offs < BIOS_HLT_BLK+BIOS_HLT_BLK_SIZE)) {
    offs -= BIOS_HLT_BLK;
#if CONFIG_HLT_TRACE > 0
    h_printf("HLT: fcn 0x%04lx called in HLT block, handler: %s\n", offs,
	     hlt_handler[hlt_handler_id[offs]].name);
#endif
    hlt_handler[hlt_handler_id[offs]].func(offs - hlt_handler[hlt_handler_id[offs]].start_addr);
  }
  else if (lina == (Bit8u *) CBACK_ADD) {
    /* we are back from a callback routine */
    callback_return();
  }
  else if (lina == (Bit8u *) Mouse_HLT_ADD) {
    int33_post();
  }
  else if (lina == (Bit8u *) (DPMI_ADD + HLT_OFF(DPMI_dpmi_init))) {
    /* The hlt instruction is 6 bytes in from DPMI_ADD */
    _IP += 1;	/* skip halt to point to FAR RET */
    CARRY;
    dpmi_init();
  }
  else if ((lina >= (Bit8u *)DPMI_ADD) &&
	   (lina < (Bit8u *)(DPMI_ADD + (Bit32u)DPMI_dummy_end-(Bit32u)DPMI_dummy_start))) {
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
int hlt_register_handler(emu_hlt_t handler)
{
  int handle, i;

  /* first find existing handle for function or create new one */
  for (handle=0; handle < hlt_handler_count; handle++) {
    if (!strcmp(hlt_handler[handle].name, handler.name))
      break;
  }

  if (handle >= hlt_handler_count) {
    /* no existing handle found, create new one */
    if (hlt_handler_count >= MAX_HLT_HANDLERS) {
      error("HLT: too many HLT handlers, increase MAX_HLT_HANDLERS\n");
      leavedos(1);
    }

    hlt_handler_count++;

    hlt_handler[handle].name = handler.name;
    hlt_handler[handle].func = handler.func;
    hlt_handler[handle].start_addr = handler.start_addr;
    }

  /* change table to reflect new handler id for that address */
  for (i=handler.start_addr; i <= handler.end_addr; i++) {
    if (i > BIOS_HLT_BLK_SIZE) {
      error("HLT: handler %s can not register values more than 0x%04x\n",
	    handler.name, BIOS_HLT_BLK_SIZE);
      leavedos(1);
    }
    if (hlt_handler_id[i] != 0) {
      error("HLT: HLT handler conflict at offset 0x%04x between %s and %s\n",
	    i, handler.name, hlt_handler[hlt_handler_id[i]].name);
      leavedos(1);
    }
    hlt_handler_id[i] = handle;
  }
  return(1);
}
