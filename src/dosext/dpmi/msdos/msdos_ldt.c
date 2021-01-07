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
#include "msdos_priv.h"
#include "msdos_ldt.h"

#define LDT_UPDATE_LIM 1

static unsigned char *ldt_backbuf;
static unsigned char *ldt_alias;
static uint32_t ldt_h;
static uint32_t ldt_alias_h;
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

unsigned short msdos_ldt_init(void)
{
    unsigned lim;
    struct SHM_desc shm;
    unsigned short name_sel;
    unsigned short alias_sel;
    dosaddr_t name;
    uint16_t attrs[PAGE_ALIGN(LDT_ENTRIES*LDT_ENTRY_SIZE) / PAGE_SIZE];
    int err;
    int i;
    int npages = PAGE_ALIGN(LDT_ENTRIES*LDT_ENTRY_SIZE) / PAGE_SIZE;
    const int name_len = 128;

    name_sel = AllocateDescriptors(1);
    name = msdos_malloc(name_len);
    strcpy((char *)MEM_BASE32(name), "ldt_alias");
    SetSegmentBaseAddress(name_sel, name);
    SetSegmentLimit(name_sel, name_len - 1);
    shm.name_selector = name_sel;
    shm.name_offset32 = 0;
    shm.req_len = PAGE_ALIGN(LDT_ENTRIES*LDT_ENTRY_SIZE);
    shm.flags = SHM_NOEXEC;
    err = DPMIAllocateShared(&shm);
    assert(!err);
    ldt_h = shm.handle;
    ldt_backbuf = MEM_BASE32(shm.addr);
    err = DPMIAllocateShared(&shm);
    assert(!err);
    ldt_alias_h = shm.handle;
    if (ldt_h == ldt_alias_h)
	error("DPMI: problems allocating shm\n");
    ldt_alias = MEM_BASE32(shm.addr);
    msdos_free(name);
    FreeDescriptor(name_sel);
    for (i = 0; i < npages; i++)
	attrs[i] = 0x83;	// NX, RO
    DPMISetPageAttributes(shm.handle, 0, attrs, npages);

    entry_upd = -1;
    alias_sel = AllocateDescriptors(1);
    assert(alias_sel);
    lim = ((alias_sel >> 3) + 1) * LDT_ENTRY_SIZE;
    SetSegmentLimit(alias_sel, PAGE_ALIGN(lim) + XTRA_LDT_LIM - 1);
    SetSegmentBaseAddress(alias_sel, shm.addr);
    /* pre-fill back-buffer */
    for (i = 0x10; i <= (alias_sel >> 3); i++)
      GetDescriptor((i << 3) | 7, (unsigned int *)
          &ldt_backbuf[i * LDT_ENTRY_SIZE]);
    dpmi_ldt_alias = alias_sel;
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
    DPMIFreeShared(ldt_alias_h);
    DPMIFreeShared(ldt_h);
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

void msdos_ldt_update(int selector, int num)
{
#if LDT_UPDATE_LIM
  if (dpmi_ldt_alias) {
    unsigned limit = GetSegmentLimit(dpmi_ldt_alias);
    unsigned new_len = (selector & 0xfff8) + num * LDT_ENTRY_SIZE;
    if (limit < new_len - 1) {
      D_printf("DPMI: expanding LDT, old_lim=0x%x\n", limit);
      SetSegmentLimit(dpmi_ldt_alias, PAGE_ALIGN(new_len) - 1);
    }
  }
#endif
  if (ldt_backbuf && (selector >> 3) != entry_upd) {
    int i;
    for (i = 0; i < num; i++) {
      int err = GetDescriptor(selector + (i << 3),
          (unsigned *)&ldt_backbuf[(selector & 0xfff8) + (i << 3)]);
      if (err) {
        memset(&ldt_backbuf[(selector & 0xfff8) + (i << 3)], 0,
            LDT_ENTRY_SIZE);
        ldt_backbuf[(selector & 0xfff8) + (i << 3) + 5] = 0x70;
      }
    }
  }
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
    lp1[5] = 0x70;
    SetDescriptor(selector, (unsigned int *)lp1);
    FreeSegRegs(scp, selector);
  }
  memcpy(&ldt_backbuf[ldt_entry * LDT_ENTRY_SIZE], lp, LDT_ENTRY_SIZE);
out:
  entry_upd = -1;
}

int msdos_ldt_access(unsigned char *cr2)
{
    return cr2 >= ldt_alias && cr2 < ldt_alias + LDT_ENTRIES * LDT_ENTRY_SIZE;
}

void msdos_ldt_write(sigcontext_t *scp, uint32_t op, int len,
    unsigned char *cr2)
{
    direct_ldt_write(scp, cr2 - ldt_alias, (char *)&op, len);
}

int msdos_ldt_pagefault(sigcontext_t *scp)
{
    uint32_t op;
    int len;
    unsigned char *cr2 = MEM_BASE32(_cr2);

    if (!msdos_ldt_access(cr2))
	return 0;
    len = decode_memop(scp, &op, cr2);
    if (!len)
	return 0;

    msdos_ldt_write(scp, op, len, cr2);
    return 1;
}

int msdos_ldt_is32(unsigned short selector)
{
  return ((ldt_backbuf[(selector & 0xfff8) + 6] >> 6) & 1);
}
