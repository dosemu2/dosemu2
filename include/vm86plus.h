#ifndef _LINUX_VM86PLUS_H
#define _LINUX_VM86PLUS_H

#include <linux/vm86.h>

#define VM86PLUS_MAGIC 0x4d564544 /* = "DEVM" */

struct vm86plus_info_struct {
	long vm86plus_magic;
	long dosemuver;      /* format 53058 = 0.53.58 or 1002003 = 1.2.3 */
        unsigned long force_return_for_pic:1;
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


/*
 * This is how vm86() should be called, if using vm86plus
 *
 */
 

#ifdef USE_VM86PLUS
  #define DO_VM86(x) ( \
    (x)->vm86plus.force_return_for_pic = (pic_irr & ~(pic_isr | pice_imr)) != 0, \
    vm86((struct vm86_struct *)(x)) )
#else
  #define DO_VM86(x) vm86(x)
#endif


#endif
