/*
 *  linux/arch/i386/kernel/signal.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  This is a "patch-module" against Linux-1.2.1 and this is
 *  Copyright (C) 1995 Dong Liu <dliu@faraday-gw.njit.edu>
 *
 *  Module implementation is
 *  Copyright (C) 1995 Hans Lermen <lermen@elserv.ffm.fgan.de> 
 *  
 *  The original source can be found on
 *  ftp.cs.helsinki.fi:/pub/Software/Linux/Kernel/v1.2
 *  in the file linux/arch/i386/kernel/signal.c
 *
 *    ... I hope we did set up all (C)'s correctly 
 *        in order to not violate the GPL (did we ?)
 */

#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/ptrace.h>
#include <linux/unistd.h>

#include <asm/segment.h>

#include "kversion.h"
#if 0
#define KERNEL_VERSION 1003028 /* last verified kernel version */
#endif

#if KERNEL_VERSION < 1001085
#error "Sorry, but this patch runs only on Linux >= 1.1.85"
#endif

#include "emumod.h"


#define _S(nr) (1<<((nr)-1))

#define _BLOCKABLE (~(_S(SIGKILL) | _S(SIGSTOP)))

/*
 * This sets regs->esp even though we don't actually use sigstacks yet..
 */
asmlinkage int sys_sigreturn(unsigned long __unused)
{
#define COPY(x) regs->x = context.x
#define COPY_SEG(x) \
if ((context.x & 0xfffc) && (context.x & 0x4) != 0x4 && (context.x & 3) != 3) goto badframe; COPY(x);
#define COPY_SEG_STRICT(x) \
if (!(context.x & 0xfffc) || (context.x & 3) != 3) goto badframe; COPY(x);
	struct sigcontext_struct context;
	struct pt_regs * regs;

#if defined(_VM86_STATISTICS_)
        extern int signalret_count;
        signalret_count++;
#endif
	regs = (struct pt_regs *) &__unused;
	if (verify_area(VERIFY_READ, (void *) regs->esp, sizeof(context)))
		goto badframe;
	memcpy_fromfs(&context,(void *) regs->esp, sizeof(context));
	current->blocked = context.oldmask & _BLOCKABLE;
	COPY_SEG(ds);
	COPY_SEG(es);
	COPY_SEG(fs);
	COPY_SEG(gs);
	COPY_SEG_STRICT(ss);
	COPY_SEG_STRICT(cs);
	COPY(eip);
	COPY(ecx); COPY(edx);
	COPY(ebx);
	COPY(esp); COPY(ebp);
	COPY(edi); COPY(esi);
	regs->eflags &= ~0x40DD5;
	regs->eflags |= context.eflags & 0x40DD5;
	regs->orig_eax = -1;		/* disable syscall checks */
	return context.eax;
badframe:
	do_exit(SIGSEGV);
}

