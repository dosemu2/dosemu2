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
 * Purpose: manage read-only LDT alias
 *
 * Author: Stas Sergeev
 */

#include <string.h>
#include "cpu.h"
#include "memory.h"
#include "dpmi.h"
#include "instr_dec.h"
#include "dosemu_debug.h"
#include "segreg_priv.h"
#include "msdos_ldt.h"

#define LDT_UPDATE_LIM 1

static unsigned char *ldt_backbuf;
static unsigned char *ldt_alias;
static unsigned short dpmi_ldt_alias;
static int entry_upd;

/* Note: krnl286.exe requires at least two extra pages in LDT (limit).
 * To calculate the amount of available ldt entries it does 'lsl' and
 * one descriptor allocation, then it does the subtraction.
 * Lets give it 4 extra pages to stay safe...
 * We can't give it all available entries because krnl386.exe
 * allocates all that are available via direct ldt writes, and then
 * the subsequent DPMI allocations fail. */
#define XTRA_LDT_LIM (DPMI_page_size * 4)

u_short msdos_ldt_setup(unsigned char *backbuf, unsigned char *alias)
{
    unsigned lim;

    /* NULL can be passed as backbuf if you have R/W LDT alias */
    ldt_backbuf = backbuf;
    ldt_alias = alias;
    entry_upd = -1;

    dpmi_ldt_alias = AllocateDescriptors(1);
    assert(dpmi_ldt_alias);
    lim = ((dpmi_ldt_alias >> 3) + 1) * LDT_ENTRY_SIZE;
    SetSegmentLimit(dpmi_ldt_alias, PAGE_ALIGN(lim) + XTRA_LDT_LIM - 1);
    SetSegmentBaseAddress(dpmi_ldt_alias, DOSADDR_REL(ldt_alias));
    return dpmi_ldt_alias;
}

void msdos_ldt_done(void)
{
    unsigned short alias;

    if (!dpmi_ldt_alias)
	return;
    alias = dpmi_ldt_alias;
    /* setting to zero before clearing or it will re-instantiate */
    dpmi_ldt_alias = 0;
    FreeDescriptor(alias);
    ldt_backbuf = NULL;
}

int msdos_ldt_fault(sigcontext_t *scp, uint16_t sel)
{
    unsigned limit;
#if LDT_UPDATE_LIM
    /* basically on dosemu this code is unused because msdos_ldt_update()
     * manages the limit too. But it may be good to keep for the cases
     * where msdos_ldt_update() is not used. For example if the LDT is R/W,
     * it may be much simpler to not use msdos_ldt_update().
     * When LDT is not R/W, we need to use msdos_ldt_update() anyway for
     * backbuffer, so why not also for managing the limit.
     * Note: in case of an R/O LDT we still need the backbuffer (similar
     * to non-readable LDT) because windows-3.1 writes crap to not-present
     * segment descriptors, which would be impossible to read back from
     * real R/O LDT. */
#endif
    if (sel != dpmi_ldt_alias)
	return MFR_NOT_HANDLED;
#if LDT_UPDATE_LIM
    if (ldt_backbuf)
	error("LDT fault with backbuffer present\n");
#endif
    limit = GetSegmentLimit(dpmi_ldt_alias);
    D_printf("DPMI: expanding LDT, old_lim=0x%x\n", limit);
    SetSegmentLimit(dpmi_ldt_alias, limit + DPMI_page_size);
    return MFR_HANDLED;
}

void msdos_ldt_update(int entry, u_char *buf, int len)
{
#if LDT_UPDATE_LIM
  if (dpmi_ldt_alias) {
    unsigned limit = GetSegmentLimit(dpmi_ldt_alias);
    unsigned new_len = entry * LDT_ENTRY_SIZE + len;
    if (limit < new_len - 1) {
      D_printf("DPMI: expanding LDT, old_lim=0x%x\n", limit);
      SetSegmentLimit(dpmi_ldt_alias, PAGE_ALIGN(new_len) - 1);
    }
  }
#endif
  if (ldt_backbuf && entry != entry_upd)
    memcpy(&ldt_backbuf[entry * LDT_ENTRY_SIZE], buf, len);
}

static void direct_ldt_write(sigcontext_t *scp, int offset,
    char *buffer, int length)
{
  int ldt_entry = offset / LDT_ENTRY_SIZE;
  int ldt_offs = offset % LDT_ENTRY_SIZE;
  int selector = (ldt_entry << 3) | 7;
  u_char lp[LDT_ENTRY_SIZE];
  int i, err;

  if (!ldt_backbuf) {
    static int warned;
    if (!warned) {
      warned = 1;
      error("LDT pagefault with no backbuffer provided\n");
    }
    return;
  }
  D_printf("Direct LDT write, offs=%#x len=%i en=%#x off=%i\n",
    offset, length, ldt_entry, ldt_offs);
  for (i = 0; i < length; i++)
    D_printf("0x%02hhx ", buffer[i]);
  D_printf("\n");

  entry_upd = ldt_entry;	// dont update from DPMI callouts
  err = GetDescriptor(selector, (unsigned int *)lp);
  if (err) {
    err = DPMI_allocate_specific_ldt_descriptor(selector);
    if (!err)
      err = GetDescriptor(selector, (unsigned int *)lp);
  }
  if (err) {
    error("Descriptor allocation at %#x failed\n", ldt_entry);
    goto out;
  }
  if (!(lp[5] & 0x80)) {
    D_printf("LDT: NP\n");
    memcpy(lp, &ldt_backbuf[ldt_entry * LDT_ENTRY_SIZE], LDT_ENTRY_SIZE);
  }
  memcpy(lp + ldt_offs, buffer, length);
  D_printf("LDT: ");
  for (i = 0; i < LDT_ENTRY_SIZE; i++)
    D_printf("0x%02hhx ", lp[i]);
  D_printf("\n");
  if (lp[5] & 0x10) {
    SetDescriptor(selector, (unsigned int *)lp);
  } else {
    u_char lp1[LDT_ENTRY_SIZE];
    D_printf("DPMI: Invalid descriptor, freeing\n");
    memset(lp1, 0, sizeof(lp1));
    lp1[5] |= 0x70;
    SetDescriptor(selector, (unsigned int *)lp1);
    FreeSegRegs(scp, selector);
  }
  memcpy(&ldt_backbuf[ldt_entry * LDT_ENTRY_SIZE], lp, LDT_ENTRY_SIZE);
out:
  entry_upd = -1;
}

int msdos_ldt_pagefault(sigcontext_t *scp)
{
    uint32_t op;
    int len;

    if ((unsigned char *)_cr2 < ldt_alias ||
	  (unsigned char *)_cr2 >= ldt_alias + LDT_ENTRIES * LDT_ENTRY_SIZE)
	return 0;
    len = decode_memop(scp, &op);
    if (!len)
	return 0;

    direct_ldt_write(scp, _cr2 - (unsigned long)ldt_alias, (char *)&op, len);
    return 1;
}
