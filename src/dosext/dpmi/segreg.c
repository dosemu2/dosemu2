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
 *
 * Normally DPMI reserved some GDT entries for the API translator,
 * like 0x40, to span some important DOS areas. We can only allocate
 * LDTs, and therefore we replace the GDT selector with LDT selector
 * in the appropriate segreg.
 */

#include "cpu.h"
#include "dpmi.h"
#ifdef DJGPP_PORT
#include "wrapper.h"
#else
#include "dosemu_debug.h"
#endif
#include "segreg.h"

typedef struct x86_regs {
  unsigned _32bit:1;	/* 16/32 bit code */
  unsigned address_size; /* in bytes so either 4 or 2 */
  unsigned operand_size;
} x86_regs;

#define R_WORD(a) LO_WORD(a)
#define SP (R_WORD(_esp))
#define sreg_idx(reg) (es_INDEX+((reg)&0x7))

static unsigned wordmask[5] = {0,0xff,0xffff,0xffffff,0xffffffff};

static void pop(unsigned *val, struct sigcontext *scp, x86_regs *x86)
{
  unsigned ss_base = GetSegmentBase(_ss);
  unsigned char *mem = MEM_BASE32(ss_base + (_esp & wordmask[(x86->_32bit+1)*2]));
  if (x86->_32bit)
    _esp += x86->operand_size;
  else
    SP += x86->operand_size;
  if (val)
    *val = (x86->operand_size == 4 ? READ_DWORDP(mem) : READ_WORDP(mem));
}

static int handle_prefixes(struct sigcontext *scp, unsigned cs_base,
	x86_regs *x86)
{
  unsigned eip = _eip;
  int prefix = 0;

  for (;; eip++) {
    switch(*(unsigned char *)MEM_BASE32(cs_base + eip)) {
    /* handle (some) prefixes */
      case 0x26:
        prefix++;
//        x86->seg_base = x86->seg_ss_base = x86->es_base;
        break;
      case 0x2e:
        prefix++;
//        x86->seg_base = x86->seg_ss_base = x86->cs_base;
        break;
      case 0x36:
        prefix++;
//        x86->seg_base = x86->seg_ss_base = x86->ss_base;
        break;
      case 0x3e:
        prefix++;
//        x86->seg_base = x86->seg_ss_base = x86->ds_base;
        break;
      case 0x64:
        prefix++;
//        x86->seg_base = x86->seg_ss_base = x86->fs_base;
        break;
      case 0x65:
        prefix++;
//        x86->seg_base = x86->seg_ss_base = x86->gs_base;
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
//        x86->rep = REPNZ;
        break;
      case 0xf3:
        prefix++;
//        x86->rep = REPZ;
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

  instr_deb2("arg_len: %02x %02x %02x %02x: %u bytes\n", p[0], p[1], p[2], p[3], u);

  return u;
}

static unsigned instr_len(unsigned char *p, int is_32)
{
  unsigned u, osp, asp;
  unsigned char *p0 = p;
#if DEBUG_INSTR >= 1
  int seg, lock, rep;
  unsigned char *p1 = p;
#endif

#if DEBUG_INSTR >= 1
  seg = lock = rep = 0;
#endif
  osp = asp = is_32;

  for(u = 1; u && p - p0 < 17;) switch(*p++) {		/* get prefixes */
    case 0x26:	/* es: */
#if DEBUG_INSTR >= 1
      seg = 1;
#endif
      break;
    case 0x2e:	/* cs: */
#if DEBUG_INSTR >= 1
      seg = 2;
#endif
      break;
    case 0x36:	/* ss: */
#if DEBUG_INSTR >= 1
      seg = 3;
#endif
      break;
    case 0x3e:	/* ds: */
#if DEBUG_INSTR >= 1
      seg = 4;
#endif
      break;
    case 0x64:	/* fs: */
#if DEBUG_INSTR >= 1
      seg = 5;
#endif
      break;
    case 0x65:	/* gs: */
#if DEBUG_INSTR >= 1
      seg = 6;
#endif
      break;
    case 0x66:	/* operand size */
#if DEBUG_INSTR >= 1
      osp ^= 1;
#endif
      break;
    case 0x67:	/* address size */
#if DEBUG_INSTR >= 1
      asp ^= 1;
#endif
      break;
    case 0xf0:	/* lock */
#if DEBUG_INSTR >= 1
      lock = 1;
#endif
      break;
    case 0xf2:	/* repnz */
#if DEBUG_INSTR >= 1
      rep = 2;
#endif
      break;
    case 0xf3:	/* rep(z) */
#if DEBUG_INSTR >= 1
      rep = 1;
#endif
      break;
    default:	/* no prefix */
      u = 0;
  }
  p--;

#if DEBUG_INSTR >= 1
  p1 = p;
#endif

  if(p - p0 >= 16) return 0;

  if(*p == 0x0f) {
    /* not yet */
    error("msdos: unsupported instr_len %x %x\n", p[0], p[1]);
    return 0;
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

#if DEBUG_INSTR >= 1
  if(p >= p0) {
    instr_deb("instr_len: instr = ");
    v_printf("%s%s%s%s%s",
      osp ? "osp " : "", asp ? "asp " : "",
      lock_txt[lock], rep_txt[rep], seg_txt[seg]
    );
    if(p > p1) for(u = 0; u < p - p1; u++) {
      v_printf("%02x ", p1[u]);
    }
    v_printf("\n");
  }
#endif

  return p - p0;
}

int decode_segreg(struct sigcontext *scp)
{
  unsigned cs, eip;
  unsigned char *csp;
  int ret = -1;
  x86_regs x86;

  x86._32bit = dpmi_mhp_get_selector_size(_cs);
  x86.address_size = x86.operand_size = (x86._32bit + 1) * 2;
  cs = GetSegmentBase(_cs);
  csp = (unsigned char *)MEM_BASE32(cs + _eip);
  eip = _eip + handle_prefixes(scp, cs, &x86);

  switch(*csp) {
    case 0x8e:		/* mov segreg,r/m16 */
      ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + eip + 1) >> 3);
      _eip += instr_len(csp, x86._32bit);
      break;

    case 0xca: /*retf imm 16*/
    case 0xcb: /*retf*/
    case 0xcf: /*iret*/
    {
      unsigned tmp_eip;
      pop(&tmp_eip, scp, &x86);
      pop(NULL, scp, &x86);
      ret = cs_INDEX;
      switch (*(unsigned char *)MEM_BASE32(cs + eip)) {
        case 0xca: /*retf imm 16*/
	  _esp += ((unsigned short *) (MEM_BASE32(cs + eip + 1)))[0];
	  break;
        case 0xcf: /*iret*/
	{
	  unsigned flags;
          pop(&flags, scp, &x86);
	  scp->eflags = flags;
	  break;
	}
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
      _eip += instr_len(csp, x86._32bit);
      break;

    case 0xc5:		/* lds */
      ret = ds_INDEX;
      _eip += instr_len(csp, x86._32bit);
      break;

    case 0x07:	/* pop es */
    case 0x17:	/* pop ss */
    case 0x1f:	/* pop ds */
      ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + eip) >> 3);
      pop(NULL, scp, &x86);
      _eip = eip + 1;
      break;

    case 0x0f:
      eip++;
      switch (*(unsigned char *)MEM_BASE32(cs + eip)) {
        case 0xa1:	/* pop fs */
        case 0xa9:	/* pop gs */
	  pop(NULL, scp, &x86);
	  ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + eip) >> 3);
	  _eip = eip + 1;
	  break;

	case 0xb2:	/* lss */
	  ret = ss_INDEX;
	  _eip += instr_len(csp, x86._32bit);
	  break;

	case 0xb4:	/* lfs */
	  ret = fs_INDEX;
	  _eip += instr_len(csp, x86._32bit);
	  break;

	case 0xb5:	/* lgs */
	  ret = gs_INDEX;
	  _eip += instr_len(csp, x86._32bit);
	  break;
      }
      break;
  }

  return ret;
}
