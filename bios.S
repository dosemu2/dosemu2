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
_INT16_dummy_start:			/* KEYBOARD BIOS ROUTINE        */
	pushl   %ds			/* save ds and bx               */
	pushl   %ebx		/* next 2 lines set ds to bios data seg	*/
	.byte   0xbb, 0x40, 0x00	/* movw   $0x40,%bx */
	.byte   0x8e, 0xdb		/* movw   %bx,%ds */
/*	orb     %ah,%ah			/* is this needed?		*/
 	cmpb    $0x0,%ah		/*   0 => read keyboard		*/
	je      L2
	cmpb    $0x10,%ah		/* 10 => read extended keyboard	*/
	je      L2
	cmpb    $0x1,%ah		/*  1 => return keyboard status	*/
	je      L4
	cmpb    $0x11,%ah		/* 11 => retn. ext. keybd. stat.*/
	je      L4
	cmpb    $0x2,%ah		/*  2 => retn. shift flag stat.	*/
	je      L5
	cmpb    $0x3,%ah		/*  3 => set rpt. speed & delay	*/
	je      L1
	cmpb    $0x4,%ah		/*  4 => reserved (do nothing)	*/
	je      L1
	cmpb    $0x5,%ah		/*  5 => store key in buffer	*/
	je      L6
	cmpb    $0x12,%ah		/* 12 => retn ext shift flg stat*/
	je      L5
L1:	popl    %ebx			/* do nothing and return	*/
	popl    %ds
	iret   
L2:	cli				/* READ KEYBOARD		*/
					/* get address of next char	*/
	.byte   0x8b, 0x1e, 0x1a, 0x00		/* movw   0x1a,%bx */
					/* is buffer empty?		*/
	.byte   0x3b, 0x1e, 0x1c, 0x00		/* cmpw   0x1c,%bx */
	jne     L3			/* jump if not empty		*/

	pushl	%eax			/* buffer empty - 		*/
	movb    $0x16,%al		/* ask dosemu for another key	*/
	sti
	int     $0xe6			/* sti with return to dosemu code */
	popl	%eax
	jmp     L2			/* got another key, go try again*/
L3:	.byte   0x8b, 0x07	/* movw   [%bx],%ax  get last key	*/
	incl    %ebx		/* advance buffer pointer 2 bytes	*/
	incl    %ebx
					/* save new pointer		*/	
	.byte   0x89, 0x1e, 0x1a, 0x00		/* movw   %bx,0x1a 	*/
					/* check for wrap around	*/
	.byte   0x3b, 0x1e, 0x82, 0x00  	/* cmpw   0x82,%bx 	*/
	jne     L1			/* no wrap - go return to caller*/
					/* wrap - get buffer start	*/
	.byte   0x8b, 0x1e, 0x80, 0x00  	/* movw   0x80,%bx 	*/
					/* save it as new pointer	*/
	.byte   0x89, 0x1e, 0x1a, 0x00  	/* movw   %bx,0x1a 	*/
	jmp     L1			/* return to caller		*/

L4:	pushl	%eax			/* RETURN KEYBOARD STATUS	*/
	movb    $0x16,%al			/* anti hogging code	*/
	int     $0xe6				/* call delay routine 	*/
	popl	%eax

	cli
					/* get address of next char	*/
	.byte   0x8b, 0x1e, 0x1a, 0x00  	/* movw   0x1a,%bx 	*/
					/* set flag if buffer empty	*/
	.byte   0x3b, 0x1e, 0x1c, 0x00		/* cmpw   0x1c,%bx 	*/
					/* put char into ax		*/
	.byte   0x8b, 0x07			/* movw   [%bx],%ax 	*/
	sti
	popl    %ebx
	popl    %ds
	lret    $0x2		/* can't do iret, it would change flags	*/
					/* RETURN SHIFT FLAG STATUS	*/
					/* get shift flags		*/
L5:	.byte   0xa0, 0x17, 0x00    		/* movb   0x17,%al 	*/
					/* get extended shift flags	*/
	.byte   0x8a, 0x26, 0x18, 0x00  	/* movb   0x18,%ah 	*/
	andb    $0xf3,%ah		/* disallow bits 2 & 3		*/
	xorl    %ebx,%ebx
					/* bits 2 & 3 come from here	*/
	.byte   0x8a, 0x3e, 0x96, 0x00  	/* movb   0x96,%bh 	*/
	andb    $0xc,%bh		/* isolate those bits		*/
	orl     %ebx,%eax		/* merge them with flags in ax	*/
	jmp     L1			/* return to caller		*/
L6:	cli				/* STORE KEY INTO BUFFER	*/
					/* get buffer "in" pointer	*/
	.byte   0x8b, 0x1e, 0x1c, 0x00  	/* movw   0x1c,%bx 	*/
	incl    %ebx			/* increment "in" pointer	*/
	incl    %ebx
					/* wrap if needed		*/
	.byte   0x3b, 0x1e, 0x82, 0x00  	/* cmpw   0x82,%bx 	*/
	jne     L7
	.byte   0x8b, 0x1e, 0x80, 0x00  	/* movw   0x80,%bx 	*/
					/* is buffer full?		*/
L7:	.byte   0x3b, 0x1e, 0x1a, 0x00  	/* cmpw   0x1a,%bx */
	jne     L8
	movb    $0x1,%al		/* buffer is full, return error	*/
	sti
	jmp     L1
L8:	pushl   %ebx			/* buffer is not full - save bx	*/
					/* get "in" pointer again	*/
	.byte   0x8b, 0x1e, 0x1c, 0x00  	/* movw   0x1c,%bx 	*/
					/* put cx into buffer		*/
	.byte   0x89, 0x0f			/* movw   %cx,[%bx] 	*/
	movb    $0x0,%al		/* clear error flag		*/
	popl    %ebx			/* restore incremented pointer	*/
					/* save it in bios data area	*/
	.byte   0x89, 0x1e, 0x1c, 0x00  	/* movw   %bx,0x1c 	*/
	sti
	jmp     L1			/* return to caller		*/
	.globl  _INT16_dummy_end
_INT16_dummy_end:

	.globl	_INT09_dummy_start
_INT09_dummy_start:			/* KEYBOARD INTERRUPT ROUTINE	*/
	pushl	%ds			/* save registers		*/
	pushl	%ebx
	pushl	%eax
	pushl	%ecx
					/* get segment of LASTSCAN_ADDR	*/
	.byte   0xbb, 0x00, 0xf1	/* movw   $0xf100,%bx */
	pushl	%ebx
	popl	%ds
#ifndef NEW_PIC				/* next 2 lines not needed?	*/
	xorl	%ecx,%ecx
	incl	%ecx
#endif
					/* 0x20f2 = pseudo 8042 output	*/
	.byte   0xbb, 0xf2, 0x20	/* movw   $0x20f2,%bx */
					/* "read" keyboard controller	*/
   	.byte   0x8b, 0x07		/* movw   [%bx],%ax */

	movb	$0x4f,%ah		/* prepare for int 15 func 4f	*/
	clc 
	cmc
	int	$0x15			/* int 15 func 4f per bios spec	*/

	movb	%al,%ah			/* move char to ah		*/
	movb	$0x6,%al		/* dosemu helper func 6		*/

	int	$0xe6			/* call dos helper		*/
#ifndef NEW_PIC
					/* get segment of OUTB_ADDR	*/
	.byte   0xbb, 0x00, 0xf1	/* movw   $0xf100,%bx */
	pushl	%ebx
	popl	%ds			/* put it into ds		*/
	xorl	%ecx,%ecx
	incl	%ecx			/* put a 1 into cx		*/
					/* set bx to OUTB_ADDR		*/
	.byte   0xbb, 0xf0, 0x20	/* movw   $0x20f0,%bx */
					/* flag outb as complete	*/
	.byte   0x89, 0x0f		/* movw   %cx,[%bx] */
#else
	movb    $0x20,%al
	outb    $0x20			/* tell pic we're done 		*/
#endif
	popl	%ecx			/* restore registers		*/
	popl	%eax
	popl	%ebx
	popl	%ds
	iret
	.globl  _INT09_dummy_end
_INT09_dummy_end:

	.globl	_INT08_dummy_start
_INT08_dummy_start:			/* TIMER INTERRUPT ROUTINE	*/

/* some defines for instructions that gas will mess up	*/
#define ADD_bx_addr_0x46c .byte	0x01,0x1e,0x6c,0x04	/* addw bx,[0x46c] */
#define ADC_addr_0x46e_cx .byte	0x13,0x0e,0x6e,0x04	/* adcw [0x46e],cx */
#define CMP_cx_24	  .byte	0x83,0xf9,0x18		/* cmpw cx,24      */
#define MOV_bl_addr_0x470 .byte	0x88,0x1e,0x70,0x04	/* movw bl,[0x470] */
#define MOV_cx_addr_0x46e .byte	0x89,0x0e,0x6e,0x04	/* movw cx,[0x46e] */
#define MOV_0xf100_bx     .byte	0xbb,0x00,0xf1		/* movw 0xf100,bx  */
#define MOV_0x20f0_bx     .byte	0xbb,0xf0,0x20		/* movw 0x20f0,bx  */
#define	MOV_cx_addr_bx    .byte	0x89,0x0f		/* movw cx,[bx]    */

/* OK, now we can start the code	*/
	int	$0x1c			/* call int 0x1c, per bios spec	*/
	pushl	%ecx
	pushl	%ds
	pushl	%ebx
	xorl	%ebx, %ebx		/* set bx to segment 0		*/
	pushl	%ebx
	popl	%ds			/* set ds to segment 0		*/
#if 0
	/* old code that never increments hour	*/
	.byte	0xff,0x06,0x6c,0x04	/* incw   [$0x046c] */
	.byte	0x01,0x1e,0x6e,0x04	/* addw   %bx,[$0x046e] */
	.byte	0x00,0x1e,0x70,0x04	/* addb   %bl,[$0x0470] */
#else
	incl	%ebx			/* we need a 1			*/
	xorl	%ecx,%ecx		/* set cx to zero		*/
	ADD_bx_addr_0x46c		/* add it to low order time	*/
	ADC_addr_0x46e_cx		/* add high order time + carry	*/

	/* 18.2044/sec = 65536/hour, therefore high order time is hours	*/

	CMP_cx_24			/* has hours reached 24 ?	*/
	jne	L81			/* jump if no			*/
	xor	%ecx,%ecx		/* set hours to zero		*/
	MOV_bl_addr_0x470		/* set overflow byte to 1	*/
L81:	MOV_cx_addr_0x46e		/* store hours			*/
#endif
#ifndef NEW_PIC
	MOV_0xf100_bx			/* set bx to segment 0xf100	*/
	pushl	%ebx
	popl	%ds			/* ds = segment of OUTB_ADDR	*/
	xorl	%ecx,%ecx
	incl	%ecx			/* put a 1 into cx		*/
	MOV_0x20f0_bx			/* set bx to OUTB_ADDR		*/
	MOV_cx_addr_bx			/* flag outb (EOI) complete	*/
#endif
#ifdef NEW_PIC
        pushl   %eax
        movb    $0x20,%al
        outb    $0x20			/* flag interrupt complete	*/
        popl    %eax
#endif
	popl	%ebx			/* restore registers		*/
	popl	%ds
	popl	%ecx
	iret				/* return to interrupted code	*/
	.globl  _INT08_dummy_end
#undef ADD_bx_addr_0x46c
#undef ADC_addr_0x46e_cx
#undef CMP_cx_24
#undef MOV_bl_addr_0x470
#undef MOV_cx_addr_0x46e
#undef MOV_0xf100_bx
#undef MOV_0x20f0_bx
#undef MOV_cx_addr_bx
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
