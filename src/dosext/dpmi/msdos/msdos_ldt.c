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

#include "cpu.h"
#include "dpmi.h"
#include "vgaemu.h"
#include "segreg.h"
#include "dosemu_debug.h"
#include "msdos_ldt.h"

#define LDT_INIT_LIMIT 0xfff

unsigned char *ldt_alias;
static unsigned short dpmi_ldt_alias;

int msdos_ldt_setup(unsigned char *alias, unsigned short alias_sel)
{
    if (SetSelector(alias_sel, DOSADDR_REL(alias),
		    LDT_INIT_LIMIT, 0,
                  MODIFY_LDT_CONTENTS_DATA, 0, 0, 0, 0))
	return 0;
    ldt_alias = alias;
    dpmi_ldt_alias = alias_sel;
    return 1;
}

u_short DPMI_ldt_alias(void)
{
  return dpmi_ldt_alias;
}

int msdos_ldt_fault(struct sigcontext *scp)
{
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
	unsigned limit = GetSegmentLimit(dpmi_ldt_alias);
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
    unsigned cs, eip;
    unsigned char *csp, *orig_csp;
    x86_ins x86;
    int inst_len;

    x86._32bit = dpmi_mhp_get_selector_size(_cs);
    cs = GetSegmentBase(_cs);
    eip = _eip + x86_handle_prefixes(scp, cs, &x86);
    csp = (unsigned char *)MEM_BASE32(cs + eip);
    orig_csp = (unsigned char *)MEM_BASE32(cs + _eip);

    switch(*csp) {
    case 0x88:		/* mov r/m8,reg8 */
	*op = reg8(scp, csp[1] >> 3);
	return 1;

    case 0x89:		/* mov r/m16,reg */
	*op = reg(scp, csp[1] >> 3);
	return x86.operand_size;

    case 0xc6:		/* mov r/m8,imm8 */
	inst_len = x86_instr_len(orig_csp, x86._32bit);
	*op = orig_csp[inst_len - 1];
	return 1;

    case 0xc7:		/* mov r/m,imm */
	inst_len = x86_instr_len(orig_csp, x86._32bit);
	switch (x86.operand_size) {
	case 2:
	    *op = *(uint16_t *)(orig_csp + inst_len - 2);
	    return 2;
	case 4:
	    *op = *(uint32_t *)(orig_csp + inst_len - 4);
	    return 4;
	default:
	    error("unknown op size\n");
	    return 0;
	}
	break;

    default:
	error("Unimplemented memop decode %#x\n", *csp);
	break;
  }

  return 0;
}

int msdos_ldt_pagefault(struct sigcontext *scp)
{
    uint32_t op;
    int len;
    unsigned cs;
    unsigned char *csp;

    if ((unsigned char *)_cr2 < ldt_alias ||
	  (unsigned char *)_cr2 >= ldt_alias + LDT_ENTRIES * LDT_ENTRY_SIZE)
	return 0;
    len = decode_memop(scp, &op);
    if (!len) {
	instr_emu(scp, 1, 1);
	return 1;
    }

    cs = GetSegmentBase(_cs);
    csp = (unsigned char *)MEM_BASE32(cs + _eip);
    direct_ldt_write(_cr2 - (unsigned long)ldt_alias, len, (char *)&op);
    _eip += x86_instr_len(csp, dpmi_mhp_get_selector_size(_cs));
    return 1;
}
