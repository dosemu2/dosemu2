/*
 *  linux/kernel/traps.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  changes for the dosemu team  by Lutz Molgedey <lutz@summa.physik.hu-berlin.de>
 *                                  Hans Lermen <lermen@elserv.ffm.fgan.de>
 */

/*
 * 'Traps.c' handles hardware traps and faults after we have saved some
 * state in 'asm.s'. Currently mostly a debugging-aid, will be extended
 * to mainly kill the offending process (probably by giving it a signal,
 * but possibly by killing it outright if necessary).
 */
#include "kversion.h"
#if KERNEL_VERSION < 1003096
  #error "sorry, kernels < 1.3.96 not supported"
#endif

#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/ptrace.h>

#include <linux/mm.h>

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#include "emumod.h"


#define DO_VM86_ERROR(trapnr, signr, str, name, tsk) \
asmlinkage void do_##name(struct pt_regs * regs, long error_code) \
{ \
	if (regs->eflags & VM_MASK) { \
		if (!handle_vm86_trap((struct vm86_regs *) regs, error_code, trapnr)) \
			return; \
		/* else fall through */ \
	} \
	tsk->tss.error_code = error_code; \
	tsk->tss.trap_no = trapnr; \
	force_sig(signr, tsk); \
	die_if_kernel(str,regs,error_code); \
}


extern void die_if_kernel(const char * str, struct pt_regs * regs, long err);


DO_VM86_ERROR( 0, SIGFPE,  "divide error", divide_error, current)
DO_VM86_ERROR( 3, SIGTRAP, "int3", int3, current)
DO_VM86_ERROR( 4, SIGSEGV, "overflow", overflow, current)
DO_VM86_ERROR( 5, SIGSEGV, "bounds", bounds, current)

DO_VM86_ERROR( 7, SIGSEGV, "device not available", device_not_available, current)


asmlinkage void do_debug(struct pt_regs * regs, long error_code)
{
	if (regs->eflags & VM_MASK) {
		handle_vm86_trap((struct vm86_regs *) regs, error_code,1);
		return;
	}
	force_sig(SIGTRAP, current);
	current->tss.trap_no = 1;
	current->tss.error_code = error_code;
	if ((regs->cs & 3) == 0) {
		/* If this is a kernel mode trap, then reset db7 and allow us to continue */
		__asm__("movl %0,%%db7"
			: /* no output */
			: "r" (0));
		return;
	}
	die_if_kernel("debug",regs,error_code);
}
