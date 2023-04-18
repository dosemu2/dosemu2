/*
 * (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Emulates most machine code instructions for VGAEmu.
 * Some ideas were taken from Bochs (see www.bochs.com).
 *
 * The emulator stays in emulation until either we have a pending
 * signal, or if COUNT instructions do not access the VGA memory, or
 * the instruction is not known.  In future this may be merged with
 * cpuemu.
 *
 * Note that this emulation short-circuits "IN" and "OUT" port reads and
 * and writes, but only if these refer to the standard vga ports,
 * that is, if VGA_emulate_outb() and VGA_emulate_inb() can handle
 * them.
 *
 * The emulator has some sort of a RISC structure: for each read or write
 * it is checked whether it refers to video memory, and if that is the case
 * we call Logical_VGA_read() or Logical_VGA_write(), respectively.
 * These functions are in vgaemu.c.
 *
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 2000/05/18: Moved instr_sim and friends from vgaemu.c (Bart)
 *
 * Updated to remove MASTERCOUNT option and get out of the emulator
 * if we have a signal pending.
 * --EB 27 May 2000
 *
 * 2000/06/01: Over the last weeks: added many 32-bit instructions,
 * improved speed and cleaned up the source. (Bart)
 *
 * DANG_END_CHANGELOG
 *
 */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "emu.h"
#include "emudpmi.h"
#include "cpu.h"
#include "dos2linux.h"
#include "instremu.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the Graphics Controller.
 * 0 - normal / 1 - useful / 2 - too much
 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define DEBUG_INSTR	0	/* (<= 2) instruction emulation */

#define instr_msg(x...) v_printf("instremu: " x)

#define instr_deb(x...) v_printf("instremu: " x)

#if DEBUG_INSTR >= 2
#define instr_deb2(x...) v_printf("instremu: " x)
#else
#define instr_deb2(x...)
#endif

#if DEBUG_INSTR >= 1
static const char *seg_txt[7] = { "", "es: ", "cs: ", "ss: ", "ds: ", "fs: ", "gs: " };
static const char *rep_txt[3] = { "", "repnz ", "repz " };
static const char *lock_txt[2] = { "", "lock " };
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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned seg, lock, rep;
static unsigned arg_len(unsigned char *, int);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * DANG_BEGIN_FUNCTION instr_len
 *
 * Returns the length of an instruction; 0 if it could not
 * be determined.
 *
 * DANG_END_FUNCTION
 *
 */

int instr_len(unsigned char *p, int is_32)
{
  unsigned u, osp, asp;
  unsigned char *p0 = p;
#if DEBUG_INSTR >= 1
  unsigned char *p1 = p;
#endif

  seg = lock = rep = 0;
  osp = asp = is_32;

  for(u = 1; u && p - p0 < 17;) switch(*p++) {		/* get prefixes */
    case 0x26:	/* es: */
      seg = 1; break;
    case 0x2e:	/* cs: */
      seg = 2; break;
    case 0x36:	/* ss: */
      seg = 3; break;
    case 0x3e:	/* ds: */
      seg = 4; break;
    case 0x64:	/* fs: */
      seg = 5; break;
    case 0x65:	/* gs: */
      seg = 6; break;
    case 0x66:	/* operand size */
      osp ^= 1; break;
    case 0x67:	/* address size */
      asp ^= 1; break;
    case 0xf0:	/* lock */
      lock = 1; break;
    case 0xf2:	/* repnz */
      rep = 2; break;
    case 0xf3:	/* rep(z) */
      rep = 1; break;
    default:	/* no prefix */
      u = 0;
  }
  p--;

#if DEBUG_INSTR >= 1
  p1 = p;
#endif

  if(p - p0 >= 16) return 0;

  if(*p == 0x0f) {
    p++;
    switch (*p) {
    case 0x80 ... 0x8f:
      p += osp ? 5 : 3;
      return p - p0;
    case 0xa4:
      p++;
      p += (u = arg_len(p, asp));
      if(!u) p = p0;
      return p + 1 - p0;
    case 0xba:
      p += 4;
      return p - p0;
    case 0xa5:
    case 0xb6:
    case 0xb7:
    case 0xbe:
    case 0xbf:
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
