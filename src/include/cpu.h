/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 */

#ifndef CPU_H
#define CPU_H
#ifdef BIOSSEG
#undef BIOSSEG
#endif
#ifdef __NetBSD__
#include <signal.h>
#include "netbsd_vm86.h"
#endif
#ifdef __linux__
#include "kversion.h"
#include <asm/vm86.h>
#include <sys/vm86.h>
#endif
#ifndef BIOSSEG
#define BIOSSEG 0xf000
#endif
#ifdef __linux__
#define _regs vm86s.regs
#endif
#ifdef __NetBSD__
#define _regs vm86s.substr.regs.vmsc
#endif

#include "extern.h"

#ifdef X86_EMULATOR
#include "x86_emulator.h"
#endif

#ifndef X86_EMULATOR
/* all registers as a structure */
#ifdef __linux__
#define REGS  vm86s.regs
/* this is used like: REG(eax) = 0xFFFFFFF */
#define REG(reg) (REGS.##reg)
#define READ_SEG_REG(reg) (REGS.##reg)
#define WRITE_SEG_REG(reg, val) REGS.##reg = (val)
#endif

#ifdef __NetBSD__
#define REGS  vm86s.substr.regs.vmsc
/* this is used like: REG(eax) = 0xFFFFFFF */
#define REG(reg) (REGS.##reg)
#define READ_SEG_REG(reg) (REGS.##reg)
#define WRITE_SEG_REG(reg, val) REGS.##reg = (val)
#define es sc_es
#define	ds sc_ds
#define	edi sc_edi
#define	esi sc_esi
#define	ebp sc_ebp
#define	ebx sc_ebx
#define	edx sc_edx
#define	ecx sc_ecx
#define	eax sc_eax
#define	eip sc_eip
#define	cs sc_cs
#define	eflags sc_eflags
#define	esp sc_esp
#define	ss sc_ss
#define	fs sc_fs
#define	gs sc_gs
#endif

#define _AL      (*(__u8 *)&vm86s.regs.eax)
#define _BL      (*(__u8 *)&vm86s.regs.ebx)
#define _CL      (*(__u8 *)&vm86s.regs.ecx)
#define _DL      (*(__u8 *)&vm86s.regs.edx)
#define _AH      (*(((__u8 *)&vm86s.regs.eax)+1))
#define _BH      (*(((__u8 *)&vm86s.regs.ebx)+1))
#define _CH      (*(((__u8 *)&vm86s.regs.ecx)+1))
#define _DH      (*(((__u8 *)&vm86s.regs.edx)+1))
#define _AX      (*(__u16 *)&vm86s.regs.eax)
#define _BX      (*(__u16 *)&vm86s.regs.ebx)
#define _CX      (*(__u16 *)&vm86s.regs.ecx)
#define _DX      (*(__u16 *)&vm86s.regs.edx)
#define _SI      (*(__u16 *)&vm86s.regs.esi)
#define _DI      (*(__u16 *)&vm86s.regs.edi)
#define _BP      (*(__u16 *)&vm86s.regs.ebp)
#define _SP      (*(__u16 *)&vm86s.regs.esp)
#define _IP      (*(__u16 *)&vm86s.regs.eip)
#define _EAX     ((__u32)(vm86s.regs.eax))
#define _EBX     ((__u32)(vm86s.regs.ebx))
#define _ECX     ((__u32)(vm86s.regs.ecx))
#define _EDX     ((__u32)(vm86s.regs.edx))
#define _ESI     ((__u32)(vm86s.regs.esi))
#define _EDI     ((__u32)(vm86s.regs.edi))
#define _EBP     ((__u32)(vm86s.regs.ebp))
#define _ESP     ((__u32)(vm86s.regs.esp))
#define _EIP     ((__u32)(vm86s.regs.eip))
#define _CS      ((__u16)(vm86s.regs.cs))
#define _DS      ((__u16)(vm86s.regs.ds))
#define _SS      ((__u16)(vm86s.regs.ss))
#define _ES      ((__u16)(vm86s.regs.es))
#define _FS      ((__u16)(vm86s.regs.fs))
#define _GS      ((__u16)(vm86s.regs.gs))
#define _EFLAGS  ((__u32)(vm86s.regs.eflags))

#define __sti__  (WRITE_FLAGS(READ_FLAGS() | IF))
#define __cli__  (WRITE_FLAGS(READ_FLAGS() & ~IF))
#define __stc__  (WRITE_FLAGS(READ_FLAGS() | CF))
#define __clc__  (WRITE_FLAGS(READ_FLAGS() & ~CF))
#define __std__  (WRITE_FLAGS(READ_FLAGS() | DF))
#define __cld__  (WRITE_FLAGS(READ_FLAGS() & ~DF))
#if 0
#define CARRY    __stc__
#define NOCARRY  __clc__
#endif

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

#define WRITE_FLAGS(val)    LWORD(eflags) = (val)
#define WRITE_FLAGSE(val)    REG(eflags) = (val)
#define READ_FLAGS()        LWORD(eflags)
#define READ_FLAGSE()        REG(eflags)

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
#ifdef __NetBSD__
struct revectored_struct {
	unsigned long __map[8];			/* 256 bits */
};
static __inline__ void set_revectored(int nr, unsigned char * bitmap)
{
	__asm__ __volatile__("btsl %1,%0"
		: /* no output */
		:"m" (* (struct revectored_struct *)bitmap),"r" (nr));
}

static __inline__ void reset_revectored(int nr, unsigned char * bitmap)
{
	__asm__ __volatile__("btrl %1,%0"
		: /* no output */
		:"m" (* (struct revectored_struct *)bitmap),"r" (nr));
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

/*#define vflags ((REG(eflags) & VIF)? LWORD(eflags) | IF : LWORD(eflags) & ~IF)*/
#define vflags ((READ_FLAGSE() & VIF)? READ_FLAGS() | IF : READ_FLAGS() & ~IF)

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

#if 0
#define CARRY (LWORD(eflags) |= CF)
#define NOCARRY (LWORD(eflags) &= ~CF)
#else 
#define CARRY   ( WRITE_FLAGS(READ_FLAGS() |  CF) )
#define NOCARRY ( WRITE_FLAGS(READ_FLAGS() & ~CF) )
#endif


#ifdef __linux__
  #include <asm/sigcontext.h>
  #if KERNEL_VERSION >= 2001000
  #define sigcontext_struct sigcontext
  #endif
#endif

#ifdef __NetBSD__
#define _gs     (scp->sc_gs)
#define _fs     (scp->sc_fs)
#define _es     (scp->sc_es)
#define _ds     (scp->sc_ds)
#define _edi    (scp->sc_edi)
#define _esi    (scp->sc_esi)
#define _ebp    (scp->sc_ebp)
#define _esp    (scp->sc_esp)
#define _ebx    (scp->sc_ebx)
#define _edx    (scp->sc_edx)
#define _ecx    (scp->sc_ecx)
#define _eax    (scp->sc_eax)
#define _err	(scp->sc_err)
#define _eip    (scp->sc_eip)
#define _cs     (scp->sc_cs)
#define _eflags (scp->sc_eflags)
#define _ss     (scp->sc_ss)
#define _cr2	0			/* XXX no cr2 in sigcontext */


#define _trapno (c2trap[code])

#ifdef INIT_C2TRAP
/* convert from trap codes in trap.h back to source interrupt #'s
   (index into IDT) */
int c2trap[] = {
	0x06,				/* T_PRIVINFLT */
	0x03,				/* T_BPTFLT */
	0x10,				/* T_ARITHTRAP */
	0xff,				/* T_ASTFLT */
	0x0D,				/* T_PROTFLT */
	0x01,				/* T_TRCTRAP */
	0x0E,				/* T_PAGEFLT */
	0x11,				/* T_ALIGNFLT */
	0x00,				/* T_DIVIDE */
	0x02,				/* T_NMI */
	0x04,				/* T_OFLOW */
	0x05,				/* T_BOUND */
	0x07,				/* T_DNA */
	0x08,				/* T_DOUBLEFLT */
	0x09,				/* T_FPOPFLT */
	0x0A,				/* T_TSSFLT */
	0x0B,				/* T_SEGNPFLT */
	0x0C,				/* T_STKFLT */
};
#else
extern int c2trap[];
#endif

void dosemu_fault(int, int, struct sigcontext *);
void vm86_return(int, int, struct sigcontext *);
void vm86_setstate(struct vm86_struct *, struct sigcontext *);
#endif

#ifdef __linux__
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
#define _err	(scp->err)
#define _trapno (scp->trapno)
#define _eip    (scp->eip)
#define _cs     (scp->cs)
#define _eflags (scp->eflags)
#define _ss     (scp->ss)
#define _cr2	(scp->cr2)

void dosemu_fault(int, struct sigcontext_struct);
#endif

void show_regs(char *, int), show_ints(int, int);
__inline__ int do_hard_int(int), do_soft_int(int);

extern unsigned int read_port_w(unsigned short port);
extern int write_port_w(unsigned int value,unsigned short port);

#endif /* CPU_H */
