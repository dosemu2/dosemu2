/* 
 * (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

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

#include "keyb_server.h"

/* some other useful definitions */

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
#ifdef USE_NEW_INT
	int	irqnum
#endif /* USE_NEW_INT */
	iret


			/* system-soft ints 0x1d .. 0x5f */
irqnum	set	0x1d
	.org	irqnum*16
.REPT (0x60-0x1d) .align 16
	int	irqnum
	retf	#2
irqnum	set	irqnum+1
.ENDR

irqnum	set	0x60
	.org	irqnum*16
int_pktdrv:
	int	irqnum
	retf	#2

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
/* ======================= Addr = F000:20F0 (F20F0) */
   
	/* 
	mouse routine simulates the stack frame of an int, then does a 
	"pusha" before here...so we just "popa; iret" to get back out
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
	/* the packet driver */
	.org	((PKTDRV_SEG - BIOSSEG) << 4) + PKTDRV_OFF
/* ======================= Addr = F000:3100 (F3100) */

	.globl	CISH(PKTDRV_size)
	.globl	CISH(PKTDRV_handle)
	.globl	CISH(PKTDRV_receiver)
	.globl	CISH(PKTDRV_buf)
	.globl	CISH(PKTDRV_helper)
	.globl	CISH(PKTDRV_driver_name)
	.globl	CISH(PKTDRV_param)
	.globl	CISH(PKTDRV_stats)
	.globl	CISH(PKTDRV_signature)
	.globl	CISH(PKTDRV_start)

CISH(PKTDRV_start):
	JMPL(int_pktdrv)

/* The packet driver signature will be written here when the packet
   driver is initialized. */
CISH(PKTDRV_signature):
		db	0,0,0,0,0,0,0,0,0

CISH(PKTDRV_driver_name):
		/* NOTE: "xx\0" is _not_ ASCIIZ "xx" but "xx0" !
		 *	 as86 doesn't recognize \0 as escape */
	.ascii	"Linux$"
	db	0,0
CISH(PKTDRV_size):	dw	0
CISH(PKTDRV_handle):	dw	0
CISH(PKTDRV_receiver): dd	0

	/* parameter packet, filled in by pkt_init */
CISH(PKTDRV_param):
	db	0,0,0,0
	dw	0,0,0,0,0
	/* driver statistics structure */
CISH(PKTDRV_stats):
	dd	0,0,0,0,0,0,0

	/* helper routine to transfer received packets and call receiver */
CISH(PKTDRV_helper):
	pusha
	push	ds
	push	es
	CSEG
	mov	cx,PKTDRV_size
	jcxz	PKTDRV_nothing
	xor	ax,ax
	CSEG
	mov	bx,PKTDRV_handle
	push	cx
	push	bx
	CSEG
	call	far [PKTDRV_receiver]
	pop	bx
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

	.align	16
CISH(PKTDRV_buf):

/* ----------------------------------------------------------------- */

	.org	((DBGload_SEG - BIOSSEG) << 4) + DBGload_OFF
/* ======================= Addr = F000:7F80 (F7F80) */
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
/* ======================= Addr = F800:0000 (F8000) */
	jmp	(*+2+3)	/* jmp short forward 3 */
	nop
	nop
	nop
	hlt		/* HLT...the current emulator trap */
	db	INT2F_XMS_MAGIC /* just an info byte. reserved for later */
	retf

/* ----------------------------------------------------------------- */
	.org	((Banner_SEG - BIOSSEG) << 4) + Banner_OFF
/* ======================= Addr = F800:2000 (FA000) */

	mov	al,#5
	int	DOSHELPER_INT
	db	0xb2          /* mov dl,#0   or mov dl,#0x80 */
	.globl	CISH(bios_f000_bootdrive)
CISH(bios_f000_bootdrive):
	db	0
	retf


/* ----------------------------------------------------------------- */
	.org	((IPX_SEG - BIOSSEG) << 4) + IPX_OFF
/* ======================= Addr = F800:3100 (FB100) */
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

	.org	((INT16_SEG-BIOSSEG) << 4)+INT16_OFF
/* ======================= Addr = F800:3500 (FB000) */

	.globl	CISH(INT16_dummy_start)
CISH(INT16_dummy_start):			/* KEYBOARD BIOS ROUTINE        */
	push	dx		/* dx needed for extended calls */
	push    ds			/* save ds and bx               */
	push    bx		/* next 2 lines set ds to bios data seg	*/
    mov     bx,#BIOS_DATA
	mov	ds,bx
	mov dl,#0x00        /* clear mask for extended calls */

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
	not	dl				/* set mask for extended calls */
	cmp	ah,#0x10
	je	INT16_10		/* read key extended */
	cmp	ah,#0x11
	je	INT16_11		/* check key extended */
	cmp	ah,#0x12
	je	INT16_12		/* get shift flags extended */

INT16_ex:
	pop     bx			/* do nothing and return	*/
	pop     ds
	pop	dx
	iret   

INT16_0_idle:
	sti
	push	ax			/* buffer empty - 		*/
	mov     ax,#0x1680		/* magic machine idle function  */
	int     0x2f
	pop	ax

INT16_0:				/* 0 => read keyboard */
INT16_10:				/* 0x10 => extended read        */
	cli				/* READ KEYBOARD		*/
	call get_key			/* get last key */
	je      INT16_0_idle		/* jump if empty		*/
	inc     bx			/* advance buffer pointer 2 bytes*/
	inc     bx
 	cmp	bx,[KEYBUFFER_END]	/* check for wrap around        */
	jne	INT16_0_nowrap		/* no wrap - go return to caller*/
	mov	bx,[KEYBUFFER_START]	/* wrap - get buffer start	*/
INT16_0_nowrap:
	mov     [KEYBUF_READ_PTR],bx	/* save it as new pointer	*/
	sti
	jmp	INT16_ex		/* return to caller		*/

INT16_1:				/* 0x01 => check for key        */
INT16_11:				/* 0x11 => extended check       */
	cli
	call	get_key			/* READ KEYBOARD		*/
	sti
	pop     bx			/* return to caller		*/
	pop     ds
	pop	dx
	retf    #2		/* can't do iret, it would change flags	*/

INT16_2:				/* RETURN SHIFT FLAG STATUS     */
INT16_12:
	mov	ax,[KEYSHIFT_FLAGS]	/* get shift flags (ah=extended)*/
	mov	bl,ah			/* get a copy to play with	*/
	and	ah,#0x73		/* disallow bits 2, 3, & 7 	*/
	and	bl,#0x04		/* clear everything but sysreq  */
	shl	bl,5			/* move it to the correct bit   */
	or	ah,bl			/* set sysreq			*/
	mov	bh,[KEYBOARD_STATUS_3]	/* bits 2 & 3 come from here	*/
	and     bh,#0xc			/* isolate those bits		*/
	or      ah,bh			/* merge them with flags in ax	*/
	jmp	INT16_ex		/* return to caller		*/

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

/* SUBROUTINE:	Get a key from the buffer if there is one.
 * Translate that key if we aren't an extended key service.
 * on call:	dl = 0 nonextended, dl=0xFF extended
 *		Assumes interrups are disabeld
 * on return:	zf=set no key, zf=clear key
 *              key in ax, bx trashed, flags trashed, dh trashed
 *		bx current value of KEYBUF_READ_PTR
 *		Interupt flag untouched
 */
get_key:
	mov	bx, [KEYBUF_READ_PTR]	/* get address of next char	*/
get_key_restart:	
	cmp	bx, [KEYBUF_WRITE_PTR]	/* set flag if buffer empty	*/
	je	get_key_ret		/* buffer empty			*/
	mov	ax, [bx]		/* put char into ax		*/
	call	do_extended		/* differences for extended calls */
	jne	get_key_ret
	inc	bx
	inc	bx
	cmp	bx, [KEYBUFFER_END]	/* check for wrap around	*/
	jne	get_key_nowrap
	mov	bx,[KEYBUFFER_START]	/* wrap - get buffer start	*/
get_key_nowrap:	
	mov	[KEYBUF_READ_PTR],bx	/* save it as new pointer	*/
	jmp	get_key_restart
get_key_ret:	
	ret	

/* SUBROUTINE:	Dumb down a key for nonextend keyboards
 * on call:	dl = 0 nonextended, dl=0xFF extended,
 *		key in ax
 * on return:	key in ax, dh trashed, flags trashed
 * 		zf=set ignore/ditch key, zf=clear keep key
 */
do_extended:				/* remove 0xE0 from AL for nonextended calls */
	mov	dh,#0xab		/* Move a value not equal to dl into dh */
	cmp	dl,#0			/* Ensure we actually need to do something */
	jne	do_extended_done
	cmp	al,#0xe0		/* is character 0xE0 ? */
	jne	do_extended_alt		/* no, continue */
	and	al,dl			/* yes, mask with 0xFF or 0x00 */
	jmp	do_extended_scan
do_extended_alt:			/* Not always alt keys but nearly */
	cmp	al,#0x00
	jne	do_extended_slash
	cmp	ah,#0x01
	je	do_extended_hide
	cmp	ah,#0x0e
	je	do_extended_hide
	cmp	ah,#0x1a
	jb	do_extended_slash
	cmp	ah,#0x1c
	jbe	do_extended_hide
	cmp	ah,#0x27
	jb	do_extended_slash
	cmp	ah,#0x29
	jbe	do_extended_hide
	cmp	ah,#0x2b
	je	do_extended_hide
	cmp	ah,#0x33
	jb	do_extended_slash
	cmp	ah,#0x35
	jbe	do_extended_hide
	cmp	ah,#0x37
	je	do_extended_hide
	cmp	ah,#0x4a
	je	do_extended_hide
	cmp	ah,#0x4c
	je	do_extended_hide
	cmp	ah,#0x4e
	je	do_extended_hide
do_extended_slash:			/* Handle special cases */
	cmp	ax, #0xe02f		/* KEY_PAD_SLASH */
	jne	do_extended_enter
	mov	ah, #0x35
	jmp	do_extended_done
do_extended_enter:
	cmp	ax, #0xe00d		/* KEY_PAD_ENTER */
	jne	do_extended_newline
	mov	ah, #0x1c
	jmp	do_extended_done
do_extended_newline:
	cmp	ax, #0xe00a		/* CTRL KEY_PAD_ENTER */
	jne	do_extended_scan
	mov	ah, #0x1c
	jmp	do_extended_done
do_extended_scan:			/* Hide new scan codes */
	cmp	ah,#0x85
	jb	do_extended_done
do_extended_hide:			/* Hide the rest of the extended keys */
	mov	dh,#0x00		/* return with zero flag set */
do_extended_done:
	cmp	dh,dl			/* return zero flag clear, normally */
	ret

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


	.globl  CISH(INT16_dummy_end)
CISH(INT16_dummy_end):

/* ----------------------------------------------------------------- */   
	.org	((INT08_SEG-BIOSSEG) << 4)+INT08_OFF

	.globl	CISH(INT08_dummy_start)
/* ======================= Addr = F800:4000 (FC000) */
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
	xor	ax, ax			/* set ax to segment 0		*/
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
	.org	((INT70_SEG-BIOSSEG) << 4)+INT70_OFF

	.globl	CISH(INT70_dummy_start)
/* ======================= Addr = F800:4100 (FC100) */
CISH(INT70_dummy_start):		/* RTC INTERRUPT ROUTINE	*/

	push	ax
	int	0x4a
        mov     al,#0x20
        out     0xa0,al			/* flag interrupt complete	*/
	out	0x20,al
	pop	ax			/* restore registers            */
	iret				/* return to interrupted code	*/
	.globl  CISH(INT70_dummy_end)
CISH(INT70_dummy_end):

/* ----------------------------------------------------------------- */   
/* This is for video init only - it calls in order:
   - first helper function (int0xe6,al=8)
   - the video BIOS init entry point at C000:3 or E000:3
   - second helper function (int 0xe6,al=9)
   It is called from memory_init() [init.c] -> init_vga_card() [vga.c]
 */
	.org	((INT10_SEG-BIOSSEG) << 4)+INT10_OFF
/* ======================= Addr = F800:4200 (FC200) */
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
/* Was: Comments and bugs to David Etherton, etherton@netcom.com
 * Current Maintainer:	Eric W. Biederman <ebiederm+eric@npwt.net>
 */	

	.org	((INT10_WATCHER_SEG-BIOSSEG) << 4)+INT10_WATCHER_OFF
/* ======================= Addr = F800:4240 (FC240) */
WINT10:
	or	ah,ah
	jz	L9	/* normal mode set */
	cmp	ah,#0x11
	je	L9	/* character generator, possibly resize the screen */
	cmp	ax,#0x4F02
	jne	L10	/* svga mode set */
L9:
	push 	ax	/* save everything */
	push	bx
	mov	ax,#0x33	/* mouse helper */
	mov	bx,#0xf0	/* start video mode set */
	int	DOSHELPER_INT
	pop	bx
	pop	ax

/* fake stack frame for iret: push original flags and avoid a GPF from pushf */
	movzx	esp,sp	/* make sure high of esp is zero */
	push	word ptr [esp+4]
	push	cs
	call	L10	/* perform the actual mode set */

/* since following code doesn't affect flags, we keep current values */
	push	ax	/* remember everything from int10 call */
	push	bx
	mov	ax,#0x33	/* mouse helper */
	mov	bx,#0xf1	/* end video mode set */
	int	DOSHELPER_INT
	pop	bx
	pop	ax
	retf	2    /* keep current flags and avoid another GPF from iret */

L10:	/* chain to original handler (probably the video bios) */
	db 	0xea /* jump far */
	.globl	CISH(bios_f000_int10_old)
CISH(bios_f000_int10_old):
	dw	0,0


/* ----------------------------------------------------------------- */   
		/* This is an int e7 used for FCB opens */
	.org	((INTE7_SEG-BIOSSEG) << 4)+INTE7_OFF
/* ======================= Addr = F800:4500 (FC500) */
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

/* ======================= Addr = F800:47F0 (FC7F0) */
	.globl	CISH(PIC_dummy_start)
CISH(PIC_dummy_start):
	hlt

/* ----------------------------------------------------------------- */   
/* This hlt address is used to return back from a call back */
/* It uses one byte.  see sigsegv.c */
	.org	((CBACK_SEG-BIOSSEG) << 4)+CBACK_OFF

/* ======================= Addr = F800:47F1 (FC7F1) */
	.globl	CISH(CBACK_return_from_dos)
CISH(CBACK_return_from_dos):
	hlt

/* ----------------------------------------------------------------- */   
	.org	((DPMI_SEG-BIOSSEG) << 4)+DPMI_OFF
/* ======================= Addr = F800:4800 (FC800) */
	.globl	CISH(DPMI_dummy_start)
CISH(DPMI_dummy_start):
	push	bx
	push	ax
	mov	ah,#0x62
	int	0x21			/* Get PSP */
	pop	ax
/* ======================= Addr = F800:4807 (FC807) */
	.globl	CISH(DPMI_dpmi_init)
CISH(DPMI_dpmi_init):
	hlt
	pop	bx
	retf
/* ======================= Addr = F800:480A (FC80A) */
	.globl	CISH(DPMI_return_from_dos)
CISH(DPMI_return_from_dos):
	hlt
/* ======================= Addr = F800:480B (FC80B) */
	.globl	CISH(DPMI_return_from_dos_exec)
CISH(DPMI_return_from_dos_exec):
	hlt
/* ======================= Addr = F800:480C (FC80C+intno) */
	.globl	CISH(DPMI_return_from_dosint)
CISH(DPMI_return_from_dosint):
	FILL_OPCODE(256,hlt)
/* ======================= Addr = F800:490C (FC90C) */
	.globl	CISH(DPMI_return_from_realmode)
CISH(DPMI_return_from_realmode):
	hlt
/* ======================= Addr = F800:490D (FC90D) */
	.globl	CISH(DPMI_return_from_dos_memory)
CISH(DPMI_return_from_dos_memory):
	hlt
/* ======================= Addr = F800:490E (FC90E+n_callback) */
	.globl   CISH(DPMI_realmode_callback)
CISH(DPMI_realmode_callback):
	FILL_OPCODE(0x10,hlt)
/* ======================= Addr = F800:491E (FC91E) */
	.globl	CISH(DPMI_mouse_callback)
CISH(DPMI_mouse_callback):
	hlt
	retf
/* ======================= Addr = F800:4920 (FC920) */
	.globl	CISH(DPMI_raw_mode_switch)
CISH(DPMI_raw_mode_switch):
	hlt
/* ======================= Addr = F800:4921 (FC921) */
	.globl	CISH(DPMI_save_restore)
CISH(DPMI_save_restore):
	hlt
	retf
/* ======================= Addr = F800:4923 (FC923) */
	.globl	CISH(DPMI_API_extension)
CISH(DPMI_API_extension):
	hlt
	retf
/* ======================= Addr = F800:4925 (FC925) */
	.globl	CISH(DPMI_return_from_pm)
CISH(DPMI_return_from_pm):
	hlt
/* ======================= Addr = F800:4926 (FC926) */
	.globl	CISH(DPMI_return_from_exception)
CISH(DPMI_return_from_exception):
	hlt
/* ======================= Addr = F800:4927 (FC927) */
	.globl	CISH(DPMI_return_from_rm_callback)
CISH(DPMI_return_from_rm_callback):
	hlt
/* ======================= Addr = F800:4928 (FC928) */
	.globl	CISH(DPMI_return_from_mouse_callback)
CISH(DPMI_return_from_mouse_callback):
	hlt
/* ======================= Addr = F800:4929 (FC929+except) */
	.globl	CISH(DPMI_exception)
CISH(DPMI_exception):
	FILL_OPCODE(32,hlt)
/* ======================= Addr = F800:4949 (FC949+intno) */
	.globl	CISH(DPMI_interrupt)
CISH(DPMI_interrupt):
	FILL_OPCODE(256,hlt)
/* ======================= Addr = F800:4A49 (FCA49) */
	.globl	CISH(DPMI_dummy_end)
CISH(DPMI_dummy_end):

/* COMPAS FE000-FE05A	reserved */
/* COMPAS FE05B		jmp to POST */
/* COMPAS FE05E-FE2C2	reserved */
/* COMPAS FE2C3		jmp to NMI */
/* COMPAS FE2C6-FE3FD	reserved */
/* COMPAS FE3FE		jmp to INT13 HD */
/* COMPAS FE401-FE6F0	HD parameter table */
/* COMPAS FE6F1		reserved */
/* COMPAS FE6F2		jmp to INT19 */
/* ----------------------------------------------------------------- */   
	.org	ROM_CONFIG_OFF /* for int15 */

/* ======================= Addr = FE6F5 */
	/* from Alan Cox's mods */
	/* we need somewhere for the bios equipment. */
	dw	9	/* 9 byte table */
	db	0xfc	/* PC AT */
	dw	0x401	/* bios revision 4 */
	db	0x70	/* no mca, no ebios, no wat, keybint,
			   rtc, slave 8259, no dma 3 */
	dd	0


/* COMPAS FE710-FE728	reserved */
/* COMPAS FE729		baud rate init table */
/* COMPAS FE73C-FE82D	reserved */
/* COMPAS FE82E		jmp to INT16 */
/* COMPAS FE831-FE986	reserved */
/* ----------------------------------------------------------------- */   
	.org	((INT09_SEG-BIOSSEG) << 4)+INT09_OFF
	.globl	CISH(INT09_dummy_start)
/* ======================= Addr = FE987 */
/* COMPAS FE987		jmp to INT09 */
/* COMPAS FE98A-FEC58	reserved */
	push	ax
	push	bx
	push	ds
	CSEG
	mov	ax,#BIOS_DATA
	mov	ds,ax

/* BIOS keyboard intercept */

/* get the RAW scancode (used only for int15,4f) */
	in	al,0x60
	
/*ERIC	* src/base/bios/bios.S (INT09_dummy_start): 
	removed spurious 'mov al,ah' between in al,0x60 and mov ah,#0x4f
	it was trashing the value read and would serve no useful purpose.
*/
	mov	ah,#0x4f
	stc
#if 1
		/* int 15 func 4f as per bios spec.
		 * We need however a _simulated_ INT else a DOS-space
		 * hooked INT15 would not be called,
		 * because _we_ intercept it in src/base/async/int.c
		 * As we currently don't do anything important with INT15-AH=4f,
		 * this doesn't make problems.
		 * -- Hans, June 15 1997
		 */
	SIM_INT(0x15,n_kbd_int15_return)
#else
	int	#0x15
#endif
	
	/* ignore the returned keycode, only skip the pre-translated
	   bios keycode if CF=0.
	   this is not completely accurate but hard to improve while
	   keeping a clean keyboard server design.
	 */
	jnc	kbd_done

        /* get the pre-translated bios key. */
	
	mov	al,#7
	int	DOSHELPER_INT		/* call get_bios_key helper       */
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


/* COMPAS FEC59		jmp to INT13 FDD */
/* COMPAS FEC5C-FEF56	reserved */
/* COMPAS FEF57		jmp to INT0E */
/* COMPAS FEF5A-FEFC6	reserved */
/* ----------------------------------------------------------------- */
	.org	0xefc7
/* COMPAS FEFC7		FDD param table */
/* no one uses it, but it's there for compatibility */
	db	0xaf	/* b7-4=step rate b3-0=head unload time */
	db	0x02	/* b7=1=head load time b0=0 */
	db	0x25	/* motor off delay in clock ticks */
	db	0x02	/* bytes per sector 00=128..03=1024 */
	db	18	/* sectors per track */
	db	0x1b	/* gap between sectors */
	db	0xff	/* ignored */
	db	0x6c	/* format gap length */
	db	0xf6	/* format filler byte */
	db	0x0f	/* head settle time (ms) */
	db	0x08	/* motor start time (1/8") */

/* COMPAS FEFD2		jmp to INT17 */
	.org	0xefd5
/* COMPAS FEFD5-FF064	reserved */
	db	0xdf
	db	0x02
	db	0x25
	db	0x02
	db	63
	db	0x1b
	db	0xff
	db	0x54
	db	0xf6
	db	0x0f
	db	0x08

/* ----------------------------------------------------------------- */
	.org	0xf065
/* ======================= Addr = FF065 */
/* COMPAS FF065		jmp to INT10 */
/* COMPAS FF068-FF0A3	reserved */
	/* relocated video handler (interrupt 0x42) */
#ifndef USE_NEW_INT
	int	0x42  /* this will be overwritten with iret, if no dualmon */
	retf	#2
#else /* USE_NEW_INT */
        /* Note:	 A conforming video-bios will redirect int 42 here if
          it doesn't find anything else.  Another fun suprise :	(
          I'll have to implement this in my video-bios. --EB  13 Jan 97 */
       JMPL(INT_OFF(0x42))  /* redirect this back to our magic location */
#endif /* USE_NEW_INT */

	.globl  CISH(bios_f000_endpart1)
CISH(bios_f000_endpart1):

/* COMPAS FF0A4		video param table */
/* COMPAS FF0FC-FF840	reserved */
/* COMPAS FF841		jmp to INT12 */
/* COMPAS FF844-FF84C	reserved */
/* COMPAS FF84D		jmp to INT11 */
/* COMPAS FF850-FF858	reserved */
/* COMPAS FF859		jmp to INT15 */
/* COMPAS FF85C-FFA6D	reserved */
/* COMPAS FFA6E		font tables */
/* COMPAS FFE6E		jmp to INT1A */
/* COMPAS FFE71-FFEA4	reserved */
/* COMPAS FFEA5		jmp to INT08 */
/* COMPAS FFEA8-FFEF2	reserved */
/* COMPAS FFEF3		vector table for INT08-INT1F */
/* COMPAS FFF23		vector table for INT70-INT77 */
/* COMPAS FFF33-FFF53	reserved */
/* COMPAS FFF54		jmp to INT05 */
/* COMPAS FFF57-FFFD8	reserved */
/* COMPAS FFFD9		EISA ident string */
/* COMPAS FFFDD-FFFEF	reserved */

/* ----------------------------------------------------------------- */
	.org	0xffe0

	.globl	CISH(bios_f000_part2)
CISH(bios_f000_part2):

	/* DOSEMU magic and version field */
	.ascii	"$DOSEMU$"
	.long	DOSEMU_VERSION_CODE

/* ----------------------------------------------------------------- */
	.org	0xfff0
/* COMPAS FFFF0		jmp to powerup */
	/* set up BIOS exit routine (we have *just* enough room for this) */
	mov	ax,#0xffff
	int	DOSHELPER_INT
/* COMPAS FFFF5		ROM BIOS date */
	.ascii	"02/25/93"  /* our bios date */
/* COMPAS FFFFD		unused */
	hlt
/* COMPAS FFFFE		system model ID */
	db	0xfc   /* model byte = IBM AT */
/* COMPAS FFFFF		unused */
	hlt

	.globl  CISH(bios_f000_end)
CISH(bios_f000_end):

/*--------------------------------------------------------------------------*/
