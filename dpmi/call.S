/*
 * Copyright  Robert J. Amstadt, 1993; Lutz Molgedey 1994
 */

#define UDATASEL 0x2b

	.data

/**********************************************************************
 *	Places to keep info about the current 32-bit (Emul;ator) stack frame.
 */
	.globl	_DPMI_SavedEMU_esp,_DPMI_SavedEMU_ebp,_DPMI_SavedEMU_ss
_DPMI_SavedEMU_esp:
	.long	0
_DPMI_SavedEMU_ebp:
	.long	0
_DPMI_SavedEMU_ss:
	.word	0

/**********************************************************************
 *	Places to keep info about the current 16/32-bit (DOS/DPMI) stack frame.
 */
	.globl	_DPMI_SavedDOS_esp,_DPMI_SavedDOS_ebp,_DPMI_SavedDOS_ss
_DPMI_SavedDOS_esp:
	.long	0
_DPMI_SavedDOS_ebp:
	.long	0
_DPMI_SavedDOS_ss:
	.word	0

selector:
	.long	0
offset:
	.long	0

	.text

/**********************************************************************
 *	void CallToInit(unsigned long eip, unsigned short cs, unsigned long esp,
 *	     	unsigned short ss, unsigned short ds, unsigned short es)
 *
 *	Stack:	 	0	ebp
 *		 	4	eip
 *		 	8	target eip
 *			12	target cs
 *			16	target esp
 *			20	target ss
 *			24	target ds
 *			28	target es
 */
	.align	4
	.globl _CallToInit
_CallToInit:
	pushl	%ebp
	movl	%esp,%ebp

	/*
 	 * Save our registers
	 */
	pushal

	/*
	 * Get target address.
	 */
	movl	8(%ebp),%eax
	movl	%eax,offset
	xorl	%eax,%eax
	movw	12(%ebp),%ax
	movl	%eax,selector

	/*
	 * Put stack registers where we can get them after stack switch.
	 */
	movw	%ss,_DPMI_SavedEMU_ss
	movl	%esp,_DPMI_SavedEMU_esp
	movl	%ebp,_DPMI_SavedEMU_ebp

	/*
	 * Load initial registers
	 */
	movl	$0,%esi
	xorl	%eax,%eax
	movw	28(%ebp),%ax
	movw	%ax,%es
	movw	24(%ebp),%ax
	movw	%ax,%ds
	movl	%eax,%edi
	xorl	%eax,%eax
	movw	16(%ebp),%ax
	movl	%eax,%esp
	movw	20(%ebp),%ax
	movw	%ax,%ss
	movl	%esp,%eax
	movl	%eax,%ebp
	/*
	 * Call entry point
	 */
	pushl	%fs:selector
	pushl	%fs:offset
	xorw	%ax,%ax
	movw	%ax,%fs
	movw	%ax,%gs

	.align	4,0x90
	.globl _ReturnFrom_dpmi_control
_ReturnFrom_dpmi_control:
	pushfl
	pushal
	pushw	%ds
	pushw	%es
	pushw	%fs
	pushw	%gs 
	movw	$UDATASEL,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%fs
	movw	%ax,%gs
	movw	%ss,_DPMI_SavedDOS_ss
	movl	%esp,_DPMI_SavedDOS_esp
	movl	%ebp,_DPMI_SavedDOS_ebp
	movw	_DPMI_SavedEMU_ss,%ss
	movl	_DPMI_SavedEMU_esp,%esp
	movl	_DPMI_SavedEMU_ebp,%ebp

	/*
 	 * Restore registers, but do not destroy return value.
	 */
	popal
	.align	2,0x90
	leave
	ret

/**********************************************************************
 *	void dpmi_control(void)
 *
 *	Stack:	 	0	ebp
 *		 	4	eip
 */
	.align	4
	.globl _dpmi_control
_dpmi_control:
	pushl	%ebp
	movl	%esp,%ebp

	/*
 	 * Save our registers
	 */
	pushal

	movw	%ss,_DPMI_SavedEMU_ss
	movl	%esp,_DPMI_SavedEMU_esp
	movl	%ebp,_DPMI_SavedEMU_ebp

	movw	_DPMI_SavedDOS_ss,%ss
	movl	_DPMI_SavedDOS_esp,%esp
	movl	_DPMI_SavedDOS_ebp,%ebp

	popw	%gs
	popw	%fs
	popw	%es
	popw	%ds

	popal
	popfl

	lret		/* Jump to dos in protected mode */
