#ifndef _LINUX_VM86PLUS_H
#define _LINUX_VM86PLUS_H

#ifdef __linux__
#include "config.h"
#include <sys/vm86.h>
#include <sys/syscall.h>

#if defined (REQUIRES_VM86PLUS) && !(defined(BUILTIN_VMP86PLUS) || defined(VM86_PLUS_INSTALL_CHECK))


/*
 * Additional return values when invoking new vm86()
 */
#define VM86_PICRETURN	4	/* return due to pending PIC request */
#define VM86_TRAP	6	/* return due to DOS-debugger request */

/*
 * function codes when invoking new vm86()
 */
#define VM86_PLUS_INSTALL_CHECK	0
#define VM86_ENTER		1
#define VM86_ENTER_NO_BYPASS	2
#define	VM86_REQUEST_IRQ	3
#define VM86_FREE_IRQ		4
#define VM86_GET_IRQ_BITS	5
#define VM86_GET_AND_RESET_IRQ	6


struct vm86plus_info_struct {
	unsigned long force_return_for_pic:1;
	unsigned long vm86dbg_active:1;       /* for debugger */
	unsigned long vm86dbg_TFpendig:1;     /* for debugger */
	unsigned long unused:28;
	unsigned long is_vm86pus:1;	      /* for vm86 internal use */
	unsigned char vm86dbg_intxxtab[32];   /* for debugger */
};

struct vm86plus_struct {
	struct vm86_regs regs;
	unsigned long flags;
	unsigned long screen_bitmap;
	unsigned long cpu_type;
	struct revectored_struct int_revectored;
	struct revectored_struct int21_revectored;
	struct vm86plus_info_struct vm86plus;
};

#ifdef __KERNEL__

struct kernel_vm86_struct {
	struct vm86_regs regs;
/*
 * the below part remains on the kernel stack while we are in VM86 mode.
 * 'tss.esp0' then contains the address of VM86_TSS_ESP0 below, and when we
 * get forced back from VM86, the CPU and "SAVE_ALL" will restore the above
 * 'struct kernel_vm86_regs' with the then actual values.
 * Therefore, pt_regs in fact points to a complete 'kernel_vm86_struct'
 * in kernelspace, hence we need not reget the data from userspace.
 */
#define VM86_TSS_ESP0 flags
	unsigned long flags;
	unsigned long screen_bitmap;
	unsigned long cpu_type;
	struct revectored_struct int_revectored;
	struct revectored_struct int21_revectored;
	struct vm86plus_info_struct vm86plus;
	struct pt_regs *regs32;   /* here we save the pointer to the old regs */
/*
 * The below is not part of the structure, but the stack layout continues
 * this way. In front of 'return-eip' may be some data, depending on
 * compilation, so we don't rely on this and save the pointer to 'oldregs'
 * in 'regs32' above.
 * However, with GCC-2.7.2 and the the current CFLAGS you see exactly this:

	long return-eip;        from call to vm86()
	struct pt_regs oldregs;  user space registers as saved by syscall
 */
};


#endif /* __KERNEL__ */

#endif /* no BUILTIN_VMP86PLUS */
#endif /* __linux__ */

#define OLD_SYS_vm86  113
#define NEW_SYS_vm86  166

#ifdef REQUIRES_VM86PLUS

static inline int vm86_plus(int function, int param)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)NEW_SYS_vm86), "b" (function), "c" (param));
	return __res;
} 

  #undef vm86
  #define vm86(x) vm86_plus(VM86_ENTER, (int) /* struct vm86_struct* */(x))
  #define _DO_VM86_(x) ( \
    (x)->vm86plus.force_return_for_pic = (pic_irr & ~(pic_isr | pice_imr)) != 0, \
    vm86((struct vm86_struct *)(x)) )
  #ifdef USE_MHPDBG
    #if 1
      #define DO_VM86(x) _DO_VM86_(x)
    #else
      /* ...hmm, this one seems not to work properly (Hans) */
      #define DO_VM86(x) (WRITE_FLAGS((READ_FLAGS() & ~TF) | mhpdbg.flags), _DO_VM86_(x))
    #endif
  #else
    #define DO_VM86(x) _DO_VM86_(x)
  #endif
#else
static inline int vm86_old(struct vm86_struct* v86)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)OLD_SYS_vm86), "b" ((int)v86));
	return __res;
} 
  #define DO_VM86(x) vm86_old(x)
#endif

#endif
