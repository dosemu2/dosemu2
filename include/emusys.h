#ifndef __EMUSYS_H
#define __EMUSYS_H

#define EMUSYS_AVAILABLE (__NR_emusys>0)


#define EMUSYSVERSION 0x00010000
#define EMUSYS_GETVERSION      0

#define EMUSYS_GETSUPPORT      1
  #define EMUSYS_HAS_SILLYINT    1

#define  EMUSYS_REQUEST_IRQ    2
#define  EMUSYS_FREE_IRQ       3


#define  FASTSYS_INT           0xe6
  #define FASTSYS_MODE_MASK                0xFF
  #define FASTSYS_GET_IRQFLAGS                1
  #define FASTSYS_CLEAR_IRQ_BIT               2
  #define FASTSYS_SET_IRQ_BIT                 3

#ifdef __KERNEL__

struct fast_syscall_regs {
  unsigned long
  /* This is the stack layout after a pusha instruction.
   * The esp value is useless for user space, as it is the esp from
   * from kernel stack. On popa, esp is ignored.
   */
/* 0   4   8   12  16  20  24  28 */
  edi,esi,ebp,esp,ebx,edx,ecx,eax,
/*32  36  40  44 */
  ds, es, fs, gs,
/*48         52 */
  caller_eip,caller_cs,
  eflags,
  user_esp,user_ss; /* if this was an INT from P3 to P0 */
};

#else

#ifdef __EMUSYS_parent
int __NR_emusys= -1;
#else
extern int __NR_emusys;
#endif

#include <linux/sys.h>
#define __NR_resolve_syscall (NR_syscalls-1)

extern inline int resolve_emusyscall(void) 
{
register int __res;
__asm__ __volatile__(
  "  
  int $0x80
  "
  :"=a" (__res):"a" ((int)__NR_resolve_syscall), "b" ((char *)"emusys"), "c" ((int)0)
  );
  __NR_emusys=__res;  
  return __res;
}
 
extern inline int emusyscall(int mode, int params)
{
register int __res;
__asm__ __volatile__(
  "  
  int $0x80
  "
  :"=a" (__res):"a" ((int)__NR_emusys), "b" ((int)mode), "c" ((int)params) 
  );  
  return __res;
}

extern inline int fastsyscall(int mode)
{
register int __res;
__asm__ __volatile__(
  "
  int %1
  "
  :"=a" (__res):"i" (FASTSYS_INT), "a" (mode)
  );
  return __res;
}

extern inline int get_and_reset_irq(int irqnum)
{
register int __res;
__asm__ __volatile__(
  "
  int %1 # 0xe6
  " 
  :"=a" (__res):"i" (FASTSYS_INT), "a" ((long)FASTSYS_CLEAR_IRQ_BIT), "b" (irqnum)
  );
  return __res;
}
 
extern inline int get_and_set_irq(int irqnum)
{
register int __res;
__asm__ __volatile__(
  "
  int %1 # 0xe6
  "
  :"=a" (__res):"i" (FASTSYS_INT), "a" ((long)FASTSYS_SET_IRQ_BIT), "b" (irqnum)
  );
  return __res;
}

extern inline int get_irq_bits(void)
{
register int __res;
__asm__ __volatile__(
  "
  int %1 # 0xe6
  "
  :"=a" (__res): "i" (FASTSYS_INT), "a" ((long)FASTSYS_GET_IRQFLAGS)
  );
  return __res;
}

#endif

#endif /* __EMUSYS_H */
