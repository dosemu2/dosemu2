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
#include "dos2linux.h"
#include "emudpmi.h"
#include "instr_dec.h"

typedef struct x86_ins {
  unsigned _32bit:1;	/* 16/32 bit code */
  unsigned address_size; /* in bytes so either 4 or 2 */
  unsigned operand_size;
  int rep;
  unsigned ds:1, es:1, fs:1, gs:1, cs:1, ss:1;
} x86_ins;

static unsigned short patch_cs;

#define R_WORD(a) LO_WORD(a)
#define SP (R_WORD(_esp))
#define sreg_idx(reg) (es_INDEX+((reg)&0x7))

enum {REP_NONE, REPZ, REPNZ};
static unsigned wordmask[5] = {0,0xff,0xffff,0xffffff,0xffffffff};

static unsigned char it[0x100] = {
  7, 7, 7, 7, 2, 3, 1, 1,    7, 7, 7, 7, 2, 3, 1, 0,
  7, 7, 7, 7, 2, 3, 1, 1,    7, 7, 7, 7, 2, 3, 1, 1,
  7, 7, 7, 7, 2, 3, 0, 1,    7, 7, 7, 7, 2, 3, 0, 1,
  7, 7, 7, 7, 2, 3, 0, 1,    7, 7, 7, 7, 2, 3, 0, 1,

  1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 7, 7, 0, 0, 0, 0,    3, 9, 2, 8, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2,    2, 2, 2, 2, 2, 2, 2, 2,

  8, 9, 8, 8, 7, 7, 7, 7,    7, 7, 7, 7, 7, 7, 7, 7,
  1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 6, 1, 1, 1, 1, 1,
  4, 4, 4, 4, 1, 1, 1, 1,    2, 3, 1, 1, 1, 1, 1, 1,
  2, 2, 2, 2, 2, 2, 2, 2,    3, 3, 3, 3, 3, 3, 3, 3,

  8, 8, 3, 1, 7, 7, 8, 9,    5, 1, 3, 1, 1, 2, 1, 1,
  7, 7, 7, 7, 2, 2, 1, 1,    0, 0, 0, 0, 0, 0, 0, 0,
  2, 2, 2, 2, 2, 2, 2, 2,    4, 4, 6, 2, 1, 1, 1, 1,
  0, 1, 0, 0, 1, 1, 7, 7,    1, 1, 1, 1, 1, 1, 7, 7
};

static unsigned arg_len(unsigned char *p, int asp)
{
  unsigned u = 0, m, s = 0;

  m = *p & 0xc7;
  if(asp) {
    if(m == 5) {
      u = 5;
    }
    else {
      if((m >> 6) < 3 && (m & 7) == 4) s = 1;
      switch(m >> 6) {
        case 1:
          u = 2; break;
        case 2:
          u = 5; break;
        default:
          u = 1;
      }
      u += s;
    }
  }
  else {
    if(m == 6)
      u = 3;
    else
      switch(m >> 6) {
        case 1:
          u = 2; break;
        case 2:
          u = 3; break;
        default:
          u = 1;
      }
  }

  return u;
}

static int _instr_len(unsigned char *p, int is_32)
{
  unsigned u, osp, asp;
  unsigned char *p0 = p;

  osp = asp = is_32;

  for(u = 1; u && p - p0 < 17;) switch(*p++) {		/* get prefixes */
    case 0x26:	/* es: */
//      seg = 1;
      break;
    case 0x2e:	/* cs: */
//      seg = 2;
      break;
    case 0x36:	/* ss: */
//      seg = 3;
      break;
    case 0x3e:	/* ds: */
//      seg = 4;
      break;
    case 0x64:	/* fs: */
//      seg = 5;
      break;
    case 0x65:	/* gs: */
//      seg = 6;
      break;
    case 0x66:	/* operand size */
      osp ^= 1; break;
    case 0x67:	/* address size */
      asp ^= 1; break;
    case 0xf0:	/* lock */
//      lock = 1;
      break;
    case 0xf2:	/* repnz */
//      rep = 2;
      break;
    case 0xf3:	/* rep(z) */
//      rep = 1;
      break;
    default:	/* no prefix */
      u = 0;
  }
  p--;

  if(p - p0 >= 16) return 0;

  if(*p == 0x0f) {
    p++;
    switch (*p) {
    case 0xba:
      p += 4;
      return p - p0;
    case 0xb2:  // lss
    case 0xb4:  // lfs
    case 0xb5:  // lgs
      p++;
      p += (u = arg_len(p, asp));
      if(!u) p = p0;
      return p - p0;
    default:
      /* not yet */
      error("unsupported instr_len %x %x\n", p[0], p[1]);
      return 0;
    }
  }

  switch(it[*p]) {
    case 1:	/* op-code */
      p += 1; break;

    case 2:	/* op-code + byte */
      p += 2; break;

    case 3:	/* op-code + word/dword */
      p += osp ? 5 : 3; break;

    case 4:	/* op-code + [word/dword] */
      p += asp ? 5 : 3; break;

    case 5:	/* op-code + word/dword + byte */
      p += osp ? 6 : 4; break;

    case 6:	/* op-code + [word/dword] + word */
      p += asp ? 7 : 5; break;

    case 7:	/* op-code + mod + ... */
      p++;
      p += (u = arg_len(p, asp));
      if(!u) p = p0;
      break;

    case 8:	/* op-code + mod + ... + byte */
      p++;
      p += (u = arg_len(p, asp)) + 1;
      if(!u) p = p0;
      break;

    case 9:	/* op-code + mod + ... + word/dword */
      p++;
      p += (u = arg_len(p, asp)) + (osp ? 4 : 2);
      if(!u) p = p0;
      break;

    default:
      p = p0;
  }

  return p - p0;
}

static unsigned char parity[256] =
   { PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
     PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
     PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF,
     PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
     PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF,
     PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
     PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
      0, PF, PF,  0, PF,  0,  0, PF, PF,  0,  0, PF,  0, PF, PF,  0,
     PF,  0,  0, PF,  0, PF, PF,  0,  0, PF, PF,  0, PF,  0,  0, PF };


// see http://www.emulators.com/docs/LazyOverflowDetect_Final.pdf
// same as used in codegen-sim.c:
// CF = cout bit 31, OF^CF = cout bit 30, AF = cout bit 3
static void FlagSync(int res, unsigned cout, unsigned *eflags)
{
	unsigned nf;

	nf = parity[res & 0xff];
	nf |= (res == 0) << 6; // ZF
	nf |= ((unsigned)res >> 24) & SF;

	nf |= (cout << 1) & AF;
	nf |= (cout >> 31) & CF;
	// 80000000->0800 ^ 40000000->0800
	nf |= ((cout >> 20) ^ (cout >> 19)) & OF;

	*eflags = (*eflags & ~(OF|ZF|SF|PF|AF|CF)) | nf;
}

/* 6 logical and arithmetic "RISC" core functions
   follow
*/
static unsigned char instr_binary_byte(unsigned char op, unsigned char op1, unsigned char op2, unsigned *eflags)
{
  unsigned char res = 0;
  unsigned cout = (*eflags & AF) ? 8 : 0;

  switch (op&0x7){
  case 1: /* or */
    res = op1 | op2;
    break;
  case 4: /* and */
    res = op1 & op2;
    break;
  case 6: /* xor */
    res = op1 ^ op2;
    break;
  case 0: /* add */
    *eflags &= ~CF; /* Fall through */
  case 2: /* adc */
    res = op1 + op2 + (*eflags & CF);
    cout = (op1 & op2) | ((op1 | op2) & ~res);
    break;
  case 5: /* sub */
  case 7: /* cmp */
    *eflags &= ~CF; /* Fall through */
  case 3: /* sbb */
    res = op1 - op2 - (*eflags & CF);
    cout = (~op1 & op2) | ((~op1 ^ op2) & res);
    break;
  }
  cout = ((cout >> 6) << 30) | (cout & 8);
  FlagSync((int32_t)(int8_t)res, cout, eflags);
  return res;
}

static unsigned instr_binary_word(unsigned op, unsigned op1, unsigned op2, unsigned *eflags)
{
  unsigned short opw1 = op1;
  unsigned short opw2 = op2;
  unsigned short res = 0;
  unsigned cout = (*eflags & AF) ? 8 : 0;

  switch (op&0x7){
  case 1: /* or */
    res = opw1 | opw2;
    break;
  case 4: /* and */
    res = opw1 & opw2;
    break;
  case 6: /* xor */
    res = opw1 ^ opw2;
    break;
  case 0: /* add */
    *eflags &= ~CF; /* Fall through */
  case 2: /* adc */
    res = opw1 + opw2 + (*eflags & CF);
    cout = (opw1 & opw2) | ((opw1 | opw2) & ~res);
    break;
  case 5: /* sub */
  case 7: /* cmp */
    *eflags &= ~CF; /* Fall through */
  case 3: /* sbb */
    res = opw1 - opw2 - (*eflags & CF);
    cout = (~opw1 & opw2) | ((~opw1 ^ opw2) & res);
    break;
  }
  cout = ((cout >> 14) << 30) | (cout & 8);
  FlagSync((int32_t)(int16_t)res, cout, eflags);
  return res;
}

static unsigned instr_binary_dword(unsigned op, unsigned op1, unsigned op2, unsigned *eflags)
{
  unsigned res = 0;
  unsigned cout = (*eflags & AF) ? 8 : 0;

  switch (op&0x7){
  case 1: /* or */
    res = op1 | op2;
    break;
  case 4: /* and */
    res = op1 & op2;
    break;
  case 6: /* xor */
    res = op1 ^ op2;
    break;
  case 0: /* add */
    *eflags &= ~CF; /* Fall through */
  case 2: /* adc */
    res = op1 + op2 + (*eflags & CF);
    cout = (op1 & op2) | ((op1 | op2) & ~res);
    break;
  case 5: /* sub */
  case 7: /* cmp */
    *eflags &= ~CF; /* Fall through */
  case 3: /* sbb */
    res = op1 - op2 - (*eflags & CF);
    cout = (~op1 & op2) | ((~op1 ^ op2) & res);
    break;
  }
  FlagSync((int32_t)res, cout, eflags);
  return res;
}

static uint32_t x86_pop(cpuctx_t *scp, x86_ins *x86)
{
  unsigned ss_base = GetSegmentBase(_ss);
  unsigned char *mem = MEM_BASE32(ss_base + (_esp & wordmask[(x86->_32bit+1)*2]));
  if (x86->_32bit)
    _esp += x86->operand_size;
  else
    _LWORD(esp) += x86->operand_size;
  return (x86->operand_size == 4 ? READ_DWORDP(mem) : READ_WORDP(mem));
}

static int x86_handle_prefixes(cpuctx_t *scp, unsigned cs_base,
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

static void make_retf_frame(cpuctx_t *scp, void *sp,
	uint32_t cs, uint32_t eip, int is_32)
{
  if (is_32) {
    unsigned int *ssp = sp;
    *--ssp = cs;
    *--ssp = eip;
    _esp -= 8;
  } else {
    unsigned short *ssp = sp;
    *--ssp = cs;
    *--ssp = eip;
    _LWORD(esp) -= 4;
  }
}

#define PATCH_SIZE 8
#define STACK_SIZE (PATCH_SIZE + 8)

static void lxx_patch(cpuctx_t *scp, unsigned char *csp, int len, int is_32)
{
  unsigned char *sp;
  unsigned int lp[LDT_ENTRY_SIZE / 4], esp, base;
  unsigned char retf[] = { 0xca, PATCH_SIZE, 0 };

  if (_LWORD(esp) < STACK_SIZE) {
    error("asm patch failure\n");
    return;
  }
  assert(len + sizeof(retf) <= PATCH_SIZE);
  _esp -= PATCH_SIZE;
  sp = SEL_ADR(_ss, _esp);
  esp = (dpmi_segment_is32(_ss) ? _esp : _LWORD(esp));
  make_retf_frame(scp, sp, _cs, _eip, is_32);
  /* prepare the patch */
  memcpy(sp, csp, len);
  sp[0] = 0x8b;  // replace lXX with mov
  memcpy(sp + len, retf, sizeof(retf));
  GetDescriptor(_cs, lp);
  SetDescriptor(patch_cs, lp);
  base = GetSegmentBase(_ss);
  SetSegmentBaseAddress(patch_cs, base + esp);
  SetSegmentLimit(patch_cs, PATCH_SIZE - 1);
  _cs = patch_cs;
  _eip = 0;
}

int decode_segreg(cpuctx_t *scp)
{
  unsigned cs, eip;
  unsigned char *csp, *orig_csp;
  int ret = -1;
  x86_ins x86;

  x86._32bit = dpmi_segment_is32(_cs);
  cs = GetSegmentBase(_cs);
  eip = _eip + x86_handle_prefixes(scp, cs, &x86);
  csp = (unsigned char *)MEM_BASE32(cs + eip);
  orig_csp = (unsigned char *)MEM_BASE32(cs + _eip);

  switch(*csp) {
    case 0x8e:		/* mov segreg,r/m16 */
      ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + eip + 1) >> 3);
      _eip += _instr_len(orig_csp, x86._32bit);
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
	  _eflags = dpmi_flags_from_stack_iret(scp, x86_pop(scp, &x86));
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

    case 0xc4: {		/* les */
      int len = _instr_len(orig_csp, x86._32bit);
      _eip += len;
      assert(len > 0);
      lxx_patch(scp, MEM_BASE32(cs + eip), len, x86._32bit);
      ret = es_INDEX;
      break;
    }

    case 0xc5: {		/* lds */
      int len = _instr_len(orig_csp, x86._32bit);
      _eip += len;
      assert(len > 0);
      lxx_patch(scp, MEM_BASE32(cs + eip), len, x86._32bit);
      ret = ds_INDEX;
      break;
    }

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
	  error("LSS unsopported\n");
	  ret = ss_INDEX;
	  break;

	case 0xb4: {	/* lfs */
	  int len = _instr_len(orig_csp, x86._32bit);
	  _eip += len;
	  assert(len > 1);
	  lxx_patch(scp, MEM_BASE32(cs + eip), len - 1, x86._32bit);
	  ret = fs_INDEX;
	  break;
	}

	case 0xb5: {	/* lgs */
	  int len = _instr_len(orig_csp, x86._32bit);
	  _eip += len;
	  assert(len > 1);
	  lxx_patch(scp, MEM_BASE32(cs + eip), len - 1, x86._32bit);
	  ret = gs_INDEX;
	  break;
	}
      }
      break;
  }

  return ret;
}

uint16_t decode_selector(cpuctx_t *scp)
{
    unsigned cs;
    int pfx;
    x86_ins x86;

    x86._32bit = dpmi_segment_is32(_cs);
    cs = GetSegmentBase(_cs);
    pfx = x86_handle_prefixes(scp, cs, &x86);
    if (!pfx)
	return _ds;	// may be also _ss
#define RS(s) \
    if (x86.s) \
	return _##s
    RS(cs);
    RS(ds);
    RS(es);
    RS(ss);
    RS(gs);
    RS(fs);
    return _ds;
}

static uint8_t reg8(cpuctx_t *scp, int reg)
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

static uint32_t reg(cpuctx_t *scp, int reg)
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

int decode_memop(cpuctx_t *scp, uint32_t *op, dosaddr_t cr2)
{
    unsigned cs, eip, seg_base;
    unsigned char *csp, *orig_csp;
    x86_ins x86;
    int inst_len, loop_inc, ret = 0;

    x86._32bit = dpmi_segment_is32(_cs);
    cs = GetSegmentBase(_cs);
    eip = _eip + x86_handle_prefixes(scp, cs, &x86);
    csp = (unsigned char *)MEM_BASE32(cs + eip);
    orig_csp = (unsigned char *)MEM_BASE32(cs + _eip);
    inst_len = _instr_len(orig_csp, x86._32bit);
    loop_inc = (_eflags & DF) ? -1 : 1;
    if (x86.rep) {
	int cnt = 0;
	switch (x86.address_size) {
	case 2:
	    cnt = _LWORD(ecx);
	    break;
	case 4:
	    cnt = _ecx;
	    break;
	}
	if (!cnt) {
	    _eip += inst_len;
	    return 0;
	}
	cnt--;
	switch (x86.address_size) {
	case 2:
	    _LWORD(ecx) = cnt;
	    break;
	case 4:
	    _ecx = cnt;
	    break;
	}
	if (cnt > 0)
	    inst_len = 0;
    }
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
	*op = instr_binary_byte(csp[1] >> 3, read_byte(cr2),
		orig_csp[inst_len - 1], (unsigned*)&_eflags);
	ret = 1;
	break;

    case 0x81:		/* logical r/m,imm */
	switch (x86.operand_size) {
	case 2:
	    *op = instr_binary_word(csp[1] >> 3, read_word(cr2),
		    *(uint16_t *)(orig_csp + inst_len - 2), (unsigned*)&_eflags);
	    ret = 2;
	    break;
	case 4:
	    *op = instr_binary_dword(csp[1] >> 3, read_dword(cr2),
		    *(uint32_t *)(orig_csp + inst_len - 4), (unsigned*)&_eflags);
	    ret = 4;
	    break;
	}
	break;

    case 0x83:		/* logical r/m,imm8 */
	switch (x86.operand_size) {
	case 2:
	    *op = instr_binary_word(csp[1] >> 3, read_word(cr2),
		    (short)*(signed char *)(orig_csp + inst_len - 1), (unsigned*)&_eflags);
	    ret = 2;
	    break;
	case 4:
	    *op = instr_binary_dword(csp[1] >> 3, read_dword(cr2),
		    (int)*(signed char *)(orig_csp + inst_len - 1), (unsigned*)&_eflags);
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
	*op = instr_binary_byte(csp[0] >> 3, read_byte(cr2),
		reg8(scp, csp[1] >> 3), (unsigned*)&_eflags);
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
	    *op = instr_binary_word(csp[0] >> 3, read_word(cr2),
		    reg(scp, csp[1] >> 3), (unsigned*)&_eflags);
	    ret = 2;
	    break;
	case 4:
	    *op = instr_binary_dword(csp[0] >> 3, read_dword(cr2),
		    reg(scp, csp[1] >> 3), (unsigned*)&_eflags);
	    ret = 4;
	    break;
	}
	break;

    case 0xfe: /* inc/dec mem */
	*op = read_byte(cr2);
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
		    *op = read_word(cr2);
		    ret = 2;
		    break;
		case 4:
		    *op = read_dword(cr2);
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
	    return -1;
	}
	break;

    default:
	error("Unimplemented memop decode %#x\n", *csp);
	return -1;
  }

  assert(ret);
  assert(inst_len || x86.rep);
  _eip += inst_len;
  return ret;
}

void instrdec_init(void)
{
  patch_cs = AllocateDescriptors(1);
}

void instrdec_done(void)
{
  FreeDescriptor(patch_cs);
}
