/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 *
 * $Date: 1993/11/12 12:32:17 $
 * $Source: /home/src/dosemu0.49pl2/RCS/cpu.h,v $
 * $Revision: 1.1 $
 * $State: Exp $
 */

#ifndef CPU_H
#define CPU_H

#include <linux/vm86.h>
#define _regs	vm86s.regs

#ifndef CPU_C
#define CPU_EXTERN extern
#else
#define CPU_EXTERN
#endif

#define SEG_ADR(type, seg, reg) type((int)(*(us *)&_regs.seg<<4)\
				 + *(us *)&_regs.e##reg)

/* these are used like:  LO(ax) = 2 (sets al to 2) */
#define LO(reg)		(int)(*(unsigned char *)&_regs.e##reg)
#define HI(reg)		(int)(*((unsigned char *)&_regs.e##reg +1))

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define LWORD(reg)      (int)(*((unsigned short *)&_regs.##reg))
#define HWORD(reg)      (int)(*((unsigned short *)&_regs.##reg + 1))

#define CF  0x01
#define PF  0x04
#define AF  0x10
#define ZF  0x40
#define SF  0x80
#define TF  0x100
#define IF  0x200
#define DF  (1<<0xa)
#define OF  (1<<0xb)
#define NT  (1<<0xe)
#define RF  (1<<0x10)
#define VM  0x20000
#define B15 (1<<15)
#define AC  (1<<18)

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
#define WORD(i) (i&0xffff)
*/

#define CARRY	(_regs.eflags|=CF)
#define NOCARRY (_regs.eflags&=~CF)

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
