/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1994/03/04 15:23:54 $
 * $Source: /home/src/dosemu0.50/RCS/cpu.h,v $
 * $Revision: 1.8 $
 * $State: Exp $
 */

#ifndef CPU_H
#define CPU_H

#include <linux/vm86.h>
#define _regs vm86s.regs
#define _regs	vm86s.regs

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

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define LWORD(reg)      (*((unsigned short *)&REG(reg)))
#define HWORD(reg)      (*((unsigned short *)&REG(reg) + 1))

/* this is used like: SEG_ADR((char *), es, bx) */
#define SEG_ADR(type, seg, reg)  type((LWORD(seg) << 4) + LWORD(e##reg))

/* flags */
#define CF  (1 <<  0)
#define PF  (1 <<  2)
#define AF  (1 <<  4)
#define ZF  (1 <<  6)
#define SF  (1 <<  7)
#define TF  (1 <<  8)
#define IF  (1 <<  9)
#define DF  (1 << 10)
#define OF  (1 << 11)
#define NT  (1 << 14)
#define RF  (1 << 16)
#define VM  (1 << 17)
#define AC  (1 << 18)

#define IOPL_MASK  (3 << 12)

/* for cpu.type */
#define CPU_286   2
#define CPU_386	  3
#define CPU_486	  4

struct CPU {
  int iflag, iopl, nt, ac, bit15;
  int type, sti;
};

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

#define CARRY (REG(eflags) |= CF)
#define NOCARRY (REG(eflags) &= ~CF)

typedef struct {
  short eip;
  short cs;
  short flags;
}

interrupt_stack_frame;

inline void update_cpu(long), update_flags(long *);

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
  unsigned short cs;
  unsigned short ip;
};

void sigtrap(int, struct sigcontext_struct);
void sigill(int, struct sigcontext_struct);
void sigfpe(int, struct sigcontext_struct);
void sigsegv(int, struct sigcontext_struct);

void show_regs(void), show_ints(int, int);
inline int do_hard_int(int), do_soft_int(int);

char pop_byte(struct vm86_regs *);
short pop_short(struct vm86_regs *);
long pop_long(struct vm86_regs *);

void push_long(struct vm86_regs *, long);
void push_word(struct vm86_regs *, short);
void push_byte(struct vm86_regs *, char);

void push_isf(struct vm86_regs *, interrupt_stack_frame);
interrupt_stack_frame pop_isf(struct vm86_regs *);

#endif /* CPU_H */
