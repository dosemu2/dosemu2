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

#define R_LO(a) LO_BYTE_d(a)
#define R_HI(a) HI_BYTE_d(a)
#define R_WORD(a) LO_WORD(a)
#define R_DWORD(a) (*((unsigned *) &(a)))
#define AL (R_LO(x86->eax))
#define AH (R_HI(x86->eax))
#define AX (R_WORD(x86->eax))
#define BL (R_LO(x86->ebx))
#define BH (R_HI(x86->ebx))
#define BX (R_WORD(x86->ebx))
#define CL (R_LO(x86->ecx))
#define CH (R_HI(x86->ecx))
#define CX (R_WORD(x86->ecx))
#define DL (R_LO(x86->edx))
#define DH (R_HI(x86->edx))
#define DX (R_WORD(x86->edx))
#define SI (R_WORD(x86->esi))
#define DI (R_WORD(x86->edi))
#define SP (R_WORD(x86->esp))
#define BP (R_WORD(x86->ebp))
#define EFLAGS (R_DWORD(x86->eflags))
#define FLAGS (R_WORD(EFLAGS))

#define instr_msg(x...) v_printf("instremu: " x)

#define instr_deb(x...) v_printf("instremu: " x)

#if DEBUG_INSTR >= 2
#define instr_deb2(x...) v_printf("instremu: " x)
#else
#define instr_deb2(x...)
#endif

enum {REPNZ = 0, REPZ = 1, REP_NONE = 2};

struct rm {
  unsigned char *r;
  dosaddr_t m;
};

typedef struct x86_regs {
  unsigned eax, ecx, edx, ebx, esp, ebp, esi, edi;
  /* this sequence is important because this is the cpu's order and thus
     gives us an optimization */
  unsigned eip;
  unsigned eflags;
  unsigned es, cs, ss, ds, fs, gs;
  unsigned cs_base, ds_base, es_base, ss_base, fs_base, gs_base;
  unsigned seg_base, seg_ss_base;
  unsigned _32bit:1;	/* 16/32 bit code */
  unsigned address_size; /* in bytes so either 4 or 2 */
  unsigned operand_size;
  unsigned prefixes, rep;
  unsigned (*instr_read)(struct rm rm);
  void (*instr_write)(struct rm rm, unsigned u);
  struct rm (*modrm)(unsigned char *cp, struct x86_regs *x86, int *inst_len);
} x86_regs;

#if DEBUG_INSTR >= 1
static char *seg_txt[7] = { "", "es: ", "cs: ", "ss: ", "ds: ", "fs: ", "gs: " };
static char *rep_txt[3] = { "", "repnz ", "repz " };
static char *lock_txt[2] = { "", "lock " };
#endif

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

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned seg, lock, rep;
#define vga_base vga.mem.bank_base
#define vga_end (vga_base + vga.mem.bank_len)

static unsigned arg_len(unsigned char *, int);

static unsigned instr_read_word(struct rm rm);
static unsigned instr_read_dword(struct rm rm);
static void instr_write_word(struct rm rm, unsigned u);
static void instr_write_dword(struct rm rm, unsigned u);
static dosaddr_t sib(unsigned char *cp, x86_regs *x86, int *inst_len);
static struct rm modrm32(unsigned char *cp, x86_regs *x86, int *inst_len);
static struct rm modrm16(unsigned char *cp, x86_regs *x86, int *inst_len);

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

/*
 * Some functions to make using the vga emulation easier.
 *
 *
 */

unsigned instr_read_word(struct rm rm)
{
  unsigned u;

  if (rm.r) {
    memcpy(&u, rm.r, 2);
    return u;
  }

  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here
   */
  u = read_word(rm.m);

#if DEBUG_INSTR >= 2
  instr_deb2("Read word 0x%x", u);
  if (addr<0x8000000) v_printf(" from address %x\n", addr); else v_printf("\n");
#endif
  return u;
}

unsigned instr_read_dword(struct rm rm)
{
  unsigned u;

  if (rm.r) {
    memcpy(&u, rm.r, 4);
    return u;
  }

  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here
   */
  u = read_dword(rm.m);

#if DEBUG_INSTR >= 2
  instr_deb2("Read word 0x%x", u);
  if (addr<0x8000000) v_printf(" from address %x\n", addr); else v_printf("\n");
#endif
  return u;
}

void instr_write_word(struct rm rm, unsigned u)
{
  if (rm.r) {
    memcpy(rm.r, &u, 2);
    return;
  }

  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here.
   * we assume application do not try to mix here
   */

  write_word(rm.m, u);

#if DEBUG_INSTR >= 2
  instr_deb2("Write word 0x%x", u);
  if (dst<0x8000000) v_printf(" at address %x\n", dst); else v_printf("\n");
#endif
}

void instr_write_dword(struct rm rm, unsigned u)
{
  if (rm.r) {
    memcpy(rm.r, &u, 4);
    return;
  }

  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here.
   * we assume application do not try to mix here
   */

  write_dword(rm.m, u);

#if DEBUG_INSTR >= 2
  instr_deb2("Write word 0x%x", u);
  if (dst<0x8000000) v_printf(" at address %x\n", dst); else v_printf("\n");
#endif
}

static inline void pop(unsigned *val, x86_regs *x86)
{
  struct rm mem = {};

  mem.m = x86->ss_base + (x86->esp & wordmask[(x86->_32bit+1)*2]);
  if (x86->_32bit)
    x86->esp += x86->operand_size;
  else
    LO_WORD(x86->esp) += x86->operand_size;
  *val = (x86->operand_size == 4 ? instr_read_dword(mem) : instr_read_word(mem));
}

/* helper functions/macros reg8/reg/sreg/sib/modrm16/32 for instr_sim
   for address and register decoding */

#define reg8(reg, x86) (((unsigned char *)(x86))+((reg)&0x3)*4+(((reg)>>2)&1))
#define reg(reg, x86) ((&(x86)->eax)+((reg)&0x7))
#define sreg(reg, x86) ((&((x86)->es))+((reg)&0x7))
#define sreg_idx(reg) (es_INDEX+((reg)&0x7))

dosaddr_t sib(unsigned char *cp, x86_regs *x86, int *inst_len)
{
  unsigned addr = 0;

  switch(cp[1] & 0xc0) { /* decode modifier */
  case 0x40:
    addr = (int)(signed char)cp[3];
    break;
  case 0x80:
    addr = R_DWORD(cp[3]);
    break;
  }

  if ((cp[2] & 0x38) != 0x20) /* index cannot be esp */
    addr += *reg(cp[2]>>3, x86) << (cp[2] >> 6);

  switch(cp[2] & 0x07) { /* decode address */
  case 0x00:
  case 0x01:
  case 0x02:
  case 0x03:
  case 0x06:
  case 0x07:
    return (addr + *reg(cp[2], x86) + x86->seg_base);
  case 0x04: /* esp */
    return (addr + x86->esp + x86->seg_ss_base);
  case 0x05:
    if (cp[1] >= 0x40)
      return (addr + x86->ebp + x86->seg_ss_base);
    else {
      *inst_len += 4;
      return (addr + R_DWORD(cp[3]) + x86->seg_base);
    }
  }
  return 0; /* keep gcc happy */
}

struct rm modrm16(unsigned char *cp, x86_regs *x86, int *inst_len)
{
  unsigned addr = 0;
  struct rm rm = {};
  *inst_len = 0;

  switch(cp[1] & 0xc0) { /* decode modifier */
  case 0x40:
    addr = (short)(signed char)cp[2];
    *inst_len = 1;
    break;
  case 0x80:
    addr = R_WORD(cp[2]);
    *inst_len = 2;
    break;
  case 0xc0:
    if (cp[0]&1) /*(d)word*/
      rm.r = (unsigned char *)reg(cp[1], x86);
    else
      rm.r = reg8(cp[1], x86);
    return rm;
  }


  switch(cp[1] & 0x07) { /* decode address */
  case 0x00:
    rm.m = (((addr + x86->ebx + x86->esi) & 0xffff) + x86->seg_base);
    break;
  case 0x01:
    rm.m = (((addr + x86->ebx + x86->edi) & 0xffff) + x86->seg_base);
    break;
  case 0x02:
    rm.m = (((addr + x86->ebp + x86->esi) & 0xffff) + x86->seg_ss_base);
    break;
  case 0x03:
    rm.m = (((addr + x86->ebp + x86->edi) & 0xffff) + x86->seg_ss_base);
    break;
  case 0x04:
    rm.m = (((addr + x86->esi) & 0xffff) + x86->seg_base);
    break;
  case 0x05:
    rm.m = (((addr + x86->edi) & 0xffff) + x86->seg_base);
    break;
  case 0x06:
    if (cp[1] >= 0x40)
      rm.m = (((addr + x86->ebp) & 0xffff) + x86->seg_ss_base);
    else {
      *inst_len += 2;
      rm.m = (R_WORD(cp[2]) + x86->seg_base);
    }
    break;
  case 0x07:
    rm.m = (((addr + x86->ebx) & 0xffff) + x86->seg_base);
    break;
  }
  return rm;
}

struct rm modrm32(unsigned char *cp, x86_regs *x86, int *inst_len)
{
  unsigned addr = 0;
  struct rm rm = {};
  *inst_len = 0;

  switch(cp[1] & 0xc0) { /* decode modifier */
  case 0x40:
    addr = (int)(signed char)cp[2];
    *inst_len = 1;
    break;
  case 0x80:
    addr = R_DWORD(cp[2]);
    *inst_len = 4;
    break;
  case 0xc0:
    if (cp[0]&1) /*(d)word*/
      rm.r = ((unsigned char *)reg(cp[1], x86));
    else
      rm.r = reg8(cp[1], x86);
    return rm;
  }

  switch(cp[1] & 0x07) { /* decode address */
  case 0x00:
  case 0x01:
  case 0x02:
  case 0x03:
  case 0x06:
  case 0x07:
    rm.m = (addr + *reg(cp[1], x86) + x86->seg_base);
    break;
  case 0x04: /* sib byte follows */
    *inst_len += 1;
    rm.m = sib(cp, x86, inst_len);
    break;
  case 0x05:
    if (cp[1] >= 0x40)
      rm.m = (addr + x86->ebp + x86->seg_ss_base);
    else {
      *inst_len += 4;
      rm.m = (R_DWORD(cp[2]) + x86->seg_base);
    }
    break;
  }
  return rm;
}

static int handle_prefixes(x86_regs *x86)
{
  unsigned eip = x86->eip;
  int prefix = 0;

  for (;; eip++) {
    switch(*(unsigned char *)MEM_BASE32(x86->cs_base + eip)) {
    /* handle (some) prefixes */
      case 0x26:
        prefix++;
        x86->seg_base = x86->seg_ss_base = x86->es_base;
        break;
      case 0x2e:
        prefix++;
        x86->seg_base = x86->seg_ss_base = x86->cs_base;
        break;
      case 0x36:
        prefix++;
        x86->seg_base = x86->seg_ss_base = x86->ss_base;
        break;
      case 0x3e:
        prefix++;
        x86->seg_base = x86->seg_ss_base = x86->ds_base;
        break;
      case 0x64:
        prefix++;
        x86->seg_base = x86->seg_ss_base = x86->fs_base;
        break;
      case 0x65:
        prefix++;
        x86->seg_base = x86->seg_ss_base = x86->gs_base;
        break;
      case 0x66:
        prefix++;
        x86->operand_size = 6 - x86->operand_size;
        if (x86->operand_size == 4) {
          x86->instr_read = instr_read_dword;
          x86->instr_write = instr_write_dword;
        } else {
          x86->instr_read = instr_read_word;
          x86->instr_write = instr_write_word;
        }
        break;
      case 0x67:
        prefix++;
        x86->address_size = 6 - x86->address_size;
        x86->modrm = (x86->address_size == 4 ? modrm32 : modrm16);
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

static void prepare_x86(x86_regs *x86)
{
  x86->seg_base = x86->ds_base;
  x86->seg_ss_base = x86->ss_base;
  x86->address_size = x86->operand_size = (x86->_32bit + 1) * 2;

  x86->modrm = (x86->address_size == 4 ? modrm32 : modrm16);
  x86->rep = REP_NONE;

  if (x86->operand_size == 4) {
    x86->instr_read = instr_read_dword;
    x86->instr_write = instr_write_dword;
  } else {
    x86->instr_read = instr_read_word;
    x86->instr_write = instr_write_word;
  }
}

#define M(a) (struct rm){.m = (a)}

static void scp_to_x86_regs(x86_regs *x86, cpuctx_t *scp, int pmode)
{
  if(pmode) {
    x86->eax = _eax;
    x86->ebx = _ebx;
    x86->ecx = _ecx;
    x86->edx = _edx;
    x86->esi = _esi;
    x86->edi = _edi;
    x86->ebp = _ebp;
    x86->esp = _esp;
    x86->eip = _eip;
    x86->eflags = _eflags;
    x86->cs = _cs;
    x86->ds = _ds;
    x86->es = _es;
    x86->ss = _ss;
    x86->fs = _fs;
    x86->gs = _gs;
    x86->cs_base = GetSegmentBase(_cs);
    x86->ds_base = GetSegmentBase(_ds);
    x86->es_base = GetSegmentBase(_es);
    x86->ss_base = GetSegmentBase(_ss);
    x86->fs_base = GetSegmentBase(_fs);
    x86->gs_base = GetSegmentBase(_gs);
    x86->_32bit = _cs && dpmi_segment_is32(_cs) ? 1 : 0;
  }
  else {
    x86->eax = REG(eax);
    x86->ebx = REG(ebx);
    x86->ecx = REG(ecx);
    x86->edx = REG(edx);
    x86->esi = REG(esi);
    x86->edi = REG(edi);
    x86->ebp = REG(ebp);
    x86->esp = REG(esp);
    x86->eip = REG(eip);
    x86->eflags = REG(eflags);
    x86->cs = SREG(cs);
    x86->ds = SREG(ds);
    x86->es = SREG(es);
    x86->ss = SREG(ss);
    x86->fs = SREG(fs);
    x86->gs = SREG(gs);
    x86->cs_base = SEGOFF2LINEAR(x86->cs, 0);
    x86->ds_base = SEGOFF2LINEAR(x86->ds, 0);
    x86->es_base = SEGOFF2LINEAR(x86->es, 0);
    x86->ss_base = SEGOFF2LINEAR(x86->ss, 0);
    x86->fs_base = SEGOFF2LINEAR(x86->fs, 0);
    x86->gs_base = SEGOFF2LINEAR(x86->gs, 0);
    x86->_32bit = 0;
  }
  prepare_x86(x86);
}

static void x86_regs_to_scp(x86_regs *x86, cpuctx_t *scp, int pmode)
{
  if(pmode) {
    _cs = x86->cs;
    _ds = x86->ds;
    _es = x86->es;
    _fs = x86->fs;
    _gs = x86->gs;
    _ss = x86->ss;
    _eax = x86->eax;
    _ebx = x86->ebx;
    _ecx = x86->ecx;
    _edx = x86->edx;
    _esi = x86->esi;
    _edi = x86->edi;
    _ebp = x86->ebp;
    _esp = x86->esp;
    _eip = x86->eip;
    _eflags = x86->eflags;
  }
  else {
    REG(eax) = x86->eax;
    REG(ebx) = x86->ebx;
    REG(ecx) = x86->ecx;
    REG(edx) = x86->edx;
    REG(esi) = x86->esi;
    REG(edi) = x86->edi;
    REG(ebp) = x86->ebp;
    REG(esp) = x86->esp;
    REG(eip) = x86->eip;
    REG(eflags) = x86->eflags;
  }
}

int decode_modify_segreg_insn(cpuctx_t *scp, int pmode,
    unsigned int *new_val)
{
  struct rm mem = {};
  unsigned cs;
  int inst_len, ret = -1;
  x86_regs x86;

  scp_to_x86_regs(&x86, scp, pmode);

  cs = x86.cs_base;
  x86.prefixes = handle_prefixes(&x86);
  x86.eip += x86.prefixes;

  switch(*(unsigned char *)MEM_BASE32(cs + x86.eip)) {
    case 0x8e:		/* mov segreg,r/m16 */
      ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3);
      mem = x86.modrm(MEM_BASE32(cs + x86.eip), &x86, &inst_len);
      if ((*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) & 0xc0) == 0xc0)  /* compensate for mov r,segreg */
        memcpy(new_val, reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1), &x86), 2);
      else
        *new_val = instr_read_word(mem);
      x86.eip += inst_len + 2;
      break;

    case 0xca: /*retf imm 16*/
    case 0xcb: /*retf*/
    case 0xcf: /*iret*/
    {
      unsigned tmp_eip;
      pop(&tmp_eip, &x86);
      pop(new_val, &x86);
      ret = cs_INDEX;
      switch (*(unsigned char *)MEM_BASE32(cs + x86.eip)) {
        case 0xca: /*retf imm 16*/
	  x86.esp += ((unsigned short *) (MEM_BASE32(cs + x86.eip + 1)))[0];
	  break;
        case 0xcf: /*iret*/
	{
	  unsigned flags;
          pop(&flags, &x86);
	  x86.eflags = flags;
	  break;
	}
      }
      x86.eip = tmp_eip;
    }
    break;

    case 0xea:			/* jmp seg:off16/off32 */
    {
      unsigned tmp_eip;
      tmp_eip = x86.instr_read(M(x86.cs_base + x86.eip + 1));
      *new_val = instr_read_word(M(x86.cs_base + x86.eip + 1 + x86.operand_size));
      ret = cs_INDEX;
      x86.eip = tmp_eip;
    }
    break;

    case 0xc4:		/* les */
      mem = x86.modrm(MEM_BASE32(cs + x86.eip), &x86, &inst_len);
      *new_val = instr_read_word(M(mem.m+x86.operand_size));
      if (x86.operand_size == 2)
	R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86)) = instr_read_word(mem);
      else
	*reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86) = instr_read_dword(mem);
      ret = es_INDEX;
      x86.eip += inst_len + 2;
      break;

    case 0xc5:		/* lds */
      mem = x86.modrm(MEM_BASE32(cs + x86.eip), &x86, &inst_len);
      *new_val = instr_read_word(M(mem.m+x86.operand_size));
      if (x86.operand_size == 2)
	R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86)) = instr_read_word(mem);
      else
	*reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86) = instr_read_dword(mem);
      ret = ds_INDEX;
      x86.eip += inst_len + 2;
      break;

    case 0x07:	/* pop es */
    case 0x17:	/* pop ss */
    case 0x1f:	/* pop ds */
      ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + x86.eip) >> 3);
      pop(new_val, &x86);
      x86.eip++;
      break;

    case 0x0f:
      x86.eip++;
      switch (*(unsigned char *)MEM_BASE32(cs + x86.eip)) {
        case 0xa1:	/* pop fs */
        case 0xa9:	/* pop gs */
	  pop(new_val, &x86);
	  ret = sreg_idx(*(unsigned char *)MEM_BASE32(cs + x86.eip) >> 3);
	  x86.eip++;
	  break;

	case 0xb2:	/* lss */
	  mem = x86.modrm(MEM_BASE32(cs + x86.eip), &x86, &inst_len);
	  *new_val = instr_read_word(M(mem.m+x86.operand_size));
	  if (x86.operand_size == 2)
	    R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86)) = instr_read_word(mem);
	  else
	    *reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86) = instr_read_dword(mem);
	  ret = ss_INDEX;
	  x86.eip += inst_len + 2;
	  break;

	case 0xb4:	/* lfs */
	  mem = x86.modrm(MEM_BASE32(cs + x86.eip), &x86, &inst_len);
	  *new_val = instr_read_word(M(mem.m+x86.operand_size));
	  if (x86.operand_size == 2)
	    R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86)) = instr_read_word(mem);
	  else
	    *reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86) = instr_read_dword(mem);
	  ret = fs_INDEX;
	  x86.eip += inst_len + 2;
	  break;

	case 0xb5:	/* lgs */
	  mem = x86.modrm(MEM_BASE32(cs + x86.eip), &x86, &inst_len);
	  *new_val = instr_read_word(M(mem.m+x86.operand_size));
	  if (x86.operand_size == 2)
	    R_WORD(*reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86)) = instr_read_word(mem);
	  else
	    *reg(*(unsigned char *)MEM_BASE32(cs + x86.eip + 1) >> 3, &x86) = instr_read_dword(mem);
	  ret = gs_INDEX;
	  x86.eip += inst_len + 2;
	  break;
      }
      break;
  }

  x86_regs_to_scp(&x86, scp, pmode);
  return ret;
}
