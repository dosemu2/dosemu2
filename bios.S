/*********************************************************/
/**                                                     **/
/** This is the bios inline code - ask for details :-)  **/
/** Remember that we use a 32 bit assembler for real    **/
/** mode code: Hence use 32 bit registers for 16 bit    **/
/** one and vice versa.                                 **/
/**                                                     **/
/*********************************************************/
	.text
	.globl	_INT16_dummy_start
_INT16_dummy_start:
	pushl   %ds
	pushl   %ebx
	.byte   0xbb, 0x40, 0x00		/* movw   $0x40,%bx */
	.byte   0x8e, 0xdb			/* movw   %bx,%ds */
	orb     %ah,%ah
 	cmpb    $0x0,%ah
	je      L2
	cmpb    $0x10,%ah
	je      L2
	cmpb    $0x1,%ah
	je      L4
	cmpb    $0x11,%ah
	je      L4
	cmpb    $0x2,%ah
	je      L5
	cmpb    $0x3,%ah
	je      L1
	cmpb    $0x4,%ah
	je      L1
	cmpb    $0x5,%ah
	je      L6
	cmpb    $0x12,%ah
	je      L5
L1:	popl    %ebx
	popl    %ds
	iret   
L2:	cli
	.byte   0x8b, 0x1e, 0x1a, 0x00		/* movw   0x1a,%bx */
	.byte   0x3b, 0x1e, 0x1c, 0x00		/* cmpw   0x1c,%bx */
	jne     L3

	pushl	%eax
	movb    $0x16,%al
	int     $0xe6				/* sti with return to dosemu code */
	popl	%eax

	jmp     L2
L3:	.byte   0x8b, 0x07			/* movw   [%bx],%ax */
	incl    %ebx
	incl    %ebx
	.byte   0x89, 0x1e, 0x1a, 0x00		/* movw   %bx,0x1a */
	.byte   0x3b, 0x1e, 0x82, 0x00  	/* cmpw   0x82,%bx */
	jne     L1
	.byte   0x8b, 0x1e, 0x80, 0x00  	/* movw   0x80,%bx */
	.byte   0x89, 0x1e, 0x1a, 0x00  	/* movw   %bx,0x1a */
	jmp     L1

L4:	cli
	.byte   0x8b, 0x1e, 0x1a, 0x00  	/* movw   0x1a,%bx */
	.byte   0x3b, 0x1e, 0x1c, 0x00		/* cmpw   0x1c,%bx */
	.byte   0x8b, 0x07			/* movw   [%bx],%ax */
	sti
	popl    %ebx
	popl    %ds
	lret    $0x2
L5:	.byte   0xa0, 0x17, 0x00    		/* movb   0x17,%al */
	.byte   0x8a, 0x26, 0x18, 0x00  	/* movb   0x18,%ah */
	andb    $0xf3,%ah
	xorl    %ebx,%ebx
	.byte   0x8a, 0x3e, 0x96, 0x00  	/* movb   0x96,%bh */
	andb    $0xc,%bh
	orl     %ebx,%eax
	jmp     L1
L6:	cli
	.byte   0x8b, 0x1e, 0x1c, 0x00  	/* movw   0x1c,%bx */
	incl    %ebx
	incl    %ebx
	.byte   0x3b, 0x1e, 0x82, 0x00  	/* cmpw   0x82,%bx */
	jne     L7
	.byte   0x8b, 0x1e, 0x80, 0x00  	/* movw   0x80,%bx */
L7:	.byte   0x3b, 0x1e, 0x1a, 0x00  	/* cmpw   0x1a,%bx */
	jne     L8
	movb    $0x1,%al
	sti
	jmp     L1
L8:	pushl   %ebx
	.byte   0x8b, 0x1e, 0x1c, 0x00  	/* movw   0x1c,%bx */
	.byte   0x89, 0x0f			/* movw   %cx,[%bx] */
	movb    $0x0,%al
	popl    %ebx
	.byte   0x89, 0x1e, 0x1c, 0x00  	/* movw   %bx,0x1c */
	sti
	jmp     L1
	.globl  _INT16_dummy_end
_INT16_dummy_end:

	.globl	_INT09_dummy_start
_INT09_dummy_start:
	pushl	%ds
	pushl	%ebx
	pushl	%eax
	pushl	%ecx

	.byte   0xbb, 0x00, 0xf1	/* movw   $0xf100,%bx */
	pushl	%ebx
	popl	%ds
	xorl	%ecx,%ecx
	incl	%ecx
	.byte   0xbb, 0xf2, 0x20	/* movw   $0x20f2,%bx */
   	.byte   0x8b, 0x07		/* movw   [%bx],%ax */

	movb	$0x4f,%ah
	clc 
	cmc
	int	$0x15

	movb	%al,%ah
	movb	$0x6,%al

	int	$0xe6
	.byte   0xbb, 0x00, 0xf1	/* movw   $0xf100,%bx */
	pushl	%ebx
	popl	%ds
	xorl	%ecx,%ecx
	incl	%ecx
	.byte   0xbb, 0xf0, 0x20	/* movw   $0x20f0,%bx */
	.byte   0x89, 0x0f		/* movw   %cx,[%bx] */
	popl	%ecx
	popl	%eax
	popl	%ebx
	popl	%ds
	iret
	.globl  _INT09_dummy_end
_INT09_dummy_end:

	.globl	_INT08_dummy_start
_INT08_dummy_start:
	int	$0x1c
	pushl	%ecx
	pushl	%ds
	pushl	%ebx
	xorl	%ebx, %ebx
	pushl	%ebx
	popl	%ds
	.byte	0xff,0x06,0x6c,0x04	/* incw   [$0x046c] */
	.byte	0x01,0x1e,0x6e,0x04	/* addw   %bx,[$0x046e] */
	.byte	0x00,0x1e,0x70,0x04	/* addb   %bl,[$0x0470] */
	.byte   0xbb, 0x00, 0xf1	/* movw   $0xf100,%bx */
	pushl	%ebx
	popl	%ds
	xorl	%ecx,%ecx
	incl	%ecx
	.byte   0xbb, 0xf0, 0x20	/* movw   $0x20f0,%bx */
	.byte   0x89, 0x0f		/* movw   %cx,[%bx] */
	popl	%ebx
	popl	%ds
	popl	%ecx
	iret
	.globl  _INT08_dummy_end
_INT08_dummy_end:

	.globl	_DPMI_dummy_start
_DPMI_dummy_start:
	pushl	%eax
	movb	$0x51,%ah
	int	$0x21			/* Get PSP */
	popl	%eax
	.globl	_DPMI_dpmi_init
_DPMI_dpmi_init:
	hlt
	lret
	.globl	_DPMI_return_from_dosint
_DPMI_return_from_dosint:
	hlt
	.globl	_DPMI_return_from_realmode
_DPMI_return_from_realmode:
	hlt
	.globl	_DPMI_raw_mode_switch
_DPMI_raw_mode_switch:
	hlt
	.globl	_DPMI_save_restore
_DPMI_save_restore:
	hlt
	lret
	.globl	_DPMI_API_extension
_DPMI_API_extension:
	hlt
	lret
	.globl	_DPMI_exception
_DPMI_exception:
	.fill	32, 1, 0xf4
	.globl	_DPMI_interrupt
_DPMI_interrupt:
	.fill	256, 1, 0xf4
	.globl	_DPMI_dummy_end
_DPMI_dummy_end:
