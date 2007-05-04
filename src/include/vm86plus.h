/* 
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

#ifndef _LINUX_VM86PLUS_H
#define _LINUX_VM86PLUS_H

#ifdef __linux__
#include "emu.h"
#include <Asm/vm86.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif /* __linux__ */

#ifdef X86_EMULATOR
int e_vm86(void);

static inline int emu_vm86(struct vm86plus_struct *x)
{
    x->vm86plus.force_return_for_pic = 0;
    return e_vm86();
}
#endif

#ifdef __i386__
#define vm86_plus(function,param) syscall(SYS_vm86, function, param)

static inline int true_vm86(struct vm86plus_struct *x)
{
    int ret;
    unsigned short fs = getsegment(fs), gs = getsegment(gs);

    x->vm86plus.force_return_for_pic = 0;
    ret = vm86_plus(VM86_ENTER, x);
    /* kernel 2.4 doesn't preserve GS -- and it doesn't hurt to restore here */
    loadregister(fs, fs);
    loadregister(gs, gs);
    return ret;
}
#endif

static inline int do_vm86(struct vm86plus_struct *x)
{
#ifdef __i386__
#ifdef X86_EMULATOR
    if (config.cpuemu)
	return emu_vm86(x);
#endif
    return true_vm86(x);
#else
    return emu_vm86(x);
#endif
}

#if defined(USE_MHPDBG) && 0
/* ...hmm, this one seems not to work properly (Hans) */
  #define DO_VM86(x) (WRITE_FLAGS((READ_FLAGS() & ~TF) | mhpdbg.flags), do_vm86(x))
#else
  #define DO_VM86(x) do_vm86(x)
#endif

#endif
