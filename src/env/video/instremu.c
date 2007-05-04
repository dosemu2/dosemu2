/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
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
#include "config.h"
#include "emu.h"
#include "vgaemu.h"
#include "dpmi.h"
#include "emu-ldt.h"
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

#define COUNT  150	/* bail out when this #instructions were simulated
			   after a VGA r/w access */			       

#define R_LO(a) (((unsigned char *) &(a))[0])
#define R_HI(a) (((unsigned char *) &(a))[1])
#define R_WORD(a) (*((unsigned short *) &(a)))
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
#define EFLAGS (x86->eflags)
#define FLAGS (R_WORD(EFLAGS))
#define OP_JCC(cond) eip += (cond) ? 2 + (signed char)cs[eip + 1] : 2; break;

/* assembly macros to speed up x86 on x86 emulation: the cpu helps us in setting
   the flags */

#define OPandFLAG0(eflags, insn, op1, istype) __asm__ __volatile__("\n\
	"#insn"	%0\n\
	pushf; pop	%1\n \
	" : #istype (op1), "=g" (eflags) : "0" (op1));

#define OPandFLAG1(eflags, insn, op1, istype) __asm__ __volatile__("\n\
	"#insn"	%0, %0\n\
	pushf; pop	%1\n \
	" : #istype (op1), "=g" (eflags) : "0" (op1));

#define OPandFLAG(eflags, insn, op1, op2, istype, type) __asm__ __volatile__("\n\
	"#insn"	%3, %0\n\
	pushf; pop	%1\n \
	" : #istype (op1), "=g" (eflags) : "0" (op1), #type (op2));

#define OPandFLAGC(eflags, insn, op1, op2, istype, type) __asm__ __volatile__("\n\
       shr     $1, %0\n\
       "#insn" %4, %1\n\
       pushf; pop     %0\n \
       " : "=r" (eflags), #istype (op1)  : "0" (eflags), "1" (op1), #type (op2));


#if !defined True
#define False 0
#define True 1
#endif

#define instr_msg(x...) v_printf("VGAEmu: " x)

#if DEBUG_INSTR >= 1
#define instr_deb(x...) v_printf("VGAEmu: " x)
#else
#define instr_deb(x...)
#endif

#if DEBUG_INSTR >= 2
#define instr_deb2(x...) v_printf("VGAEmu: " x)
#else
#define instr_deb2(x...)
#endif

enum {REPNZ = 0, REPZ = 1, REP_NONE = 2};

typedef struct x86_regs {
  unsigned eax, ecx, edx, ebx, esp, ebp, esi, edi;
  /* this sequence is important because this is the cpu's order and thus
     gives us an optimization */
  unsigned eip;
  unsigned long eflags;
  unsigned es, cs, ss, ds, fs, gs;
  unsigned char *cs_base, *ds_base, *es_base, *ss_base, *fs_base, *gs_base;
  unsigned char *seg_base, *seg_ss_base;
  unsigned _32bit:1;	/* 16/32 bit code */
  unsigned address_size; /* in bytes so either 4 or 2 */
  unsigned operand_size;
  unsigned prefixes, rep;
  unsigned (*instr_binary)(unsigned op, unsigned op1,
                           unsigned op2, unsigned long *eflags);
  unsigned (*instr_read)(const unsigned char *addr);
  void (*instr_write)(unsigned char *addr, unsigned u);
  unsigned char *(*modrm)(unsigned char *cp, struct x86_regs *x86, int *inst_len);
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
static unsigned seg, osp, asp, lock, rep;
static int count;
static unsigned char *vga_base, *vga_end;

static unsigned arg_len(unsigned char *);

static unsigned char instr_read_byte(const unsigned char *addr);
static unsigned instr_read_word(const unsigned char *addr);
static unsigned instr_read_dword(const unsigned char *addr);
static void instr_write_byte(unsigned char *addr, unsigned char u);
static void instr_write_word(unsigned char *addr, unsigned u);
static void instr_write_dword(unsigned char *addr, unsigned u);
static void instr_flags(unsigned val, unsigned smask, unsigned long *eflags);
static unsigned char instr_binary_byte(unsigned char op, unsigned char op1,
                                       unsigned char op2, unsigned long *eflags);
static unsigned instr_binary_word(unsigned op, unsigned op1,
                                  unsigned op2, unsigned long *eflags);
static unsigned instr_binary_dword(unsigned op, unsigned op1,
                                   unsigned op2, unsigned long *eflags);
static unsigned instr_shift(unsigned op, int op1, unsigned op2, unsigned size, unsigned long *eflags);
static unsigned char *sib(unsigned char *cp, x86_regs *x86, int *inst_len);
static unsigned char *modrm32(unsigned char *cp, x86_regs *x86, int *inst_len);
static unsigned char *modrm16(unsigned char *cp, x86_regs *x86, int *inst_len);

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

unsigned char instr_read_byte(const unsigned char *addr)
{
  unsigned char u;

  if(addr >= vga_base && addr < vga_end) {
    count = COUNT;
    u = vga_read(addr);
  }
  else {
    u = *addr;
  }
#if DEBUG_INSTR >= 2  
  instr_deb2("Read byte 0x%x", u);
  if (addr<0x8000000) v_printf(" from address %x\n", addr); else v_printf("\n");
#endif  
  
  return u;
}

unsigned instr_read_word(const unsigned char *addr)
{
  unsigned u;

  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here
   */
  if(addr >= vga_base && addr < vga_end) {
    count = COUNT;
    u = 0;
    R_LO(u) = vga_read(addr);
    R_HI(u) = vga_read(addr+1);
  } else 
    u = *(unsigned short *)addr;

#if DEBUG_INSTR >= 2  
  instr_deb2("Read word 0x%x", u);
  if (addr<0x8000000) v_printf(" from address %x\n", addr); else v_printf("\n");
#endif  
  return u;
}

unsigned instr_read_dword(const unsigned char *addr)
{
  unsigned u;
  
  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here
   */
  if(addr >= vga_base && addr < vga_end) {
    count = COUNT;
    R_LO(u) = vga_read(addr);
    R_HI(u) = vga_read(addr+1);
    ((unsigned char *) &u)[2] = vga_read(addr+2);
    ((unsigned char *) &u)[3] = vga_read(addr+3);
  } else 
    u = *(unsigned *)addr;

#if DEBUG_INSTR >= 2  
  instr_deb2("Read word 0x%x", u);
  if (addr<0x8000000) v_printf(" from address %x\n", addr); else v_printf("\n");
#endif  
  return u;
}

void instr_write_byte(unsigned char *addr, unsigned char u)
{
  if(addr >= vga_base && addr < vga_end) {
    count = COUNT;
    vga_write(addr, u);
  }
  else if (ldt_buffer && addr >= ldt_buffer &&
      addr < ldt_buffer + LDT_ENTRIES*LDT_ENTRY_SIZE) {
    direct_ldt_write(addr - ldt_buffer, 1, (char*)&u);
  }
  else {
    *addr = u;
  }
#if DEBUG_INSTR >= 2  
  instr_deb2("Write byte 0x%x", u);
  if (addr<0x8000000) v_printf(" at address %x\n", addr); else v_printf("\n");
#endif  
}

void instr_write_word(unsigned char *dst, unsigned u)
{
  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here.
   * we assume application do not try to mix here
   */

  if(dst >= vga_base && dst < vga_end) {
    count = COUNT;
    vga_write(dst, R_LO(u));
    vga_write(dst+1, R_HI(u));
  }
  else if (ldt_buffer && dst >= ldt_buffer &&
      dst < ldt_buffer + LDT_ENTRIES*LDT_ENTRY_SIZE) {
    direct_ldt_write(dst - ldt_buffer, 2, (char*)&u);
  }
  else
    *(unsigned short *)dst = u;

#if DEBUG_INSTR >= 2  
  instr_deb2("Write word 0x%x", u);
  if (dst<0x8000000) v_printf(" at address %x\n", dst); else v_printf("\n");
#endif  
}

void instr_write_dword(unsigned char *dst, unsigned u)
{
  /*
   * segment wrap-arounds within a data word are not allowed since
   * at least i286, so no problems here.
   * we assume application do not try to mix here
   */

  if(dst >= vga_base && dst < vga_end) {
    count = COUNT;
    vga_write(dst, R_LO(u));
    vga_write(dst+1, R_HI(u));
    vga_write(dst+2, ((unsigned char *) &u)[2]);
    vga_write(dst+3, ((unsigned char *) &u)[3]);
  }
  else if (ldt_buffer && dst >= ldt_buffer &&
      dst < ldt_buffer + LDT_ENTRIES*LDT_ENTRY_SIZE) {
    direct_ldt_write(dst - ldt_buffer, 4, (char*)&u);
  }
  else 
    *(unsigned *)dst = u;

#if DEBUG_INSTR >= 2  
  instr_deb2("Write word 0x%x", u);
  if (dst<0x8000000) v_printf(" at address %x\n", dst); else v_printf("\n");
#endif  
}

/* We use the cpu itself to set the flags, which is easy since we are
   emulating x86 on x86. */
void instr_flags(unsigned val, unsigned smask, unsigned long *eflags)
{
  unsigned long flags;

  *eflags &= ~(OF|ZF|SF|PF|CF);
  if (val & smask)
    *eflags |= SF;
  OPandFLAG1(flags, orl, val, =r);
  *eflags |= flags & (ZF|PF);
}

/* 6 logical and arithmetic "RISC" core functions
   follow
*/
unsigned char instr_binary_byte(unsigned char op, unsigned char op1, unsigned char op2, unsigned long *eflags)
{
  unsigned long flags;
  
  switch (op&0x7){
  case 1: /* or */
    OPandFLAG(flags, orb, op1, op2, =q, q);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return op1;
  case 4: /* and */
    OPandFLAG(flags, andb, op1, op2, =q, q);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return op1;
  case 6: /* xor */
    OPandFLAG(flags, xorb, op1, op2, =q, q);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return op1;
  case 0: /* add */
    *eflags &= ~CF; /* Fall through */
  case 2: /* adc */
    flags = *eflags;
    OPandFLAGC(flags, adcb, op1, op2, =q, q);
    *eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
    return op1;
  case 5: /* sub */
  case 7: /* cmp */
    *eflags &= ~CF; /* Fall through */
  case 3: /* sbb */
    flags = *eflags;
    OPandFLAGC(flags, sbbb, op1, op2, =q, q);
    *eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
    return op1;
  }
  return 0;
}

unsigned instr_binary_word(unsigned op, unsigned op1, unsigned op2, unsigned long *eflags)
{
  unsigned long flags;
  unsigned short opw1 = op1;
  unsigned short opw2 = op2;
  
  switch (op&0x7){
  case 1: /* or */
    OPandFLAG(flags, orw, opw1, opw2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return opw1;
  case 4: /* and */
    OPandFLAG(flags, andw, opw1, opw2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return opw1;
  case 6: /* xor */
    OPandFLAG(flags, xorw, opw1, opw2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return opw1;
  case 0: /* add */
    *eflags &= ~CF; /* Fall through */
  case 2: /* adc */
    flags = *eflags;
    OPandFLAGC(flags, adcw, opw1, opw2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
    return opw1;
  case 5: /* sub */
  case 7: /* cmp */
    *eflags &= ~CF; /* Fall through */
  case 3: /* sbb */
    flags = *eflags;
    OPandFLAGC(flags, sbbw, opw1, opw2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
    return opw1;
  }
  return 0;
}

unsigned instr_binary_dword(unsigned op, unsigned op1, unsigned op2, unsigned long *eflags)
{
  unsigned long flags;
  
  switch (op&0x7){
  case 1: /* or */
    OPandFLAG(flags, orl, op1, op2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return op1;
  case 4: /* and */
    OPandFLAG(flags, andl, op1, op2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return op1;
  case 6: /* xor */
    OPandFLAG(flags, xorl, op1, op2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|PF|CF)) | (flags & (OF|ZF|SF|PF|CF));
    return op1;
  case 0: /* add */
    *eflags &= ~CF; /* Fall through */
  case 2: /* adc */
    flags = *eflags;
    OPandFLAGC(flags, adcl, op1, op2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
    return op1;
  case 5: /* sub */
  case 7: /* cmp */
    *eflags &= ~CF; /* Fall through */
  case 3: /* sbb */
    flags = *eflags;
    OPandFLAGC(flags, sbbl, op1, op2, =r, r);
    *eflags = (*eflags & ~(OF|ZF|SF|AF|PF|CF)) | (flags & (OF|ZF|AF|SF|PF|CF));
    return op1;
  }
  return 0;
}

unsigned instr_shift(unsigned op, int op1, unsigned op2, unsigned size, unsigned long *eflags)
{
  unsigned result, carry;
  unsigned width = size * 8;
  unsigned mask = wordmask[size];
  unsigned smask = (mask >> 1) + 1;
  op2 &= 31;
        
  switch (op&0x7){
  case 0: /* rol */
    op2 &= width-1;
    result = (((op1 << op2) | ((op1&mask) >> (width-op2)))) & mask;
    *eflags &= ~(CF|OF);
    *eflags |= (result & CF) | ((((result >> (width-1)) ^ result) << 11) & OF);
    return result;
  case 1:/* ror */
    op2 &= width-1;
    result = ((((op1&mask) >> op2) | (op1 << (width-op2)))) & mask;
    *eflags &= ~(CF|OF);
    carry = (result >> (width-1)) & CF;
    *eflags |=  carry |
      (((carry ^ (result >> (width-2))) << 11) & OF);
    return result;
  case 2: /* rcl */
    op2 %= width+1;
    carry = (op1>>(width-op2))&CF;
    result = (((op1 << op2) | ((op1&mask) >> (width+1-op2))) | ((*eflags&CF) << (op2-1))) & mask;
    *eflags &= ~(CF|OF);
    *eflags |= carry | ((((result >> (width-1)) ^ carry) << 11) & OF);
    return result;
  case 3:/* rcr */
    op2 %= width+1;
    carry = (op1>>(op2-1))&CF;
    result = ((((op1&mask) >> op2) | (op1 << (width+1-op2))) | ((*eflags&CF) << (width-op2))) & mask;
    *eflags &= ~(CF|OF);
    *eflags |= carry | ((((result >> (width-1)) ^ (result >> (width-2))) << 11) & OF);
    return result;
  case 4: /* shl */        
    result = (op1 << op2) & mask;
    instr_flags(result, smask, eflags);
    *eflags &= ~(CF|OF);
    *eflags |= ((op1 >> (width-op2))&CF) |
      ((((op1 >> (width-1)) ^ (op1 >> (width-2))) << 11) & OF);
    return result;
  case 5: /* shr */        
    result = ((unsigned)(op1&mask) >> op2);
    instr_flags(result, smask, eflags);
    *eflags &= ~(CF|OF);
    *eflags |= ((op1 >> (op2-1)) & CF) | (((op1 >> (width-1)) << 11) & OF);
    return result;
  case 7: /* sar */
    result = op1 >> op2;
    instr_flags(result, smask, eflags);
    *eflags &= ~(CF|OF);
    *eflags |= (op1 >> (op2-1)) & CF;
    return result;
  }
  return 0;
}

static inline void push(unsigned val, x86_regs *x86)
{
  unsigned char *mem; 
  
  if (x86->_32bit)
    x86->esp -= x86->operand_size;
  else
    SP -= x86->operand_size;
  mem = x86->ss_base + (x86->esp & wordmask[(x86->_32bit+1)*2]);
  if (x86->operand_size == 4) 
    instr_write_dword(mem, val);
  else
    instr_write_word(mem, val);
}

static inline void pop(unsigned *val, x86_regs *x86)
{
  unsigned char *mem = x86->ss_base + (x86->esp & wordmask[(x86->_32bit+1)*2]);
  if (x86->_32bit)
    x86->esp += x86->operand_size;
  else
    SP += x86->operand_size;
  *val = (x86->operand_size == 4 ? instr_read_dword(mem) : instr_read_word(mem));
}

/*
 * DANG_BEGIN_FUNCTION instr_sim
 *
 * description:
 * instr_sim is used to simulate instructions that access the
 * VGA video memory in planar modes when using X as the video output
 * device.  
 *
 * It is necessary to do this in order to simulate the effects 
 * of the hardware VGA controller in X mode.
 *
 * If the return value is 0, it means the instruction was not one
 * that for which a simulation is provided.  The return value is 1 for success,
 * but the function exits because we need to go back to the DOSEMU's main loop
 * or count runs out.
 *
 * arguments:
 * x86: the structure holding everything about the cpu-state we need.
 *
 * DANG_END_FUNCTION                        
 */

/* helper functions/macros reg8/reg/sreg/sib/modrm16/32 for instr_sim
   for address and register decoding */

#define reg8(reg, x86) (((unsigned char *)(x86))+((reg)&0x3)*4+(((reg)>>2)&1))
#define reg(reg, x86) (((unsigned *)(x86))+((reg)&0x7))
#define sreg(reg, x86) (((unsigned *)(&((x86)->es)))+((reg)&0x7))
#define sreg_idx(reg) (es_INDEX+((reg)&0x7))

unsigned char *sib(unsigned char *cp, x86_regs *x86, int *inst_len)
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
    return addr + *reg(cp[2], x86) + x86->seg_base;
  case 0x04: /* esp */
    return addr + x86->esp + x86->seg_ss_base;
  case 0x05: 
    if (cp[1] >= 0x40)
      return addr + x86->ebp + x86->seg_ss_base;
    else {
      *inst_len += 4; 
      return addr + R_DWORD(cp[3]) + x86->seg_base;
    }	  
  }
  return 0; /* keep gcc happy */
}

unsigned char *modrm16(unsigned char *cp, x86_regs *x86, int *inst_len)
{
  unsigned addr = 0;
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
      return (unsigned char *)reg(cp[1], x86);
    else
      return reg8(cp[1], x86);
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

unsigned char *modrm32(unsigned char *cp, x86_regs *x86, int *inst_len)
{
  unsigned addr = 0;
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
      return ((unsigned char *)reg(cp[1], x86));
    else
      return reg8(cp[1], x86);
  }
  switch(cp[1] & 0x07) { /* decode address */
  case 0x00: 
  case 0x01: 
  case 0x02: 
  case 0x03: 
  case 0x06: 
  case 0x07: 
    return addr + *reg(cp[1], x86) + x86->seg_base;
  case 0x04: /* sib byte follows */
    *inst_len += 1;
    return sib(cp, x86, inst_len);
  case 0x05: 
    if (cp[1] >= 0x40)
      return addr + x86->ebp + x86->seg_ss_base;
    else {
      *inst_len += 4; 
      return R_DWORD(cp[2]) + x86->seg_base;
    }	  
  }
  return 0; /* keep gcc happy */
}

static int handle_prefixes(x86_regs *x86)
{
  unsigned eip = x86->eip;
  unsigned char *cs = (unsigned char *) x86->cs_base;
  int prefix = 0;

  for (;;) {
    switch(cs[eip++]) {
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
          x86->instr_binary = instr_binary_dword;
          x86->instr_read = instr_read_dword;
          x86->instr_write = instr_write_dword;
        } else {
          x86->instr_binary = instr_binary_word;
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
    x86->instr_binary = instr_binary_dword;
    x86->instr_read = instr_read_dword;
    x86->instr_write = instr_write_dword;
  } else {
    x86->instr_binary = instr_binary_word;
    x86->instr_read = instr_read_word;
    x86->instr_write = instr_write_word;
  }
}

/* return value: 1 => instruction known; 0 => instruction not known */
static inline int instr_sim(x86_regs *x86, int pmode)
{
  unsigned char *reg_8;
  unsigned char uc;
  unsigned short uns;
  unsigned *dstreg;
  unsigned und, und2, repcount;
  unsigned long unl;
  unsigned char *mem;
  int i, i2, inst_len;
  int loop_inc = (EFLAGS&DF) ? -1 : 1;		// make it a char ?
  unsigned eip = x86->eip;
  unsigned char *cs = (unsigned char *) x86->cs_base;

#if DEBUG_INSTR >= 2
  {
    int refseg, rc;
    unsigned char frmtbuf[256];
    refseg = x86->cs;
    dump_x86_regs(x86);
    rc = dis_8086(cs+eip, frmtbuf, x86->_32bit, &refseg, cs);
    instr_deb("vga_emu_fault: about to simulate %d: %s\n", count, frmtbuf);
  }
#endif

  if (x86->prefixes) {
    prepare_x86(x86);
  }
  
  x86->prefixes = handle_prefixes(x86);
  eip += x86->prefixes;
  
  if (x86->rep != REP_NONE) {
    /* TODO: All these rep instruction can still be heavily optimized */
    i2 = 0;
    if (x86->address_size == 4) {
      repcount = x86->ecx;
      switch(cs[eip]) {
      case 0xa4:         /* rep movsb */
#if DEBUG_INSTR >= 1      
        if (x86->es_base >= 0xa0000 && x86->es_base < 0xb0000 &&
            x86->seg_base >= 0xa0000 && x86->seg_base < 0xb0000)
          instr_deb("VGAEMU: Video to video memcpy, ecx=%x\n", x86->ecx);
        /* TODO: accelerate this using memcpy */
#endif
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0;
             i += loop_inc, und++, count--) 
          instr_write_byte(x86->es_base + x86->edi+i,
            instr_read_byte(x86->seg_base + x86->esi+i));
        x86->edi += i;
        x86->esi += i;
        break;
        
      case 0xa5:         /* rep movsw/d */
        /* TODO: accelerate this using memcpy */
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0;
             i += loop_inc*x86->operand_size, und++, count--) 
          x86->instr_write(x86->es_base + x86->edi+i,
            x86->instr_read(x86->seg_base + x86->esi+i));
        x86->edi += i;
        x86->esi += i;
        break;
        
      case 0xa6:         /* rep cmpsb */
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0; count--) {
          instr_binary_byte(7, instr_read_byte(x86->seg_base + x86->esi+i),
            instr_read_byte(x86->es_base + x86->edi+i), &EFLAGS);
          i += loop_inc;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0xf2 repnz 0xf3 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        x86->edi += i;
        x86->esi += i;
        break;
        
      case 0xa7:         /* rep cmpsw/d */
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0; count--) {
          x86->instr_binary(7, instr_read_byte(x86->seg_base + x86->esi+i),
            x86->instr_read(x86->es_base + x86->edi+i), &EFLAGS);
          i += loop_inc*x86->operand_size;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0xf2 repnz 0xf3 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        x86->edi += i;
        x86->esi += i;
        break;
        
      case 0xaa: /* rep stosb */
        /* TODO: accelerate this using memset */
        for (und2 = x86->edi, und = 0; und < repcount && !signal_pending && count>0;
             und2 += loop_inc, und++, count--) 
          instr_write_byte(x86->es_base + und2, AL);
        x86->edi = und2;
        break;
        
      case 0xab: /* rep stosw */
        /* TODO: accelerate this using memset */
        for (und2 = x86->edi, und = 0; und < repcount && !signal_pending && count>0;
             und2 += loop_inc*x86->operand_size, und++, count--) 
          x86->instr_write(x86->es_base + und2, x86->eax);
        x86->edi = und2;
        break;

      case 0xae: /* rep scasb */
        for (und2 = x86->edi, und = 0; und < repcount && !signal_pending && count>0; count--) {
          instr_binary_byte(7, AL, instr_read_byte(x86->es_base + und2), &EFLAGS);
          und2 += loop_inc;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        x86->edi = und2;
        break;
        
      case 0xaf: /* rep scasw */
        for (und2 = x86->edi, und = 0; und < repcount && !signal_pending && count>0; count--) {
          x86->instr_binary(7, x86->eax, x86->instr_read(x86->es_base + und2), &EFLAGS);
          und2 += loop_inc*x86->operand_size;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        x86->edi = und2;
        break;
        
      default:
        return 0;
      }
      
      x86->ecx -= und;
      if (x86->ecx > 0 && i2 == 0) return 1;
        
    } else {
      repcount = CX;
      switch(cs[eip]) {
      case 0xa4:         /* rep movsb */
#if DEBUG_INSTR >= 1      
        if (x86->es_base >= 0xa0000 && x86->es_base < 0xb0000 &&
            x86->seg_base >= 0xa0000 && x86->seg_base < 0xb0000)
          instr_deb("VGAEMU: Video to video memcpy, cx=%x\n", CX);
        /* TODO: accelerate this using memcpy */
#endif
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0;
             i += loop_inc, und++, count--) 
          instr_write_byte(x86->es_base + ((x86->edi+i) & 0xffff),
            instr_read_byte(x86->seg_base + ((x86->esi+i) & 0xffff)));
        DI += i;
        SI += i;
        break;
        
      case 0xa5:         /* rep movsw/d */
        /* TODO: accelerate this using memcpy */
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0;
             i += loop_inc*x86->operand_size, und++, count--) 
          x86->instr_write(x86->es_base + ((x86->edi+i) & 0xffff),
            x86->instr_read(x86->seg_base + ((x86->esi+i) & 0xffff)));
        DI += i;
        SI += i;
        break;
        
      case 0xa6: /* rep?z cmpsb */
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0; count--) {
          instr_binary_byte(7, instr_read_byte(x86->seg_base + ((x86->esi+i) & 0xffff)),
            instr_read_byte(x86->es_base + ((x86->edi+i) & 0xffff)), &EFLAGS);
          i += loop_inc;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        DI += i;
        SI += i;
        break;
        
      case 0xa7: /* rep?z cmpsw/d */
        for (i = 0, und = 0; und < repcount && !signal_pending && count>0; count--) {
          x86->instr_binary(7, x86->instr_read(x86->seg_base + ((x86->esi+i) & 0xffff)),
                      x86->instr_read(x86->es_base + ((x86->edi+i) & 0xffff)), &EFLAGS);
          i += loop_inc * x86->operand_size;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        DI += i;
        SI += i;
        break;
    
      case 0xaa: /* rep stosb */
        /* TODO: accelerate this using memset */
        for (uns = DI, und = 0; und < repcount && !signal_pending && count>0;
             uns += loop_inc, und++, count--) 
          instr_write_byte(x86->es_base + uns, AL);
        DI = uns;
        break;
        
      case 0xab: /* rep stosw/d */
        /* TODO: accelerate this using memset */
        for (uns = DI, und = 0; und < repcount && !signal_pending && count>0;
             uns += loop_inc*x86->operand_size, und++, count--) 
          x86->instr_write(x86->es_base + uns, (x86->operand_size == 4 ? x86->eax : AX));
        DI = uns;
        break;
        
      case 0xae: /* rep scasb */
        for (uns = DI, und = 0; und < repcount && !signal_pending && count>0; count--) {
          instr_binary_byte(7, AL, instr_read_byte(x86->es_base + uns), &EFLAGS);          
          uns += loop_inc;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        DI = uns;
        break;
        
      case 0xaf: /* rep scasw/d */
        for (uns = DI, und = 0; und < repcount && !signal_pending && count>0; count--) {
          x86->instr_binary(7, AX, instr_read_word(x86->es_base + uns), &EFLAGS);
          uns += loop_inc*x86->operand_size;
          und++;
          if (((EFLAGS & ZF) >> 6) != x86->rep) /* 0x0 repnz 0x1 repz */ {
            i2 = 1; /* we're fine now! */
            break;
          }
        }
        DI = uns;
        break;
      
      default:
        return 0;
      }
      CX -= und;
      if (CX > 0 && i2 == 0) return 1;
    }
    eip++;
  }
  else switch(cs[eip]) {
  case 0x00:		/* add r/m8,reg8 */
  case 0x08:		/* or r/m8,reg8 */
  case 0x10:		/* adc r/m8,reg8 */
  case 0x18:		/* sbb r/m8,reg8 */
  case 0x20:		/* and r/m8,reg8 */
  case 0x28:		/* sub r/m8,reg8 */
  case 0x30:		/* xor r/m8,reg8 */
  case 0x38:		/* cmp r/m8,reg8 */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    uc = instr_binary_byte(cs[eip]>>3, instr_read_byte(mem), *reg8(cs[eip + 1]>>3, x86), &EFLAGS);
    if (cs[eip]<0x38)
      instr_write_byte(mem, uc);
    eip += 2 + inst_len; break;
            
  case 0x01:		/* add r/m16,reg16 */
  case 0x09:		/* or r/m16,reg16 */
  case 0x11:		/* adc r/m16,reg16 */
  case 0x19:		/* sbb r/m16,reg16 */
  case 0x21:		/* and r/m16,reg16 */
  case 0x29:		/* sub r/m16,reg16 */
  case 0x31:		/* xor r/m16,reg16 */
  case 0x39:		/* cmp r/m16,reg16 */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    und = x86->instr_binary(cs[eip]>>3, x86->instr_read(mem), *reg(cs[eip + 1]>>3, x86), &EFLAGS);
    if (cs[eip]<0x38)
      x86->instr_write(mem, und);
    eip += 2 + inst_len; break;

  case 0x02:		/* add reg8,r/m8 */
  case 0x0a:		/* or reg8,r/m8 */
  case 0x12:		/* adc reg8,r/m8 */
  case 0x1a:		/* sbb reg8,r/m8 */
  case 0x22:		/* and reg8,r/m8 */
  case 0x2a:		/* sub reg8,r/m8 */
  case 0x32:		/* xor reg8,r/m8 */
  case 0x3a:		/* cmp reg8,r/m8 */
    reg_8 = reg8(cs[eip + 1]>>3, x86);
    uc = instr_binary_byte(cs[eip]>>3, *reg_8, instr_read_byte(x86->modrm(cs + eip, x86, &inst_len)), &EFLAGS);
    if (cs[eip]<0x38) *reg_8 = uc;
    eip += 2 + inst_len; break;

  case 0x03:		/* add reg,r/m16 */
  case 0x0b:		/* or reg,r/m16 */
  case 0x13:		/* adc reg,r/m16 */
  case 0x1b:		/* sbb reg,r/m16 */
  case 0x23:		/* and reg,r/m16 */
  case 0x2b:		/* sub reg,r/m16 */
  case 0x33:		/* xor reg,r/m16 */
  case 0x3b:		/* cmp reg,r/m16 */
    dstreg = reg(cs[eip + 1]>>3, x86);
    und = x86->instr_binary(cs[eip]>>3, *dstreg, x86->instr_read(x86->modrm(cs + eip, x86, &inst_len)), &EFLAGS);
    if (cs[eip]<0x38) {
      if (x86->operand_size == 2)
        R_WORD(*dstreg) = und;
      else
        *dstreg = und;
    }
    eip += 2 + inst_len; break;

  case 0x04:		/* add al,imm8 */
  case 0x0c:		/* or al,imm8 */
  case 0x14:		/* adc al,imm8 */
  case 0x1c:		/* sbb al,imm8 */
  case 0x24:		/* and al,imm8 */
  case 0x2c:		/* sub al,imm8 */
  case 0x34:		/* xor al,imm8 */
  case 0x3c:		/* cmp al,imm8 */
    uc = instr_binary_byte(cs[eip]>>3, AL, cs[eip + 1], &EFLAGS);
    if (cs[eip]<0x38) AL = uc;
    eip += 2; break;
            
  case 0x05:		/* add ax,imm16 */
  case 0x0d:		/* or ax,imm16 */
  case 0x15:		/* adc ax,imm16 */
  case 0x1d:		/* sbb ax,imm16 */
  case 0x25:		/* and ax,imm16 */
  case 0x2d:		/* sub ax,imm16 */
  case 0x35:		/* xor ax,imm16 */
  case 0x3d:		/* cmp ax,imm16 */
    und = x86->instr_binary(cs[eip]>>3, x86->eax, R_DWORD(cs[eip + 1]), &EFLAGS);
    if (x86->operand_size == 2) { 
      if (cs[eip]<0x38) AX = und;
      eip += 3;
    } else {
      if (cs[eip]<0x38) x86->eax = und;
      eip += 5; 
    }
    break;
    
  case 0x06:  /* push sreg */
  case 0x0e:
  case 0x16:
  case 0x1e:        
    push(*sreg(cs[eip]>>3, x86), x86);
    eip++; break;

  case 0x07:		/* pop es */  
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      pop(&x86->es, x86);
      REG(es)  = x86->es;
      x86->es_base = SEG2LINEAR(x86->es);
      eip++; 
    }   
    break;
    
  /* don't do 0x0f (extended instructions) for now */ 
  /* 0x17 pop ss is a bit dangerous and rarely used */  

  case 0x1f:		/* pop ds */  
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      pop(&x86->ds, x86);
      REG(ds)  = x86->ds;
      x86->ds_base = SEG2LINEAR(x86->ds);
      x86->seg_base = x86->ds_base;
      eip++; 
    }
    break;

  case 0x27:  /* daa */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL += 6;
      EFLAGS |= AF;
    } else                  
      EFLAGS &= ~AF;
    if ((AL > 0x9f) || (EFLAGS&CF)) {
      AL += 0x60;
      instr_flags(AL, 0x80, &EFLAGS);
      EFLAGS |= CF;
    } else 
      instr_flags(AL, 0x80, &EFLAGS);
    eip++; break;
    
  case 0x2f:  /* das */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL -= 6;
      EFLAGS |= AF;
    } else                  
      EFLAGS &= ~AF;
    if ((AL > 0x9f) || (EFLAGS&CF)) {
      AL -= 0x60;
      instr_flags(AL, 0x80, &EFLAGS);
      EFLAGS |= CF;
    } else
      instr_flags(AL, 0x80, &EFLAGS);
    eip++; break;  

  case 0x37:  /* aaa */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL = (x86->eax+6) & 0xf;
      AH++;
      EFLAGS |= (CF|AF);
    } else                  
      EFLAGS &= ~(CF|AF);
    eip++; break;  
      
  case 0x3f:  /* aas */
    if (((AL & 0xf) > 9) || (EFLAGS&AF)) {
      AL = (x86->eax-6) & 0xf;
      AH--;
      EFLAGS |= (CF|AF);
    } else                  
      EFLAGS &= ~(CF|AF);
    eip++; break;  
      
  case 0x40:
  case 0x41:
  case 0x42:
  case 0x43:
  case 0x44:
  case 0x45:
  case 0x46:
  case 0x47: /* inc reg */
    EFLAGS &= ~(OF|ZF|SF|PF|AF);
    dstreg = reg(cs[eip], x86);
    if (x86->operand_size == 2) {
      OPandFLAG0(unl, incw, *((unsigned short *)dstreg), =r);
    } else {
      OPandFLAG0(unl, incl, *dstreg, =r);
    }
    EFLAGS |= unl & (OF|ZF|SF|PF|AF);
    eip++; break;
       
  case 0x48:
  case 0x49:
  case 0x4a:
  case 0x4b:
  case 0x4c:
  case 0x4d:
  case 0x4e:
  case 0x4f: /* dec reg */
    EFLAGS &= ~(OF|ZF|SF|PF|AF);
    dstreg = reg(cs[eip], x86);
    if (x86->operand_size == 2) {
      OPandFLAG0(unl, decw, *((unsigned short *)dstreg), =r);
    } else {
      OPandFLAG0(unl, decl, *dstreg, =r);
    }
    EFLAGS |= unl & (OF|ZF|SF|PF|AF);
    eip++; break;
       
  case 0x50:
  case 0x51:
  case 0x52:
  case 0x53:
  case 0x54:
  case 0x55:
  case 0x56:
  case 0x57: /* push reg */
    push(*reg(cs[eip],x86), x86);
    eip++; break;
    
  case 0x58:
  case 0x59:
  case 0x5a:
  case 0x5b:
  case 0x5c:
  case 0x5d:
  case 0x5e:
  case 0x5f: /* pop reg */
    pop(&und, x86);
    if (x86->operand_size == 2)
      R_WORD(*reg(cs[eip],x86)) = und;
    else
      *reg(cs[eip],x86) = und;
    eip++; break;

    /* 0x60 */
  case 0x68: /* push imm16 */
    push(R_DWORD(cs[eip + 1]), x86);
    eip += x86->operand_size + 1; break;
         
  case 0x6a: /* push imm8 */
    push((int)(signed char)cs[eip + 1], x86);
    eip += 2; break;
         
  case 0x70: OP_JCC(EFLAGS & OF);         /*jo*/
  case 0x71: OP_JCC(!(EFLAGS & OF));      /*jno*/
  case 0x72: OP_JCC(EFLAGS & CF);         /*jc*/
  case 0x73: OP_JCC(!(EFLAGS & CF));      /*jnc*/
  case 0x74: OP_JCC(EFLAGS & ZF);         /*jz*/
  case 0x75: OP_JCC(!(EFLAGS & ZF));      /*jnz*/
  case 0x76: OP_JCC(EFLAGS & (ZF|CF));    /*jbe*/
  case 0x77: OP_JCC(!(EFLAGS & (ZF|CF))); /*ja*/
  case 0x78: OP_JCC(EFLAGS & SF);         /*js*/
  case 0x79: OP_JCC(!(EFLAGS & SF));      /*jns*/
  case 0x7a: OP_JCC(EFLAGS & PF);         /*jp*/
  case 0x7b: OP_JCC(!(EFLAGS & PF));      /*jnp*/
  case 0x7c: OP_JCC((EFLAGS & SF)^((EFLAGS & OF)>>4))         /*jl*/
  case 0x7d: OP_JCC(!((EFLAGS & SF)^((EFLAGS & OF)>>4)))      /*jnl*/
  case 0x7e: OP_JCC((EFLAGS & (SF|ZF))^((EFLAGS & OF)>>4))    /*jle*/
  case 0x7f: OP_JCC(!((EFLAGS & (SF|ZF))^((EFLAGS & OF)>>4))) /*jg*/

  case 0x80:		/* logical r/m8,imm8 */
  case 0x82:
    mem = x86->modrm(cs + eip, x86, &inst_len);
    uc = instr_binary_byte(cs[eip + 1]>>3, instr_read_byte(mem), cs[eip + 2 + inst_len], &EFLAGS);
    if ((cs[eip + 1]&0x38) < 0x38)
      instr_write_byte(mem, uc);
    eip += 3 + inst_len; break;

  case 0x81:		/* logical r/m,imm */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    und = x86->instr_binary(cs[eip + 1]>>3, x86->instr_read(mem), R_DWORD(cs[eip + 2 + inst_len]), &EFLAGS);
    if ((cs[eip + 1]&0x38) < 0x38) x86->instr_write(mem, und);
    eip += x86->operand_size + 2 + inst_len; 
    break;

  case 0x83:		/* logical r/m,imm8 */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    und = x86->instr_binary(cs[eip + 1]>>3, x86->instr_read(mem), (int)(signed char)cs[eip + 2 + inst_len],
        &EFLAGS);
    if ((cs[eip + 1]&0x38) < 0x38) x86->instr_write(mem, und);
    eip += inst_len + 3; break;

  case 0x84: /* test r/m8, reg8 */
    instr_flags(instr_read_byte(x86->modrm(cs + eip, x86, &inst_len)) & *reg8(cs[eip + 1]>>3,x86),
                0x80, &EFLAGS);
    eip += inst_len + 2; break;
      
  case 0x85: /* test r/m16, reg */
    if (x86->operand_size == 2) 
      instr_flags(instr_read_word(x86->modrm(cs + eip, x86, &inst_len)) & R_WORD(*reg(cs[eip + 1]>>3,x86)),
                0x8000, &EFLAGS);
    else
      instr_flags(instr_read_dword(x86->modrm(cs + eip, x86, &inst_len)) & *reg(cs[eip + 1]>>3,x86),
                0x80000000, &EFLAGS);
    eip += inst_len + 2; break;
      
  case 0x86:		/* xchg r/m8,reg8 */
    reg_8 = reg8(cs[eip + 1]>>3, x86);
    mem = x86->modrm(cs + eip, x86, &inst_len);
    uc = *reg_8;
    *reg_8 = instr_read_byte(mem);
    instr_write_byte(mem, uc);
    eip += inst_len + 2; break;
      
  case 0x87:		/* xchg r/m16,reg */
    dstreg = reg(cs[eip + 1]>>3, x86);
    mem = x86->modrm(cs + eip, x86, &inst_len);
    und = *dstreg;
    if (x86->operand_size == 2)
      R_WORD(*dstreg) = instr_read_word(mem);
    else
      *dstreg = instr_read_dword(mem);
    x86->instr_write(mem, und);
    eip += inst_len + 2; break;

  case 0x88:		/* mov r/m8,reg8 */
    instr_write_byte(x86->modrm(cs + eip, x86, &inst_len), *reg8(cs[eip + 1]>>3, x86));
    eip += inst_len + 2; break;

  case 0x89:		/* mov r/m16,reg */
    x86->instr_write(x86->modrm(cs + eip, x86, &inst_len), *reg(cs[eip + 1]>>3, x86));
    eip += inst_len + 2; break;

  case 0x8a:		/* mov reg8,r/m8 */
    *reg8(cs[eip + 1]>>3, x86) = instr_read_byte(x86->modrm(cs + eip, x86, &inst_len));
    eip += inst_len + 2; break;

  case 0x8b:		/* mov reg,r/m16 */
    if (x86->operand_size == 2)
      R_WORD(*reg(cs[eip + 1]>>3, x86)) = instr_read_word(x86->modrm(cs + eip, x86, &inst_len));
    else
      *reg(cs[eip + 1]>>3, x86) = instr_read_dword(x86->modrm(cs + eip, x86, &inst_len));
    eip += inst_len + 2; break;

  case 0x8c: /* mov r/m16,segreg */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    if ((cs[eip + 1] & 0xc0) == 0xc0) /* compensate for mov r,segreg */
      mem = (unsigned char *)reg(cs[eip + 1], x86);
    instr_write_word(mem, *sreg(cs[eip + 1]>>3, x86));
    eip += inst_len + 2; break;

  case 0x8d: /* lea */
    mem = x86->seg_ss_base;
    x86->seg_ss_base = x86->seg_base;
    if (x86->operand_size == 2)
      R_WORD(*reg(cs[eip + 1]>>3,x86)) = x86->modrm(cs + eip, x86, &inst_len) - x86->seg_base;
    else
      *reg(cs[eip + 1]>>3,x86) = x86->modrm(cs + eip, x86, &inst_len) - x86->seg_base;
    x86->seg_ss_base = mem;
    eip += inst_len + 2; break;

  case 0x8e:		/* mov segreg,r/m16 */
    if (pmode || x86->operand_size == 4) 
      return 0;
    else switch (cs[eip + 1]&0x38) {
    case 0:      
      mem = x86->modrm(cs + eip, x86, &inst_len);
      if ((cs[eip + 1] & 0xc0) == 0xc0)  /* compensate for mov r,segreg */
        mem = (unsigned char *)reg(cs[eip + 1], x86);
      REG(es) = x86->es = instr_read_word(mem);
      x86->es_base = SEG2LINEAR(x86->es);
      eip += inst_len + 2; break;
    case 0x18:  
      mem = x86->modrm(cs + eip, x86, &inst_len);
      if ((cs[eip + 1] & 0xc0) == 0xc0) /* compensate for mov es,reg */
	mem = (unsigned char *)reg(cs[eip + 1], x86);
      REG(ds) = x86->ds = instr_read_word(mem);
      x86->ds_base = SEG2LINEAR(x86->ds);
      x86->seg_base = x86->ds_base;
      eip += inst_len + 2; break;
    default:
      return 0;
    }
    break;

  case 0x8f: /*pop*/
    if ((cs[eip + 1]&0x38) == 0){
      pop(&und, x86);
      x86->instr_write(x86->modrm(cs + eip, x86, &inst_len), und);
      eip += inst_len + 2; 
    } else
      return 0;
    break;

  case 0x90: /* nop */
    eip++; break;
  case 0x91:
  case 0x92:
  case 0x93:
  case 0x94:
  case 0x95:
  case 0x96:
  case 0x97: /* xchg reg, ax */
    dstreg = reg(cs[eip],x86);
    und = x86->eax;
    if (x86->operand_size == 2) {
      AX = *dstreg;
      R_WORD(*dstreg) = und;
    } else {
      x86->eax = *dstreg;
      *dstreg = und;
    }
    eip++; break;

  case 0x98: 
    if (x86->operand_size == 2) /* cbw */
      AX = (short)(signed char)AL;
    else /* cwde */
      x86->eax = (int)(short)AX;
    eip++; break;

  case 0x99:
    if (x86->operand_size == 2) /* cwd */
      DX = (AX > 0x7fff ? 0xffff : 0);
    else /* cdq */
      x86->edx = (x86->eax > 0x7fffffff ? 0xffffffff : 0);
    eip++; break;

  case 0x9a: /*call far*/
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      push(x86->cs, x86);
      push(eip + 5, x86);
      x86->cs = R_WORD(cs[eip + 3]);
      REG(cs)  = x86->cs;
      x86->cs_base = SEG2LINEAR(x86->cs);
      eip = R_WORD(cs[eip + 1]); 
      cs = (unsigned char *)x86->cs_base;
    }
    break;
    /* NO: 0x9b wait 0x9c pushf 0x9d popf*/    

  case 0x9e: /* sahf */
    R_LO(EFLAGS) = AH;
    eip++; break;

  case 0x9f: /* lahf */
    AH = R_LO(EFLAGS);
    eip++; break;

  case 0xa0:		/* mov al,moff16 */
    AL = instr_read_byte((R_DWORD(cs[eip + 1]) & wordmask[x86->address_size])+x86->seg_base);
    eip += 1 + x86->address_size; break;

  case 0xa1:		/* mov ax,moff16 */
    if (x86->operand_size == 2)
      AX = instr_read_word((R_DWORD(cs[eip + 1]) & wordmask[x86->address_size])+x86->seg_base);
    else
      x86->eax = instr_read_dword((R_DWORD(cs[eip + 1]) & wordmask[x86->address_size])+x86->seg_base);
    eip += 1 + x86->address_size; break;
   
  case 0xa2:		/* mov moff16,al */
    instr_write_byte((R_DWORD(cs[eip + 1]) & wordmask[x86->address_size])+x86->seg_base, AL);
    eip += 1 + x86->address_size; break;

  case 0xa3:		/* mov moff16,ax */
    x86->instr_write((R_DWORD(cs[eip + 1]) & wordmask[x86->address_size])+x86->seg_base, x86->eax);
    eip += 1 + x86->address_size; break;

  case 0xa4:		/* movsb */
    if (x86->address_size == 4) {
      instr_write_byte(x86->es_base + x86->edi, instr_read_byte(x86->seg_base + x86->esi));
      x86->edi += loop_inc;
      x86->esi += loop_inc;
    } else {
      instr_write_byte(x86->es_base + DI, instr_read_byte(x86->seg_base + SI));
      DI += loop_inc;
      SI += loop_inc;
    }
    eip++; break;

  case 0xa5:		/* movsw */
    if (x86->address_size == 4) {
      x86->instr_write(x86->es_base + x86->edi, x86->instr_read(x86->seg_base + x86->esi));
      x86->edi += loop_inc * x86->operand_size;
      x86->esi += loop_inc * x86->operand_size;
    }
    else {
      x86->instr_write(x86->es_base + DI, x86->instr_read(x86->seg_base + SI));
      DI += loop_inc * x86->operand_size;
      SI += loop_inc * x86->operand_size;
    }
    eip++; break;

  case 0xa6: /*cmpsb */
    if (x86->address_size == 4) {
      instr_binary_byte(7, instr_read_byte(x86->seg_base + x86->esi),
                  instr_read_byte(x86->es_base + x86->edi), &EFLAGS);
      x86->edi += loop_inc;
      x86->esi += loop_inc;
    } else {
      instr_binary_byte(7, instr_read_byte(x86->seg_base + SI),
                  instr_read_byte(x86->es_base + DI), &EFLAGS);
      DI += loop_inc;
      SI += loop_inc;
    }
    eip++; break;

  case 0xa7: /* cmpsw */
    if (x86->address_size == 4) {
      x86->instr_binary(7, x86->instr_read(x86->seg_base + x86->esi),
                  x86->instr_read(x86->es_base + x86->edi), &EFLAGS);
      x86->edi += loop_inc * x86->operand_size;
      x86->esi += loop_inc * x86->operand_size;
    } else {
      x86->instr_binary(7, x86->instr_read(x86->seg_base + SI),
                  x86->instr_read(x86->es_base + DI), &EFLAGS);
      DI += loop_inc * x86->operand_size;
      SI += loop_inc * x86->operand_size;
    }
    eip++; break;

  case 0xa8: /* test al, imm */
    instr_flags(AL & cs[eip + 1], 0x80, &EFLAGS);
    eip += 2; break;

  case 0xa9: /* test ax, imm */
    if (x86->operand_size == 2) {
      instr_flags(AX & R_WORD(cs[eip + 1]), 0x8000, &EFLAGS);
      eip += 3; break;
    } else {
      instr_flags(x86->eax & R_DWORD(cs[eip + 1]), 0x80000000, &EFLAGS);
      eip += 5; break;
    }

  case 0xaa:		/* stosb */
    if (x86->address_size == 4) {
      instr_write_byte(x86->es_base + x86->edi, AL);
      x86->edi += loop_inc;
    } else {
      instr_write_byte(x86->es_base + DI, AL);
      DI += loop_inc;
    }
    eip++; break;

  case 0xab:		/* stosw */
    if (x86->address_size == 4) {
      x86->instr_write(x86->es_base + x86->edi, x86->eax);
      x86->edi += loop_inc * x86->operand_size;
    } else {
      x86->instr_write(x86->es_base + DI, x86->eax);
      DI += loop_inc * x86->operand_size;
    }
    eip++; break;

  case 0xac:		/* lodsb */
    if (x86->address_size == 4) {
      AL = instr_read_byte(x86->seg_base + x86->esi);
      x86->esi += loop_inc;
    } else {
      AL = instr_read_byte(x86->seg_base + SI);
      SI += loop_inc;
    }
    eip++; break;

  case 0xad: /* lodsw */
    if (x86->address_size == 4) {
      und = x86->instr_read(x86->seg_base + x86->esi);
      x86->esi += loop_inc * x86->operand_size;
    } else {
      und = x86->instr_read(x86->seg_base + SI);
      SI += loop_inc * x86->operand_size;
    }
    if (x86->operand_size == 2)
      AX = und;
    else
      x86->eax = und;
    eip++; break;

  case 0xae: /* scasb */
    if (x86->address_size == 4) {
      instr_binary_byte(7, AL, instr_read_byte(x86->es_base + x86->edi), &EFLAGS);
      x86->edi += loop_inc;
    } else {
      instr_binary_byte(7, AL, instr_read_byte(x86->es_base + DI), &EFLAGS);
      DI += loop_inc;
    }
    eip++; break;

  case 0xaf: /* scasw */
    if (x86->address_size == 4) {
      x86->instr_binary(7, x86->eax, x86->instr_read(x86->es_base + x86->edi), &EFLAGS);
      x86->edi += loop_inc * x86->operand_size;
    } else {
      x86->instr_binary(7, x86->eax, x86->instr_read(x86->es_base + DI), &EFLAGS);
      DI += loop_inc * x86->operand_size;
    }
    eip++; break;

  case 0xb0:
  case 0xb1:
  case 0xb2:
  case 0xb3:
  case 0xb4:
  case 0xb5:
  case 0xb6:
  case 0xb7:
    *reg8(cs[eip], x86) = cs[eip + 1];
    eip += 2; break;

  case 0xb8:
  case 0xb9:
  case 0xba:
  case 0xbb:
  case 0xbc:
  case 0xbd:
  case 0xbe:
  case 0xbf:
    if (x86->operand_size == 2) {
      R_WORD(*reg(cs[eip], x86)) = R_WORD(cs[eip + 1]);
      eip += 3; break;
    } else {
      *reg(cs[eip], x86) = R_DWORD(cs[eip + 1]);
      eip += 5; break;
    }

  case 0xc0: /* shift byte, imm8 */
    if ((cs[eip + 1]&0x38)==0x30) return 0;
    mem = x86->modrm(cs + eip, x86, &inst_len);
    instr_write_byte(mem,instr_shift(cs[eip + 1]>>3, (signed char) instr_read_byte(mem),
                                       cs[eip + 2+inst_len], 1, &EFLAGS));
    eip += inst_len + 3; break;

  case 0xc1: /* shift word, imm8 */
    if ((cs[eip + 1]&0x38)==0x30) return 0;
    mem = x86->modrm(cs + eip, x86, &inst_len);
    if (x86->operand_size == 2)
      instr_write_word(mem, instr_shift(cs[eip + 1]>>3, (short)instr_read_word(mem),
                                      cs[eip + 2+inst_len], 2, &EFLAGS));
    else
      instr_write_dword(mem, instr_shift(cs[eip + 1]>>3, instr_read_dword(mem),
                                      cs[eip + 2+inst_len], 4, &EFLAGS));
    eip += inst_len + 3; break;

  case 0xc2:		/* ret imm16*/
    pop(&i, x86);
    if (x86->_32bit)
      x86->esp += R_WORD(cs[eip + 1]);
    else
      SP += R_WORD(cs[eip + 1]);
    eip = i;
    break;

  case 0xc3:		/* ret */
    pop(&eip, x86);
    break;

  case 0xc4:		/* les */
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      mem = x86->modrm(cs + eip, x86, &inst_len);
      x86->es = instr_read_word(mem+2);
      REG(es)  = x86->es;
      x86->es_base = SEG2LINEAR(x86->es);
      R_WORD(*reg(cs[eip + 1] >> 3, x86)) = instr_read_word(mem);
      eip += inst_len + 2; break;
    }   

  case 0xc5:		/* lds */
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      mem = x86->modrm(cs + eip, x86, &inst_len);
      x86->ds = instr_read_word(mem+2);
      REG(ds)  = x86->ds;
      x86->ds_base = x86->seg_base = SEG2LINEAR(x86->ds);
      R_WORD(*reg(cs[eip + 1] >> 3, x86)) = instr_read_word(mem);
      eip += inst_len + 2; break;
    }

  case 0xc6:		/* mov r/m8,imm8 */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    instr_write_byte(mem, cs[eip + 2 + inst_len]);
    eip += inst_len + 3; break;

  case 0xc7:		/* mov r/m,imm */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    x86->instr_write(mem, R_DWORD(cs[eip + 2 + inst_len]));
    eip += x86->operand_size + inst_len + 2;
    break;
    /* 0xc8 enter */  

  case 0xc9: /*leave*/   
    if (x86->_32bit)
      x86->esp = x86->ebp;
    else
      SP = BP;
    pop(&x86->ebp, x86);
    eip++; break;

  case 0xca: /*retf imm 16*/          
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      pop(&i, x86);
      pop(&x86->cs, x86);
      REG(cs)  = x86->cs;
      x86->cs_base = SEG2LINEAR(x86->cs);
      SP += R_WORD(cs[eip + 1]);
      cs = (unsigned char *)x86->cs_base;
      eip = i;
    }
    break;

  case 0xcb: /*retf*/          
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      pop(&eip, x86);
      pop(&x86->cs, x86);
      REG(cs)  = x86->cs;
      x86->cs_base = SEG2LINEAR(x86->cs);
      cs = (unsigned char *)x86->cs_base;
    }
    break;
    
    /* 0xcc int3 0xcd int 0xce into 0xcf iret */

  case 0xd0: /* shift r/m8, 1 */
    if ((cs[eip + 1]&0x38)==0x30) return 0;
    mem = x86->modrm(cs + eip, x86, &inst_len);
    instr_write_byte(mem, instr_shift(cs[eip + 1]>>3, (signed char) instr_read_byte(mem),
                                        1, 1, &EFLAGS));
    eip += inst_len + 2; break;

  case 0xd1: /* shift r/m16, 1 */
    if ((cs[eip + 1]&0x38)==0x30) return 0;
    mem = x86->modrm(cs + eip, x86, &inst_len);
    if (x86->operand_size == 2)
      instr_write_word(mem, instr_shift(cs[eip + 1]>>3, (short) instr_read_word(mem),
                                        1, 2, &EFLAGS));
    else
      instr_write_dword(mem, instr_shift(cs[eip + 1]>>3, instr_read_dword(mem), 1, 4, &EFLAGS));
    eip += inst_len + 2; break;
    
  case 0xd2: /* shift r/m8, cl */
    if ((cs[eip + 1]&0x38)==0x30) return 0;
    mem = x86->modrm(cs + eip, x86, &inst_len);
    instr_write_byte(mem,instr_shift(cs[eip + 1]>>3, (signed char) instr_read_byte(mem),
                                       CL, 1, &EFLAGS));
    eip += inst_len + 2; break;

  case 0xd3: /* shift r/m16, cl */
    if ((cs[eip + 1]&0x38)==0x30) return 0;
    mem = x86->modrm(cs + eip, x86, &inst_len);
    if (x86->operand_size == 2)
      instr_write_word(mem, instr_shift(cs[eip + 1]>>3, (short) instr_read_word(mem),
                                       CL, 2, &EFLAGS));
    else
      instr_write_dword(mem, instr_shift(cs[eip + 1]>>3, instr_read_dword(mem),
                                       CL, 4, &EFLAGS));
    eip += inst_len + 2; break;

  case 0xd4:  /* aam byte */
    AH = AL / cs[eip + 1];
    AL = AL % cs[eip + 1];
    instr_flags(AL, 0x80, &EFLAGS);
    eip += 2; break;
    
  case 0xd5:  /* aad byte */ 
    AL = AH * cs[eip + 1] + AL;
    AH = 0;
    instr_flags(AL, 0x80, &EFLAGS);
    eip += 2; break;

  case 0xd6: /* salc */
    AL = EFLAGS & CF ? 0xff : 0;
    eip++; break;

  case 0xd7: /* xlat */
    AL =  instr_read_byte(x86->seg_base+(x86->ebx & wordmask[x86->address_size])+AL);
    eip++; break;
    /* 0xd8 - 0xdf copro */

  case 0xe0: /* loopnz */
    eip += ( (x86->address_size == 4 ? --x86->ecx : --CX) && !(EFLAGS & ZF) ?
             2 + (signed char)(cs[eip + 1]) : 2); break;

  case 0xe1: /* loopz */
    eip += ( (x86->address_size == 4 ? --x86->ecx : --CX) && (EFLAGS & ZF) ?
             2 + (signed char)(cs[eip + 1]) : 2); break;

  case 0xe2: /* loop */
    eip += ( (x86->address_size == 4 ? --x86->ecx : --CX) ?
             2 + (signed char)(cs[eip + 1]) : 2); break;

  case 0xe3:  /* jcxz */
    eip += ((x86->address_size == 4 ? x86->ecx : CX) ? 2 : 2 + (signed char)cs[eip + 1]);
    break;
    
  /* 0xe4 in ib 0xe5 in iw 0xe6 out ib 0xe7 out iw */

  case 0xe8: /* call near */
    push(eip + 1 + x86->operand_size, x86);   
    /* fall through */
    
  case 0xe9: /* jmp near */
    eip += x86->operand_size + 1 + (R_DWORD(cs[eip + 1]) & wordmask[x86->operand_size]);
    break;
    
  case 0xea: /*jmp far*/
    if (pmode || x86->operand_size == 4)
      return 0;
    else {
      x86->cs = R_WORD(cs[eip+3]);
      REG(cs)  = x86->cs;
      x86->cs_base = SEG2LINEAR(x86->cs);
      eip = R_WORD(cs[eip + 1]);
      cs = (unsigned char *)x86->cs_base;
    }
    break;

  case 0xeb: /* jmp short */
    eip += 2 + (signed char)(cs[eip + 1]); break;

  case 0xec: /* in al, dx */
    /* Note that we short circuit if we can */
    if ((uc=VGA_emulate_inb(DX))!=0xff) {
      AL = uc;
      eip++; break;
    }
    else
      return 0;
  /* 0xed in ax,dx */

  case 0xee: /* out dx, al */
    /* Note that we short circuit if we can */
    if (VGA_emulate_outb(DX, AL) && vga.inst_emu)
      eip++; 
    else
      return 0;
    break;
    
  case 0xef: /* out dx, ax */
    if ((x86->operand_size == 2) &&
        VGA_emulate_outb(DX, AL) &&
	      VGA_emulate_outb(DX + 1, AH) &&
        vga.inst_emu)
      eip++; 
    else
      return 0;
    break;

    /* 0xf0 lock 0xf1 int1 */

    /* 0xf2 repnz 0xf3 repz handled above */
    /* 0xf4 hlt */
    
  case 0xf5: /* cmc */
    EFLAGS ^= CF;
    eip++; break;

  case 0xf6:
    mem = x86->modrm(cs + eip, x86, &inst_len);
    switch (cs[eip + 1]&0x38) {
    case 0x00: /* test mem byte, imm */
      instr_flags(instr_read_byte(mem) & cs[eip + 2+inst_len], 0x80, &EFLAGS);
      eip += inst_len + 3; break;
    case 0x08: return 0;
    case 0x10: /*not byte*/
      instr_write_byte(mem, ~instr_read_byte(mem));
      eip += inst_len + 2; break;
    case 0x18: /*neg byte*/
      instr_write_byte(mem, instr_binary_byte(7, 0, instr_read_byte(mem), &EFLAGS));
      eip += inst_len + 2; break;
    case 0x20: /*mul byte*/
      AX = AL * instr_read_byte(mem);
      EFLAGS &= ~(CF|OF);
      if (AH)
        EFLAGS |= (CF|OF);
      eip += inst_len + 2; break;
    case 0x28: /*imul byte*/
      AX = (signed char)AL * (signed char)instr_read_byte(mem);
      EFLAGS &= ~(CF|OF);
      if (AH)
        EFLAGS |= (CF|OF);
      eip += inst_len + 2; break;
    case 0x30: /*div byte*/
      und = AX;
      uc = instr_read_byte(mem);
      if (uc == 0) return 0;
      und2 = und / uc;
      if (und2 & 0xffffff00) return 0;
      AL = und2 & 0xff;
      AH = und % uc;
      eip += inst_len + 2; break;
    case 0x38: /*idiv byte*/
      i = (short)AX;
      uc = instr_read_byte(mem);
      if (uc == 0) return 0;
      i2 = i / (signed char)uc;
      if (i2<-128 || i2>127) return 0;
      AL = i2 & 0xff;
      AH = i % (signed char)uc;
      eip += inst_len + 2; break;
    }
    break;

  case 0xf7: 
    mem = x86->modrm(cs + eip, x86, &inst_len);
    switch (cs[eip + 1]&0x38) {
    case 0x00: /* test mem word, imm */
      if (x86->operand_size == 4) return 0;
      instr_flags(instr_read_word(mem) & R_WORD(cs[eip + 2+inst_len]), 0x8000, &EFLAGS);
      eip += inst_len + 4; break;
    case 0x08: return 0;
    case 0x10: /*not word*/
      x86->instr_write(mem, ~x86->instr_read(mem));
      eip += inst_len + 2; break;
    case 0x18: /*neg word*/       
      x86->instr_write(mem, x86->instr_binary(7, 0, x86->instr_read(mem), &EFLAGS));
      eip += inst_len + 2; break;
    case 0x20: /*mul word*/       
      if (x86->operand_size == 4) return 0;
      und = AX * instr_read_word(mem);
      AX = und & 0xffff;
      DX = und >> 16;
      EFLAGS &= ~(CF|OF);
      if (DX)
        EFLAGS |= (CF|OF);
      eip += inst_len + 2; break;
    case 0x28: /*imul word*/
      if (x86->operand_size == 4) return 0;
      i = (short)AX * (short)instr_read_word(mem);
      AX = i & 0xffff;
      DX = i >> 16;
      EFLAGS &= ~(CF|OF);
      if (DX)
        EFLAGS |= (CF|OF);
      eip += inst_len + 2; break;      
    case 0x30: /*div word*/
      if (x86->operand_size == 4) return 0;
      und = (DX<<16) + AX;
      uns = instr_read_word(mem);
      if (uns == 0) return 0;
      und2 = und / uns;
      if (und2 & 0xffff0000) return 0;
      AX = und2 & 0xffff;
      DX = und % uns;
      eip += inst_len + 2; break;
    case 0x38: /*idiv word*/
      if (x86->operand_size == 4) return 0;
      i = ((short)DX<<16) + AX;
      uns = instr_read_word(mem);
      if (uns == 0) return 0;
      i2 = i / (short)uns;
      if (i2<-32768 || i2>32767) return 0;
      AX = i2 & 0xffff;
      DX = i % (short)uns;
      eip += inst_len + 2; break;
    }
    break;
    
  case 0xf8: /* clc */
    EFLAGS &= ~CF;
    eip++; break;;

  case 0xf9: /* stc */
    EFLAGS |= CF;
    eip++; break;;

    /* 0xfa cli 0xfb sti */

  case 0xfc: /* cld */
    EFLAGS &= ~DF;
    loop_inc = 1;
    eip++; break;;

  case 0xfd: /* std */
    EFLAGS |= DF;
    loop_inc = -1;
    eip++; break;;

  case 0xfe: /* inc/dec mem */
    mem = x86->modrm(cs + eip, x86, &inst_len);
    uc = instr_read_byte(mem);
    switch (cs[eip + 1]&0x38) {
    case 0x00:
      EFLAGS &= ~(OF|ZF|SF|PF|AF);
      OPandFLAG0(unl, incb, uc, =q);
      EFLAGS |= unl & (OF|ZF|SF|PF|AF);
      instr_write_byte(mem, uc);
      eip += inst_len + 2; break;
    case 0x08:        
      EFLAGS &= ~(OF|ZF|SF|PF|AF);
      OPandFLAG0(unl, decb, uc, =q);
      EFLAGS |= unl & (OF|ZF|SF|PF|AF);
      instr_write_byte(mem, uc);
      eip += inst_len + 2; break;
    default:              
      return 0;
    }
    break;

  case 0xff:
    if (x86->operand_size == 4) return 0;
    mem = x86->modrm(cs + eip, x86, &inst_len);
    uns = instr_read_word(mem);
    switch (cs[eip + 1]&0x38) {
    case 0x00: /* inc */
      EFLAGS &= ~(OF|ZF|SF|PF|AF);
      OPandFLAG0(unl, incw, uns, =r);
      EFLAGS |= unl & (OF|ZF|SF|PF|AF);
      instr_write_word(mem, uns);
      eip += inst_len + 2; break;
    case 0x08: /* dec */       
      EFLAGS &= ~(OF|ZF|SF|PF|AF);
      OPandFLAG0(unl, decw, uns, =r);
      EFLAGS |= unl & (OF|ZF|SF|PF|AF);
      instr_write_word(mem, uns);
      eip += inst_len + 2; break;;
    case 0x10: /*call near*/          
      push(eip + inst_len + 2, x86);
      eip = uns;
      break;
      
    case 0x18: /*call far*/          
      if (pmode || x86->operand_size == 4)
        return 0;
      else {
        push(x86->cs, x86);
        x86->cs = instr_read_word(mem+2);
        push(eip + inst_len + 2, x86);
        REG(cs)  = x86->cs;
        x86->cs_base = SEG2LINEAR(x86->cs);
        eip = uns;
        cs = (unsigned char *)x86->cs_base;
      }
      break;
      
    case 0x20: /*jmp near*/          
      eip = uns;
      break;
      
    case 0x28: /*jmp far*/          
      if (pmode || x86->operand_size == 4)
        return 0;
      else {
        x86->cs = instr_read_word(mem+2);
        REG(cs)  = x86->cs;
        x86->cs_base = SEG2LINEAR(x86->cs);
        eip = uns;
        cs = (unsigned char *)x86->cs_base;
      }
      break;
      
    case 0x30: /*push*/
      push(uns, x86);
      eip += inst_len + 2; break;
    default:              
      return 0;
    }
    break;

  default:		/* First byte doesn't match anything */
    return 0;
  }	/* switch (cs[eip]) */

  eip &= wordmask[(x86->_32bit + 1) * 2];
  x86->eip = eip;

#if DEBUG_INSTR >= 2
    dump_x86_regs(x86);
#endif

  return 1;
}  

static void scp_to_x86_regs(x86_regs *x86, struct sigcontext_struct *scp, int pmode)
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
    x86->cs_base = (unsigned char *)dpmi_GetSegmentBaseAddress(_cs);
    x86->ds_base = (unsigned char *)dpmi_GetSegmentBaseAddress(_ds);
    x86->es_base = (unsigned char *)dpmi_GetSegmentBaseAddress(_es);
    x86->ss_base = (unsigned char *)dpmi_GetSegmentBaseAddress(_ss);
    x86->fs_base = (unsigned char *)dpmi_GetSegmentBaseAddress(_fs);
    x86->gs_base = (unsigned char *)dpmi_GetSegmentBaseAddress(_gs);
    x86->_32bit = _cs && dpmi_mhp_get_selector_size(_cs) ? 1 : 0;
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
    x86->cs = REG(cs);
    x86->ds = REG(ds);
    x86->es = REG(es);
    x86->ss = REG(ss);
    x86->fs = REG(fs);
    x86->gs = REG(gs);
    x86->cs_base = SEG2LINEAR(x86->cs);
    x86->ds_base = SEG2LINEAR(x86->ds);
    x86->es_base = SEG2LINEAR(x86->es);
    x86->ss_base = SEG2LINEAR(x86->ss);
    x86->fs_base = SEG2LINEAR(x86->fs);
    x86->gs_base = SEG2LINEAR(x86->gs);
    x86->_32bit = 0;
  }
  prepare_x86(x86);
}

static void x86_regs_to_scp(x86_regs *x86, struct sigcontext_struct *scp, int pmode)
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

/*
 * DANG_BEGIN_FUNCTION instr_emu
 *
 * description:
 * instr_emu is the main interface to instr_sim. It puts the processor
 * state in the x86 structure.
 *
 * arguments:
 * scp - A pointer to a struct sigcontext_struct holding some relevant data.
 * pmode - flags protected mode
 * cnt - number of instructions to be simulated
 *
 * DANG_END_FUNCTION                        
 */
 
void instr_emu(struct sigcontext_struct *scp, int pmode, int cnt)
{
#if DEBUG_INSTR >= 1
  unsigned int ref;
  int refseg, rc;
  unsigned char frmtbuf[256];
#endif
  int i = 0;
  x86_regs x86;
  sigset_t set;

  /* unblock SIGIO, SIGALRM, SIG_ACQUIRE, SIG_RELEASE */
  sigemptyset(&set);
  addset_signals_that_queue(&set);
  sigprocmask(SIG_UNBLOCK, &set, NULL);

  scp_to_x86_regs(&x86, scp, pmode);

#if DEBUG_INSTR >= 1
{   unsigned char *cp = (unsigned char *) (x86.cs_base + x86.eip);
    refseg = x86.cs;
    rc = dis_8086((unsigned long) cp, cp, frmtbuf, x86._32bit ? 3 : 0, &refseg, &ref, x86.cs_base, 1);
    instr_deb("vga_emu_fault: %u bytes not simulated %d: %s fault addr=%08x\n",
            instr_len(cp), count, frmtbuf, (unsigned) scp->cr2);
    dump_x86_regs(&x86);
}
#endif

  count = cnt ? : COUNT + 1;
  vga_base = (char *)(uintptr_t)(vga.mem.bank_base);
  vga_end =  vga_base + vga.mem.bank_len;

  x86.prefixes = 1;
  
  do {
    if ((!cnt && signal_pending) || !instr_sim(&x86, pmode))
      break;
    i = 1;
  } while (--count > 0);

#if DEBUG_INSTR >= 1
  if (count > 0) {
    unsigned char *cp = (unsigned char *) (x86.cs_base + x86.eip);
    refseg = x86.cs;
    rc = dis_8086((unsigned long) cp, cp, frmtbuf, x86._32bit ? 3 : 0, &refseg, &ref, x86.cs_base, 1);
    instr_deb("vga_emu_fault: %u bytes not simulated %d: %s fault addr=%08x\n",
            instr_len(cp), count, frmtbuf, (unsigned) scp->cr2);
    dump_x86_regs(&x86);
  }
#endif
  if (i == 0 && !signal_pending) { /* really an unknown instruction from the beginning */
    x86.eip += instr_len((unsigned char *) (x86.cs_base + x86.eip));
    if(!x86._32bit)
      x86.eip &= 0xffff;
  }
  

  x86_regs_to_scp(&x86, scp, pmode);
}

int decode_modify_segreg_insn(struct sigcontext_struct *scp, int pmode,
    unsigned int *new_val)
{
  unsigned char *mem;
  unsigned char *cs;
  int inst_len, ret = -1;
  x86_regs x86;

  scp_to_x86_regs(&x86, scp, pmode);

  cs = (unsigned char *) x86.cs_base;
  x86.prefixes = handle_prefixes(&x86);
  x86.eip += x86.prefixes;

  switch(cs[x86.eip]) {
    case 0x8e:		/* mov segreg,r/m16 */
      ret = sreg_idx(cs[x86.eip + 1] >> 3);
      mem = x86.modrm(cs + x86.eip, &x86, &inst_len);
      if ((cs[x86.eip + 1] & 0xc0) == 0xc0)  /* compensate for mov r,segreg */
        mem = (unsigned char *)reg(cs[x86.eip + 1], &x86);
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
      switch (cs[x86.eip]) {
        case 0xca: /*retf imm 16*/
	  x86.esp += ((unsigned short *) (&cs[x86.eip + 1]))[0];
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
      tmp_eip = x86.instr_read(x86.cs_base + x86.eip + 1);
      *new_val = instr_read_word(x86.cs_base + x86.eip + 1 + x86.operand_size);
      ret = cs_INDEX;
      x86.eip = tmp_eip;
    }
    break;

    case 0xc4:		/* les */
      mem = x86.modrm(cs + x86.eip, &x86, &inst_len);
      *new_val = instr_read_word(mem+x86.operand_size);
      if (x86.operand_size == 2)
	R_WORD(*reg(cs[x86.eip + 1] >> 3, &x86)) = instr_read_word(mem);
      else
	*reg(cs[x86.eip + 1] >> 3, &x86) = instr_read_dword(mem);
      ret = es_INDEX;
      x86.eip += inst_len + 2;
      break;

    case 0xc5:		/* lds */
      mem = x86.modrm(cs + x86.eip, &x86, &inst_len);
      *new_val = instr_read_word(mem+x86.operand_size);
      if (x86.operand_size == 2)
	R_WORD(*reg(cs[x86.eip + 1] >> 3, &x86)) = instr_read_word(mem);
      else
	*reg(cs[x86.eip + 1] >> 3, &x86) = instr_read_dword(mem);
      ret = ds_INDEX;
      x86.eip += inst_len + 2;
      break;

    case 0x07:	/* pop es */
    case 0x17:	/* pop ss */
    case 0x1f:	/* pop ds */
      ret = sreg_idx(cs[x86.eip] >> 3);
      pop(new_val, &x86);
      x86.eip++; 
      break;

    case 0x0f:
      x86.eip++;
      switch (cs[x86.eip]) {
        case 0xa1:	/* pop fs */
        case 0xa9:	/* pop gs */
	  pop(new_val, &x86);
	  ret = sreg_idx(cs[x86.eip] >> 3);
	  x86.eip++; 
	  break;

	case 0xb2:	/* lss */
	  mem = x86.modrm(cs + x86.eip, &x86, &inst_len);
	  *new_val = instr_read_word(mem+x86.operand_size);
	  if (x86.operand_size == 2)
	    R_WORD(*reg(cs[x86.eip + 1] >> 3, &x86)) = instr_read_word(mem);
	  else
	    *reg(cs[x86.eip + 1] >> 3, &x86) = instr_read_dword(mem);
	  ret = ss_INDEX;
	  x86.eip += inst_len + 2;
	  break;

	case 0xb4:	/* lfs */
	  mem = x86.modrm(cs + x86.eip, &x86, &inst_len);
	  *new_val = instr_read_word(mem+x86.operand_size);
	  if (x86.operand_size == 2)
	    R_WORD(*reg(cs[x86.eip + 1] >> 3, &x86)) = instr_read_word(mem);
	  else
	    *reg(cs[x86.eip + 1] >> 3, &x86) = instr_read_dword(mem);
	  ret = fs_INDEX;
	  x86.eip += inst_len + 2;
	  break;

	case 0xb5:	/* lgs */
	  mem = x86.modrm(cs + x86.eip, &x86, &inst_len);
	  *new_val = instr_read_word(mem+x86.operand_size);
	  if (x86.operand_size == 2)
	    R_WORD(*reg(cs[x86.eip + 1] >> 3, &x86)) = instr_read_word(mem);
	  else
	    *reg(cs[x86.eip + 1] >> 3, &x86) = instr_read_dword(mem);
	  ret = gs_INDEX;
	  x86.eip += inst_len + 2;
	  break;
      }
      break;
  }

  x86_regs_to_scp(&x86, scp, pmode);
  return ret;
}
