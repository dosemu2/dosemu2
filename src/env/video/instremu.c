/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * DANG_BEGIN_MODULE
 *
 * REMARK
 * Emulates some machine code instructions for VGAEmu.
 * /REMARK
 * DANG_END_MODULE
 *
 * DANG_BEGIN_CHANGELOG
 *
 * 2000/05/18: Moved instr_sim and friends from vgaemu.c (Bart)
 *
 * DANG_END_CHANGELOG
 *
 */

/* Notes:
 *
 * Some formulas and the parity check table were taken from Bochs
 * (see www.bochs.com).
 *
 * The emulator stays in emulation for a maximum of MASTERCOUNT
 * instructions, but gets out of it if COUNT instructions do not
 * access the VGA memory, or the instruction is not known. 
 * In future this may be merged with cpuemu.
 * 2000/05/22 Bart.Oldeman@bris.ac.uk
 */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#include "config.h"
#include "emu.h"
#include "vgaemu.h"
#include "dpmi.h"
#include "cpu.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * some configurable options
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Debug level for the Graphics Controller.
 * 0 - normal / 1 - useful / 2 - too much
 */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
#define DEBUG_INSTR	0	/* (<= 2) instruction emulation */
#define R_LO(a) (((unsigned char *) &(a))[0])
#define R_HI(a) (((unsigned char *) &(a))[1])
#define R_WORD(a) (*((unsigned short *) &(a)))
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
#define EFLAGS (x86->eflags)
#define FLAGS (R_WORD(EFLAGS))

#if !defined True
#define False 0
#define True 1
#endif

#define COUNT  25         /* bail out when this #instructions were simulated
                               after a VGA r/w access */			       
#ifdef _EVIL_
#define MASTERCOUNT 1000000 /* maximal #instructions to simulate */
#else
#define MASTERCOUNT COUNT
#endif

#define instr_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_INSTR >= 1
#define instr_deb(x...) if (DEBUG_INST > 0) v_printf("VGAEmu: " x)
#else
#define instr_deb(x...)
#endif

#if DEBUG_INSTR >= 2
#define instr_deb2(x...) v_printf("VGAEmu: " x)
#else
#define instr_deb2(x...)
#endif

typedef struct {
  unsigned eax, ebx, ecx, edx, esi, edi, ebp, esp;
  unsigned eip, eflags;
  unsigned cs, ds, es, ss, fs, gs;
  unsigned cs_base, ds_base, es_base, ss_base, fs_base, gs_base;
  unsigned seg_base, seg_ss_base;
  unsigned _32bit:1;	/* 16/32 bit code */
} x86_regs;

#if DEBUG_INSTR >= 1
static char *seg_txt[7] = { "", "es: ", "cs: ", "ss: ", "ds: ", "fs: ", "gs: " };
static char *rep_txt[3] = { "", "repnz ", "repz " };
static char *lock_txt[2] = { "", "lock " };
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

/* This array defines a look-up table for the even parity-ness
   of an 8bit quantity, for optimal assignment of the parity bit
   in the EFLAGS register */
static char parity_lookup[256] = {
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF,
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF,
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  0,PF,PF, 0,PF, 0, 0,PF,PF, 0, 0,PF, 0,PF,PF, 0,
  PF, 0, 0,PF, 0,PF,PF, 0, 0,PF,PF, 0,PF, 0, 0,PF
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
static unsigned seg, osp, asp, lock, rep;
static int count, mastercount;
static unsigned arg_len(unsigned char *);

/* from dosext/dpmi/dpmi.c */
unsigned long dpmi_GetSegmentBaseAddress(unsigned short);
static unsigned char instr_read_byte(unsigned addr);
static unsigned short instr_read_word(unsigned addr);
static void instr_write_byte(unsigned addr, unsigned char u);
static void instr_write_word(unsigned addr, unsigned short u);
// static void instr_move_dword(unsigned dst, unsigned src);
static void instr_flags(unsigned val, unsigned smask, unsigned *eflags);
static int instr_inc_dec(int op, int result, unsigned *eflags);
static unsigned instr_binary(unsigned op, unsigned op1, unsigned op2, unsigned width, unsigned *eflags);
static unsigned instr_shift(unsigned op, int op1, unsigned op2, unsigned width, unsigned *eflags);
static unsigned modm(unsigned char *cp, x86_regs *x86, int *inst_len);
static unsigned char *reg8(unsigned char reg, x86_regs *x86);
static unsigned short *reg16(unsigned char reg, x86_regs *x86);
static unsigned short sreg(unsigned char reg, x86_regs *x86);

#if DEBUG_INSTR >= 1
static void dump_x86_regs(x86_regs *x86)
{
  instr_deb(
    "eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x ebp=%08x esp=%08x\n",
    x86->eax, x86->ebx, x86->ecx, x86->edx, x86->esi, x86->edi, x86-> ebp, x86->esp
    );
  instr_deb(
    "eip=%08x cs=%04x/%08x ds=%04x/%08x es=%04x/%08x d=%u c=%u p=%u a=%u z=%u s=%u o=%u\n",
    x86->eip, x86->cs, x86->cs_base, x86->ds, x86->ds_base, x86->es, x86->es_base,
    (EFLAGS&DF)>>10,
    EFLAGS&CF,(EFLAGS&PF)>>2,(EFLAGS&AF)>>4,
    (EFLAGS&ZF)>>6,(EFLAGS&SF)>>7,(EFLAGS&OF)>>11
    );
}
#endif


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

unsigned instr_len(unsigned char *p)
{
  unsigned u;
  unsigned char *p0 = p, *p1 = p;

  seg = osp = asp = lock = rep = 0;

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

  p1 = p;

  if(p - p0 >= 16) return 0;

  if(*p == 0x0f) {
    /* not yet */
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
      p += (u = arg_len(p));
      if(!u) p = p0;
      break;

    case 8:	/* op-code + mod + ... + byte */
      p++;
      p += (u = arg_len(p)) + 1;
      if(!u) p = p0;
      break;

    case 9:	/* op-code + mod + ... + word/dword */
      p++;
      p += (u = arg_len(p)) + (osp ? 4 : 2);
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


unsigned arg_len(unsigned char *p)
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

unsigned char instr_read_byte(unsigned addr)
{
  int i = (addr >> 12) - vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page;
  unsigned char u;

  if(i >= 0 && i <= vga.mem.map[VGAEMU_MAP_BANK_MODE].pages) {
    count = COUNT;
    u = Logical_VGA_read(addr - (vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12));
  }
  else {
    u = *(unsigned char *) addr;
  }
  instr_deb("Read byte 0x%x\n", u);
  
  return u;
}

void instr_write_byte(unsigned addr, unsigned char u)
{
  int i = (addr >> 12) - vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page;

  if(i >= 0 && i <= vga.mem.map[VGAEMU_MAP_BANK_MODE].pages) {
    count = COUNT;
    Logical_VGA_write(addr - (vga.mem.map[VGAEMU_MAP_BANK_MODE].base_page << 12), u);
  }
  else {
    *(unsigned char *) addr = u;
  }
  instr_deb ("Write byte 0x%x\n", u);
}

void instr_write_word(unsigned dst, unsigned short u)
{
  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here
   */
  instr_write_byte(dst, R_LO(u));
  instr_write_byte(dst+1, R_HI(u));
}

unsigned short instr_read_word(unsigned addr)
{
  unsigned short u;

  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here
   */
  R_LO(u) = instr_read_byte(addr);
  R_HI(u) = instr_read_byte(addr+1);

  return u;
}

#if 0		// still unused...
void instr_move_dword(unsigned dst, unsigned src)
{
  unsigned u;

  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here
   */
  ((unsigned char *) &u)[0] = instr_move_byte(dst, src);
  ((unsigned char *) &u)[1] = instr_move_byte(dst + 1, src + 1);
  ((unsigned char *) &u)[2] = instr_move_byte(dst + 2, src + 2);
  ((unsigned char *) &u)[3] = instr_move_byte(dst + 3, src + 3);
}
#endif

void instr_flags(unsigned val, unsigned smask, unsigned *eflags)
{
  *eflags &= ~(OF|ZF|SF|PF|CF);
  if (val & smask)
    *eflags |= SF;
  else if (val == 0) 
    *eflags |= ZF;
  *eflags |= parity_lookup[val&0xff];
}

int instr_inc_dec(int op, int result, unsigned *eflags)
{
  *eflags &= ~(OF|ZF|SF|PF|AF);
  if (result < 0) 
    *eflags |= SF;
  else if (result == 0) 
    *eflags |= ZF;
  if ((op<0 && result>0) || (op>0 && result<0))
    *eflags |= OF;
  *eflags |= ((op^result) & AF) | parity_lookup[result&0xff];
  return result;
}

unsigned instr_binary(unsigned op, unsigned op1, unsigned op2, unsigned width, unsigned *eflags)
{
  unsigned mask = (1 << width) - 1;
  unsigned smask = 1 << (width-1);
  unsigned result;
        
  switch (op&0x7){
  case 1: /* or */
    result = op1 | op2;
    instr_flags(result, smask, eflags);
    return result;
  case 4: /* and */
    result = op1 & op2;
    instr_flags(result, smask, eflags);
    return result;
  case 6: /* xor */
    result = op1 ^ op2;
    instr_flags(result, smask, eflags);
    return result;
  case 0: /* add */
    *eflags &= ~CF; /* Fall through */
  case 2: /* adc */
    *eflags &= ~(OF|ZF|SF|AF|PF);
    result = (op1 + op2 + (*eflags&CF)) & mask;
    if (result == 0) 
      *eflags |= ZF;
    else if (result & smask)
      *eflags |= SF;
    if (result < op1 || ((*eflags & CF) && (result == op1)))
      *eflags |= CF;
    else
      *eflags &= ~CF;
    if (((op1 & smask) == (op2 & smask)) && ((result & smask) != (op1 & smask)))
      *eflags |= OF;
    *eflags |= ((op1^op2^result)&AF) | parity_lookup[result&0xff];
    return result;
  case 5: /* sub */
  case 7: /* cmp */
    *eflags &= ~CF; /* Fall through */
  case 3: /* sbb */
    *eflags &= ~(OF|ZF|SF|AF|PF);
    result = (op1 - op2 - (*eflags&CF)) & mask;
    if (result == 0) 
      *eflags |= ZF;
    else if (result & smask)
      *eflags |= SF;
    if (op1 < result || ((*eflags&CF) && op2==mask))
      *eflags |= CF;
    else
      *eflags &= ~CF;
    if ((op1&smask)!=(op2&smask) && (op1&smask)!=(result&smask))
      *eflags |= OF;
    *eflags |= ((op1^op2^result)&AF) | parity_lookup[result&0xff];
    return result;
  }
  return 0;
}

unsigned instr_shift(unsigned op, int op1, unsigned op2, unsigned width, unsigned *eflags)
{
  unsigned result, carry;
  unsigned mask = (1 << width) - 1;
  op2 &= (width-1);
        
  switch (op&0x7){
  case 0: /* rol */
    result = (((op1 << op2) | ((op1&mask) >> (width-op2)))) & mask;
    *eflags &= ~(CF|OF);
    *eflags |= (result & CF) | ((((result >> (width-1)) ^ result) << 11) & OF);
    return result;
  case 1:/* ror */
    result = ((((op1&mask) >> op2) | (op1 << (width-op2)))) & mask;
    *eflags &= ~(CF|OF);
    carry = (result >> (width-1)) & CF;
    *eflags |=  carry |
      (((carry ^ (result >> (width-2))) << 11) & OF);
    return result;
  case 2: /* rcl */
    carry = (op1>>(width-op2))&CF;
    result = (((op1 << op2) | ((op1&mask) >> (width+1-op2))) | ((*eflags&CF) << (op2-1))) & mask;
    *eflags &= ~(CF|OF);
    *eflags |= carry | ((((result >> (width-1)) ^ carry) << 11) & OF);
    return result;
  case 3:/* rcr */
    carry = (op1>>(op2-1))&CF;
    result = ((((op1&mask) >> op2) | (op1 << (width+1-op2))) | ((*eflags&CF) << (width-op2))) & mask;
    *eflags &= ~(CF|OF);
    *eflags |= carry | ((((result >> (width-1)) ^ (result >> (width-2))) << 11) & OF);
    return result;
  case 4: /* shl */        
    *eflags &= ~(CF|OF);
    *eflags |= ((op1 >> (width-op2))&CF) |
      ((((op1 >> (width-1)) ^ (op1 >> (width-2))) << 11) & OF);
    result = (op1 << op2) & mask;
    return result;
  case 5: /* shr */        
    *eflags &= ~(CF|OF);
    *eflags |= ((op1 >> (op2-1)) & CF) | (((op1 >> (width-1)) << 11) & OF);
    result = ((unsigned)(op1&mask) >> op2);
    return result;
  case 7: /* sar */
    *eflags &= ~(CF|OF);
    *eflags |= (op1 >> (op2-1)) & CF;
    result = op1 >> op2;
    return result;
  }
  return 0;
}

/*
 * DANG_BEGIN_FUNCTION instr_sim
 *
 * description:
 * instr_sim is used to simulate instructions that access the
 * VGA video memory in mode 12h when using X as the video output
 * device.  
 *
 * It is necessary to do this in order to simulate the effects 
 * of the hardware VGA controller in X mode.
 *
 * If the return value is 0, it means the instruction was not one
 * that for which a simulation is provided.  The video output will
 * probably not work.  The return value is 1 for success.
 *
 * arguments:
 * scp - A pointer to a struct sigcontext_struct holding some relevant data.
 * cp - A pointer to the instruction to be simulated
 *
 * DANG_END_FUNCTION                        
 */
 

/* helper functions modm and reg8 for instr_sim for address decoding */
unsigned modm(unsigned char *cp, x86_regs *x86, int *inst_len)
{
  unsigned addr = 0; 
  
  switch(cp[1] & 0xc0) { /* decode modifier */
  case 0x40: 
    addr = (short)(signed char)cp[2]; 
    *inst_len += 1;
    break;
  case 0x80: 
    addr = R_WORD(cp[2]); 
    *inst_len += 2;
    break;
  case 0xc0:
    if (cp[0]&1) /*word*/
      return ((unsigned) reg16(cp[1], x86));
    else
      return ((unsigned) reg8(cp[1], x86));
  default:
  }
  switch(cp[1] & 0x07) { /* decode address */
  case 0x00: 
    return ((addr + x86->ebx + x86->esi) & 0xffff) + x86->seg_base;
  case 0x01: 
    return ((addr + x86->ebx + x86->edi) & 0xffff) + x86->seg_base;
  case 0x02: 
    return ((addr + x86->ebp + x86->esi) & 0xffff) + x86->seg_ss_base;
  case 0x03: 
    return ((addr + x86->ebp + x86->edi) & 0xffff) + x86->seg_ss_base;
  case 0x04: 
    return ((addr + x86->esi) & 0xffff) + x86->seg_base;
  case 0x05: 
    return ((addr + x86->edi) & 0xffff) + x86->seg_base;
  case 0x06: 
    if (cp[1] >= 0x40)
      return ((addr + x86->ebp) & 0xffff) + x86->seg_ss_base;
    else {
      *inst_len += 2; 
      return R_WORD(cp[2]) + x86->seg_base;
    }	  
  case 0x07: 
    return ((addr + x86->ebx) & 0xffff) + x86->seg_base;
  }
  return 0; /* keep gcc happy */
}
  
unsigned char *reg8(unsigned char reg, x86_regs *x86)
{  
  switch(reg & 0x07) {
  case 0x00: return(&AL);
  case 0x01: return(&CL);
  case 0x02: return(&DL);
  case 0x03: return(&BL);
  case 0x04: return(&AH);
  case 0x05: return(&CH);
  case 0x06: return(&DH);
  case 0x07: return(&BH);
  }
  return 0; /* keep gcc happy */
}  

unsigned short *reg16(unsigned char reg, x86_regs *x86)
{  
  switch(reg & 0x07) {
  case 0x00: return(&AX);
  case 0x01: return(&CX);
  case 0x02: return(&DX);
  case 0x03: return(&BX);
  case 0x04: return(&SP);
  case 0x05: return(&BP);
  case 0x06: return(&SI);
  case 0x07: return(&DI);
  }
  return 0; /* keep gcc happy */
}  

unsigned short sreg(unsigned char reg, x86_regs *x86)
{  
  switch(reg & 0x07) {
  case 0x00: return(x86->es);
  case 0x01: return(x86->cs);
  case 0x02: return(x86->ss);
  case 0x03: return(x86->ds);
  case 0x04: return(x86->fs);
  case 0x05: return(x86->gs);
  }
  return 0;
}  

int instr_sim(x86_regs *x86)
{
  unsigned char *cp, *reg;
  signed char sc;
  unsigned char uc;
  unsigned short uns;
  unsigned short *reg_16;
  short sis;
  unsigned und, und2, mem, repcount;
  int loop_inc = (EFLAGS&DF) ? -1 : 1;		// make it a char ?
  int inst_len = 0;
  int i, i2, no_pref;

  cp = (unsigned char *) (x86->cs_base + x86->eip);
  
  x86->seg_base = x86->ds_base;
  x86->seg_ss_base = x86->ss_base;

  /* handle (some) prefixes */
  for(i = no_pref = 0; i < 15 && !no_pref; i++) {
    inst_len++;
    switch(*cp++) {
    case 0x26:
      x86->seg_base = x86->seg_ss_base = x86->es_base;
      break;
    case 0x2e:
      x86->seg_base = x86->seg_ss_base = x86->cs_base;
      break;
    case 0x36:
      x86->seg_base = x86->seg_ss_base = x86->ss_base;
      break;
    case 0x3e:
      x86->seg_base = x86->seg_ss_base = x86->ds_base;
      break;
    case 0x64:
      x86->seg_base = x86->seg_ss_base = x86->fs_base;
      break;
    case 0x65:
      x86->seg_base = x86->seg_ss_base = x86->gs_base;
      break;
        
    default:
      cp--;
      inst_len--;
      no_pref = 1;
    }
  }

  switch(*cp) {
  case 0x00:		/* add r/m8,reg8 */
  case 0x08:		/* or r/m8,reg8 */
  case 0x10:		/* adc r/m8,reg8 */
  case 0x18:		/* sbb r/m8,reg8 */
  case 0x20:		/* and r/m8,reg8 */
  case 0x28:		/* sub r/m8,reg8 */
  case 0x30:		/* xor r/m8,reg8 */
  case 0x38:		/* cmp r/m8,reg8 */
    mem = modm(cp, x86, &inst_len);
    uc = instr_binary(cp[0]>>3, instr_read_byte(mem), *reg8(cp[1]>>3, x86), 8,
                       &EFLAGS);
    if (cp[0]<0x38)
      instr_write_byte(mem, uc);
    return inst_len + 2;
            
  case 0x01:		/* add r/m16,reg16 */
  case 0x09:		/* or r/m16,reg16 */
  case 0x11:		/* adc r/m16,reg16 */
  case 0x19:		/* sbb r/m16,reg16 */
  case 0x21:		/* and r/m16,reg16 */
  case 0x29:		/* sub r/m16,reg16 */
  case 0x31:		/* xor r/m16,reg16 */
  case 0x39:		/* cmp r/m16,reg16 */
    mem = modm(cp, x86, &inst_len);
    uns = instr_binary(cp[0]>>3, instr_read_word(mem), *reg16(cp[1]>>3, x86), 16,
                        &EFLAGS);
    if (cp[0]<0x38)
      instr_write_word(mem, uns);
    return inst_len + 2;

  case 0x02:		/* add reg8,r/m8 */
  case 0x0a:		/* or reg8,r/m8 */
  case 0x12:		/* adc reg8,r/m8 */
  case 0x1a:		/* sbb reg8,r/m8 */
  case 0x22:		/* and reg8,r/m8 */
  case 0x2a:		/* sub reg8,r/m8 */
  case 0x32:		/* xor reg8,r/m8 */
  case 0x3a:		/* cmp reg8,r/m8 */
    reg = reg8(cp[1]>>3, x86);
    uc = instr_binary(cp[0]>>3, *reg, instr_read_byte(modm(cp, x86, &inst_len)),
                       8, &EFLAGS);
    if (cp[0]<0x38) *reg = uc;
    return inst_len + 2;

  case 0x03:		/* add reg16,r/m16 */
  case 0x0b:		/* or reg16,r/m16 */
  case 0x13:		/* adc reg16,r/m16 */
  case 0x1b:		/* sbb reg16,r/m16 */
  case 0x23:		/* and reg16,r/m16 */
  case 0x2b:		/* sub reg16,r/m16 */
  case 0x33:		/* xor reg16,r/m16 */
  case 0x3b:		/* cmp reg16,r/m16 */
    reg_16 = reg16(cp[1]>>3, x86);
    uns = instr_binary(cp[0]>>3, *reg_16, instr_read_word(modm(cp, x86, &inst_len)),
                        16, &EFLAGS);
    if (cp[0]<0x38) *reg_16 = uns;
    return inst_len + 2;

  case 0x04:		/* add al,imm8 */
  case 0x0c:		/* or al,imm8 */
  case 0x14:		/* adc al,imm8 */
  case 0x1c:		/* sbb al,imm8 */
  case 0x24:		/* and al,imm8 */
  case 0x2c:		/* sub al,imm8 */
  case 0x34:		/* xor al,imm8 */
  case 0x3c:		/* cmp al,imm8 */
    uc = instr_binary(cp[0]>>3, AL, cp[1], 8, &EFLAGS);
    if (cp[0]<0x38) AL = uc;
    return inst_len + 2;
            
  case 0x05:		/* add ax,imm16 */
  case 0x0d:		/* or ax,imm16 */
  case 0x15:		/* adc ax,imm16 */
  case 0x1d:		/* sbb ax,imm16 */
  case 0x25:		/* and ax,imm16 */
  case 0x2d:		/* sub ax,imm16 */
  case 0x35:		/* xor ax,imm16 */
  case 0x3d:		/* cmp ax,imm16 */
    uns = instr_binary(cp[0]>>3, AX, R_WORD(cp[1]), 16, &EFLAGS);
    if (cp[0]<0x38) AX = uns;
    return inst_len + 3;
            
  case 0x06:  /* push sreg */
  case 0x0e:
  case 0x16:
  case 0x1e:        
    SP -= 2;
    instr_write_word(x86->ss_base+(x86->esp&0xffff), sreg(cp[0]>>3,x86));
    return inst_len + 1;

  case 0x07:		/* pop es */  
    if (in_dpmi) 
      return 0;
    else {
      x86->es = instr_read_word(x86->ss_base+(x86->esp&0xffff));
      REG(es)  = x86->es;
      x86->es_base = x86->es << 4;
      SP += 2;
      return inst_len + 1;
    }   

  /* don't do 0x0f (extended instructions) for now */ 
  /* 0x17 pop ss is a bit dangerous and rarely used */  

  case 0x1f:		/* pop ds */  
    if (in_dpmi) 
      return 0;
    else {
      x86->ds = instr_read_word(x86->ss_base+(x86->esp&0xffff));
      REG(ds)  = x86->ds;
      x86->ds_base = x86->ds << 4;
      SP += 2;
      return inst_len + 1;
    }   

  case 0x27:  /* daa */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL += 6;
      EFLAGS |= AF;
    } else                  
      EFLAGS &= ~AF;
    if ((AL > 0x9f) || (EFLAGS&CF)) {
      AL += 0x60;
      EFLAGS |= CF;
    } else                  
      EFLAGS &= ~CF;
    return inst_len + 1;  
    
  case 0x2f:  /* das */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL -= 6;
      EFLAGS |= AF;
    } else                  
      EFLAGS &= ~AF;
    if ((AL > 0x9f) || (EFLAGS&CF)) {
      AL -= 0x60;
      EFLAGS |= CF;
    } else                  
      EFLAGS &= ~CF;
    return inst_len + 1;  

  case 0x37:  /* aaa */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL = (x86->eax+6) & 0xf;
      AH++;
      EFLAGS |= (CF|AF);
    } else                  
      EFLAGS &= ~(CF|AF);
    return inst_len + 1;  
      
  case 0x3f:  /* aas */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL = (x86->eax-6) & 0xf;
      AH--;
      EFLAGS |= (CF|AF);
    } else                  
      EFLAGS &= ~(CF|AF);
    return inst_len + 1;  
      
  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
  case 0x45:
  case 0x46:
  case 0x47: /* inc reg16 */
    reg_16 = reg16(cp[0], x86);
    *reg_16 = instr_inc_dec(*reg_16, (short)(*reg_16+1), &EFLAGS);
    return inst_len + 1;
       
  case 0x48:
  case 0x49:
  case 0x4a:
  case 0x4b:
  case 0x4c:
  case 0x4d:
  case 0x4e:
  case 0x4f: /* dec reg16 */
    reg_16 = reg16(cp[0],x86);
    *reg_16 = instr_inc_dec(*reg_16, (short)(*reg_16-1), &EFLAGS);
    return inst_len + 1;
       
  case 0x50:
  case 0x51:
  case 0x52:
  case 0x53:
  case 0x54:
  case 0x55:
  case 0x56:
  case 0x57: /* push reg16 */
    SP -= 2;
    instr_write_word(x86->ss_base+SP, *reg16(cp[0],x86));
    return inst_len + 1;
    
  case 0x58:
  case 0x59:
  case 0x5a:
  case 0x5b:
  case 0x5c:
  case 0x5d:
  case 0x5e:
  case 0x5f: /* pop reg16 */
    *reg16(cp[0],x86) = instr_read_word(x86->ss_base+SP);
    SP += 2;
    return inst_len + 1;

    /* 0x60 */
  case 0x68: /* push imm16 */
    SP -= 2;
    instr_write_word(x86->ss_base+SP, R_WORD(cp[1]));
    return inst_len + 3;
         
  case 0x6a: /* push imm8 */
    SP -= 2;
    instr_write_word(x86->ss_base+SP, (short)(signed char)(cp[1]));
    return inst_len + 2;
         
  case 0x70:          /*jo*/
    return EFLAGS & OF ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x71:          /*jno*/
    return EFLAGS & OF ?
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x72:          /*jc*/
    return EFLAGS & CF ? 
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x73:          /*jnc*/
    return EFLAGS & CF ?
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x74:          /*jz*/
    return EFLAGS & ZF ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x75:          /*jnz*/
    return EFLAGS & ZF ?
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x76:          /*jbe*/
    return (EFLAGS & ZF)||(EFLAGS & CF) ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x77:          /*ja*/
    return (EFLAGS & ZF)||(EFLAGS & CF) ?
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x78:          /*js*/
    return EFLAGS & SF ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x79:          /*jns*/
    return EFLAGS & SF ?
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x7a:          /*jp*/
    return EFLAGS & PF ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x7b:          /*jnp*/
    return EFLAGS & PF ?
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x7c:          /*jl*/
    return (EFLAGS & SF)!=((EFLAGS & OF)>>4) ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x7d:          /*jnl*/
    return (EFLAGS & SF)!=((EFLAGS & OF)>>4) ? 
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x7e:          /*jle*/
    return (EFLAGS & SF)!=((EFLAGS & OF)>>4) || (EFLAGS & ZF) ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0x7f:          /*jg*/
    return (EFLAGS & SF)!=((EFLAGS & OF)>>4) || (EFLAGS & ZF) ?
      inst_len + 2 :
      inst_len + 2 + (signed char)cp[1];

  case 0x80:		/* logical r/m8,imm8 */
  case 0x82:
    i = inst_len;
    mem = modm(cp, x86, &inst_len);
    uc = instr_binary(cp[1]>>3, instr_read_byte(mem), cp[2+inst_len-i], 8, &EFLAGS);
    if ((cp[1]&0x38) < 0x38)
      instr_write_byte(mem, uc);
    return inst_len + 3;

  case 0x81:		/* logical r/m16,imm16 */
    i = inst_len;
    mem = modm(cp, x86, &inst_len);
    uns = instr_binary(cp[1]>>3, instr_read_word(mem), R_WORD(cp[2+inst_len-i]), 16, &EFLAGS);
    if ((cp[1]&0x38) < 0x38)
      instr_write_word(mem, uns);
    return inst_len + 4;

  case 0x83:		/* logical r/m16,imm8 */
    i = inst_len;
    mem = modm(cp, x86, &inst_len);
    uns = instr_binary(cp[1]>>3, instr_read_word(mem), (short)(signed char)cp[2+inst_len-i], 16, &EFLAGS);
    if ((cp[1]&0x38) < 0x38)
      instr_write_word(mem, uns);
    return inst_len + 3;

  case 0x84: /* test r/m8, reg8 */
    instr_flags(instr_read_byte(modm(cp, x86, &inst_len)) & *reg8(cp[1]>>3,x86),
                0x80, &EFLAGS);
    return inst_len + 2;
      
  case 0x85: /* test r/m16, reg16 */
    instr_flags(instr_read_word(modm(cp, x86, &inst_len)) & *reg16(cp[1]>>3,x86),
                0x8000, &EFLAGS);
    return inst_len + 2;
      
  case 0x86:		/* xchg r/m8,reg8 */
    reg = reg8(cp[1]>>3, x86);
    mem = modm(cp, x86, &inst_len);
    uc = *reg;
    *reg = instr_read_byte(mem);
    instr_write_byte(mem, uc);
    return inst_len + 2;
      
  case 0x87:		/* xchg r/m16,reg16 */
    reg_16 = reg16(cp[1]>>3, x86);
    mem = modm(cp, x86, &inst_len);
    uns = *reg_16;
    *reg_16 = instr_read_word(mem);
    instr_write_word(mem, uns);
    return inst_len + 2;

  case 0x88:		/* mov r/m8,reg8 */
    instr_write_byte(modm(cp, x86, &inst_len), *reg8(cp[1]>>3, x86));
    return inst_len + 2;

  case 0x89:		/* mov r/m16,reg16 */
    instr_write_word(modm(cp, x86, &inst_len), *reg16(cp[1]>>3, x86));
    return inst_len + 2;

  case 0x8a:		/* mov reg8,r/m8 */
    *reg8(cp[1]>>3, x86) = instr_read_byte(modm(cp, x86, &inst_len));
    return inst_len + 2;

  case 0x8b:		/* mov reg16,r/m16 */
    *reg16(cp[1]>>3, x86) = instr_read_word(modm(cp, x86, &inst_len));
    return inst_len + 2;

  case 0x8c: /* mov r/m16,segreg */
    cp[0]++;
    mem = modm(cp, x86, &inst_len);
    cp[0]--;
    instr_write_word(mem, sreg(cp[1]>>3,x86));
    return inst_len + 2;

  case 0x8d: /* lea */
    x86->seg_ss_base = x86->seg_base;
    *reg16(cp[1]>>3,x86) = modm(cp, x86, &inst_len) - x86->seg_base;
    return inst_len + 2;

  case 0x8e:		/* mov segreg,r/m16 */
    if (in_dpmi) 
      return 0;
    else switch (cp[1]&0x38) {
    case 0:      
      cp[0]++;
      x86->es = instr_read_word(modm(cp, x86, &inst_len));
      cp[0]--;
      REG(es)  = x86->es;
      x86->es_base = x86->es << 4;
      return inst_len + 2;
    case 0x18:  
      cp[0]++;
      x86->ds = instr_read_word(modm(cp, x86, &inst_len));
      cp[0]--;
      REG(ds)  = x86->ds;
      x86->ds_base = x86->ds << 4;
      return inst_len + 2;
    default:
      return 0;
    }

  case 0x8f: /*pop*/
    if ((cp[1]&0x38) == 0){
      instr_write_word(modm(cp, x86, &inst_len), instr_read_word(x86->ss_base+SP));
      SP += 2;
      return inst_len + 2;
    } else
      return 0;

  case 0x90: /* nop */
    return inst_len + 1;
  case 0x91:
  case 0x92:
  case 0x93:
  case 0x94:
  case 0x95:
  case 0x96:
  case 0x97: /* xchg reg16, ax */
    reg_16 = reg16(cp[0],x86);
    uns = AX;
    AX = *reg_16;
    *reg_16 = uns;
    return inst_len + 1;

  case 0x98: /* cbw */
    AX = (short)(signed char)AL;
    return inst_len + 1;

  case 0x99: /* cwd */
    DX = (AX>0x7fff ? 0xffff : 0);
    return inst_len + 1;

  case 0x9a: /*call far*/
    if (in_dpmi)
      return 0;
    else {
      SP -= 4;
      instr_write_word(x86->ss_base+(x86->esp&0xffff), R_WORD(x86->eip)+inst_len+5);
      instr_write_word(x86->ss_base+(x86->esp&0xffff)+2, x86->cs);
      x86->cs = R_WORD(cp[3]);
      REG(cs)  = x86->cs;
      x86->cs_base = x86->cs << 4;
      return (R_WORD(cp[1]) - R_WORD(x86->eip));
    }
    /* NO: 0x9b wait 0x9c pushf 0x9d popf*/    

  case 0x9e: /* sahf */
    R_LO(EFLAGS) = AH;
    return inst_len + 1;

  case 0x9f: /* lahf */
    AH = R_LO(EFLAGS);
    return inst_len + 1;

  case 0xa0:		/* mov al,moff16 */
    AL = instr_read_byte(R_WORD(cp[1])+x86->seg_base);
    return inst_len + 3;

  case 0xa1:		/* mov ax,moff16 */
    AX = instr_read_word(R_WORD(cp[1])+x86->seg_base);
    return inst_len + 3;

  case 0xa2:		/* mov moff16,al */
    instr_write_byte(R_WORD(cp[1])+x86->seg_base, AL);
    return inst_len + 3;

  case 0xa3:		/* mov moff16,ax */
    instr_write_word(R_WORD(cp[1])+x86->seg_base, AX);
    return inst_len + 3;

  case 0xa4:		/* movsb */
    instr_write_byte(x86->es_base + DI, instr_read_byte(x86->seg_base + SI));
    DI += loop_inc;
    SI += loop_inc;
    return inst_len + 1;

  case 0xa5:		/* movsw */
    instr_write_word(x86->es_base + DI, instr_read_word(x86->seg_base + SI));
    DI += loop_inc*2;
    SI += loop_inc*2;
    return inst_len + 1;

  case 0xa6: /*cmpsb */
    instr_binary(7, instr_read_byte(x86->seg_base + SI),
                  instr_read_byte(x86->es_base + DI), 8, &EFLAGS);
    DI += loop_inc;
    SI += loop_inc;
    return inst_len + 1;

  case 0xa7: /* cmpsw */
    instr_binary(7, instr_read_word(x86->seg_base + SI),
                  instr_read_word(x86->es_base + DI), 16, &EFLAGS);
    DI += loop_inc*2;
    SI += loop_inc*2;
    return inst_len + 1;

  case 0xa8: /* test al, imm */
    instr_flags(AL & cp[1], 0x80, &EFLAGS);
    return inst_len+2;

  case 0xa9: /* test ax, imm */
    instr_flags(AX & R_WORD(cp[1]), 0x8000, &EFLAGS);
    return inst_len+3;

  case 0xaa:		/* stosb */
    instr_write_byte(x86->es_base + DI, AL);
    DI += loop_inc;
    return inst_len + 1;

  case 0xab:		/* stosw */
    instr_write_word(x86->es_base + DI, AX);
    DI += loop_inc*2;
    return inst_len + 1;

  case 0xac:		/* lodsb */
    AL = instr_read_byte(x86->seg_base + SI);
    SI += loop_inc;
    return inst_len + 1;

  case 0xad: /* lodsw */
    AX = instr_read_word(x86->seg_base + SI);
    SI += loop_inc*2;
    return inst_len + 1;

  case 0xae: /* scasb */
    instr_binary(7, AL, instr_read_byte(x86->es_base + DI), 8, &EFLAGS);
    DI += loop_inc;
    return inst_len + 1;

  case 0xaf: /* scasw */
    instr_binary(7, AX, instr_read_word(x86->es_base + DI),16, &EFLAGS);
    DI += loop_inc*2;
    return inst_len + 1;

  case 0xb0:
  case 0xb1:
  case 0xb2:
  case 0xb3:
  case 0xb4:
  case 0xb5:
  case 0xb6:
  case 0xb7:
    *reg8(cp[0], x86) = cp[1];
    return inst_len + 2;

  case 0xb8:
  case 0xb9:
  case 0xba:
  case 0xbb:
  case 0xbc:
  case 0xbd:
  case 0xbe:
  case 0xbf:
    *reg16(cp[0], x86) = R_WORD(cp[1]);
    return inst_len + 3;

  case 0xc0: /* shift byte, imm8 */
    i = inst_len;
    if ((cp[1]&0x38)==0x30) return 0;
    mem = modm(cp, x86, &inst_len);
    instr_write_byte(mem,instr_shift(cp[1]>>3, (signed char) instr_read_byte(mem),
                                       cp[2+inst_len-i], 8, &EFLAGS));
    return inst_len + 3;

  case 0xc1: /* shift word, imm8 */
    i = inst_len;
    if ((cp[1]&0x38)==0x30) return 0;
    mem = modm(cp, x86, &inst_len);
    instr_write_word(mem, instr_shift(cp[1]>>3, (short) instr_read_word(mem),
                                        cp[2+inst_len-i], 16, &EFLAGS));
    return inst_len + 3;

  case 0xc2:		/* ret imm16*/
    i = instr_read_word(x86->ss_base+SP) - R_WORD(x86->eip);
    SP += 2 + R_WORD(cp[1]);
    return (i);

  case 0xc3:		/* ret */
    i = instr_read_word(x86->ss_base+SP) - R_WORD(x86->eip);
    SP += 2;
    return (i);

  case 0xc4:		/* les */
    if (in_dpmi) 
      return 0;
    else {
      mem = modm(cp, x86, &inst_len);
      x86->es = instr_read_word(mem+2);
      REG(es)  = x86->es;
      x86->es_base = x86->es << 4;
      *reg16(cp[1] >> 3, x86) = instr_read_word(mem);
      return inst_len + 2;
    }   

  case 0xc5:		/* lds */
    if (in_dpmi) 
      return 0;
    else {
      mem = modm(cp, x86, &inst_len);
      x86->ds = instr_read_word(mem+2);
      REG(ds)  = x86->ds;
      x86->ds_base = x86->ds << 4;
      *reg16(cp[1] >> 3, x86) = instr_read_word(mem);
      return inst_len + 2;
    }

  case 0xc6:		/* mov r/m8,imm8 */
    i = inst_len;
    mem = modm(cp, x86, &inst_len);
    instr_write_byte(mem, cp[2 + inst_len - i]);
    return inst_len + 3;

  case 0xc7:		/* mov r/m8,imm16 */
    i = inst_len;
    mem = modm(cp, x86, &inst_len);
    instr_write_word(mem, R_WORD(cp[2 + inst_len - i]));
    return inst_len + 4;
    /* 0xc8 enter */  

  case 0xc9: /*leave*/   
    SP = BP;
    BP = instr_read_word(x86->ss_base+SP);
    SP += 2;
    return inst_len + 1;

  case 0xca: /*retf imm 16*/          
    if (in_dpmi) 
      return 0;
    else {
      i = instr_read_word(x86->ss_base+SP) - R_WORD(x86->eip);
      x86->cs = instr_read_word(x86->ss_base+SP+2);
      REG(cs)  = x86->cs;
      x86->cs_base = x86->cs << 4;
      SP += 4 + R_WORD(cp[1]);
      return (i);
    }

  case 0xcb: /*retf*/          
    if (in_dpmi) 
      return 0;
    else {
      i = instr_read_word(x86->ss_base+SP) - R_WORD(x86->eip);
      x86->cs = instr_read_word(x86->ss_base+SP+2);
      REG(cs)  = x86->cs;
      x86->cs_base = x86->cs << 4;
      SP += 4;
      return (i);
    }
    /* 0xcc int3 0xcd int 0xce into 0xcf iret */

  case 0xd0: /* shift r/m8, 1 */
    if ((cp[1]&0x38)==0x30) return 0;
    mem = modm(cp, x86, &inst_len);
    instr_write_byte(mem, instr_shift(cp[1]>>3, (signed char) instr_read_byte(mem),
                                        1, 8, &EFLAGS));
    return inst_len + 2;

  case 0xd1: /* shift r/m16, 1 */
    if ((cp[1]&0x38)==0x30) return 0;
    mem = modm(cp, x86, &inst_len);
    instr_write_word(mem, instr_shift(cp[1]>>3, (short) instr_read_word(mem),
                                        1, 16, &EFLAGS));
    return inst_len + 2;
    
  case 0xd2: /* shift r/m8, cl */
    if ((cp[1]&0x38)==0x30) return 0;
    mem = modm(cp, x86, &inst_len);
    instr_write_byte(mem,instr_shift(cp[1]>>3, (signed char) instr_read_byte(mem),
                                       CL, 8, &EFLAGS));
    return inst_len + 2;

  case 0xd3: /* shift r/m16, cl */
    if ((cp[1]&0x38)==0x30) return 0;
    mem = modm(cp, x86, &inst_len);
    instr_write_word(mem, instr_shift(cp[1]>>3, (short) instr_read_word(mem),
                                       CL, 16, &EFLAGS));
    return inst_len + 2;

  case 0xd4:  /* aam byte */
    AH = AL / cp[1];
    AL = AL % cp[1];
    return inst_len+2;
    
  case 0xd5:  /* aad byte */ 
    AL = AH * cp[1] + AL;
    AH = 0;
    return inst_len+2;

  case 0xd6: /* salc */
    AL = EFLAGS & CF ? 0xff : 0;
    return inst_len+1;

  case 0xd7: /* xlat */
    AL = instr_read_byte(x86->seg_base+BX+AL);
    return inst_len + 1;
    /* 0xd8 - 0xdf copro */

  case 0xe0: /* loopnz */
    return (--CX) && !(EFLAGS & ZF) ?
      inst_len + 2 + (signed char)(cp[1]) :
      inst_len + 2;

  case 0xe1: /* loopz */
    return (--CX) && (EFLAGS & ZF) ?
      inst_len + 2 + (signed char)cp[1] :
      inst_len + 2;

  case 0xe2: /* loop */
    return --CX ? inst_len + 2 + (signed char)cp[1] : inst_len + 2;

  case 0xe3:  /* jcxz */
    return CX == 0 ? inst_len + 2 + (signed char)cp[1] : inst_len + 2;

  /* 0xe4 in ib 0xe5 in iw 0xe6 out ib 0xe7 out iw */

  case 0xe8: /* call near */
    SP -= 2;
    instr_write_word(x86->ss_base+(x86->esp&0xffff), R_WORD(x86->eip)+inst_len+3);
    return inst_len + 3 + (short)R_WORD(cp[1]);

  case 0xe9: /* jmp near */
    return inst_len + 3 + (short)R_WORD(cp[1]);

  case 0xea: /*jmp far*/
    if (in_dpmi)
      return 0;
    else {
      x86->cs = *((short *)(cp+3));
      REG(cs)  = x86->cs;
      x86->cs_base = x86->cs << 4;
      return (*((short *)(cp+1)) - x86->eip);
    }

  case 0xeb: /* jmp short */
    return inst_len + 2 + (signed char)(cp[1]);

  /* 0xec in al,dx 0xed in ax,dx */

  case 0xee: /* out dx, al */
    if (VGA_emulate_outb(DX, AL))
      return inst_len + 1;
    else
      return 0;

  case 0xef: /* out dx, ax */
    if ((VGA_emulate_outb(DX, AL)) &&
	      (VGA_emulate_outb(DX + 1, AH)))
      return inst_len + 1;
    else
      return 0;

    /* 0xf0 lock 0xf1 int1 */

  case 0xf2:    /* repnz */
  case 0xf3:		/* repe prefix; no diff except for cmps and scas */
    repcount = CX;
    i2 = inst_len + 2; /* default return value */
    if (repcount > mastercount) { /* do more than we allow to */
      repcount = mastercount;
      i2 = 0;
    }
    CX -= repcount;
    mastercount -= repcount;
    count -= repcount;
    
    switch (cp[1]) {
    case 0xa4:		/* rep movsb */
      for (i = 0, und = 0; und < repcount; i += loop_inc, und++) 
        instr_write_byte(
          x86->es_base + ((x86->edi+i) & 0xffff),
          instr_read_byte(x86->seg_base + ((x86->esi+i) & 0xffff)));
      DI += repcount*loop_inc;
      SI += repcount*loop_inc;
      return i2;
    case 0xa5:		/* rep movsw */
      for (i = 0, und = 0; und < repcount; i += loop_inc*2, und++) 
        instr_write_word(
          x86->es_base + ((x86->edi+i) & 0xffff),
          instr_read_word(x86->seg_base + ((x86->esi+i) & 0xffff)));
      DI += repcount*loop_inc*2;
      SI += repcount*loop_inc*2;
      return i2;
    case 0xa6: /* rep?z cmpsb */
      for (i = 0, und = 0; und < repcount; i += loop_inc) {
        instr_binary(7, instr_read_byte(x86->seg_base + ((x86->esi+i) & 0xffff)),
                      instr_read_byte(x86->es_base + ((x86->edi+i) & 0xffff)),
                      8, &EFLAGS);
        und++;
        if (((EFLAGS & ZF) >> 6) != (cp[0]&1)) /* 0xf2 repnz 0xf3 repz */ {
          CX += repcount - und;
          mastercount += repcount - und;
          count += repcount - und;
          i2 = inst_len + 2; /* we're fine now! */          
          break;
        }
      }
      DI += und*loop_inc;
      SI += und*loop_inc;
      return i2;
    case 0xa7: /* rep?z cmpsw */
      for (i = 0, und = 0; und < repcount; i += loop_inc*2, und++) {
        instr_binary(7, instr_read_word(x86->seg_base + ((x86->esi+i) & 0xffff)),
                      instr_read_word(x86->es_base + ((x86->edi+i) & 0xffff)),
                      16, &EFLAGS);
        und++;
        if (((EFLAGS & ZF) >> 6) != (cp[0]&1)) /* 0xf2 repnz 0xf3 repz */ {
          CX += repcount - und;
          mastercount += repcount - und;
          count += repcount - und;
          i2 = inst_len + 2; /* we're fine now! */          
          break;
        }
      }
      DI += und*loop_inc*2;
      SI += und*loop_inc*2;
      return i2;
    case 0xaa: /* rep stosb */
      for (uns = DI, und = 0; und < repcount; uns += loop_inc, und++) 
        instr_write_byte(x86->es_base + uns, AL);
      DI += repcount*loop_inc;
      return i2;

    case 0xab: /* rep stosw */
      for (uns = DI, und = 0; und < repcount; uns += loop_inc*2, und++) 
        instr_write_word(x86->es_base + uns, AX);
      DI += repcount*loop_inc*2;
      return i2;

    case 0xae: /* rep scasb */
      for (uns = DI, und = 0; und < repcount; uns += loop_inc) {
        instr_binary(7, AL, instr_read_byte(x86->es_base + uns),
                      8, &EFLAGS);
        und++;
        if (((EFLAGS & ZF) >> 6) != (cp[0]&1)) /* 0xf2 repnz 0xf3 repz */ {
          CX += repcount - und;
          mastercount += repcount - und;
          count += repcount - und;
          i2 = inst_len + 2; /* we're fine now! */          
          break;
        }
      }
      DI += und*loop_inc;
      return i2;
    case 0xaf: /* scasw */
      for (uns = DI, und = 0; und < repcount; uns += loop_inc*2) {
        instr_binary(7, AX, instr_read_byte(x86->es_base + uns),
                      8, &EFLAGS);
        und++;
        if (((EFLAGS & ZF) >> 6) != (cp[0]&1)) /* 0xf2 repnz 0xf3 repz */ {
          CX += repcount - und;
          mastercount += repcount - und;
          count += repcount - und;
          i2 = inst_len + 2; /* we're fine now! */          
          break;
        }
      }
      DI += und*loop_inc*2;
      return i2;
    default: return 0;
    }

  /*0xf4 hlt */
    
  case 0xf5: /* cmc */
    EFLAGS ^= CF;
    return inst_len + 1;

  case 0xf6: 
    i=inst_len;
    mem = modm(cp, x86, &inst_len);
    switch (cp[1]&0x38) {
    case 0x00: /* test mem byte, imm */
      instr_flags(instr_read_byte(mem) & cp[2+inst_len-i], 0x80, &EFLAGS);
      return inst_len+3;
    case 0x08: return 0;
    case 0x10: /*not byte*/
      instr_write_byte(mem, ~instr_read_byte(mem));
      return inst_len+2;
    case 0x18: /*neg byte*/
      instr_write_byte(mem, instr_binary(7, 0, instr_read_byte(mem), 8, &EFLAGS));
      return inst_len + 2;
    case 0x20: /*mul byte*/
      AX = AL * instr_read_byte(mem);
      EFLAGS &= ~(CF|OF);
      if (AH)
        EFLAGS |= (CF|OF);
      return inst_len + 2;
    case 0x28: /*imul byte*/
      AX = (signed char)AL * (signed char)instr_read_byte(mem);
      EFLAGS &= ~(CF|OF);
      if (AH)
        EFLAGS |= (CF|OF);
      return inst_len + 2;
    case 0x30: /*div byte*/
      und = AX;
      uc = instr_read_byte(mem);
      if (uc == 0) return 0;
      und2 = und / uc;
      if (und2 & 0xffffff00) return 0;
      AL = und2 & 0xff;
      AH = und % uc;
      return inst_len+2;
    case 0x38: /*idiv byte*/
      i = (short)AX;
      sc = instr_read_byte(mem);
      if (sc == 0) return 0;
      i2 = i / sc;
      if (i2<-128 || i2>127) return 0;
      AL = i2 & 0xff;
      AH = i % sc;
      return inst_len+2;
    }

  case 0xf7: 
    i=inst_len;
    mem = modm(cp, x86, &inst_len);
    switch (cp[1]&0x38) {
    case 0x00: /* test mem word, imm */
      instr_flags(instr_read_word(mem) & R_WORD(cp[2+inst_len-i]), 0x8000, &EFLAGS);
      return inst_len+4;
    case 0x08: return 0;
    case 0x10: /*not word*/
      instr_write_word(mem, ~instr_read_word(mem));
      return inst_len+2;
    case 0x18: /*neg word*/       
      instr_write_word(mem, instr_binary(7, 0, instr_read_word(mem), 16, &EFLAGS));
      return inst_len+2;
    case 0x20: /*mul word*/       
      und = AX * instr_read_word(mem);
      AX = und & 0xffff;
      DX = und >> 16;
      EFLAGS &= ~(CF|OF);
      if (DX)
        EFLAGS |= (CF|OF);
      return inst_len+2;
    case 0x28: /*imul word*/
      i = (short)AX * (short)instr_read_word(mem);
      AX = i & 0xffff;
      DX = i >> 16;
      EFLAGS &= ~(CF|OF);
      if (DX)
        EFLAGS |= (CF|OF);
      return inst_len+2;      
    case 0x30: /*div word*/
      und = (DX<<16) + AX;
      uns = instr_read_word(mem);
      if (uns == 0) return 0;
      und2 = und / uns;
      if (und2 & 0xffff0000) return 0;
      AX = und2 & 0xffff;
      DX = und % uns;
      return inst_len+2;
    case 0x38: /*idiv word*/
      i = ((short)DX<<16) + AX;
      sis = instr_read_word(mem);
      if (sis == 0) return 0;
      i2 = i / sis;
      if (i2<-32768 || i2>32767) return 0;
      AX = i2 & 0xffff;
      DX = i % sis;
      return inst_len+2;
    }
    
  case 0xf8: /* clc */
    EFLAGS &= ~CF;
    return inst_len+1;

  case 0xf9: /* stc */
    EFLAGS |= CF;
    return inst_len+1;

    /* 0xfa cli 0xfb sti */

  case 0xfc: /* cld */
    EFLAGS &= ~DF;
    return inst_len+1;

  case 0xfd: /* std */
    EFLAGS |= DF;
    return inst_len+1;

  case 0xfe: /* inc/dec mem */
    mem = modm(cp, x86, &inst_len);
    sc = instr_read_byte(mem);
    switch (cp[1]&0x38) {
    case 0x00:
      instr_write_byte(mem, instr_inc_dec(sc, (signed char)(sc+1), &EFLAGS));
      return inst_len+2;
    case 0x08:        
      instr_write_byte(mem, instr_inc_dec(sc, (signed char)(sc-1), &EFLAGS));
      return inst_len+2;
    default:              
      return 0;
    }

  case 0xff:
    mem = modm(cp, x86, &inst_len);
    sis = instr_read_word(mem);
    switch (cp[1]&0x38) {
    case 0x00: /* inc */
      instr_write_word(mem, instr_inc_dec(sis, (short)(sis+1), &EFLAGS));
      return inst_len+2;
    case 0x08: /* dec */       
      instr_write_word(mem, instr_inc_dec(sis, (short)(sis-1), &EFLAGS));
      return inst_len+2;
    case 0x10: /*call near*/          
      SP -= 2;
      instr_write_word(x86->ss_base+(x86->esp&0xffff), R_WORD(x86->eip)+inst_len+2);
      return (sis - R_WORD(x86->eip));
    case 0x18: /*call far*/          
      if (in_dpmi) 
        return 0;
      else {
        SP -= 4;
        instr_write_word(x86->ss_base+(x86->esp&0xffff), R_WORD(x86->eip)+inst_len+2);
        instr_write_word(x86->ss_base+(x86->esp&0xffff)+2, x86->cs);
        x86->cs = instr_read_word(mem+2);
        REG(cs)  = x86->cs;
        x86->cs_base = x86->cs << 4;
        return (sis - R_WORD(x86->eip));
      }
    case 0x20: /*jmp near*/          
      return (sis - R_WORD(x86->eip));
    case 0x28: /*jmp far*/          
      if (in_dpmi) 
        return 0;
      else {
        x86->cs = instr_read_word(mem+2);
        REG(cs)  = x86->cs;
        x86->cs_base = x86->cs << 4;
        return (sis - R_WORD(x86->eip));
      }
    case 0x30: /*push*/
      SP -= 2;
      instr_write_word(x86->ss_base+SP, sis);
      return inst_len + 2;
    default:              
      return 0;
    }

  default:		/* First byte doesn't match anything */
    return 0;
  }	/* switch (*cp) */

  return 0;
}  


void instr_emu(struct sigcontext_struct *scp)
{
  extern int dis_8086(unsigned int, 
                      const unsigned char *,
                      unsigned char *, 
                      int, unsigned int *,
                      unsigned int *, 
                      unsigned int, int);
  unsigned char *cp;
      
#if DEBUG_INSTR >= 1
  unsigned int ref;
  int refseg, rc;
  unsigned char frmtbuf[256];
#endif
  int i_len;
  x86_regs x86;
      
  if(in_dpmi) {
    x86.eax = _eax;
    x86.ebx = _ebx;
    x86.ecx = _ecx;
    x86.edx = _edx;
    x86.esi = _esi;
    x86.edi = _edi;
    x86.ebp = _ebp;
    x86.esp = _esp;
    x86.eip = _eip;
    x86.eflags = _eflags;
    x86.cs = _cs;
    x86.ds = _ds;
    x86.es = _es;
    x86.ss = _ss;
    x86.fs = _fs;
    x86.gs = _gs;
    x86.cs_base = dpmi_GetSegmentBaseAddress(_cs);
    x86.ds_base = dpmi_GetSegmentBaseAddress(_ds);
    x86.es_base = dpmi_GetSegmentBaseAddress(_es);
    x86.ss_base = dpmi_GetSegmentBaseAddress(_ss);
    x86.fs_base = dpmi_GetSegmentBaseAddress(_fs);
    x86.gs_base = dpmi_GetSegmentBaseAddress(_gs);
    x86._32bit = 0;		// 16 bit code, just for now
  }
  else {
    x86.eax = REG(eax);
    x86.ebx = REG(ebx);
    x86.ecx = REG(ecx);
    x86.edx = REG(edx);
    x86.esi = REG(esi);
    x86.edi = REG(edi);
    x86.ebp = REG(ebp);
    x86.esp = REG(esp);
    x86.eip = REG(eip);
    x86.eflags = REG(eflags);
    x86.cs = REG(cs);
    x86.ds = REG(ds);
    x86.es = REG(es);
    x86.ss = REG(ss);
    x86.fs = REG(fs);
    x86.gs = REG(gs);
    x86.cs_base = x86.cs << 4;
    x86.ds_base = x86.ds << 4;
    x86.es_base = x86.es << 4;
    x86.ss_base = x86.ss << 4;
    x86.fs_base = x86.fs << 4;
    x86.gs_base = x86.gs << 4;
    x86._32bit = 0;
  }

#if DEBUG_INSTR >= 1
  dump_x86_regs(&x86);
#endif

  cp = (unsigned char *) (x86.cs_base + x86.eip);
  mastercount = MASTERCOUNT; count = COUNT; i_len = 0;
      
  while (count>0 && mastercount>0 && (i_len = instr_sim(&x86))) {

#if DEBUG_INSTR >= 1
    refseg = x86.cs;
    rc = dis_8086((unsigned long) cp, cp, frmtbuf, x86._32bit ? 3 : 0, &x86.cs, &refseg, x86.cs_base, 1);
    instr_deb("vga_emu_fault: %d bytes simulated %d %d: %s  fault addr=%08x\n", i_len, mastercount, count,
              frmtbuf, (unsigned) scp->cr2);
#endif
    if(x86._32bit)
      x86.eip += i_len;
    else
      R_WORD(x86.eip) += i_len;
#if DEBUG_INSTR >= 1
    dump_x86_regs(&x86);
#endif
    cp = (unsigned char *) (x86.cs_base + x86.eip);
    count--; mastercount--;
  }

  if (mastercount == MASTERCOUNT) { /* really an unknown instruction from the beginning */
    i_len = instr_len((unsigned char *) (x86.cs_base + x86.eip));
    if(x86._32bit)
      x86.eip += i_len;
    else
      R_WORD(x86.eip) += i_len;
  }
  
#if DEBUG_INSTR >= 1
  refseg = x86.cs;
  rc = dis_8086((unsigned long) cp, cp, frmtbuf, x86._32bit ? 3 : 0, &refseg, &ref, x86.cs_base, 1);
  instr_deb("vga_emu_fault: %u bytes not simulated %d %d: %s  fault addr=%08x\n",
            i_len, mastercount, count, frmtbuf, (unsigned) scp->cr2);
  dump_x86_regs(&x86);
#endif

  if(in_dpmi) {
    _eax = x86.eax;
    _ebx = x86.ebx;
    _ecx = x86.ecx;
    _edx = x86.edx;
    _esi = x86.esi;
    _edi = x86.edi;
    _ebp = x86.ebp;
    _esp = x86.esp;
    _eip = x86.eip;
    _eflags = x86.eflags;
  }
  else {
    REG(eax) = x86.eax;
    REG(ebx) = x86.ebx;
    REG(ecx) = x86.ecx;
    REG(edx) = x86.edx;
    REG(esi) = x86.esi;
    REG(edi) = x86.edi;
    REG(ebp) = x86.ebp;
    REG(esp) = x86.esp;
    REG(eip) = x86.eip;
    REG(eflags) = x86.eflags;
  }
}
