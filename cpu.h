/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/11/30 22:21:03 $
 * $Source: /home/src/dosemu0.49pl3/RCS/cpu.h,v $
 * $Revision: 1.3 $
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

typedef struct 
{
  short eip;
  short cs;
  short flags;
} interrupt_stack_frame;

inline void update_cpu(long),
       update_flags(long *);

/* this was taken from the 0.99pl10 + ALPHA-diff kernel sources...
 * I believe the parts previous to linux_eflags correspond to the
 * SYSV ABI standard.
 */
#define SIGSTACK int sig, long gs, long fs, long es, long ds, long edi, \
     long esi, long ebp, long esp, long ebx, long edx, long ecx, long eax, \
     long trapno, long err, long eip, long cs, long eflags, long ss, \
     long state387, long linux_eflags, long linux_eip

void sigtrap (SIGSTACK);
void sigill  (SIGSTACK);
void sigfpe  (SIGSTACK);
void sigsegv (SIGSTACK);

void show_regs(void), show_ints(int, int);
inline int do_hard_int(int), do_soft_int(int);

char  pop_byte  (struct vm86_regs *);
short pop_short (struct vm86_regs *);
long  pop_long  (struct vm86_regs *);

void push_long (struct vm86_regs *, long);
void push_word (struct vm86_regs *, short);
void push_byte (struct vm86_regs *, char);

void push_isf(struct vm86_regs *, interrupt_stack_frame);
interrupt_stack_frame pop_isf(struct vm86_regs *);

#endif /* CPU_H */
