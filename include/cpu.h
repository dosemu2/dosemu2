/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1995/01/14 15:31:55 $
 * $Source: /home/src/dosemu0.60/include/RCS/cpu.h,v $
 * $Revision: 2.6 $
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

#include "extern.h"

#ifdef X86_EMULATOR
#include "x86_emulator.h"
#endif

#ifndef X86_EMULATOR
/* all registers as a structure */
#define REGS  vm86s.regs

/* this is used like: REG(eax) = 0xFFFFFFF */
#define REG(reg) (REGS.##reg)
#define READ_SEG_REG(reg) (REGS.##reg)
#define WRITE_SEG_REG(reg, val) REGS.##reg = (val)

/* these are used like:  LO(ax) = 2 (sets al to 2) */
#define LO(reg)  (*(unsigned char *)&REG(e##reg))
#define HI(reg)  (*((unsigned char *)&REG(e##reg) + 1))

#define _LO(reg) (*(unsigned char *)&(scp->e##reg))
#define _HI(reg) (*((unsigned char *)&(scp->e##reg) + 1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define LWORD(reg)	(*((unsigned short *)&REG(reg)))
#define HWORD(reg)	(*((unsigned short *)&REG(reg) + 1))

#define _LWORD(reg)	(*((unsigned short *)&(scp->##reg)))
#define _HWORD(reg)	(*((unsigned short *)&(scp->##reg) + 1))

/* this is used like: SEG_ADR((char *), es, bx) */
#define SEG_ADR(type, seg, reg)  type((LWORD(seg) << 4) + LWORD(e##reg))

#define WRITE_FLAGS(val)    REG(eflags) = (val)
#define READ_FLAGS()        REG(eflags)

#endif /* #ifndef X86_EMULATOR */

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
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define popb(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb (%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

static __inline__ void set_revectored(int nr, struct revectored_struct * bitmap)
{
	__asm__ __volatile__("btsl %1,%0"
		: /* no output */
		:"m" (*bitmap),"r" (nr));
}

static __inline__ void reset_revectored(int nr, struct revectored_struct * bitmap)
{
	__asm__ __volatile__("btrl %1,%0"
		: /* no output */
		:"m" (*bitmap),"r" (nr));
}

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

/*#define vflags ((REG(eflags) & VIF)? LWORD(eflags) | IF : LWORD(eflags) & ~IF)*/
#define vflags ((READ_FLAGS() & VIF)? WORD(READ_FLAGS()) | IF : WORD(READ_FLAGS()) & ~IF)

#if 0
/* this is the array of interrupt vectors */
struct vec_t {
  unsigned short offset;
  unsigned short segment;
};

EXTERN struct vec_t *ivecs;

#endif
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

/*
#define CARRY (LWORD(eflags) |= CF)
#define NOCARRY (LWORD(eflags) &= ~CF)
*/
#define CARRY   ( WRITE_FLAGS(WORD(READ_FLAGS()) | CF) )
#define NOCARRY ( WRITE_FLAGS(WORD(READ_FLAGS()) & ~CF) )

struct sigcontext_struct {
  unsigned short gs, __gsh;
  unsigned short fs, __fsh;
  unsigned short es, __esh;
  unsigned short ds, __dsh;
  unsigned long edi;
  unsigned long esi;
  unsigned long ebp;
  unsigned long esp;
  unsigned long ebx;
  unsigned long edx;
  unsigned long ecx;
  unsigned long eax;
  unsigned long trapno;
  unsigned long err;
  unsigned long eip;
  unsigned short cs, __csh;
  unsigned long eflags;
  unsigned long esp_at_signal;
  unsigned short ss, __ssh;
  unsigned long i387;
  unsigned long oldmask;
  unsigned long cr2;
};

#define _gs     (scp->gs)
#define _fs     (scp->fs)
#define _es     (scp->es)
#define _ds     (scp->ds)
#define _edi    (scp->edi)
#define _esi    (scp->esi)
#define _ebp    (scp->ebp)
#define _esp    (scp->esp)
#define _ebx    (scp->ebx)
#define _edx    (scp->edx)
#define _ecx    (scp->ecx)
#define _eax    (scp->eax)
#define _trapno (scp->trapno)
#define _eip    (scp->eip)
#define _cs     (scp->cs)
#define _eflags (scp->eflags)
#define _ss     (scp->ss)

void dosemu_fault(int, struct sigcontext_struct);

void show_regs(char *, int), show_ints(int, int);
__inline__ int do_hard_int(int), do_soft_int(int);

#endif /* CPU_H */
