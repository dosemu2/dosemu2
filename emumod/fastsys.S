#include "kversion.h"
#if 0
#define KERNEL_VERSION 1003028 /* last verified kernel version */
#endif
#if KERNEL_VERSION >= 01001067
#include <asm/segment.h>
#else
#include <linux/segment.h>
#endif

#include <linux/sys.h>


/*
 * these are offsets into the task-struct. (taken from linux/kernel/entry.S)
 *
 * NOTE:
 *  This is subject to change in future Linux releases, as there is much
 *  discussion on optimizing the scheduler.
 *  So, at this this moment we don't use it !
 *
 *  But, ... if there is need to run dosemu under DEBUGGER (GDB)
 *  you *must* set #define HANDLE_DEBUG_REGISTERS 1
 *  and check, if the task struct fits the definition below
 */
#define HANDLE_DEBUG_REGISTERS 0
#if HANDLE_DEBUG_REGISTERS
state           =  0
counter         =  4
priority        =  8
signal          = 12
blocked         = 16
flags           = 20
errno           = 24
dbgreg6         = 52
dbgreg7         = 56
exec_domain     = 60
#endif

callers_CS = 52          

#ifdef __ELF__
  #define CISH(x) x
#else
  #define CISH(x) _##x
#endif

.globl	CISH(fast_syscall)
.align 4
CISH(fast_syscall):
        cld
        push %gs
        push %fs
        push %es
        push %ds
	pusha
#if HANDLE_DEBUG_REGISTERS
        movl $0,%eax
        movl %eax,%db7                  # disable hardware debugging...
#endif
	movl $(KERNEL_DS),%eax
	mov %ax,%ds
	mov %ax,%es
	movl $(USER_DS),%eax
	mov %ax,%fs
        cmpw $(KERNEL_CS),callers_CS(%esp)
        je 1f             # oops, this should not happen 
	pushl	%esp
        call	CISH(do_fast_syscall)
	leal	4(%esp),%esp
#if HANDLE_DEBUG_REGISTERS
        movl _current,%eax
        movl dbgreg7(%eax),%ebx
        movl %ebx,%db7
#endif
1:
	popa
	pop %ds
	pop %es
	pop %fs
	pop %gs
	iret

