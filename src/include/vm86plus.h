/* 
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

#ifndef _LINUX_VM86PLUS_H
#define _LINUX_VM86PLUS_H

#ifdef __linux__
#include "config.h"
#include <sys/vm86.h>
#include <sys/syscall.h>
#endif /* __linux__ */

#define OLD_SYS_vm86  113
#define NEW_SYS_vm86  166

#ifdef REQUIRES_VM86PLUS

#ifdef X86_EMULATOR
int e_vm86(void);

#define E_VM86(x) ( \
    (x)->vm86plus.force_return_for_pic = (pic_irr & ~(pic_isr | pice_imr)) != 0, \
    e_vm86() )
#endif

static inline int vm86_plus(int function, int param)
{
	int __res;
	__asm__ __volatile__("int $0x80\n"
	:"=a" (__res):"a" ((int)NEW_SYS_vm86), "b" (function), "c" (param));
	return __res;
} 

  #undef vm86
  #define vm86(x) vm86_plus(VM86_ENTER, (int) /* struct vm86_struct* */(x))
  #define _DO_VM86__(x) ( \
    (x)->vm86plus.force_return_for_pic = (pic_irr & ~(pic_isr | pice_imr)) != 0, \
    vm86((struct vm86_struct *)(x)) )
 #ifdef X86_EMULATOR
  #define _DO_VM86_(x) ( \
    config.cpuemu? E_VM86(x) : _DO_VM86__(x) )
  #define TRUE_VM86(x)	_DO_VM86__(x)
 #else
  #define _DO_VM86_(x)	_DO_VM86__(x)
 #endif
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
#ifdef X86_EMULATOR
#error You need vm86plus with cpuemu
#endif
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
