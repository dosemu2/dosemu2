/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/04/30 01:05:16 $
 * $Source: /home/src/dosemu0.60/RCS/cpu.h,v $
 * $Revision: 1.18 $
 * $State: Exp $
 */

#ifndef CPU_H
#define CPU_H
#ifdef BIOSSEG
#undef BIOSSEG
#endif
#include <linux/vm86.h>
#ifndef BIOSSEG
#define BIOSSEG 0xf000
#endif
#define _regs vm86s.regs

#ifndef CPU_C
#define CPU_EXTERN extern
#else
#define CPU_EXTERN
#endif

/* all registers as a structure */
#define REGS  vm86s.regs

/* this is used like: REG(eax) = 0xFFFFFFF */
#define REG(reg) (REGS.##reg)

/* these are used like:  LO(ax) = 2 (sets al to 2) */
#define LO(reg)  (*(unsigned char *)&REG(e##reg))
#define HI(reg)  (*((unsigned char *)&REG(e##reg) + 1))

#define _LO(reg) (*(unsigned char *)&(scp->sc_e##reg))
#define _HI(reg) (*((unsigned char *)&(scp->sc_e##reg) + 1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define LWORD(reg)	(*((unsigned short *)&REG(reg)))
#define HWORD(reg)	(*((unsigned short *)&REG(reg) + 1))

#define _LWORD(reg)	(*((unsigned short *)&(scp->sc_##reg)))
#define _HWORD(reg)	(*((unsigned short *)&(scp->sc_##reg) + 1))

/* this is used like: SEG_ADR((char *), es, bx) */
#define SEG_ADR(type, seg, reg)  type((LWORD(seg) << 4) + LWORD(e##reg))

/*
 * nearly directly stolen from Linus : linux/kernel/vm86.c
 *
 * Boy are these ugly, but we need to do the correct 16-bit arithmetic.
 * Gcc makes a mess of it, so we do it inline and use non-obvious calling
 * conventions..
 */
#define pushw(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %h2,(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define popw(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb (%1,%0),%h2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=r" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

/* flags */
#define CF  (1 <<  0)
#define PF  (1 <<  2)
#define AF  (1 <<  4)
#define ZF  (1 <<  6)
#define SF  (1 <<  7)
#define TF  TF_MASK	/* (1 <<  8) */
#define IF  IF_MASK	/* (1 <<  9) */
#define DF  (1 << 10)
#define OF  (1 << 11)
#define NT  NT_MASK	/* (1 << 14) */
#define RF  (1 << 16)
#define VM  VM_MASK	/* (1 << 17) */
#define AC  AC_MASK	/* (1 << 18) */
#define VIF VIF_MASK
#define VIP VIP_MASK
#define ID  ID_MASK

#ifndef IOPL_MASK
#define IOPL_MASK  (3 << 12)
#endif

#define vflags ((REG(eflags) & VIF)? LWORD(eflags) | IF : LWORD(eflags) & ~IF)

/* this is the array of interrupt vectors */
struct vec_t {
  unsigned short offset;
  unsigned short segment;
};

CPU_EXTERN struct vec_t *ivecs;

#ifdef TRUST_VEC_STRUCT
#define IOFF(i) ivecs[i].offset
#define ISEG(i) ivecs[i].segment
#else
#define IOFF(i) ((us *)0)[  (i)<<1    ]
#define ISEG(i) ((us *)0)[ ((i)<<1) +1]
#endif

#define IVEC(i) ((ISEG(i)<<4) + IOFF(i))
#define SETIVEC(i, seg, ofs)	((us *)0)[ ((i)<<1) +1] = (us)seg; \
				((us *)0)[  (i)<<1    ] = (us)ofs

#define OP_IRET			0xcf

#define IS_REDIRECTED(i)	(ISEG(i) != BIOSSEG)
#define IS_IRET(i)		(*(unsigned char *)IVEC(i) == OP_IRET)

/*
#define WORD(i) (unsigned short)(i)
*/

#define CARRY (LWORD(eflags) |= CF)
#define NOCARRY (LWORD(eflags) &= ~CF)

struct sigcontext_struct {
  unsigned short sc_gs, __gsh;
  unsigned short sc_fs, __fsh;
  unsigned short sc_es, __esh;
  unsigned short sc_ds, __dsh;
  unsigned long sc_edi;
  unsigned long sc_esi;
  unsigned long sc_ebp;
  unsigned long sc_esp;
  unsigned long sc_ebx;
  unsigned long sc_edx;
  unsigned long sc_ecx;
  unsigned long sc_eax;
  unsigned long sc_trapno;
  unsigned long sc_err;
  unsigned long sc_eip;
  unsigned short sc_cs, __csh;
  unsigned long sc_efl;
  unsigned long esp_at_signal;
  unsigned short sc_ss, __ssh;
  unsigned long i387;
  unsigned long oldmask;
  unsigned long cr2;
};

#define _gs	scp->sc_gs
#define _fs	scp->sc_fs
#define _es	scp->sc_es
#define _ds	scp->sc_ds
#define _edi	scp->sc_edi
#define _esi	scp->sc_esi
#define _ebp	scp->sc_ebp
#define _esp	scp->sc_esp
#define _ebx	scp->sc_ebx
#define _edx	scp->sc_edx
#define _ecx	scp->sc_ecx
#define _eax	scp->sc_eax
#define _eip	scp->sc_eip
#define _cs	scp->sc_cs
#define _eflags	scp->sc_efl
#define _ss	scp->sc_ss

#ifdef DPMI
struct pm86 {
  unsigned short gs;
  unsigned short fs;
  unsigned short es;
  unsigned short ds;
  unsigned long edi;
  unsigned long esi;
  unsigned long ebp;
  unsigned long esp;
  unsigned long ebx;
  unsigned long edx;
  unsigned long ecx;
  unsigned long eax;
  unsigned long eflags;
  unsigned long eip;
  unsigned short cs, __csh;
};

struct RealModeCallStructure {
  unsigned long edi;
  unsigned long esi;
  unsigned long ebp;
  unsigned long esp;
  unsigned long ebx;
  unsigned long edx;
  unsigned long ecx;
  unsigned long eax;
  unsigned short flags;
  unsigned short es;
  unsigned short ds;
  unsigned short fs;
  unsigned short gs;
  unsigned short ip;
  unsigned short cs;
  unsigned short sp;
  unsigned short ss;
};
#endif	/* DPMI */

void sigtrap(int);
void sigill(int, struct sigcontext_struct);
void sigfpe(int);
void sigsegv(int, struct sigcontext_struct);

void show_regs(void), show_ints(int, int);
__inline__ int do_hard_int(int), do_soft_int(int);

short pop_short(struct vm86_regs *);

void push_word(struct vm86_regs *, short);

#ifdef DPMI
#define DPMI_show_state \
    D_printf("eip: 0x%08lx  esp: 0x%08lx  eflags: 0x%lx\n" \
	     "cs: 0x%04lx  ds: 0x%04lx  es: 0x%04lx  ss: 0x%04lx\n", \
	     (long)_eip, (long)_esp, (long)_eflags, (long)_cs, (long)_ds, (long)_es, (long)_ss); \
    D_printf("EAX: %08lx  EBX: %08lx  ECX: %08lx  EDX: %08lx  EFLAG: %08lx\n", \
	     (long)_eax, (long)_ebx, (long)_ecx, (long)_edx, (long)_eflags); \
    D_printf("ESI: %08lx  EDI: %08lx  EBP: %08lx\n", \
	     (long)_esi, (long)_edi, (long)_ebp); \
    D_printf("CS: %04lx  DS: %04lx  ES: %04lx  FS: %04lx  GS: %04lx\n", \
	     (long)_cs, (long)_ds, (long)_es, (long)_fs, (long)_gs); \
    /* display the 10 bytes before and after CS:EIP.  the -> points \
     * to the byte at address CS:EIP \
     */ \
    if (!((_cs) & 0x0004)) { \
      /* GTD */ \
      csp2 = (unsigned char *) _eip - 10; \
    } \
    else { \
      /* LDT */ \
      csp2 = (unsigned char *) (GetSegmentBaseAddress(_cs) + _eip) - 10; \
    } \
    D_printf("OPS  : "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *csp2++); \
    D_printf("-> "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *csp2++); \
    D_printf("\n"); \
    if (!((_ss) & 0x0004)) { \
      /* GTD */ \
      ssp2 = (unsigned char *) _esp - 10; \
    } \
    else { \
      /* LDT */ \
      if (Segments[_ss>>3].is_32) \
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _esp ) - 10; \
      else \
	ssp2 = (unsigned char *) (GetSegmentBaseAddress(_ss) + _LWORD(esp) ) - 10; \
    } \
    D_printf("STACK: "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *ssp2++); \
    D_printf("-> "); \
    for (i = 0; i < 10; i++) \
      D_printf("%02x ", *ssp2++); \
    D_printf("\n");
#endif /*DPMI*/

#endif /* CPU_H */
