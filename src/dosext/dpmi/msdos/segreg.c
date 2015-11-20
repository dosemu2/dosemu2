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
 * Purpose: decode segreg access
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 * Based on instremu.c code by Bart Oldeman
 *
 * Normally DPMI reserved some GDT entries for the API translator,
 * like 0x40, to span some important DOS areas. We can only allocate
 * LDTs, and therefore we replace the GDT selector with LDT selector
 * in the appropriate segreg.
 */

#include "cpu.h"
#include "dpmi.h"
#include "dosemu_debug.h"
#include "instremu.h"
#include "msdos_ldt.h"
#include "msdos_priv.h"
#include "segreg.h"

#define R_WORD(a) LO_WORD(a)
#define SP (R_WORD(_esp))
#define sreg_idx(reg) (es_INDEX+((reg)&0x7))

enum {REP_NONE, REPZ, REPNZ};
static unsigned wordmask[5] = {0,0xff,0xffff,0xffffff,0xffffffff};

uint32_t x86_pop(struct sigcontext *scp, x86_ins *x86)
{
  unsigned ss_base = GetSegmentBase(_ss);
  unsigned char *mem = MEM_BASE32(ss_base + (_esp & wordmask[(x86->_32bit+1)*2]));
  if (x86->_32bit)
    _esp += x86->operand_size;
  else
    SP += x86->operand_size;
  return (x86->operand_size == 4 ? READ_DWORDP(mem) : READ_WORDP(mem));
}

int x86_handle_prefixes(struct sigcontext *scp, unsigned cs_base,
	x86_ins *x86)
{
  unsigned eip = _eip;
  int prefix = 0;

  x86->rep = 0;
  x86->cs = 0;
  x86->ds = 0;
  x86->es = 0;
  x86->fs = 0;
  x86->gs = 0;
  x86->ss = 0;
  x86->address_size = x86->operand_size = (x86->_32bit + 1) * 2;
  for (;; eip++) {
    switch(*(unsigned char *)MEM_BASE32(cs_base + eip)) {
    /* handle (some) prefixes */
      case 0x26:
        prefix++;
        x86->es = 1;
        break;
      case 0x2e:
        prefix++;
        x86->cs = 1;
        break;
      case 0x36:
        prefix++;
        x86->ss = 1;
        break;
      case 0x3e:
        prefix++;
        x86->ds = 1;
        break;
      case 0x64:
        prefix++;
        x86->fs = 1;
        break;
      case 0x65:
        prefix++;
        x86->gs = 1;
        break;
      case 0x66:
        prefix++;
        x86->operand_size = 6 - x86->operand_size;
        break;
      case 0x67:
        prefix++;
        x86->address_size = 6 - x86->address_size;
        break;
      case 0xf2:
        prefix++;
        x86->rep = REPNZ;
        break;
      case 0xf3:
        prefix++;
        x86->rep = REPZ;
        break;
      default:
        return prefix;
    }
  }
  return prefix;
}

#if DEBUG_INSTR >= 2
#define instr_deb2(x...) v_printf("VGAEmu: " x)
#else
#define instr_deb2(x...)
#endif

static int decode_segreg(struct sigcontext *scp)
{
  unsigned cs, eip;
  unsigned char *csp, *orig_csp;
  int ret = -1;
  x86_ins x86;

  x86._32bit = dpmi_mhp_get_selector_size(_cs);
  cs = GetSegmentBase(_cs);
  eip = _eip + x86_handle_prefixes(scp, cs, &x86);
  csp = (unsigned char *)MEM_BASE32(cs + eip);
  orig_csp = (unsigned char *)MEM_BASE32(cs + _eip);

  switch(*csp) {
    case 0x8e:		/* mov segreg,r/m16 */
      ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + eip + 1) >> 3);
      _eip += instr_len(orig_csp, x86._32bit);
      break;

    case 0xca: /*retf imm 16*/
    case 0xcb: /*retf*/
    case 0xcf: /*iret*/
    {
      unsigned tmp_eip = x86_pop(scp, &x86);
      x86_pop(scp, &x86);
      ret = cs_INDEX;
      switch (*(unsigned char *)MEM_BASE32(cs + eip)) {
        case 0xca: /*retf imm 16*/
	  _esp += ((unsigned short *) (MEM_BASE32(cs + eip + 1)))[0];
	  break;
        case 0xcf: /*iret*/
	  scp->eflags = x86_pop(scp, &x86);
	  break;
      }
      _eip = tmp_eip;
    }
    break;

    case 0xea:			/* jmp seg:off16/off32 */
    {
      unsigned tmp_eip;
      tmp_eip = x86.operand_size == 4 ? READ_DWORDP(MEM_BASE32(cs + eip + 1)) :
		READ_WORDP(MEM_BASE32(cs + eip + 1));
      ret = cs_INDEX;
      _eip = tmp_eip;
    }
    break;

    case 0xc4:		/* les */
      ret = es_INDEX;
      _eip += instr_len(orig_csp, x86._32bit);
      break;

    case 0xc5:		/* lds */
      ret = ds_INDEX;
      _eip += instr_len(orig_csp, x86._32bit);
      break;

    case 0x07:	/* pop es */
    case 0x17:	/* pop ss */
    case 0x1f:	/* pop ds */
      ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + eip) >> 3);
      x86_pop(scp, &x86);
      _eip = eip + 1;
      break;

    case 0x0f:
      eip++;
      switch (*(unsigned char *)MEM_BASE32(cs + eip)) {
        case 0xa1:	/* pop fs */
        case 0xa9:	/* pop gs */
	  x86_pop(scp, &x86);
	  ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + eip) >> 3);
	  _eip = eip + 1;
	  break;

	case 0xb2:	/* lss */
	  ret = ss_INDEX;
	  _eip += instr_len(orig_csp, x86._32bit);
	  break;

	case 0xb4:	/* lfs */
	  ret = fs_INDEX;
	  _eip += instr_len(orig_csp, x86._32bit);
	  break;

	case 0xb5:	/* lgs */
	  ret = gs_INDEX;
	  _eip += instr_len(orig_csp, x86._32bit);
	  break;
      }
      break;
  }

  return ret;
}

int msdos_fault(struct sigcontext *scp)
{
    struct sigcontext new_sct;
    int reg;
    unsigned int segment;
    unsigned short desc;

    D_printf("MSDOS: msdos_fault, err=%#lx\n", _err);
    if ((_err & 0xffff) == 0)	/*  not a selector error */
	return msdos_ldt_fault(scp);

    /* now it is a invalid selector error, try to fix it if it is */
    /* caused by an instruction such as mov Sreg,r/m16            */
#define ALL_GDTS 0
#if !ALL_GDTS
    segment = (_err & 0xfff8);
    /* only allow using some special GTDs */
    switch (segment) {
    case 0x0040:
    case 0xa000:
    case 0xb000:
    case 0xb800:
    case 0xc000:
    case 0xe000:
    case 0xf000:
    case 0xbf8:
    case 0xf800:
    case 0xff00:
    case 0x38:		// ShellShock installer
	break;
    default:
	return 0;
    }
    copy_context(&new_sct, scp, 0);
    reg = decode_segreg(&new_sct);
    if (reg == -1)
	return 0;
#else
    copy_context(&new_sct, scp, 0);
    reg = decode_modify_segreg_insn(&new_sct, 1, &segment);
    if (reg == -1)
	return 0;

    if (ValidAndUsedSelector(segment)) {
	/*
	 * The selector itself is OK, but the descriptor (type) is not.
	 * We cannot fix this! So just give up immediately and dont
	 * screw up the context.
	 */
	D_printf("MSDOS: msdos_fault: Illegal use of selector %#x\n",
		 segment);
	return 0;
    }
#endif

    D_printf("MSDOS: try mov to a invalid selector 0x%04x\n", segment);

    switch (segment) {
    case 0x38:
	/* dos4gw sets VCPI descriptors 0x28, 0x30, 0x38 */
	/* The 0x38 is the "flat data" segment (0,4G) */
	desc = ConvertSegmentToDescriptor_lim(0, 0xffffffff);
	break;
    default:
	/* any other special cases? */
	desc = ConvertSegmentToDescriptor(segment);
    }
    if (!desc)
	return 0;

    /* OKay, all the sanity checks passed. Now we go and fix the selector */
    copy_context(scp, &new_sct, 0);
    switch (reg) {
    case es_INDEX:
	_es = desc;
	break;
    case cs_INDEX:
	_cs = desc;
	break;
    case ss_INDEX:
	_ss = desc;
	break;
    case ds_INDEX:
	_ds = desc;
	break;
    case fs_INDEX:
	_fs = desc;
	break;
    case gs_INDEX:
	_gs = desc;
	break;
    }

    /* let's hope we fixed the thing, and return */
    return 1;
}
