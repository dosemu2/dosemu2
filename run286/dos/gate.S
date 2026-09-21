/*
 * The 16 -> 32 bit gate.
 *
 * The program is 16bit code in its own descriptors; we are a 32bit DPMI
 * client. Every PHAPI or DOSCALLS import is a short stub in a 16bit code
 * segment that does "mov ax,index; int 0x66", so the transition is made by
 * the DPMI host, which builds an iret frame in our bitness and enters
 * gate_entry below. Nothing here writes a descriptor table.
 *
 * Free software, GPL v2 or later.
 */
#include <libc/asmdefs.h>
#include "asm.h"

.text

/*
 * int ne_enter(cs, ip, ss, sp, ds, es)
 *
 * Does not return the usual way: the far jump below never comes back. The
 * program leaves through gate_entry, which unwinds to our caller.
 */
.global _ne_enter
_ne_enter:
    pushl %ebp
    movl %esp, %ebp
    pushl %ebx
    pushl %esi
    pushl %edi
    /* remember where to come back to */
    movl %esp, _gate_ret_esp
    xorl %eax, %eax
    movw %ss, %ax
    movl %eax, _gate_ret_ss

    /* dj64 enters us with a far call frame, so the args start at 0xc */
    movl 12(%ebp), %eax			/* cs */
    movw %ax, _gate_jmp + 2
    movl 16(%ebp), %eax			/* ip */
    movw %ax, _gate_jmp
    movl 32(%ebp), %eax			/* es */
    movl 20(%ebp), %edx			/* ss */
    movl 24(%ebp), %ebx			/* sp */
    movl 28(%ebp), %esi			/* ds */
    movl %eax, %es
    /* from here on our own DS is gone; everything is read through %cs: */
    movl %esi, %ds
    movl %edx, %ss
    movl %ebx, %esp
    ljmpw *%cs:_gate_jmp

/*
 * The int 0x66 handler. Entered from the program's 16bit stub with a 32bit
 * iret frame on the program's own 16bit stack.
 */
.global _gate_entry
_gate_entry:
    pushal
    pushl %ds
    pushl %es
    movl %cs:_gate_ds32, %edx
    movl %edx, %ds
    movl %edx, %es
    movzwl %ax, %eax
    movl %eax, _gate_index
    xorl %edx, %edx
    movw %ss, %dx
    movl %edx, _gate_cli_ss
    movl %esp, _gate_cli_esp
    /* onto a stack of our own, the program's may be tiny */
    lss _gate_stk, %esp
    pushl %edx				/* program SS */
    pushl _gate_cli_esp			/* program ESP */
    call _run286_import
    testl %eax, %eax
    jnz 1f
    popl %ecx				/* program ESP */
    popl %edx				/* program SS */
    movl %edx, %ss
    movl %ecx, %esp
    popl %es
    popl %ds
    popal
    iretl

    /* the program asked to terminate: unwind ne_enter() */
1:
    lss _gate_ret, %esp
    movl _gate_exit_code, %eax
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp
    lret

.data

/* read through %cs: from code that no longer has our DS, so it has to live
 * at an address both descriptors cover. In a flat client they do. */
.global _gate_ds32
_gate_ds32:	.long 0
_gate_jmp:	.word 0			/* offset */
		.word 0			/* selector */
_gate_ret:
_gate_ret_esp:	.long 0
_gate_ret_ss:	.long 0
_gate_stk:
.global _gate_stk_esp
_gate_stk_esp:	.long 0
.global _gate_stk_ss
_gate_stk_ss:	.long 0
.global _gate_index
_gate_index:	.long 0
.global _gate_cli_ss
_gate_cli_ss:	.long 0
.global _gate_cli_esp
_gate_cli_esp:	.long 0
.global _gate_exit_code
_gate_exit_code: .long 0

.global _gate_stack
_gate_stack:
.rept GATE_STACK_LEN
.byte 0
.endr
.global _gate_stack_end
_gate_stack_end:
