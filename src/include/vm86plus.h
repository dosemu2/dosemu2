#ifndef _LINUX_VM86PLUS_H
#define _LINUX_VM86PLUS_H

#ifdef __linux__
#include "config.h"
#include <sys/vm86.h>
#include <sys/syscall.h>

#if defined (REQUIRES_VM86PLUS) && !(defined(BUILTIN_VMP86PLUS) || defined(VM86_PLUS_INSTALL_CHECK))

struct vm86plus_info_struct {
        unsigned long force_return_for_pic:1;
        unsigned long vm86dbg_active:1;       /* for debugger */
        unsigned long vm86dbg_TFpendig:1;     /* for debugger */
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

/*
 * Additional return values for the 'vm86()' system call
 *
 * These are the existing ones in <linux/vm86.h> :
 *   VM86_SIGNAL	0	 return due to signal 
 *   VM86_UNKNOWN	1	 unhandled GP fault - IO-instruction or similar
 *   VM86_INTx		2	 int3/int x instruction (ARG = x)
 *   VM86_STI		3	 sti/popf/iret instruction enabled virtual interrupts 
 */
#define VM86_PICRETURN  4     /* return due to pending PIC request */
#define VM86_TRAP	6     /* return due to DOS-debugger request */

#endif /* no BUILTIN_VMP86PLUS */
#endif /* __linux__ */


#ifdef REQUIRES_VM86PLUS
/*
 * This is how vm86() should be called, if using vm86plus
 *
 */

#if !(defined(BUILTIN_VMP86PLUS) || defined(VM86_PLUS_INSTALL_CHECK))
#define VM86_PLUS_INSTALL_CHECK	((unsigned long)-255)
#define VM86_ENTER		((unsigned long)-1)
#define VM86_ENTER_NO_BYPASS	((unsigned long)-2)
#define	VM86_REQUEST_IRQ	((unsigned long)-3)
#define VM86_FREE_IRQ		((unsigned long)-4)
#define VM86_GET_IRQ_BITS	((unsigned long)-5)
#define VM86_GET_AND_RESET_IRQ	((unsigned long)-6)
#endif

static inline int vm86_plus(int function, int param)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)SYS_vm86), "b" (function), "c" (param));
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
  #define DO_VM86(x) vm86(x)
#endif


#endif
