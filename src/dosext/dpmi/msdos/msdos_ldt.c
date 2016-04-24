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
#include <assert.h>
#include "cpu.h"
#include "dpmi.h"
#include "instremu.h"
#include "segreg.h"
#include "dosemu_debug.h"
#include "msdos_ldt.h"

static unsigned char *ldt_backbuf;
static unsigned char *ldt_alias;
static unsigned short dpmi_ldt_alias;
static int entry_upd;

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
    /* need to set limit before base_addr to avoid ldt autoexpanding */
    SetSegmentLimit(dpmi_ldt_alias, PAGE_ALIGN(lim) - 1);
    SetSegmentBaseAddress(dpmi_ldt_alias, DOSADDR_REL(ldt_alias));
    return dpmi_ldt_alias;
}

int msdos_ldt_fault(struct sigcontext *scp)
{
    /* basically on dosemu this code is unused because msdos_ldt_update()
     * manages the limit too. But it may be good to keep for the cases
     * where msdos_ldt_update() is not used. For example if the LDT is R/W,
     * it may be much simpler to not use msdos_ldt_update(). */
    int pref_seg = -1, done = 0;
    unsigned char *csp;

    csp = (unsigned char *) SEL_ADR(_cs, _eip);
    do {
      switch (*(csp++)) {
         case 0x66:      /* operand prefix */  /*prefix66=1;*/ break;
         case 0x67:      /* address prefix */  /*prefix67=1;*/ break;
         case 0x2e:      /* CS */              pref_seg=_cs; break;
         case 0x3e:      /* DS */              pref_seg=_ds; break;
         case 0x26:      /* ES */              pref_seg=_es; break;
         case 0x36:      /* SS */              pref_seg=_ss; break;
         case 0x65:      /* GS */              pref_seg=_gs; break;
         case 0x64:      /* FS */              pref_seg=_fs; break;
         case 0xf2:      /* repnz */
         case 0xf3:      /* rep */             /*is_rep=1;*/ break;
         default: done=1;
      }
    } while (!done);

    if (pref_seg == -1)
	pref_seg = _ds;
    if (pref_seg == dpmi_ldt_alias) {
	unsigned limit;
	if (ldt_backbuf)
	    error("LDT fault with backbuffer present\n");
	limit = GetSegmentLimit(dpmi_ldt_alias);
	D_printf("DPMI: expanding LDT, old_lim=0x%x\n", limit);
	SetSegmentLimit(dpmi_ldt_alias, limit + DPMI_page_size);
	return 1;
    }

    return 0;
}

static uint8_t reg8(struct sigcontext *scp, int reg)
{
#define RG8(x, r) ((_e##x >> ((r & 4) << 1)) & 0xff)
    switch (reg & 3) {
    case 0:
	return RG8(ax, reg);
    case 1:
	return RG8(cx, reg);
    case 2:
	return RG8(dx, reg);
    case 3:
	return RG8(bx, reg);
    }
    return -1;
}

static uint32_t reg(struct sigcontext *scp, int reg)
{
    switch (reg & 7) {
    case 0:
	return _eax;
    case 1:
	return _ecx;
    case 2:
	return _edx;
    case 3:
	return _ebx;
    case 4:
	return _esp;
    case 5:
	return _ebp;
    case 6:
	return _esi;
    case 7:
	return _edi;
    }
    return -1;
}

static int decode_memop(struct sigcontext *scp, uint32_t *op)
{
    unsigned cs, eip, seg_base;
    unsigned char *csp, *orig_csp;
    x86_ins x86;
    int inst_len, loop_inc, ret = 0;

    x86._32bit = dpmi_mhp_get_selector_size(_cs);
    cs = GetSegmentBase(_cs);
    eip = _eip + x86_handle_prefixes(scp, cs, &x86);
    if (x86.rep) {		// FIXME
	error("LDT: Unimplemented rep\n");
	return 0;
    }
    csp = (unsigned char *)MEM_BASE32(cs + eip);
    orig_csp = (unsigned char *)MEM_BASE32(cs + _eip);
    inst_len = instr_len(orig_csp, x86._32bit);
    loop_inc = (_eflags & DF) ? -1 : 1;
    if (x86.es)
	seg_base = GetSegmentBase(_es);
    else if (x86.fs)
	seg_base = GetSegmentBase(_fs);
    else if (x86.gs)
	seg_base = GetSegmentBase(_gs);
    else if (x86.cs)
	seg_base = GetSegmentBase(_cs);
    else if (x86.ss)
	seg_base = GetSegmentBase(_ss);
    else
	seg_base = GetSegmentBase(_ds);

    switch(*csp) {
    case 0x88:		/* mov r/m8,reg8 */
	*op = reg8(scp, csp[1] >> 3);
	ret = 1;
	break;

    case 0x89:		/* mov r/m16,reg */
	*op = reg(scp, csp[1] >> 3);
	ret = x86.operand_size;
	break;

    case 0xc6:		/* mov r/m8,imm8 */
	*op = orig_csp[inst_len - 1];
	ret = 1;
	break;

    case 0xc7:		/* mov r/m,imm */
	switch (x86.operand_size) {
	case 2:
	    *op = *(uint16_t *)(orig_csp + inst_len - 2);
	    ret = 2;
	    break;
	case 4:
	    *op = *(uint32_t *)(orig_csp + inst_len - 4);
	    ret = 4;
	    break;
	}
	break;

     case 0x80:		/* logical r/m8,imm8 */
     case 0x82:
	*op = instr_binary_byte(csp[1] >> 3, *(unsigned char *)_cr2,
		orig_csp[inst_len - 1], &_eflags);
	ret = 1;
	break;

    case 0x81:		/* logical r/m,imm */
	switch (x86.operand_size) {
	case 2:
	    *op = instr_binary_word(csp[1] >> 3, *(uint16_t *)_cr2,
		    *(uint16_t *)(orig_csp + inst_len - 2), &_eflags);
	    ret = 2;
	    break;
	case 4:
	    *op = instr_binary_dword(csp[1] >> 3, *(uint32_t *)_cr2,
		    *(uint32_t *)(orig_csp + inst_len - 4), &_eflags);
	    ret = 4;
	    break;
	}
	break;

    case 0x83:		/* logical r/m,imm8 */
	switch (x86.operand_size) {
	case 2:
	    *op = instr_binary_word(csp[1] >> 3, *(uint16_t *)_cr2,
		    (short)*(signed char *)(orig_csp + inst_len - 1), &_eflags);
	    ret = 2;
	    break;
	case 4:
	    *op = instr_binary_dword(csp[1] >> 3, *(uint32_t *)_cr2,
		    (int)*(signed char *)(orig_csp + inst_len - 1), &_eflags);
	    ret = 4;
	    break;
	}
	break;

    case 0x8f:	/*pop*/
	*op = x86_pop(scp, &x86);
	ret = x86.operand_size;
	break;

    case 0xa2:		/* mov moff16,al */
	*op = _eax & 0xff;
	ret = 1;
	break;

    case 0xa3:		/* mov moff16,ax */
	switch (x86.operand_size) {
	case 2:
	    *op = _eax & 0xffff;
	    ret = 2;
	    break;
	case 4:
	    *op = _eax;
	    ret = 4;
	    break;
	}
	break;

    case 0xa4:		/* movsb */
	switch (x86.address_size) {
	case 2:
	    *op = *(unsigned char *)MEM_BASE32(seg_base + _LWORD(esi));
	    _LWORD(edi) += loop_inc;
	    _LWORD(esi) += loop_inc;
	    break;
	case 4:
	    *op = *(unsigned char *)MEM_BASE32(seg_base + _esi);
	    _edi += loop_inc;
	    _esi += loop_inc;
	    break;
	}
	ret = 1;
	break;

    case 0xa5:		/* movsw */
	switch (x86.operand_size) {
	case 2:
	    switch (x86.address_size) {
	    case 2:
		*op = *(uint16_t *)MEM_BASE32(seg_base + _LWORD(esi));
		_LWORD(edi) += loop_inc * 2;
		_LWORD(esi) += loop_inc * 2;
		break;
	    case 4:
		*op = *(uint16_t *)MEM_BASE32(seg_base + _esi);
		_edi += loop_inc * 2;
		_esi += loop_inc * 2;
		break;
	    }
	    ret = 2;
	    break;
	case 4:
	    switch (x86.address_size) {
	    case 2:
		*op = *(uint32_t *)MEM_BASE32(seg_base + _LWORD(esi));
		_LWORD(edi) += loop_inc * 4;
		_LWORD(esi) += loop_inc * 4;
		break;
	    case 4:
		*op = *(uint32_t *)MEM_BASE32(seg_base + _esi);
		_edi += loop_inc * 4;
		_esi += loop_inc * 4;
		break;
	    }
	    ret = 4;
	    break;
	}
	break;

    case 0xaa:		/* stosb */
	*op = _eax & 0xff;
	switch (x86.address_size) {
	case 2:
	    _LWORD(edi) += loop_inc;
	    break;
	case 4:
	    _edi += loop_inc;
	    break;
	}
	ret = 1;
	break;

    case 0xab:		/* stosw */
	switch (x86.operand_size) {
	case 2:
	    *op = _LWORD(eax);
	    switch (x86.address_size) {
	    case 2:
		_LWORD(edi) += loop_inc * 2;
		break;
	    case 4:
		_edi += loop_inc * 2;
		break;
	    }
	    ret = 2;
	    break;
	case 4:
	    *op = _eax;
	    switch (x86.address_size) {
	    case 2:
		_LWORD(edi) += loop_inc * 4;
		break;
	    case 4:
		_edi += loop_inc * 4;
		break;
	    }
	    ret = 4;
	    break;
	}
	break;

    case 0x00:		/* add r/m8,reg8 */
    case 0x08:		/* or r/m8,reg8 */
    case 0x10:		/* adc r/m8,reg8 */
    case 0x18:		/* sbb r/m8,reg8 */
    case 0x20:		/* and r/m8,reg8 */
    case 0x28:		/* sub r/m8,reg8 */
    case 0x30:		/* xor r/m8,reg8 */
//    case 0x38:		/* cmp r/m8,reg8 */
	*op = instr_binary_byte(csp[0] >> 3, *(unsigned char *)_cr2,
		reg8(scp, csp[1] >> 3), &_eflags);
	ret = 1;
	break;

    case 0x01:		/* add r/m16,reg16 */
    case 0x09:		/* or r/m16,reg16 */
    case 0x11:		/* adc r/m16,reg16 */
    case 0x19:		/* sbb r/m16,reg16 */
    case 0x21:		/* and r/m16,reg16 */
    case 0x29:		/* sub r/m16,reg16 */
    case 0x31:		/* xor r/m16,reg16 */
//  case 0x39:		/* cmp r/m16,reg16 */
	switch (x86.operand_size) {
	case 2:
	    *op = instr_binary_word(csp[0] >> 3, *(uint16_t *)_cr2,
		    reg(scp, csp[1] >> 3), &_eflags);
	    ret = 2;
	    break;
	case 4:
	    *op = instr_binary_dword(csp[0] >> 3, *(uint32_t *)_cr2,
		    reg(scp, csp[1] >> 3), &_eflags);
	    ret = 4;
	    break;
	}
	break;

    default:
	error("Unimplemented memop decode %#x\n", *csp);
	return 0;
  }

  assert(ret);
  _eip += inst_len;
  return ret;
}

void msdos_ldt_update(int entry, u_char *buf, int len)
{
  if (dpmi_ldt_alias) {
    unsigned limit = GetSegmentLimit(dpmi_ldt_alias);
    unsigned new_len = entry * LDT_ENTRY_SIZE + len;
    if (limit < new_len - 1) {
      D_printf("DPMI: expanding LDT, old_lim=0x%x\n", limit);
      SetSegmentLimit(dpmi_ldt_alias, PAGE_ALIGN(new_len) - 1);
    }
  }
  if (ldt_backbuf && entry != entry_upd)
    memcpy(&ldt_backbuf[entry * LDT_ENTRY_SIZE], buf, len);
}

static void direct_ldt_write(int offset, char *buffer, int length)
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

    direct_ldt_write(_cr2 - (unsigned long)ldt_alias, (char *)&op, len);
    return 1;
}
