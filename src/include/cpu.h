/* cpu.h, for the Linux DOS emulator
 *    Copyright (C) 1993 Robert Sanders, gt8134b@prism.gatech.edu
 */

#ifndef CPU_H
#define CPU_H

#include "pic.h"
#include "types.h"
#include "bios.h"
#include "memory.h"
#include "sig.h"
#include <signal.h>
#include <inttypes.h>
#include <fenv.h>
#include "vm86_compat.h"

#ifndef PAGE_SIZE
#define PAGE_SIZE	4096
#endif

#define _regs vm86s.regs

#ifndef HAVE_STD_C11
#undef static_assert
#define static_assert(c, m) ((const char *) 0)
#else
#ifndef HAVE_STATIC_ASSERT
#define static_assert _Static_assert
#endif
#endif

/* all registers as a structure */
#define REGS  vm86s.regs
/* this is used like: REG(eax) = 0xFFFFFFF */
#ifdef __i386__
/* unfortunately the regs are defined as long (not even unsigned) in vm86.h */
#define REG(reg) (*(uint32_t *)({static_assert(sizeof(REGS.reg) == 4, "bad reg"); &REGS.reg; }))
#else
#define REG(reg) (*({static_assert(sizeof(REGS.reg) == 4, "bad reg"); &REGS.reg; }))
#endif
#define SREG(reg) (*({static_assert(sizeof(REGS.reg) == 2, "bad sreg"); &REGS.reg; }))
#define READ_SEG_REG(reg) (REGS.reg)
#define WRITE_SEG_REG(reg, val) REGS.reg = (val)

union dword {
  Bit32u d;
  struct { Bit16u l, h; } w;
#ifdef __i386__
  /* unsigned long member is needed only for strict aliasing,
   * we never convert to it, but sometimes convert _from_ it */
  unsigned long ul;
#endif
  struct { Bit8u l, h, b2, b3; } b;
};

union g_reg {
  greg_t reg;
#ifdef __x86_64__
  uint64_t q;
  uint32_t d[2];
  uint16_t w[4];
#else
  uint32_t d[1];
  uint16_t w[2];
#endif
};

#define DWORD_(reg)	(((union g_reg *)&(reg))->d[0])
/* vxd.c redefines DWORD */
#define DWORD(wrd)	DWORD_(wrd)

#define LO_WORD(wrd)	(((union dword *)&(wrd))->w.l)
#define HI_WORD(wrd)    (((union dword *)&(wrd))->w.h)

#define LO_BYTE(wrd)	(((union dword *)&(wrd))->b.l)
#define HI_BYTE(wrd)    (((union dword *)&(wrd))->b.h)

#define _AL      LO(ax)
#define _BL      LO(bx)
#define _CL      LO(cx)
#define _DL      LO(dx)
#define _AH      HI(ax)
#define _BH      HI(bx)
#define _CH      HI(cx)
#define _DH      HI(dx)
#define _AX      LWORD(eax)
#define _BX      LWORD(ebx)
#define _CX      LWORD(ecx)
#define _DX      LWORD(edx)
#define _SI      LWORD(esi)
#define _DI      LWORD(edi)
#define _BP      LWORD(ebp)
#define _SP      LWORD(esp)
#define _IP      LWORD(eip)
#define _EAX     UDWORD(eax)
#define _EBX     UDWORD(ebx)
#define _ECX     UDWORD(ecx)
#define _EDX     UDWORD(edx)
#define _ESI     UDWORD(esi)
#define _EDI     UDWORD(edi)
#define _EBP     UDWORD(ebp)
#define _ESP     UDWORD(esp)
#define _EIP     UDWORD(eip)
#define _CS      (vm86s.regs.cs)
#define _DS      (vm86s.regs.ds)
#define _SS      (vm86s.regs.ss)
#define _ES      (vm86s.regs.es)
#define _FS      (vm86s.regs.fs)
#define _GS      (vm86s.regs.gs)
#define _EFLAGS  UDWORD(eflags)
#define _FLAGS   REG(eflags)

/* these are used like:  LO(ax) = 2 (sets al to 2) */
#define LO(reg)  vm86u.b[offsetof(struct vm86_struct, regs.e##reg)]
#define HI(reg)  vm86u.b[offsetof(struct vm86_struct, regs.e##reg)+1]

#define _LO(reg) LO_BYTE(_##e##reg)
#define _HI(reg) HI_BYTE(_##e##reg)

/* these are used like: LWORD(eax) = 65535 (sets ax to 65535) */
#define LWORD(reg)	(*({ static_assert(sizeof(REGS.reg) == 4, "bad reg"); \
	&vm86u.w[offsetof(struct vm86_struct, regs.reg)/2]; }))
#define HWORD(reg)	(*({ static_assert(sizeof(REGS.reg) == 4, "bad reg"); \
	&vm86u.w[offsetof(struct vm86_struct, regs.reg)/2+1]; }))
#define UDWORD(reg)	(*({ static_assert(sizeof(REGS.reg) == 4, "bad reg"); \
	&vm86u.d[offsetof(struct vm86_struct, regs.reg)/4]; }))

#define _LWORD(reg)	LO_WORD(_##reg)
#define _HWORD(reg)	HI_WORD(_##reg)

/* this is used like: SEG_ADR((char *), es, bx) */
#define SEG_ADR(type, seg, reg)  type(LINEAR2UNIX(SEGOFF2LINEAR(SREG(seg), LWORD(e##reg))))

/* alternative SEG:OFF to linear conversion macro */
#define SEGOFF2LINEAR(seg, off)  ((((unsigned)(seg)) << 4) + (off))

#define SEG2LINEAR(seg)		LINEAR2UNIX(SEGOFF2LINEAR(seg, 0))

typedef unsigned int FAR_PTR;	/* non-normalized seg:off 32 bit DOS pointer */
typedef struct {
  u_short offset;
  u_short segment;
} far_t;
#define MK_FP16(s,o)		((((unsigned int)(s)) << 16) | ((o) & 0xffff))
#define MK_FP			MK_FP16
#define FP_OFF16(far_ptr)	((far_ptr) & 0xffff)
#define FP_SEG16(far_ptr)	(((far_ptr) >> 16) & 0xffff)
#define MK_FP32(s,o)		LINEAR2UNIX(SEGOFF2LINEAR(s,o))
#define FP_OFF32(linear)	((linear) & 15)
#define FP_SEG32(linear)	(((linear) >> 4) & 0xffff)
#define rFAR_PTR(type,far_ptr) ((type)((FP_SEG16(far_ptr) << 4)+(FP_OFF16(far_ptr))))
#define FARt_PTR(f_t_ptr) (MK_FP32((f_t_ptr).segment, (f_t_ptr).offset))
#define MK_FARt(seg, off) ((far_t){(off), (seg)})
static inline far_t rFAR_FARt(FAR_PTR far_ptr) {
  return MK_FARt(FP_SEG16(far_ptr), FP_OFF16(far_ptr));
}
static inline void *FAR2PTR(FAR_PTR far_ptr) {
  return MK_FP32(FP_SEG16(far_ptr), FP_OFF16(far_ptr));
}

#define peek(seg, off)	(READ_WORD(SEGOFF2LINEAR(seg, off)))

extern fpregset_t vm86_fpu_state;
extern fenv_t dosemu_fenv;

/*
 * Boy are these ugly, but we need to do the correct 16-bit arithmetic.
 * Using non-obvious calling conventions..
 */
#define pushw(base, ptr, val) \
	do { \
		ptr = (Bit16u)(ptr - 1); \
		WRITE_BYTE((base) + ptr, (val) >> 8); \
		ptr = (Bit16u)(ptr - 1); \
		WRITE_BYTE((base) + ptr, val); \
	} while(0)

#define pushl(base, ptr, val) \
	do { \
		pushw(base, ptr, (Bit16u)((val) >> 16)); \
		pushw(base, ptr, (Bit16u)(val)); \
	} while(0)

#define popb(base, ptr) \
	({ \
		Bit8u __res = READ_BYTE((base) + ptr); \
		ptr = (Bit16u)(ptr + 1); \
		__res; \
	})

#define popw(base, ptr) \
	({ \
		Bit8u __res0, __res1; \
		__res0 = READ_BYTE((base) + ptr); \
		ptr = (Bit16u)(ptr + 1); \
		__res1 = READ_BYTE((base) + ptr); \
		ptr = (Bit16u)(ptr + 1); \
		(__res1 << 8) | __res0; \
	})

#define popl(base, ptr) \
	({ \
		Bit16u __res0, __res1; \
		__res0 = popw(base, ptr); \
		__res1 = popw(base, ptr); \
		(__res1 << 16) | __res0; \
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
	asm volatile("fxrstor64  %0\n" :: "m"(value))

#define savefpstate(value) \
	asm volatile("fxsave64 %0; fninit\n": "=m"(value))
#else
#define loadfpstate(value) \
	do { \
		if (config.cpufxsr) \
			asm volatile("fxrstor %0\n" :: \
				    "m"(*((char *)&value+112))); \
		else \
			asm volatile("frstor %0\n" :: "m"(value)); \
	} while(0)

#define savefpstate(value) \
	do { \
		if (config.cpufxsr) { \
			asm volatile("fxsave %0; fninit\n" : \
				     "=m"(*((char *)&value+112))); \
		} else \
			asm volatile("fnsave %0; fwait\n" : "=m"(value)); \
	} while(0)
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

static __inline__ int is_revectored(int nr, struct revectored_struct * bitmap)
{
	uint8_t ret;
	__asm__ __volatile__(
			    "btl %2,%1\n"
			    "setcb %0\n"
		: "=r"(ret)
		:"m" (*bitmap),"r" (nr));
	return ret;
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
#define set_IF() (_EFLAGS |= VIF)
#define clear_IF() (_EFLAGS &= ~VIF)
#define clear_IF_timed() (clear_IF(), ({if (!is_cli) is_cli++;}))
#define isset_IF() ((_EFLAGS & VIF) != 0)
       /* carry flag */
#define set_CF() (_EFLAGS |= CF)
#define clear_CF() (_EFLAGS &= ~CF)
#define isset_CF() ((_EFLAGS & CF) != 0)
       /* zero flag */
#define set_ZF() (_EFLAGS |= ZF)
#define clear_ZF() (_EFLAGS &= ~ZF)
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
#define set_FLAGS(flags) set_EFLAGS(_FLAGS, flags)
#define get_EFLAGS(flags) ({ \
  int __flgs = (flags); \
  (((__flgs & IF) ? __flgs | VIF : __flgs & ~VIF) | IF | IOPL_MASK); \
})
#define get_FLAGS(flags) ({ \
  int __flgs = (flags); \
  (((__flgs & VIF) ? __flgs | IF : __flgs & ~IF)); \
})
#define read_EFLAGS() (isset_IF()? (_EFLAGS | IF):(_EFLAGS & ~IF))
#define read_FLAGS()  (isset_IF()? (_FLAGS | IF):(_FLAGS & ~IF))

#define CARRY   set_CF()
#define NOCARRY clear_CF()

#define vflags read_FLAGS()

/* this is the array of interrupt vectors */
struct vec_t {
  unsigned short offset;
  unsigned short segment;
};

#if 0
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

#ifdef __x86_64__
#define _es     (((union g_reg *)&(scp->gregs[REG_TRAPNO]))->w[1])
#define _ds     (((union g_reg *)&(scp->gregs[REG_TRAPNO]))->w[2])
#define _rdi    (scp->gregs[REG_RDI])
#define _rsi    (scp->gregs[REG_RSI])
#define _rbp    (scp->gregs[REG_RBP])
#define _rsp    (scp->gregs[REG_RSP])
#define _rbx    (scp->gregs[REG_RBX])
#define _rdx    (scp->gregs[REG_RDX])
#define _rcx    (scp->gregs[REG_RCX])
#define _rax    (scp->gregs[REG_RAX])
#define _rip    (scp->gregs[REG_RIP])
#define _cs     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[0])
#define _gs     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[1])
#define _fs     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[2])
#define _ss     (((union g_reg *)&scp->gregs[REG_CSGSFS])->w[3])
#define _err    DWORD_(scp->gregs[REG_ERR])
#define _eflags DWORD_(scp->gregs[REG_EFL])
#define _cr2    (scp->gregs[REG_CR2])
#define PRI_RG  "llx"
#else
#define _es     (scp->gregs[REG_ES])
#define _ds     (scp->gregs[REG_DS])
#define _rdi    (scp->gregs[REG_EDI])
#define _rsi    (scp->gregs[REG_ESI])
#define _rbp    (scp->gregs[REG_EBP])
#define _rsp    (scp->gregs[REG_ESP])
#define _rbx    (scp->gregs[REG_EBX])
#define _rdx    (scp->gregs[REG_EDX])
#define _rcx    (scp->gregs[REG_ECX])
#define _rax    (scp->gregs[REG_EAX])
#define _rip    (scp->gregs[REG_EIP])
#define _cs     (scp->gregs[REG_CS])
#define _gs     (scp->gregs[REG_GS])
#define _fs     (scp->gregs[REG_FS])
#define _ss     (scp->gregs[REG_SS])
#define _err    (scp->gregs[REG_ERR])
#define _eflags (scp->gregs[REG_EFL])
#define _cr2    (((union dword *)&scp->cr2)->d)
#define PRI_RG  PRIx32
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
#define _trapno (((union g_reg *)&(scp->gregs[REG_TRAPNO]))->w[0])
#define __fpstate (scp->fpregs)

void show_regs(void);
void show_ints(int, int);
char *emu_disasm(unsigned int ip);
void dump_state(void);

int cpu_trap_0f (unsigned char *, sigcontext_t *);

#define PAGE_MASK	(~(PAGE_SIZE-1))
/* to align the pointer to the (next) page boundary */
#define PAGE_ALIGN(addr)	(((addr)+PAGE_SIZE-1)&PAGE_MASK)

enum { es_INDEX, cs_INDEX, ss_INDEX, ds_INDEX, fs_INDEX, gs_INDEX,
  eax_INDEX, ebx_INDEX, ecx_INDEX, edx_INDEX, esi_INDEX, edi_INDEX,
  ebp_INDEX, esp_INDEX, eip_INDEX, eflags_INDEX };

extern int is_cli;

#endif /* CPU_H */
