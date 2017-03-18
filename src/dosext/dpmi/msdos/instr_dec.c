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
 * Purpose: library for instructions decoding
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 * Based on instremu.c code by Bart Oldeman
 */

#include <inttypes.h>
#include <sys/types.h>
#include <assert.h>
#include "cpu.h"
#include "dosemu_debug.h"
#include "memory.h"
#include "dpmi.h"
#include "instremu.h"
#include "instr_dec.h"

typedef struct x86_ins {
  int _32bit:1;	/* 16/32 bit code */
  unsigned address_size; /* in bytes so either 4 or 2 */
  unsigned operand_size;
  int rep;
  int ds:1, es:1, fs:1, gs:1, cs:1, ss:1;
} x86_ins;

#define R_WORD(a) LO_WORD(a)
#define SP (R_WORD(_esp))
#define sreg_idx(reg) (es_INDEX+((reg)&0x7))

enum {REP_NONE, REPZ, REPNZ};
static unsigned wordmask[5] = {0,0xff,0xffff,0xffffff,0xffffffff};

static uint32_t x86_pop(struct sigcontext *scp, x86_ins *x86)
{
  unsigned ss_base = GetSegmentBase(_ss);
  unsigned char *mem = MEM_BASE32(ss_base + (_esp & wordmask[(x86->_32bit+1)*2]));
  if (x86->_32bit)
    _esp += x86->operand_size;
  else
    SP += x86->operand_size;
  return (x86->operand_size == 4 ? READ_DWORDP(mem) : READ_WORDP(mem));
}

static int x86_handle_prefixes(struct sigcontext *scp, unsigned cs_base,
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

int decode_segreg(struct sigcontext *scp)
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
	  _eflags = get_EFLAGS(x86_pop(scp, &x86));
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

int decode_memop(struct sigcontext *scp, uint32_t *op)
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

    case 0xfe: /* inc/dec mem */
	*op = *(unsigned char *)_cr2;
	switch (csp[1] & 0x38) {
	case 0:	/* inc */
	    (*op)++;
	    break;
	case 8:	/* dec */
	    (*op)--;
	    break;
	}
	ret = 1;
	break;

    case 0x0f:
	switch (csp[1]) {
	case 0xba: { /* GRP8 - Code Extension 22 */
	    switch (csp[2] & 0x38) {
	    case 0x30: { /* BTR r/m16, imm8 */
		uint32_t mask = 1 << (csp[4] & 0x1f);
		switch (x86.operand_size) {
		case 2:
		    *op = *(uint16_t *)_cr2;
		    ret = 2;
		    break;
		case 4:
		    *op = *(uint32_t *)_cr2;
		    ret = 4;
		    break;
		}
		if (*op & mask)
		    _eflags |= CF;
		else
		    _eflags &= ~CF;
		*op &= ~mask;
		break;
	    }
	    default:
		error("Unimplemented memop decode GRP8 %#x\n", csp[2]);
		break;
	    }
	    break;
	}
	default:
	    error("Unimplemented memop decode 0x0f %#x\n", csp[1]);
	    break;
	}
	break;

    default:
	error("Unimplemented memop decode %#x\n", *csp);
	return 0;
  }

  assert(ret);
  assert(inst_len);
  _eip += inst_len;
  return ret;
}
