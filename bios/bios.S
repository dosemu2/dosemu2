/*************************************************************************/
/**                                                                     **/
/** This is the virtual PC's Bios (F000:0 .. F000:FFFF)                 **/
/**                                                                     **/
/** We must compile this with as86/ld86 as follows:                     **/
/**   AS86FLAGS="-DNEW_PIC"    (or AS86FLAGS="")                        **/
/**   gcc -E ${AS86FLAGS} bios.S | tools/tools86 -E >bios.s             **/
/**   as86 -0 -w -g -o bios.o bios.s                                    **/
/**   ld86 -0 -r -o bios bios.o                                         **/
/**   tools/tools86 bios                                                **/
/**   mv -f bios bios.o                                                 **/
/**                                                                     **/
/** We also need to use the macro JMPL (label) if we want have a 16-bit **/
/** displacement. As86 is not able to do so without the -j switch,      **/
/** and the -j switch also make all conditional jmp's 5 byte long.      **/
/** See file macros86.h for details.                                    **/
/**                                                                     **/
/** NOTE: be careful with overlapping ".org",                           **/
/**       or ld86 will then reach your disk quotas !                    **/
/* $Id$
/*************************************************************************/

#define __ASM__
#include "memory.h"
#include "macros86.h"

/* some other usefull definitions */

#define DOSHELPER_INT 0xe6

#define BIOS_DATA 0x40
#define KEYBUF_READ_PTR  0x1a
#define KEYBUF_WRITE_PTR 0x1c
#define KEYBUFFER_START 0x80
#define KEYBUFFER_END 0x82
#define KEYSHIFT_FLAGS 0x17
#define KEYBOARD_STATUS_3 0x96
#define BIOS_TIMER 0x46c
#define BIOS_TIMER_OVERFLOW 0x470
#define HOUR24_ADJUST 176



/* NOTE: The following definition need to be in memory.h, but at this
 *       moment they aren't, so I define them here.
 *       NEED TO BE CLEANED UP !
 */
		/* out of xms.h */
#define INT2F_XMS_MAGIC         0x43  /* AH for all int 2f XMS calls */
		/* out of include/mouse.h */
#define Mouse_OFF  0x1500
#define Mouse_ROUTINE_OFF 0x1550




.text
	.globl	_bios_f000
_bios_f000:
			/* this sets up all 256 intvector stubs 
			   each starting at an address aligned to 16 byte */

			/* first 8 ints */
irqnum	set	0
	.org	irqnum*16
.REPT 8 .align 16
IF	irqnum=3
	db	0xcd,3
ELSE
	int	irqnum
ENDIF
	retf	#2
irqnum	set	irqnum+1
.ENDR

			/* hardware IRQ 0..7 (master PIC) */
irqnum	set	8
	.org	irqnum*16
.REPT 8 .align 16
	int	irqnum
	iret
irqnum	set	irqnum+1
.ENDR

			/* system-soft ints 0x10 .. 0x1b */
irqnum	set	0x10
	.org	irqnum*16
.REPT (0x1c-0x10) .align 16
	int	irqnum
	retf	#2
irqnum	set	irqnum+1
.ENDR


			/* user timer tick, should be an IRET */
irqnum	set	0x1c
	.org	irqnum*16
	iret


			/* system-soft ints 0x1d .. 0x5f */
irqnum	set	0x1d
	.org	irqnum*16
.REPT (0x60-0x1d) .align 16
	int	irqnum
	retf	#2
irqnum	set	irqnum+1
.ENDR


			/* user-soft ints 0x60 .. 0x67 spared */
			/* system-soft ints 0x68 .. 0x6f */
irqnum	set	0x68
	.org	irqnum*16
.REPT (0x70-0x68) .align 16
	int	irqnum
	retf	#2
irqnum	set	irqnum+1
.ENDR


			/*  hardware IRQ 0x70..0x77 (slave PIC) */
irqnum	set	0x70
	.org	irqnum*16
.REPT (0x78-0x70) .align 16
	int	irqnum
	retf	#2	/* NOTE: this should be "iret", isn't it?
				 NEED TO BE CHECKED ! */
irqnum	set	irqnum+1
.ENDR


			/* soft ints 0x78 .. 0xff */
irqnum	set	0x78
	.org	irqnum*16
.REPT (0x100-0x78) .align 16
	int	irqnum
	retf	#2
irqnum	set	irqnum+1
.ENDR


/* ----------------------------------------------------------------- */

	/* XMS has it's handler just after the interrupt dummy segment */
	.org	XMSControl_OFF
	jmp	(*+2+3)	/* jmp short forward 3 */
	nop
	nop
	nop
	hlt		/* HLT...the current emulator trap */
	db	INT2F_XMS_MAGIC /* just an info byte. reserved for later */
	retf


/* ----------------------------------------------------------------- */
	/* this is the mouse handler */
	.org	Mouse_OFF
   
	/* 
	mouse routine simulates the stack frame of an int, then does a 
	"pushad" before here...so we just "popad; iret" to get back out
	*/

	call	far [mouse_handler]
	pop	es
	pop	ds
	popa
	iret
	.org	Mouse_OFF+8
mouse_handler: 
	dd	0x1c810227
	.org	Mouse_ROUTINE_OFF
	push	ax
	pop	ax
	iret


/* ----------------------------------------------------------------- */
	.org	Banner_OFF

	mov	al,#5
	int	DOSHELPER_INT
	db	0xb2          /* mov dl,#0   or mov dl,#0x80 */
	.globl	_bios_f000_bootdrive
_bios_f000_bootdrive:
	db	0
	retf

/* ----------------------------------------------------------------- */
	.org	IPX_OFF
	push	ax	/* FarCallHandler removes this before returning */
	mov	al,#0x7a
	int	DOSHELPER_INT
	retf

	.globl	_bios_IPX_PopRegistersReturn
_bios_IPX_PopRegistersReturn:
	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	retf
	.globl	_bios_IPX_PopRegistersIRet
_bios_IPX_PopRegistersIRet:
	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	iret

/* ----------------------------------------------------------------- */

	.org	((INT16_SEG-BIOSSEG) << 4)+INT16_OFF

	.globl	_INT16_dummy_start
_INT16_dummy_start:			/* KEYBOARD BIOS ROUTINE        */
	push    ds			/* save ds and bx               */
	push    bx		/* next 2 lines set ds to bios data seg	*/
        mov     bx,#BIOS_DATA
	mov	ds,bx

#if 1 /* new */
	cmp	ah,#2
	ja	INT16_3			/* >2 */
	je	INT16_2			/* 2 => retn. shift flag stat. */
	jp	INT16_1			/* 1 => return keyboard status */
	jmp	INT16_0			/* 0 => read keyboard */

INT16_0_idle:
	sti
	push	ax			/* buffer empty - 		*/
	mov     ax,#0x1680		/* magic machine idle function  */
	int     0x2f
	pop	ax
INT16_0:				/* 0 => read keyboard */
	cli				/* READ KEYBOARD		*/
	mov	bx,[KEYBUF_READ_PTR]    /* get address of next char	*/
	cmp	bx,[KEYBUF_WRITE_PTR]	/* is buffer empty?		*/
	je      INT16_0_idle		/* jump if not empty		*/
INT16_0_a:
	mov	ax,[bx]			/* get last key	*/
	inc     bx			/* advance buffer pointer 2 bytes*/
	inc     bx
	mov	[KEYBUF_READ_PTR],bx	/* save new pointer		*/
	cmp	bx,[KEYBUFFER_END]	/* check for wrap around        */
	jne	INT16_ex		/* no wrap - go return to caller*/
					/* wrap - get buffer start	*/
	mov	bx,[KEYBUFFER_START]
					/* save it as new pointer	*/
	mov     [KEYBUF_READ_PTR],bx
	jmp	INT16_ex		/* return to caller		*/

INT16_1:
	cli
	mov	bx,[KEYBUF_READ_PTR]	/* get address of next char	*/
	cmp	bx,[KEYBUF_WRITE_PTR]	/* set flag if buffer empty	*/
	mov	ax,[bx]			/* put char into ax		*/
	sti
	pop     bx
	pop     ds
	retf    #2		/* can't do iret, it would change flags	*/

INT16_2:				/* RETURN SHIFT FLAG STATUS     */
	mov	ax,[KEYSHIFT_FLAGS]	/* get shift flags (ah=extended)*/
	and     ah,#0xf3		/* disallow bits 2 & 3		*/
	mov	bh,[KEYBOARD_STATUS_3]	/* bits 2 & 3 come from here	*/
	and     bh,#0xc			/* isolate those bits		*/
	or      ah,bh			/* merge them with flags in ax	*/
	jmp	INT16_ex		/* return to caller		*/

INT16_3:
	cmp	ah,#5
	je	INT16_5			/*  5 => store key in buffer    */
	jb	INT16_ex		/*  3, 4 */
					/*  3 => set rpt. speed & delay */
					/*  4 => reserved (do nothing)  */
	cmp	ah,#0x10
	je	INT16_0			/* 0x10 => read extended keyboard */
	jb	INT16_ex
	cmp	ah,#0x12
	je	INT16_2			/* 0x12 => retn ext shift flg stat */
	jb	INT16_1			/* 0x11 => retn. ext. keybd. stat.*/
INT16_ex:
	pop     bx			/* do nothing and return	*/
	pop     ds
	iret   

INT16_5:
	cli				/* STORE KEY INTO BUFFER	*/
	mov	bx,[KEYBUF_WRITE_PTR]	/* get buffer "in" pointer	*/
	inc     bx			/* increment "in" pointer	*/
	inc     bx
	cmp	bx,[KEYBUFFER_END]	/* wrap if needed		*/
	jne     INT16_5_a
	mov	bx,[KEYBUFFER_START]
INT16_5_a:
	cmp	bx,[KEYBUF_READ_PTR]	/* is buffer full?              */
	jne     INT16_5_b
	mov     al,#1			/* buffer is full, return error	*/
	sti
	jmp	INT16_ex
INT16_5_b:
	push    bx			/* buffer is not full - save bx	*/
	mov     bx,[KEYBUF_WRITE_PTR]	/* get "in" pointer again	*/
	mov	[bx],cx			/* put cx into buffer		*/
	mov     al,#0			/* clear error flag		*/
	pop     bx			/* restore incremented pointer	*/
	mov	[KEYBUF_WRITE_PTR],bx	/* save it in bios data area	*/
	sti
	jmp	INT16_ex		/* return to caller		*/

#else /* old */
	/* ------------------------------------ */
 	cmp     ah,#0			/*   0 => read keyboard		*/
	je      L2
	cmp     ah,#0x10		/* 10 => read extended keyboard	*/
	je      L2
	cmp     ah,#0x1			/*  1 => return keyboard status	*/
	je      L4
	cmp     ah,#0x11		/* 11 => retn. ext. keybd. stat.*/
	je      L4
	cmp     ah,#0x2			/*  2 => retn. shift flag stat.	*/
	je      L5
	cmp     ah,#0x3			/*  3 => set rpt. speed & delay	*/
	je      L1
	cmp     ah,#0x4			/*  4 => reserved (do nothing)	*/
	je      L1
	cmp     ah,#0x5			/*  5 => store key in buffer	*/
	je      L6
	cmp     ah,#0x12		/* 12 => retn ext shift flg stat*/
	je      L5
L1:	pop     bx			/* do nothing and return	*/
	pop     ds
	iret   
L2:	cli				/* READ KEYBOARD		*/
	mov	bx,[KEYBUF_READ_PTR]    /* get address of next char	*/
	cmp	bx,[KEYBUF_WRITE_PTR]	/* is buffer empty?		*/
	jne     L3			/* jump if not empty		*/

	push	ax			/* buffer empty - 		*/
	mov     al,#0x16		/* ask dosemu for another key	*/
	sti
	int     0xe6			/* sti with return to dosemu code */
	pop	ax
	jmp     L2			/* got another key, go try again*/
L3:
	mov	ax,[bx]			/* movw   [%bx],%ax  get last key	*/
	inc     bx			/* advance buffer pointer 2 bytes	*/
	inc     bx
					/* save new pointer		*/	
	mov	[KEYBUF_READ_PTR],bx
	cmp	bx,[KEYBUFFER_END]	/* check for wrap around        */
	JNEL    (L1)			/* no wrap - go return to caller*/
					/* wrap - get buffer start	*/
	mov	bx,[KEYBUFFER_START]
					/* save it as new pointer	*/
	mov     [KEYBUF_READ_PTR],bx
	JMPL    (L1)			/* return to caller		*/


L4:	push	ax			/* RETURN KEYBOARD STATUS	*/
	movb    al,#0x16		/* anti hogging code	*/
	int     0xe6			/* call delay routine 	*/
	pop	ax

	cli
	mov	bx,[KEYBUF_READ_PTR]	/* get address of next char	*/
	cmp	bx,[KEYBUF_WRITE_PTR]	/* set flag if buffer empty	*/
	mov	ax,[bx]			/* put char into ax		*/
	sti
	pop     bx
	pop     ds
	retf    #2		/* can't do iret, it would change flags	*/
					/* RETURN SHIFT FLAG STATUS	*/
L5:	
	mov	ax,[KEYSHIFT_FLAGS]	/* get shift flags (ah=extended)*/
	and     ah,#0xf3		/* disallow bits 2 & 3		*/
	mov	bh,[KEYBOARD_STATUS_3]	/* bits 2 & 3 come from here	*/
	and     bh,#0xc			/* isolate those bits		*/
	or      ah,bh			/* merge them with flags in ax	*/
	JMPL    (L1)			/* return to caller		*/

L6:	cli				/* STORE KEY INTO BUFFER	*/
	mov	bx,[KEYBUF_WRITE_PTR]	/* get buffer "in" pointer	*/
	inc     bx			/* increment "in" pointer	*/
	inc     bx
	cmp	bx,[KEYBUFFER_END]	/* wrap if needed		*/
	jne     L7
	mov	bx,[KEYBUFFER_START]
L7:
	cmp	bx,[KEYBUF_READ_PTR]	/* is buffer full?              */
	jne     L8
	mov     al,#1			/* buffer is full, return error	*/
	sti
	JMPL    (L1)
L8:	push    bx			/* buffer is not full - save bx	*/
	mov     bx,[KEYBUF_WRITE_PTR]	/* get "in" pointer again	*/
	mov	[bx],cx			/* put cx into buffer		*/
	mov     al,#0			/* clear error flag		*/
	pop     bx			/* restore incremented pointer	*/
	mov	[KEYBUF_WRITE_PTR],bx	/* save it in bios data area	*/
	sti
	JMPL    (L1)			/* return to caller		*/
#endif

	.globl  _INT16_dummy_end
_INT16_dummy_end:


/* ----------------------------------------------------------------- */   
	.org	((INT08_SEG-BIOSSEG) << 4)+INT08_OFF

	.globl	_INT08_dummy_start
_INT08_dummy_start:			/* TIMER INTERRUPT ROUTINE	*/

#if 0
	int	0x1c			/* call int 0x1c, per bios spec	*/
#endif
/* NOTE: The above int 0x1c is a compatibility fault, because
 *       the original IBM Bios calls the user *after* the timer is
 *       increased. So, I moved it down.
 *       THIS NEEDS TO BE CHECKED for side effects in dosemu !
 */
	push	ds
	push	ax
	xor	ax, ax			/* set bx to segment 0		*/
        mov	ds,ax
	inc	dword ptr [BIOS_TIMER]
	cmp	word ptr [BIOS_TIMER+2],#24 /* 24 hour check */
	jb	_INT08_L1
	cmp	word ptr [BIOS_TIMER],#HOUR24_ADJUST
	jb	_INT08_L1
	mov	dword ptr [BIOS_TIMER],#0
	mov	byte ptr [BIOS_TIMER_OVERFLOW],#1
_INT08_L1:
	int	0x1c		/* call int 0x1c, per bios spec	*/
				/* must do it before EOI, but after count */
#ifndef NEW_PIC
	mov	ax,#OUTB_SEG		/* set bx to segment 0xf100	*/
	mov	ds,ax			/* ds = segment of OUTB_ADDR    */
	mov	word ptr [OUTB_OFF],#1	/* flag outb (EOI) complete     */
#else
        mov     al,#0x20
        out     0x20,al			/* flag interrupt complete	*/
#endif
	pop	ax			/* restore registers            */
	pop	ds
	iret				/* return to interrupted code	*/
	.globl  _INT08_dummy_end
_INT08_dummy_end:

/* ----------------------------------------------------------------- */   
	.org	((INT10_SEG-BIOSSEG) << 4)+INT10_OFF
	push	ax
	sti
	mov	al,#8		/*  Start Video init */
	int	DOSHELPER_INT
	db	0x9a   /* call far 0xc000:3  or call far 0xe000:3 */
	.globl	_bios_f000_int10ptr
_bios_f000_int10ptr:
	dd	0xc0000003
	mov	al,#9		/* Finished video init */
	int     DOSHELPER_INT
	pop	ax
	sti
	retf

/* ----------------------------------------------------------------- */   
		/* This is an int e7 used for FCB opens */
	.org	((INTE7_SEG-BIOSSEG) << 4)+INTE7_OFF
	push	es
	push	di
	push	ax
	mov	ax,#0x120c
	int	0x2f
	pop	ax
	pop	di
	pop	es
	iret

/* ----------------------------------------------------------------- */   
	.org	((DPMI_SEG-BIOSSEG) << 4)+DPMI_OFF

	.globl	_DPMI_dummy_start
_DPMI_dummy_start:
	push	ax
	mov	ah,#0x51
	int	0x21			/* Get PSP */
	pop	ax
	.globl	_DPMI_dpmi_init
_DPMI_dpmi_init:
	hlt
	retf
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
	retf
	.globl	_DPMI_API_extension
_DPMI_API_extension:
	hlt
	retf
	.globl	_DPMI_return_from_pm
_DPMI_return_from_pm:
	hlt
	.globl	_DPMI_return_from_pm_eh
_DPMI_return_from_pm_eh:
	hlt
	.globl	_DPMI_exception
_DPMI_exception:
	FILL_OPCODE(32,hlt)

	.globl	_DPMI_interrupt
_DPMI_interrupt:
	FILL_OPCODE(256,hlt)

	.globl	_DPMI_dummy_end
_DPMI_dummy_end:



/* ----------------------------------------------------------------- */   
	/* NOTE:
	 * This part currently is overwritten by Vinod's pkt_init()
	 * because the current address layout depends on "struct pkt_globs"
	 * I guess, Vinod is the man to actually integrate this stuff.
	 * ( Vinod ? ).
	 * So, this is only to be taken as template or example !
	 */
	.org	PKTDRV_OFF

#define PKTDRV_VECTOR 0x60

	.globl	_PKTDRV_signature
_PKTDRV_signature:
	jmp	PKTDRV_start
	nop
	.ascii	"PKT DRVR"
	db	0
PKTDRV_start:
	int	PKTDRV_VECTOR
	retf	#2
	.org	PKTDRV_OFF+0x50
	.ascii	"Linux$\0\0"
	dw	12	/* type */
PKTDRV_size: dw	0
PKTDRV_receiver: dd	0
	dd	PKTDRV_VECTOR+1 /* helpvec */

PKTDRV_helper:
	pusha
	push	ds
	push	es
	CSEG
	mov	cx,PKTDRV_size
	jcxz	PKTDRV_nothing
	xor	ax,ax
	push	cx
	CSEG
	call	far [PKTDRV_receiver]
	pop	cx
	mov	ax,es
	or	ax,di
	jz	PKTDRV_norecv
	push	cs
	pop	ds
	mov	si,#PKTDRV_buf
	push	di
	push	cx
	cld
	shr	cx,#1
	rep
	movsw
	jnc	PKTDRV_nobyte
	movsb
PKTDRV_nobyte:
	pop	cx
	pop	si
	push	es
	pop	ds
	mov	ax,#1
	CSEG
	call	far [PKTDRV_receiver]
PKTDRV_norecv:
	CSEG
	mov	word ptr PKTDRV_size,#0
PKTDRV_nothing:
	pop	es
	pop	ds
	popa
	iret

	.org	PKTDRV_OFF+0x1c4  /* 0x62c4 */
PKTDRV_buf:

/* ----------------------------------------------------------------- */   
	.org	ROM_CONFIG_OFF /* for int15 */

	/* from Alan Cox's mods */
	/* we need somewhere for the bios equipment. */
	dw	9	/* 9 byte table */
	db	0xfc	/* PC AT */
	dw	0x401	/* bios revision 4 */
	db	0x70	/* no mca, no ebios, no wat, keybint,
			   rtc, slave 8259, no dma 3 */
	dd	0



/* ----------------------------------------------------------------- */   
	.org	((INT09_SEG-BIOSSEG) << 4)+INT09_OFF
	.globl	_INT09_dummy_start
_INT09_dummy_start:			/* KEYBOARD INTERRUPT ROUTINE	*/
	push	ds			/* save registers		*/
	push	bx
	push	ax
	push	cx
	mov	bx, #LASTSCAN_SEG	/* get segment of LASTSCAN_ADDR	*/
	push	bx
	pop	ds
#ifndef NEW_PIC				/* next 2 lines not needed?	*/
	xor	cx,cx
	inc	cx
#endif
			/* LASTSCAN_OFF = 0x20f2 = pseudo 8042 output	*/
	mov	bx,#LASTSCAN_OFF
	mov	ax,[bx]			/* "read" keyboard controller	*/

	mov	ah,#0x4f		/* prepare for int 15 func 4f	*/
	clc 
	cmc
	int	0x15			/* int 15 func 4f per bios spec	*/

	mov	ah,al			/* move char to ah		*/
	mov	al,#6			/* dosemu helper func 6		*/

	int	0xe6			/* call dos helper		*/
#ifndef NEW_PIC
	mov	bx,#OUTB_SEG		/* get segment of OUTB_ADDR	*/
	push	bx
	pop	ds			/* put it into ds		*/
	xor	cx,cx
	inc	cx			/* put a 1 into cx		*/
	mov	bx,#OUTB_OFF		/* set bx to OUTB_ADDR		*/
	mov	[bx],cx			/* flag outb as complete	*/
#else
	mov    	al,#0x20
	out     0x20,al			/* tell pic we're done 		*/
#endif
	pop	cx			/* restore registers		*/
	pop	ax
	pop	bx
	pop	ds
	iret
	.globl  _INT09_dummy_end
_INT09_dummy_end:



/* ----------------------------------------------------------------- */
	.org	0xf065
	/* relocated video handler (interrupt 0x42) */
	int	0x42  /* this will be overwritten with iret, if no dualmon */
	retf	#2

/* ----------------------------------------------------------------- */
	.org	0xfff0
	/* set up BIOS exit routine (we have *just* enough room for this) */
	mov	ax,#0xffff
	int	DOSHELPER_INT
	.ascii	"02/25/93"  /* our bios date */
	hlt
	db	0xfc   /* model byte = IBM AT */
	hlt

	.globl  _bios_f000_end
_bios_f000_end:

/*--------------------------------------------------------------------------*/
