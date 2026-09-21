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
    movl 36(%ebp), %eax			/* ax: the environment selector */
    movl 40(%ebp), %ecx			/* cx: size of the data segment */
    /* from here on our own DS is gone; everything is read through %cs: */
    movl %esi, %ds
    movl %edx, %ss
    movl %ebx, %esp
    ljmpw *%cs:_gate_jmp

/* int desc_probe(void) - what the machine says its tables are, and the
 * selector dosemu2 hands out for the LDT alias (int 2Fh AX=1688h, the
 * call Windows uses for the same thing).
 *
 * Also asks dosemu2 for THUNK_16_32x. We are a 32bit DPMI client, but
 * the program we run is 16bit and hands DOS a DS:DX with nothing in the
 * high half of EDX; the extension makes the host take the bitness from
 * the code that made the call instead of from us. */
.global _desc_probe
_desc_probe:
    pushl %ebx
    pushl %esi
    pushl %edi
    pushl %es
    sgdt _gate_gdt
    sidt _gate_idt
    sldt _gate_ldt_sel
    xorl %eax, %eax
    movw %cs, %ax
    movl %eax, _gate_cs32
    movl $0x1688, %eax
    xorl %ebx, %ebx
    int $0x2f
    movzwl %bx, %eax
    movl %eax, _gate_ldt_alias
    movl $0x168a, %eax
    leal _gate_thunk_name, %esi
    movl $1, %ebx
    int $0x2f
    movzbl %al, %eax		/* 0 if the host took it */
    movl %eax, _gate_thunk_err
    popl %es
    popl %edi
    popl %esi
    popl %ebx
    lret

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
    /*
     * Origin's wrapper frees a descriptor while it is still in ES and
     * carries on with it: on a 286 the entry it freed was its own to
     * recycle and stayed loadable, here DPMI really takes it away. Put a
     * null selector in its place rather than faulting on the way out,
     * which is what dosemu2 does to a segment register holding one.
     */
    popl %eax
    lar %eax, %ecx
    jz 2f
    xorl %eax, %eax
2:
    movl %eax, %es
    popl %eax
    lar %eax, %ecx
    jz 3f
    xorl %eax, %eax
3:
    movl %eax, %ds
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

/*
 * One stub per exception vector, so the handler knows which one it was.
 * The host enters these as a 32bit DPMI exception handler, with the
 * return address, the error code and the faulting frame on the stack it
 * chose; we do not return, we report and unwind ne_enter().
 */
.global _exc_stubs
_exc_stubs:
.set exc_n, 0
.rept EXC_SLOTS
    pushl $exc_n
    jmp _exc_common
    .balign EXC_SLOT_SIZE
.set exc_n, exc_n + 1
.endr

_exc_common:
    pushal
    pushl %gs
    pushl %fs
    pushl %es
    pushl %ds
    movl %cs:_gate_ds32, %edx
    movl %edx, %ds
    movl %edx, %es
    movl %esp, _gate_exc_esp
    xorl %edx, %edx
    movw %ss, %dx
    movl %edx, _gate_exc_ss
    /* a stack of our own: the one that faulted is not to be trusted */
    lss _gate_exc_stk, %esp
    call _run286_exception
    /* there is no going back to a program that faulted */
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
.global _gate_gdt
_gate_gdt:	.word 0
		.long 0
		.word 0
.global _gate_idt
_gate_idt:	.word 0
		.long 0
		.word 0
.global _gate_ldt_sel
_gate_ldt_sel:	.long 0
.global _gate_ldt_alias
_gate_ldt_alias: .long 0
.global _gate_cs32
_gate_cs32:	.long 0
.global _gate_exc_ss
_gate_exc_ss:	.long 0
.global _gate_exc_esp
_gate_exc_esp:	.long 0
_gate_exc_stk:
.global _gate_exc_stk_esp
_gate_exc_stk_esp: .long 0
.global _gate_exc_stk_ss
_gate_exc_stk_ss: .long 0
.global _gate_thunk_err
_gate_thunk_err: .long 0
_gate_thunk_name: .asciz "THUNK_16_32x"

/* what the host pushes on this stack is dwords, and the program's own
 * handlers run on it too, so it has to start aligned */
.balign 16
.global _gate_stack
_gate_stack:
.rept GATE_STACK_LEN
.byte 0
.endr
.balign 16
.global _gate_stack_end
_gate_stack_end:

.balign 16
.global _exc_stack
_exc_stack:
.rept EXC_STACK_LEN
.byte 0
.endr
.balign 16
.global _exc_stack_end
_exc_stack_end:
