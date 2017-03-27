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
#include "msdos_ldt.h"

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

int msdos_ldt_setup(unsigned char *backbuf, unsigned char *alias)
{
    /* NULL can be passed as backbuf if you have R/W LDT alias */
    ldt_backbuf = backbuf;
    ldt_alias = alias;
    entry_upd = -1;
    return 1;
}

u_short msdos_ldt_init(int clnt_num)
{
    unsigned lim;
    if (clnt_num > 1)		// one LDT alias for all clients
	return dpmi_ldt_alias;
    dpmi_ldt_alias = AllocateDescriptors(1);
    if (!dpmi_ldt_alias)
	return 0;
    lim = ((dpmi_ldt_alias >> 3) + 1) * LDT_ENTRY_SIZE;
    SetSegmentLimit(dpmi_ldt_alias, PAGE_ALIGN(lim) + XTRA_LDT_LIM - 1);
    SetSegmentBaseAddress(dpmi_ldt_alias, DOSADDR_REL(ldt_alias));
    return dpmi_ldt_alias;
}

void msdos_ldt_done(int clnt_num)
{
    unsigned short alias;
    if (clnt_num > 1)
	return;
    if (!dpmi_ldt_alias)
	return;
    alias = dpmi_ldt_alias;
    /* setting to zero before clearing or it will re-instantiate */
    dpmi_ldt_alias = 0;
    FreeDescriptor(alias);
}

void msdos_ldt_update(int entry, u_char *buf, int len)
{
  if (ldt_backbuf && entry != entry_upd)
    memcpy(&ldt_backbuf[entry * LDT_ENTRY_SIZE], buf, len);
}

static void direct_ldt_write(struct sigcontext *scp, int offset,
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

int msdos_ldt_pagefault(struct sigcontext *scp)
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
