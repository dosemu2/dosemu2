/* This code is taken from libtask library.
 * Rip-off done by stsp for dosemu2 project.
 * Original copyrights below. */

/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */

#if defined(__i386__)
.globl _setmcontext
_setmcontext:
	movl	4(%esp), %eax

	mov	8(%eax), %fs
	mov	12(%eax), %es
	mov	16(%eax), %ds
	mov	76(%eax), %ss
	movl	20(%eax), %edi
	movl	24(%eax), %esi
	movl	28(%eax), %ebp
	movl	36(%eax), %ebx
	movl	40(%eax), %edx
	movl	44(%eax), %ecx

	movl	72(%eax), %esp
	pushl	60(%eax)	/* new %eip */
	movl	48(%eax), %eax
	ret

.globl _getmcontext
_getmcontext:
	movl	4(%esp), %eax

	mov	%fs, 8(%eax)
	mov	%es, 12(%eax)
	mov	%ds, 16(%eax)
	mov	%ss, 76(%eax)
	movl	%edi, 20(%eax)
	movl	%esi, 24(%eax)
	movl	%ebp, 28(%eax)
	movl	%ebx, 36(%eax)
	movl	%edx, 40(%eax)
	movl	%ecx, 44(%eax)

	movl	$1, 48(%eax)	/* %eax */
	movl	(%esp), %ecx	/* %eip */
	movl	%ecx, 60(%eax)
	leal	4(%esp), %ecx	/* %esp */
	movl	%ecx, 72(%eax)

	movl	44(%eax), %ecx	/* restore %ecx */
	movl	$0, %eax
	ret
#endif

#if defined(__x86_64__)
.globl _setmcontext
_setmcontext:
	movq	16(%rdi), %rsi
	movq	24(%rdi), %rdx
	movq	32(%rdi), %rcx
	movq	40(%rdi), %r8
	movq	48(%rdi), %r9
	movq	56(%rdi), %rax
	movq	64(%rdi), %rbx
	movq	72(%rdi), %rbp
	movq	80(%rdi), %r10
	movq	88(%rdi), %r11
	movq	96(%rdi), %r12
	movq	104(%rdi), %r13
	movq	112(%rdi), %r14
	movq	120(%rdi), %r15
	movq	184(%rdi), %rsp
	pushq	160(%rdi)	/* new %eip */
	movq	8(%rdi), %rdi
	ret

.globl _getmcontext
_getmcontext:
	movq	%rdi, 8(%rdi)
	movq	%rsi, 16(%rdi)
	movq	%rdx, 24(%rdi)
	movq	%rcx, 32(%rdi)
	movq	%r8, 40(%rdi)
	movq	%r9, 48(%rdi)
	movq	$1, 56(%rdi)	/* %rax */
	movq	%rbx, 64(%rdi)
	movq	%rbp, 72(%rdi)
	movq	%r10, 80(%rdi)
	movq	%r11, 88(%rdi)
	movq	%r12, 96(%rdi)
	movq	%r13, 104(%rdi)
	movq	%r14, 112(%rdi)
	movq	%r15, 120(%rdi)

	movq	(%rsp), %rcx	/* %rip */
	movq	%rcx, 160(%rdi)
	leaq	8(%rsp), %rcx	/* %rsp */
	movq	%rcx, 184(%rdi)

	movq	32(%rdi), %rcx	/* restore %rcx */
	movq	$0, %rax
	ret
#endif
