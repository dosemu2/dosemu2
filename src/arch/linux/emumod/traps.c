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
#include <linux/head.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>
#if KERNEL_VERSION < 1001067
#include <linux/segment.h>
#endif
#include <linux/ptrace.h>

#if KERNEL_VERSION >= 1001085
#include <linux/mm.h>
#endif

#include <asm/system.h>
#include <asm/segment.h>
#include <asm/io.h>

#include "emumod.h"


#if KERNEL_VERSION < 1003096
#define DO_VM86_ERROR(trapnr, signr, str, name, tsk) \
asmlinkage void do_##name(struct pt_regs * regs, long error_code) \
{ \
	if (regs->eflags & VM_MASK) { \
		handle_vm86_trap((struct vm86_regs *) regs, error_code, trapnr); \
		return; \
	} \
	tsk->tss.error_code = error_code; \
	tsk->tss.trap_no = trapnr; \
	if (signr == SIGTRAP && current->flags & PF_PTRACED) \
		current->blocked &= ~(1 << (SIGTRAP-1)); \
	send_sig(signr, tsk, 1); \
	die_if_kernel(str,regs,error_code); \
}
#else
#define DO_VM86_ERROR(trapnr, signr, str, name, tsk) \
asmlinkage void do_##name(struct pt_regs * regs, long error_code) \
{ \
	if (regs->eflags & VM_MASK) { \
		handle_vm86_trap((struct vm86_regs *) regs, error_code, trapnr); \
		return; \
	} \
	tsk->tss.error_code = error_code; \
	tsk->tss.trap_no = trapnr; \
	force_sig(signr, tsk); \
	die_if_kernel(str,regs,error_code); \
}
#endif


#if KERNEL_VERSION < 1003015
extern void die_if_kernel(char * str, struct pt_regs * regs, long err);
#else
extern void die_if_kernel(const char * str, struct pt_regs * regs, long err);
#endif


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
#if KERNEL_VERSION < 1003096
	if (current->flags & PF_PTRACED)
		current->blocked &= ~(1 << (SIGTRAP-1));
	send_sig(SIGTRAP, current, 1);
#else
	force_sig(SIGTRAP, current);
#endif
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
