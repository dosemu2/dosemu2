/*************************************************************************/
/**                                                                     **/
/** This is the virtual PC's Bios (F000:0 .. F000:FFFF)                 **/
/**                                                                     **/
/** We must compile this with as86/ld86 as follows:                     **/
/**   gcc -E  bios.S | tools/tools86 -E >bios.s             **/
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
/*************************************************************************/

#define __ASM__
#include "config.h"
#include "memory.h"
#include "macros86.h"
#ifdef __linux__
#include <linux/linkage.h>
#endif

#ifdef NEW_KBD_CODE
#include "keyb_server.h"
#endif


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
#define DISKETTE_MOTOR_TIMEOUT 0x440

/* NOTE: The following definition need to be in memory.h, but at this
 *       moment they aren't, so I define them here.
 *       NEED TO BE CLEANED UP !
 */
		/* out of xms.h */
#define INT2F_XMS_MAGIC         0x43  /* AH for all int 2f XMS calls */

.text
	.globl	CISH(bios_f000)
CISH(bios_f000):
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

			/* system-soft ints 0x10 .. 0x12 */
irqnum	set	0x10
	.org	irqnum*16
.REPT (0x13-0x10) .align 16
	int	irqnum
	iret
irqnum	set	irqnum+1
.ENDR


			/* system-soft ints 0x13 .. 0x16 */
irqnum	set	0x13
	.org	irqnum*16
.REPT (0x17-0x13) .align 16
	int	irqnum
	retf	#2
irqnum	set	irqnum+1
.ENDR


			/* system-soft ints 0x17 .. 0x1b */
irqnum	set	0x17
	.org	irqnum*16
.REPT (0x1c-0x17) .align 16
	int	irqnum
	iret
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
	/* this is the mouse handler */
	.org	((Mouse_SEG - BIOSSEG) << 4) + Mouse_OFF
   
	/* 
	mouse routine simulates the stack frame of an int, then does a 
	"pushad" before here...so we just "popad; iret" to get back out
	*/

	call	far [mouse_handler]
	pop	es
	pop	ds
	popa
	iret
	.org	((Mouse_SEG - BIOSSEG) << 4)+Mouse_OFF+8
mouse_handler: 
	dd	0x1c810227
	.org	((Mouse_SEG - BIOSSEG) << 4)+Mouse_ROUTINE_OFF
	push	ax
        mov     al,#0x20
        out     0x20,al			/* flag interrupt complete	*/
	pop	ax
	iret

/* ----------------------------------------------------------------- */   
	/* NOTE:
	 * This part currently is overwritten by Vinod's pkt_init()
	 * because the current address layout depends on "struct pkt_globs"
	 * I guess, Vinod is the man to actually integrate this stuff.
	 * ( Vinod ? ).
	 * So, this is only to be taken as template or example !
	 */
	.org	((PKTDRV_SEG - BIOSSEG) << 4) + PKTDRV_OFF

#define PKTDRV_VECTOR 0x60

	.globl	CISH(PKTDRV_signature)
CISH(PKTDRV_signature):
	jmp	PKTDRV_start
	nop
	.ascii	"PKT DRVR"
	db	0
PKTDRV_start:
	int	PKTDRV_VECTOR
	retf	#2
	.org	((PKTDRV_SEG - BIOSSEG) << 4) + PKTDRV_OFF+0x50
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

	.org ((PKTDRV_SEG - BIOSSEG) << 4) + PKTDRV_OFF+0x1c4  /* 0x62c4 */
PKTDRV_buf:


/* ----------------------------------------------------------------- */


	.org	((DBGload_SEG - BIOSSEG) << 4) + DBGload_OFF
/* we come here after we have intercepted INT21 AX=4B00
 * in order to get a breakpoint for the debugger
 * (wanting to debug a program from it's very beginning)
 */
	.globl	CISH(DBGload)
	.globl	CISH(DBGload_CSIP)
	.globl	CISH(DBGload_parblock)
CISH(DBGload):
	cli	/* first we set up the users stack */
	CSEG
	mov	ss,word ptr DBGload_SSSP+2
	CSEG
	mov	sp,word ptr DBGload_SSSP
	mov	ah,#$62	/* we must get the PSP of the loaded program */
	int	#$21
        mov	es,bx
	mov     ds,bx
	xor     ax,ax
	mov     bx,ax
	mov     cx,ax
	mov     dx,ax
	mov     si,ax
	mov     di,ax
	mov     bp,ax
	sti
	CSEG
	jmp	far [CISH(DBGload_CSIP)] /* and give control to the program */


	/* this is the paramblock, we told DOS to use for INT21 AX=4B01 */
	.align	16
CISH(DBGload_parblock):	dw	0
			dd	0,0,0
DBGload_SSSP:		dd	0
CISH(DBGload_CSIP):	dd	0

/* ----------------------------------------------------------------- */


	/* XMS has it's handler just after the interrupt dummy segment */
	.org	((XMSControl_SEG - BIOSSEG) << 4) + XMSControl_OFF
	jmp	(*+2+3)	/* jmp short forward 3 */
	nop
	nop
	nop
	hlt		/* HLT...the current emulator trap */
	db	INT2F_XMS_MAGIC /* just an info byte. reserved for later */
	retf

/* ----------------------------------------------------------------- */
	.org	((Banner_SEG - BIOSSEG) << 4) + Banner_OFF

	mov	al,#5
	int	DOSHELPER_INT
	db	0xb2          /* mov dl,#0   or mov dl,#0x80 */
	.globl	CISH(bios_f000_bootdrive)
CISH(bios_f000_bootdrive):
	db	0
	retf


/* ----------------------------------------------------------------- */

	.org	((INT16_SEG-BIOSSEG) << 4)+INT16_OFF

	.globl	CISH(INT16_dummy_start)
CISH(INT16_dummy_start):			/* KEYBOARD BIOS ROUTINE        */
	push    ds			/* save ds and bx               */
	push    bx		/* next 2 lines set ds to bios data seg	*/
        mov     bx,#BIOS_DATA
	mov	ds,bx

#if 1 /* new */
#ifdef NEW_KBD_CODE
	cmp	ah,#0
	je	INT16_0			/* read key */
	cmp	ah,#1
	je	INT16_1			/* check key available */
	cmp	ah,#2
	je	INT16_2			/* get shift flags */
#if 0
	cmp	ah,#3
	je	INT16_3			/* set typematic rate */
	cmp	ah,#4
	je	INT16_4			/* adjust keyclick */
#endif
	cmp	ah,#5
	je	INT16_5			/* store key in buffer */
	cmp	ah,#0x10
	je	INT16_10		/* read key extended */
	cmp	ah,#0x11
	je	INT16_11		/* check key extended */
	cmp	ah,#0x12
	je	INT16_12		/* get shift flags extended */

INT16_ex:
	pop     bx			/* do nothing and return	*/
	pop     ds
	iret   
#else
	cmp	ah,#2
	ja	INT16_3			/* >2 */
	je	INT16_2			/* 2 => retn. shift flag stat. */
	jp	INT16_1			/* 1 => return keyboard status */
	jmp	INT16_0			/* 0 => read keyboard */
#endif

INT16_0_idle:
	sti
	push	ax			/* buffer empty - 		*/
	mov     ax,#0x1680		/* magic machine idle function  */
	int     0x2f
	pop	ax
INT16_0:				/* 0 => read keyboard */
#ifdef NEW_KBD_CODE
INT16_10:				/* 0x10 => extended read        */
#endif
	cli				/* READ KEYBOARD		*/
	mov	bx,[KEYBUF_READ_PTR]    /* get address of next char	*/
	cmp	bx,[KEYBUF_WRITE_PTR]	/* is buffer empty?		*/
	je      INT16_0_idle		/* jump if not empty		*/
INT16_0_a:
	mov	ax,[bx]			/* get last key	*/
	inc     bx			/* advance buffer pointer 2 bytes*/
	inc     bx
#ifdef NEW_KBD_CODE
 	cmp	bx,[KEYBUFFER_END]	/* check for wrap around        */
	jne	INT16_0_nowrap		/* no wrap - go return to caller*/
	mov	bx,[KEYBUFFER_START]	/* wrap - get buffer start	*/
INT16_0_nowrap:
	mov     [KEYBUF_READ_PTR],bx	/* save it as new pointer	*/
	sti
#else
	mov	[KEYBUF_READ_PTR],bx	/* save new pointer		*/
	cmp	bx,[KEYBUFFER_END]	/* check for wrap around        */
	jne	INT16_ex		/* no wrap - go return to caller*/
					/* wrap - get buffer start	*/
	mov	bx,[KEYBUFFER_START]
					/* save it as new pointer	*/
	mov     [KEYBUF_READ_PTR],bx
#endif
	jmp	INT16_ex		/* return to caller		*/

INT16_1:				/* 0x01 => check for key        */
#ifdef NEW_KBD_CODE
INT16_11:				/* 0x11 => extended check       */
#endif
	cli
	mov	bx,[KEYBUF_READ_PTR]	/* get address of next char	*/
	cmp	bx,[KEYBUF_WRITE_PTR]	/* set flag if buffer empty	*/
	mov	ax,[bx]			/* put char into ax		*/
	sti
	pop     bx
	pop     ds
	retf    #2		/* can't do iret, it would change flags	*/

INT16_2:				/* RETURN SHIFT FLAG STATUS     */
#ifdef NEW_KBD_CODE
INT16_12:
#endif
	mov	ax,[KEYSHIFT_FLAGS]	/* get shift flags (ah=extended)*/
	and     ah,#0xf3		/* disallow bits 2 & 3		*/
	mov	bh,[KEYBOARD_STATUS_3]	/* bits 2 & 3 come from here	*/
	and     bh,#0xc			/* isolate those bits		*/
	or      ah,bh			/* merge them with flags in ax	*/
	jmp	INT16_ex		/* return to caller		*/

#ifdef NEW_KBD_CODE
INT16_5:				/* STORE KEY INTO BUFFER	*/
	mov	ax,cx
	cli
	call	store_key
	sti
	jc	INT16_5_full
	mov	al,#0			/* return al=0 (success)	*/
	jmp	INT16_ex
INT16_5_full:
	mov	al,#1			/* return al=1 (buffer full)	*/
 	jmp	INT16_ex

/* SUBROUTINE: store keycode in AX in the BIOS keyboard buffer, 
 * return CF=1 if buffer is full.
 */
store_key:
	mov	bx,[KEYBUF_WRITE_PTR]
	inc	bx
	inc	bx
	cmp	bx,[KEYBUFFER_END]
	jne	no_wrap
	mov	bx,[KEYBUFFER_START]
no_wrap:
	cmp	bx,[KEYBUF_READ_PTR]
	stc
	je	keybuffer_full
	xchg	bx,[KEYBUF_WRITE_PTR]	/* ok, update write pointer	*/
	mov	[bx],ax			/* and store key in buffer	*/
	clc
keybuffer_full:
	ret

#else   /* not NEW_KBD_CODE */

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
#endif /* not NEW_KBD_CODE */
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

	.globl  CISH(INT16_dummy_end)
CISH(INT16_dummy_end):



/* ----------------------------------------------------------------- */
	.org	((IPX_SEG - BIOSSEG) << 4) + IPX_OFF
ipx_handler:
	push	ax	/* FarCallHandler removes this before returning */
	mov	al,#0x7a
	int	DOSHELPER_INT
	retf

	.globl	CISH(bios_IPX_FarCall)
CISH(bios_IPX_FarCall):
	push	bx
	mov	bx,#0x0a
	db	0x9a		/* Official JES hack for call far ipx_handler */
	dd	0xf8003100	/* ### This is address of bios_IPX_FarCall */
	pop	bx
	retf

	.globl	CISH(bios_IPX_PopRegistersReturn)
CISH(bios_IPX_PopRegistersReturn):
	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	mov	al,#0x7b	/* Allow IPX to know ESR completed */
	int	DOSHELPER_INT
	pop	ax
	retf
	.globl	CISH(bios_IPX_PopRegistersIRet)
CISH(bios_IPX_PopRegistersIRet):
	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	mov	al,#0x7b
	int	DOSHELPER_INT
	pop	ax
	iret


/* ----------------------------------------------------------------- */   
	.org	((INT08_SEG-BIOSSEG) << 4)+INT08_OFF

	.globl	CISH(INT08_dummy_start)
CISH(INT08_dummy_start):		/* TIMER INTERRUPT ROUTINE	*/

#if 0
	int	0x1c	
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
	jb	CISH(INT08_L1)
	cmp	word ptr [BIOS_TIMER],#HOUR24_ADJUST
	jb	CISH(INT08_L1)
	mov	dword ptr [BIOS_TIMER],#0
	mov	byte ptr [BIOS_TIMER_OVERFLOW],#1
CISH(INT08_L1):
			/* emulate 'diskette motor running */
			/* some old games rely on that --SW, --Hans */
	cmp	byte ptr [DISKETTE_MOTOR_TIMEOUT],#0
	jz	CISH(INT08_L2)
	dec	byte ptr [DISKETTE_MOTOR_TIMEOUT]
CISH(INT08_L2):
	int	0x1c		/* call int 0x1c, per bios spec	*/
				/* must do it before EOI, but after count */
        mov     al,#0x20
        out     0x20,al			/* flag interrupt complete	*/
	pop	ax			/* restore registers            */
	pop	ds
	iret				/* return to interrupted code	*/
	.globl  CISH(INT08_dummy_end)
CISH(INT08_dummy_end):

/* ----------------------------------------------------------------- */   
	.org	((INT10_SEG-BIOSSEG) << 4)+INT10_OFF
	push	ax
	sti
	mov	al,#8		/*  Start Video init */
	int	DOSHELPER_INT
	db	0x9a   /* call far 0xc000:3  or call far 0xe000:3 */
	.globl	CISH(bios_f000_int10ptr)
CISH(bios_f000_int10ptr):
	dd	0xc0000003
	mov	al,#9		/* Finished video init */
	int     DOSHELPER_INT
	pop	ax
	sti
	retf


/* This is installed after video init (helper fcn 0x9) when the internal
	mouse driver is in use.  It watches for mouse set commands and
	resets the mouse driver when it sees one. */
/* Comments and bugs to David Etherton, etherton@netcom.com */

	.org	((INT10_SEG-BIOSSEG) << 4)+INT10_WATCHER_OFF
	or	ah,ah
	jz	L9	/* normal mode set */
	cmp	ax,#0x4F02
	jne	L10	/* svga mode set */
L9:
	pusha		/* save everything */
	mov	ax,#0x33	/* mouse helper */
	mov	bx,#0xf0	/* start video mode set */
	int	DOSHELPER_INT
	popa

	pushf		/* fake stack frame for iret */
	push	cs
	call	L10	/* perform the actual mode set */

	pushf		/* remember everything from int10 call */
	pusha
	mov	ax,#0x33	/* mouse helper */
	mov	bx,#0xf1	/* end video mode set */
	int	DOSHELPER_INT
	popa
	popf
	iret		/* return to original caller */

L10:	/* chain to original handler (probably the video bios) */
	pushf
	db	0x9a
	.globl	CISH(bios_f000_int10_old)
CISH(bios_f000_int10_old):
	dw	0,0
	iret

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
/* This hlt address is used to guarantee a return from a real-mode irq */
/* It uses one byte.  see sigsegv.c */
	.org	((PIC_SEG-BIOSSEG) << 4)+PIC_OFF

	.globl	CISH(PIC_dummy_start)
CISH(PIC_dummy_start):
	hlt

/* ----------------------------------------------------------------- */   
	.org	((DPMI_SEG-BIOSSEG) << 4)+DPMI_OFF

	.globl	CISH(DPMI_dummy_start)
CISH(DPMI_dummy_start):
	push	bx
	push	ax
	mov	ah,#0x62
	int	0x21			/* Get PSP */
	pop	ax
	.globl	CISH(DPMI_dpmi_init)
CISH(DPMI_dpmi_init):
	hlt
	pop	bx
	retf
	.globl	CISH(DPMI_return_from_dos)
CISH(DPMI_return_from_dos):
	hlt
	.globl	CISH(DPMI_return_from_dos_exec)
CISH(DPMI_return_from_dos_exec):
	hlt
	.globl	CISH(DPMI_return_from_dosint)
CISH(DPMI_return_from_dosint):
	FILL_OPCODE(256,hlt)
	.globl	CISH(DPMI_return_from_realmode)
CISH(DPMI_return_from_realmode):
	hlt
	.globl	CISH(DPMI_return_from_dos_memory)
CISH(DPMI_return_from_dos_memory):
	hlt
	.globl   CISH(DPMI_realmode_callback)
CISH(DPMI_realmode_callback):
	FILL_OPCODE(0x10,hlt)
	.globl	CISH(DPMI_mouse_callback)
CISH(DPMI_mouse_callback):
	hlt
	retf
	.globl	CISH(DPMI_raw_mode_switch)
CISH(DPMI_raw_mode_switch):
	hlt
	.globl	CISH(DPMI_save_restore)
CISH(DPMI_save_restore):
	hlt
	retf
	.globl	CISH(DPMI_API_extension)
CISH(DPMI_API_extension):
	hlt
	retf
	.globl	CISH(DPMI_return_from_pm)
CISH(DPMI_return_from_pm):
	hlt
	.globl	CISH(DPMI_return_from_exception)
CISH(DPMI_return_from_exception):
	hlt
	.globl	CISH(DPMI_return_from_rm_callback)
CISH(DPMI_return_from_rm_callback):
	hlt
	.globl	CISH(DPMI_return_from_mouse_callback)
CISH(DPMI_return_from_mouse_callback):
	hlt
	.globl	CISH(DPMI_exception)
CISH(DPMI_exception):
	FILL_OPCODE(32,hlt)
	.globl	CISH(DPMI_interrupt)
CISH(DPMI_interrupt):
	FILL_OPCODE(256,hlt)

	.globl	CISH(DPMI_dummy_end)
CISH(DPMI_dummy_end):


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
#ifdef NEW_KBD_CODE
	.org	((INT09_SEG-BIOSSEG) << 4)+INT09_OFF
	.globl	CISH(INT09_dummy_start)
	push	ax
	push	bx
	push	ds
	CSEG
	mov	ax,#BIOS_DATA
	mov	ds,ax

/* BIOS keyboard intercept */

/* get the RAW scancode (used only for int15,4f) */
	in	al,0x60
	
	mov	al,ah
	mov	ah,#0x4f
	stc
	int	#0x15
	
	/* ignore the returned keycode, only skip the pre-translated
	   bios keycode if CF=0.
	   this is not completely accurate but hard to improve while
	   keeping a clean keyboard server design.
	 */
	jnc	kbd_done

        /* get the pre-translated bios key. */
	
	mov	al,#7
	int	0xe6			/* call get_bios_key helper       */
					/* returns ax=keycode or 0,       */
					/* also copies new shift state to */
					/* seg 0x40                       */

	test	ax,ax
	jz	kbd_done		/* no keycode returned */

/* check for "special action" codes
 */
	cmp	ax, #SP_PAUSE
	je	kbd_do_pause
	cmp	ax, #SP_BREAK
	je	kbd_do_break
	cmp	ax, #SP_PRTSCR
	je	kbd_do_prtscr
	cmp	ax, #SP_SYSRQ_MAKE
	je	kbd_do_sysrq_make
	cmp	ax, #SP_SYSRQ_BREAK
	je	kbd_do_sysrq_break

	call	store_key

kbd_done:
	mov    	al,#0x20
	out     0x20,al			/* tell pic we're done 		*/
	pop	ds
	pop	bx
	pop	ax			/* restore registers		*/
	iret

kbd_do_pause:
	jmp	kbd_done

kbd_do_break:				/* CTRL-BREAK pressed		*/
	xor	ax,ax
	call	store_key		/* put null word into buffer	*/
	int	0x1b			/* call BREAK interrupt		*/
	jmp	kbd_done

kbd_do_prtscr:				/* PRINT SCREEN pressed		*/
	int	0x05
	jmp	kbd_done

kbd_do_sysrq_make:			/* Alt-SYSRQ pressed		*/
	mov	ax,0x8500
	int	0x15
	jmp	kbd_done

kbd_do_sysrq_break:			/* ALT-SYSRQ released		*/
	mov	ax,0x8501
	int	0x15
	jmp	kbd_done

	.globl  CISH(INT09_dummy_end)
CISH(INT09_dummy_end):


#else /* not NEW_KBD_CODE */

	.org	((INT09_SEG-BIOSSEG) << 4)+INT09_OFF
	.globl	CISH(INT09_dummy_start)
CISH(INT09_dummy_start):		/* KEYBOARD INTERRUPT ROUTINE	*/
	push	ds			/* save registers		*/
	push	bx
	push	ax
	push	cx
	mov	bx, #LASTSCAN_SEG	/* get segment of LASTSCAN_ADDR	*/
	push	bx
	pop	ds
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
	mov    	al,#0x20
	out     0x20,al			/* tell pic we're done 		*/
	pop	cx			/* restore registers		*/
	pop	ax
	pop	bx
	pop	ds
	iret
	.globl  CISH(INT09_dummy_end)
CISH(INT09_dummy_end):

#endif /* not NEW_KBD_CODE */

/* ----------------------------------------------------------------- */
	.org	0xf065
	/* relocated video handler (interrupt 0x42) */
	int	0x42  /* this will be overwritten with iret, if no dualmon */
	retf	#2

	.globl  CISH(bios_f000_end)
CISH(bios_f000_end):

/* ----------------------------------------------------------------- */
	.org	0xfff0
	/* set up BIOS exit routine (we have *just* enough room for this) */
	mov	ax,#0xffff
	int	DOSHELPER_INT
	.ascii	"02/25/93"  /* our bios date */
	hlt
	db	0xfc   /* model byte = IBM AT */
	hlt

/*--------------------------------------------------------------------------*/
