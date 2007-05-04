/* 
 * All modifications in this file to the original code are
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 */

#ifndef CPU_H
#define CPU_H

#include "pic.h"
#include "types.h"
#include "bios.h"

#ifdef BIOSSEG
#undef BIOSSEG
#endif
#include <signal.h>
#ifdef __linux__
#include "Asm/vm86.h"
#endif
#ifndef BIOSSEG
#define BIOSSEG 0xf000
#endif
#ifdef __linux__
#define _regs vm86s.regs
#endif
#ifndef sigcontext_struct
#define sigcontext_struct sigcontext
#endif

#include "extern.h"

/* all registers as a structure */
#ifdef __linux__
#define REGS  vm86s.regs
/* this is used like: REG(eax) = 0xFFFFFFF */
#define REG(reg) (REGS.reg)
#define READ_SEG_REG(reg) (REGS.reg)
#define WRITE_SEG_REG(reg, val) REGS.reg = (val)
#endif

union dword {
  unsigned long l;
  Bit32u d;
#ifdef __x86_64__
  struct { Bit16u l, h, w2, w3; } w;
#else
  struct { Bit16u l, h; } w;
#endif
  struct { Bit8u l, h, b2, b3; } b;
};

#define DWORD_(wrd)	(((union dword *)&(wrd))->d)
/* vxd.c redefines DWORD */
#define DWORD(wrd)	DWORD_(wrd)

#define LO_WORD(wrd)	(((union dword *)&(wrd))->w.l)
#define HI_WORD(wrd)    (((union dword *)&(wrd))->w.h)

#define LO_BYTE(wrd)	(((union dword *)&(wrd))->b.l)
#define HI_BYTE(wrd)    (((union dword *)&(wrd))->b.h)

#define _AL      LO_BYTE(vm86s.regs.eax)
#define _BL      LO_BYTE(vm86s.regs.ebx)
#define _CL      LO_BYTE(vm86s.regs.ecx)
#define _DL      LO_BYTE(vm86s.regs.edx)
#define _AH      HI_BYTE(vm86s.regs.eax)
#define _BH      HI_BYTE(vm86s.regs.ebx)
#define _CH      HI_BYTE(vm86s.regs.ecx)
#define _DH      HI_BYTE(vm86s.regs.edx)
#define _AX      LO_WORD(vm86s.regs.eax)
#define _BX      LO_WORD(vm86s.regs.ebx)
#define _CX      LO_WORD(vm86s.regs.ecx)
#define _DX      LO_WORD(vm86s.regs.edx)
#define _SI      LO_WORD(vm86s.regs.esi)
#define _DI      LO_WORD(vm86s.regs.edi)
#define _BP      LO_WORD(vm86s.regs.ebp)
#define _SP      LO_WORD(vm86s.regs.esp)
#define _IP      LO_WORD(vm86s.regs.eip)
#define _EAX     DWORD(vm86s.regs.eax)
#define _EBX     DWORD(vm86s.regs.ebx)
#define _ECX     DWORD(vm86s.regs.ecx)
#define _EDX     DWORD(vm86s.regs.edx)
#define _ESI     DWORD(vm86s.regs.esi)
#define _EDI     DWORD(vm86s.regs.edi)
#define _EBP     DWORD(vm86s.regs.ebp)
#define _ESP     DWORD(vm86s.regs.esp)
#define _EIP     DWORD(vm86s.regs.eip)
#define _CS      (vm86s.regs.cs)
#define _DS      (vm86s.regs.ds)
#define _SS      (vm86s.regs.ss)
#define _ES      (vm86s.regs.es)
#define _FS      (vm86s.regs.fs)
#define _GS      (vm86s.regs.gs)
#define _EFLAGS  DWORD(vm86s.regs.eflags)
#define _FLAGS   LO_WORD(vm86s.regs.eflags)

/* these are used like:  LO(ax) = 2 (sets al to 2) */
#define LO(reg)  LO_BYTE(REG(e##reg))
#define HI(reg)  HI_BYTE(REG(e##reg))

#define _LO(reg) LO_BYTE(_##e##reg)
#define _HI(reg) HI_BYTE(_##e##reg)

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define LWORD(reg)	LO_WORD(REG(reg))
#define HWORD(reg)	HI_WORD(REG(reg))

#define _LWORD(reg)	LO_WORD(_##reg)
#define _HWORD(reg)	HI_WORD(_##reg)

/* this is used like: SEG_ADR((char *), es, bx) */
#define SEG_ADR(type, seg, reg)  type((uintptr_t)((LWORD(seg) << 4) + LWORD(e##reg)))

/* alternative SEG:OFF to linear conversion macro */
#define SEGOFF2LINEAR(seg, off)  ((((uintptr_t)(seg)) << 4) + (off))

#define SEG2LINEAR(seg)	((void *)  ( ((uintptr_t)(seg)) << 4)  )

typedef unsigned int FAR_PTR;	/* non-normalized seg:off 32 bit DOS pointer */
typedef struct {
  u_short offset;
  u_short segment;
} far_t;
#define MK_FP16(s,o)		((((unsigned int)s) << 16) | (o & 0xffff))
#define MK_FP			MK_FP16
#define FP_OFF16(far_ptr)	((int)far_ptr & 0xffff)
#define FP_SEG16(far_ptr)	(((unsigned int)far_ptr >> 16) & 0xffff)
#define MK_FP32(s,o)		((void *)SEGOFF2LINEAR(s,o))
#define FP_OFF32(void_ptr)	((uintptr_t)void_ptr & 15)
#define FP_SEG32(void_ptr)	(((uintptr_t)void_ptr >> 4) & 0xffff)
#define rFAR_PTR(type,far_ptr) ((type)((FP_SEG16(far_ptr) << 4)+(FP_OFF16(far_ptr))))
#define FARt_PTR(f_t_ptr) ((void*)SEGOFF2LINEAR((f_t_ptr).segment, (f_t_ptr).offset))
#define MK_FARt(seg, off) ((far_t){(off), (seg)})

#define peek(seg, off)	(READ_WORD(SEGOFF2LINEAR(seg, off)))

#define WRITE_FLAGS(val)    LWORD(eflags) = (val)
#define WRITE_FLAGSE(val)    REG(eflags) = (val)
#define READ_FLAGS()        LWORD(eflags)
#define READ_FLAGSE()        REG(eflags)

extern struct _fpstate vm86_fpu_state;

/*
 * Boy are these ugly, but we need to do the correct 16-bit arithmetic.
 * Using non-obvious calling conventions..
 */
#define pushw(base, ptr, val) \
	do { \
		ptr = (Bit16u)(ptr - 1); \
		WRITE_BYTE((size_t)(base) + ptr, (val) >> 8); \
		ptr = (Bit16u)(ptr - 1); \
		WRITE_BYTE((size_t)(base) + ptr, val); \
	} while(0)

#define popb(base, ptr) \
	({ \
		Bit8u __res = READ_BYTE((size_t)(base) + ptr); \
		ptr = (Bit16u)(ptr + 1); \
		__res; \
	})

#define popw(base, ptr) \
	({ \
		Bit8u __res0, __res1; \
		__res0 = READ_BYTE((size_t)(base) + ptr); \
		ptr = (Bit16u)(ptr + 1); \
		__res1 = READ_BYTE((size_t)(base) + ptr); \
		ptr = (Bit16u)(ptr + 1); \
		(__res1 << 8) | __res0; \
	})

#define loadflags(value) asm volatile("push %0 ; popf"::"g" (value): "cc" )

#define getflags() \
	({ \
		unsigned long __value; \
		asm volatile("pushf ; pop %0":"=g" (__value)); \
		__value; \
	})

#define loadregister(reg, value) \
	asm volatile("mov %0, %%" #reg ::"rm" (value))

#define getregister(reg) \
	({ \
		unsigned long __value; \
		asm volatile("mov %%" #reg ",%0":"=rm" (__value)); \
		__value; \
	})

#define getsegment(reg) \
	({ \
		Bit16u __value; \
		asm volatile("mov %%" #reg ",%0":"=rm" (__value)); \
		__value; \
	})

#ifdef __x86_64__
#define loadfpstate(value) \
	asm volatile("rex64/fxrstor  %0\n" :: "m"(value), "cdaSDb"(&value));

#define savefpstate(value) \
	asm volatile("rex64/fxsave %0; fninit\n": "=m"(value) : "cdaSDb"(&value));
#else
#define loadfpstate(value) \
	do { \
		if (config.cpufxsr) \
			asm volatile("fxrstor %0\n" :: \
				    "m"(*((char *)&value+112)),"m"(value)); \
		else \
			asm volatile("frstor %0\n" :: "m"(value)); \
	} while(0);

#define savefpstate(value) \
	do { \
		if (config.cpufxsr) \
			asm volatile("fxsave %0; fninit\n" : \
				    "=m"(*((char *)&value+112)),"=m"(value)); \
		else \
			asm volatile("fnsave %0; fwait\n" : "=m"(value)); \
	} while(0);
#endif

#ifdef __linux__
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
#endif

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

  /* Flag setting and clearing, and testing */
        /* interrupt flag */
#define set_IF() (_EFLAGS |= VIF, clear_VIP(), is_cli = 0)
#define clear_IF() (_EFLAGS &= ~VIF)
#define isset_IF() ((_EFLAGS & VIF) != 0)
       /* carry flag */
#define set_CF() (_EFLAGS |= CF)
#define clear_CF() (_EFLAGS &= ~CF)
#define isset_CF() ((_EFLAGS & CF) != 0)
       /* direction flag */
#define set_DF() (_EFLAGS |= DF)
#define clear_DF() (_EFLAGS &= ~DF)
#define isset_DF() ((_EFLAGS & DF) != 0)
       /* nested task flag */
#define set_NT() (_EFLAGS |= NT)
#define clear_NT() (_EFLAGS &= ~NT)
#define isset_NT() ((_EFLAGS & NT) != 0)
       /* trap flag */
#define set_TF() (_EFLAGS |= TF)
#define clear_TF() (_EFLAGS &= ~TF)
#define isset_TF() ((_EFLAGS & TF) != 0)
       /* alignment flag */
#define set_AC() (_EFLAGS |= AC)
#define clear_AC() (_EFLAGS &= ~AC)
#define isset_AC() ((_EFLAGS & AC) != 0)
       /* Virtual Interrupt Pending flag */
#define set_VIP()   (_EFLAGS |= VIP)
#define clear_VIP() (_EFLAGS &= ~VIP)
#define isset_VIP()   ((_EFLAGS & VIP) != 0)

#define set_EFLAGS(flgs, new_flgs) ({ \
  int __nflgs = (new_flgs); \
  (flgs)=(__nflgs) | IF | IOPL_MASK; \
  ((__nflgs & IF) ? set_IF() : clear_IF()); \
})
#define set_FLAGS(flags) ((_FLAGS = (flags) | IF | IOPL_MASK), ((_FLAGS & IF)? set_IF(): clear_IF()))
#define get_EFLAGS(flags) ({ \
  int __flgs = flags; \
  (((__flgs & IF) ? __flgs | VIF : __flgs & ~VIF) | IF | IOPL_MASK); \
})
#define get_vFLAGS(flags) ({ \
  int __flgs = flags; \
  ((isset_IF() ? __flgs | IF : __flgs & ~IF) | IOPL_MASK); \
})
#define eflags_VIF(flags) (((flags) & ~VIF) | (isset_IF() ? VIF : 0) | IF | IOPL_MASK)
#define read_EFLAGS() (isset_IF()? (_EFLAGS | IF):(_EFLAGS & ~IF))
#define read_FLAGS()  (isset_IF()? (_FLAGS | IF):(_FLAGS & ~IF))

#define CARRY   set_CF()
#define NOCARRY clear_CF()

#define vflags read_FLAGS()

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
#define IOFF(i) READ_WORD(i * 4)
#define ISEG(i) READ_WORD(i * 4 + 2)
#endif

#define IVEC(i) ((ISEG(i)<<4) + IOFF(i))
#define SETIVEC(i, seg, ofs)	{ WRITE_WORD(i * 4 + 2, seg); \
				  WRITE_WORD(i * 4, ofs); }

#define OP_IRET			0xcf

#include "memory.h" /* for INT_OFF */
#define IS_REDIRECTED(i)	(IVEC(i) != SEGOFF2LINEAR(BIOSSEG, INT_OFF(i)))
#define IS_IRET(i)		(READ_BYTE(IVEC(i)) == OP_IRET)

/*
#define WORD(i) (unsigned short)(i)
*/

#ifdef __linux__
#define _gs     (scp->gs)
#define _fs     (scp->fs)
#ifdef __x86_64__
#define _es     HI_WORD(scp->trapno)
#define _ds     (((union dword *)&(scp->trapno))->w.w2)
#define _rdi    (scp->rdi)
#define _rsi    (scp->rsi)
#define _rbp    (scp->rbp)
#define _rsp    (scp->rsp)
#define _rbx    (scp->rbx)
#define _rdx    (scp->rdx)
#define _rcx    (scp->rcx)
#define _rax    (scp->rax)
#define _rip    (scp->rip)
#define _ss     (((union dword *)&(scp->trapno))->w.w3)
#else
#define _es     (scp->es)
#define _ds     (scp->ds)
#define _rdi    (scp->edi)
#define _rsi    (scp->esi)
#define _rbp    (scp->ebp)
#define _rsp    (scp->esp)
#define _rbx    (scp->ebx)
#define _rdx    (scp->edx)
#define _rcx    (scp->ecx)
#define _rax    (scp->eax)
#define _rip    (scp->eip)
#define _ss     (scp->ss)
#endif
#define _edi    DWORD_(_rdi)
#define _esi    DWORD_(_rsi)
#define _ebp    DWORD_(_rbp)
#define _esp    DWORD_(_rsp)
#define _ebx    DWORD_(_rbx)
#define _edx    DWORD_(_rdx)
#define _ecx    DWORD_(_rcx)
#define _eax    DWORD_(_rax)
#define _eip    DWORD_(_rip)
#define _eax    DWORD_(_rax)
#define _eip    DWORD_(_rip)
#define _err	(scp->err)
#define _trapno LO_WORD(scp->trapno)
#define _cs     (scp->cs)
#define _eflags (scp->eflags)
#define _cr2	(scp->cr2)

#ifdef __x86_64__
void dosemu_fault(int, siginfo_t *, void *);
#else
void dosemu_fault(int, struct sigcontext_struct);
#endif
int dosemu_fault1(int signal, struct sigcontext_struct *scp);
#endif

void show_regs(char *, int), show_ints(int, int);
char *emu_disasm(int sga, unsigned int ip);

int cpu_trap_0f (unsigned char *, struct sigcontext_struct *);

extern unsigned int read_port_w(unsigned short port);
extern int write_port_w(unsigned int value,unsigned short port);
int do_soft_int(int intno);

#endif /* CPU_H */
