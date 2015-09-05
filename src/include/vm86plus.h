#ifndef _LINUX_VM86PLUS_H
#define _LINUX_VM86PLUS_H

#ifdef __linux__
#include "emu.h"
#include "vm86_compat.h"
#include "cpu-emu.h"
#include <unistd.h>
#include <sys/syscall.h>
#endif /* __linux__ */

#define CBACK_SEG BIOS_HLT_BLK_SEG
EXTERN Bit16u CBACK_OFF;

int vm86_init(void);

#ifdef __i386__
#define vm86_plus(function,param) syscall(SYS_vm86, function, param)

static inline int true_vm86(struct vm86plus_struct *x)
{
    int ret;
    unsigned short fs = getsegment(fs), gs = getsegment(gs);

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
	return e_vm86();
#endif
    return true_vm86(x);
#else
    return e_vm86();
#endif
}

#define DO_VM86(x) do_vm86(x)

#endif
