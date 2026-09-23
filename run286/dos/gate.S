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
 * One stub per hooked interrupt. The host enters these in our bitness,
 * on whatever stack was current, which is not what a 16bit handler was
 * written for: when the interrupt lands while we are in the gate, that
 * stack is our own 32bit one, and the handler then pushes at ESP far
 * above any 16bit segment. A 286 extender hands its client a 16bit
 * stack, so do the same.
 */
.global _int_stubs
_int_stubs:
.set int_n, 0
.rept INT_SLOTS
    pushl $int_n
    jmp _int_common
    .balign INT_SLOT_SIZE
.set int_n, int_n + 1
.endr

_int_common:
    /*
     * Before anything that touches the stack. The per-vector counters
     * below say which handler ran; this one says only that the host came
     * here, so that "the host says it called us and our counter is zero"
     * can be told apart from "the host never came". Read through %cs
     * because our own DS is not loaded yet, and the stack is whatever the
     * host left us, which is the thing in question.
     */
    incl %cs:_int_entered
    pushal
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    movl %cs:_gate_ds32, %eax
    movl %eax, %ds
    incl _int_taken
    /* the flags the host pushed, which the handler's iret will want */
    movl 60(%esp), %ebx
    /* which vector: the stub said so, just under the host's frame */
    movl 48(%esp), %ecx
    /* where we are now, to come back to */
    movl %esp, %edx
    xorl %eax, %eax
    movw %ss, %ax
    /* where the interrupt landed, for the exception report */
    movl %ecx, _int_last
    /* and one counter per vector, so a handler that never runs shows up */
    incl _int_count(,%ecx,4)
    movl %eax, _int_last_ss
    movl %edx, _int_last_esp
    /*
     * A 16bit stack of our own, as the extender would have given it.
     * Each vector gets its own piece of it: one interrupt arrives while
     * another's handler runs, and that handler has by then moved to a
     * stack of its own, so there is no way to tell from SS that ours is
     * already in use.
     */
    movl %ecx, %ebp
    shll $INT_STACK_SHIFT, %ebp
    movl _int_stk_esp, %esi
    subl %ebp, %esi
    movl _int_stk_ss, %ebp
    movw %bp, %ss
    movl %esi, %esp
    pushl %edx				/* the stack we came from */
    pushl %eax
    /* the frame the program's handler will iret from */
    pushw %bx
    pushw _int_ret_sel
    pushw $0
    /* and the segments it would have seen had it interrupted the
     * program rather than us */
    movl _int_ds, %eax
    movl %eax, %ds
    movl %eax, %es
    ljmpw *%cs:_int_target(, %ecx, 4)

/*
 * int 21h, on the way to whoever had the vector.
 *
 * We are a 32bit DPMI client because dj64 makes us one, but the program
 * is 16bit and writes only DX where DOS is handed a DS:DX. The host, told
 * once that the client is 32bit, reads the whole of EDX and follows a
 * pointer built half from the program and half from whatever was in the
 * high word. A 286 extender owns int 21h and passes on 16bit registers,
 * so do that here: narrow them when the interrupted code is 16bit, and
 * leave our own calls alone.
 */
.global _int21_stub
_int21_stub:
    pushl %eax
    pushfl
    lar 12(%esp), %eax			/* the interrupted CS */
    jnz 1f				/* unreadable: not ours to judge */
    testl $0x400000, %eax		/* the D bit, so 32bit code */
    jnz 1f
    popfl
    popl %eax
    movzwl %ax, %eax
    movzwl %bx, %ebx
    movzwl %cx, %ecx
    movzwl %dx, %edx
    movzwl %si, %esi
    movzwl %di, %edi
    movzwl %bp, %ebp
    ljmpl *%cs:_int21_prev
1:
    popfl
    popl %eax
    ljmpl *%cs:_int21_prev

/*
 * Where the program's handler irets to, in a 16bit segment of its own so
 * that a 16bit iret lands on it. Puts back the stack we came from and
 * returns to the host.
 */
.code16
.global _int_ret16
_int_ret16:
    popl %eax				/* the stack we came from */
    popl %edx
    movw %ax, %ss
    movl %edx, %esp
    popl %gs
    popl %fs
    popl %es
    popl %ds
    popal
    addl $4, %esp			/* the number the stub pushed */
    iretl
.code32

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
_int_stk:
.global _int_stk_esp
_int_stk_esp:	.long 0
.global _int_stk_ss
_int_stk_ss:	.long 0
.global _int_ret_sel
_int_ret_sel:	.long 0			/* 16bit cs holding _int_ret16 */
.global _int_ds
_int_ds:	.long 0			/* the program's own DGROUP */
.global _int_target
_int_target:				/* offset, selector, per vector */
.rept INT_SLOTS
.word 0
.word 0
.endr
.global _int_taken
_int_taken:	.long 0
.global _int_entered
_int_entered:	.long 0
.global _int_count
_int_count:
.rept INT_SLOTS
	.long 0
.endr
.global _int_last
_int_last:	.long 0
.global _int_last_ss
_int_last_ss:	.long 0
.global _int_last_esp
_int_last_esp:	.long 0
.global _int21_prev
_int21_prev: .word 0, 0, 0

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
.global _int_stack
_int_stack:
.rept INT_SLOTS * INT_STACK_LEN
.byte 0
.endr
.balign 16
.global _int_stack_end
_int_stack_end:

.balign 16
.global _exc_stack
_exc_stack:
.rept EXC_STACK_LEN
.byte 0
.endr
.balign 16
.global _exc_stack_end
_exc_stack_end:
