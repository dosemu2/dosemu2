/*
 * Copyright Lutz Molgedey 1994
 */

#define UDATASEL 0x2b

GS	= 0x00
FS	= 0x04
ES	= 0x08
DS	= 0x0c
EDI	= 0x10
ESI	= 0x14
EBP	= 0x18
ESP	= 0x1c
EBX	= 0x20
EDX	= 0x24
ECX	= 0x28
EAX	= 0x2c
TRAPNO	= 0x30
ERR	= 0x34
EIP	= 0x38
CS	= 0x3c
EFLAGS	= 0x40
__ESP	= 0x44
SS	= 0x48

	.data

/**********************************************************************
 *	Places to keep info about the current 32-bit (Emulator) stack frame.
 */
	.globl	_DPMI_Saved_esp,_DPMI_Saved_ss
_DPMI_Saved_esp:
	.long	0
_DPMI_Saved_ss:
	.word	0

	.text

/**********************************************************************
 *	void dpmi_control(struct sigcontext_struct *)
 *
 *	Stack:	 	0	eax
 *			4	eip
 *			8	&sigcontext_struct
 */
	.align	4
	.globl _dpmi_control
_dpmi_control:
	pushl	%eax
	movl	8(%esp),%eax

	/*
 	 * Save our registers
	 */
	pushfl
	pushal

	movw	%ss,_DPMI_Saved_ss
	movl	%esp,_DPMI_Saved_esp

	/* DANG_FIXTHIS block signals */

	movl	ESP(%eax),%esp
	movw	SS(%eax),%ss

	/* DANG_FIXTHIS unblock signals */

	movw	GS(%eax),%gs
	movw	FS(%eax),%fs
	movw	ES(%eax),%es
	movl	EDI(%eax),%edi
	movl	ESI(%eax),%esi
	movl	EBP(%eax),%ebp
	movl	EBX(%eax),%ebx
	movl	EDX(%eax),%edx
	movl	ECX(%eax),%ecx

	pushl	CS(%eax)
	pushl	EIP(%eax)
	pushl	EFLAGS(%eax)

	pushw	DS(%eax)
	movl	EAX(%eax),%eax
	popw	%ds

	popfl

	lret		/* Jump to DPMI client */

        .align  4,0x90
        .globl _ReturnFrom_dpmi_control
_ReturnFrom_dpmi_control:
        movw    $UDATASEL,%ax
        movw    %ax,%ds
        movw    %ax,%es
        movw    %ax,%fs
        movw    %ax,%gs

	/* DANG_FIXTHIS block signals */

        movw    _DPMI_Saved_ss,%ss
        movl    _DPMI_Saved_esp,%esp

	/* DANG_FIXTHIS unblock signals */

        popal
	popfl
	popl	%eax
        ret
