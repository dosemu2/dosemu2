/* 
 * (C) Copyright 1992, ..., 2001 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*************************************************************************/
/**                                                                     **/
/** This is the virtual PC's Bios (F000:0 .. F000:FFFF)                 **/
/**                                                                     **/
/** We must compile this with gcc/as/ld as follows:	                **/
/**       gcc -nostdlib -Wl,-Ttext,0,-e,0 -traditional bios.S -o bios.o **/
/**       strip -N _edata -N __bss_start -N _end bios.o                 **/
/** modified from as86 to gas by Bart Oldeman Jan 2001			**/
/**                                                                     **/
/*************************************************************************/

#define __ASM__
#include "config.h"
#include "memory.h"
#include "macros86.h"

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

.code16
.text
	.globl	CISH(bios_f000)
CISH(bios_f000):
	
			/* this sets up all 256 intvector stubs 
			   each starting at an address aligned to 16 byte */

			/* first 8 ints */
irqnum  =	0
	.org	irqnum*16
.REPT 8 
	.align 16,0
.ifeq	irqnum-3
	.byte	0xcd,3
.ELSE
	int	$irqnum
.ENDIF
	lret	$2
irqnum	=	irqnum+1
.ENDR

			/* hardware IRQ 0..7 (master PIC) */
irqnum	=	8
	.org	irqnum*16
.REPT 8
	.align 16,0
	int	$irqnum
	iret
irqnum	=	irqnum+1
.ENDR

			/* system-soft ints 0x10 .. 0x12 */
irqnum	=	0x10
	.org	irqnum*16
.REPT (0x13-0x10) 
	.align 16,0
	int	$irqnum
	iret
irqnum	=	irqnum+1
.ENDR


			/* system-soft ints 0x13 .. 0x16 */
irqnum	=	0x13
	.org	irqnum*16
.REPT (0x17-0x13) 
	.align 16,0
	int	$irqnum
	lret	$2
irqnum	=	irqnum+1
.ENDR


			/* system-soft ints 0x17 .. 0x1b */
irqnum	=	0x17
	.org	irqnum*16
.REPT (0x1c-0x17) 
	.align 16,0
	int	$irqnum
	iret
irqnum	=	irqnum+1
.ENDR


			/* user timer tick, should be an IRET */
irqnum	=	0x1c
	.org	irqnum*16
#ifdef USE_NEW_INT
	int	$irqnum
#endif /* USE_NEW_INT */
	iret


			/* system-soft ints 0x1d .. 0x44 */
irqnum	=	0x1d
	.org	irqnum*16
.REPT (0x45-0x1d) 
	.align 16,0
	int	$irqnum
	lret	$2
irqnum	=	irqnum+1
.ENDR

			/* soft ints 0x45 .. 0x5f */
irqnum	=	0x45
	.org	irqnum*16
.REPT (0x60-0x45) 
	.align 16,0
	iret
irqnum	=	irqnum+1
.ENDR

irqnum	=	0x60
	.org	irqnum*16
int_pktdrv:
	int	$irqnum
	lret	$2

/* ??? Where are 0x61-0x67 ??? */
		
			/* soft ints 0x68 .. 0x6f */
irqnum	=	0x68
	.org	irqnum*16
.REPT (0x70-0x68) 
	.align 16,0
        iret
irqnum	=	irqnum+1
.ENDR


			/*  hardware IRQ 0x70..0x77 (slave PIC) */
irqnum	=	0x70
	.org	irqnum*16
.REPT (0x78-0x70) 
	.align 16,0
	int	$irqnum
	lret	$2	/* NOTE: this should be "iret", isn't it?
				 NEED TO BE CHECKED ! */
irqnum	=	irqnum+1
.ENDR


			/* soft ints 0x78 .. 0xff */
irqnum	=	0x78
	.org	irqnum*16
.REPT (0x100-0x78) 
	.align 16,0
	int	$irqnum
	lret	$2
irqnum	=	irqnum+1
.ENDR

/* ----------------------------------------------------------------- */
	/* this is the mouse handler */
	.org	((Mouse_SEG - BIOSSEG) << 4) + Mouse_OFF
/* ======================= Addr = F000:20F0 (F20F0) */
   
	/* 
	mouse routine simulates the stack frame of an int, then does a 
	"pusha" before here...so we just "popa; iret" to get back out
	*/

	lcall	INDIRECT_PTR(mouse_handler)
	popw	%es
	popw	%ds
	popa
	iret
	.org	((Mouse_SEG - BIOSSEG) << 4)+Mouse_OFF+8
mouse_handler: 
	.long	0x1c810227
	.org	((Mouse_SEG - BIOSSEG) << 4)+Mouse_ROUTINE_OFF
	pushw	%ax
        movb    $0x20,%al
        outb    %al,$0x20		/* flag interrupt complete	*/
	popw	%ax
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
	jmp int_pktdrv

/* The packet driver signature will be written here when the packet
   driver is initialized. */
CISH(PKTDRV_signature):
		.byte	0,0,0,0,0,0,0,0,0

CISH(PKTDRV_driver_name):
	.asciz	"Linux$"
	.byte	0
CISH(PKTDRV_size):	.word	0
CISH(PKTDRV_handle):	.word	0
CISH(PKTDRV_receiver): .long	0

	/* parameter packet, filled in by pkt_init */
CISH(PKTDRV_param):
	.byte	0,0,0,0
	.word	0,0,0,0,0
	/* driver statistics structure */
CISH(PKTDRV_stats):
	.long	0,0,0,0,0,0,0

	/* helper routine to transfer received packets and call receiver */
CISH(PKTDRV_helper):
	pusha
	pushw	%ds
	pushw	%es
	movw	%cs:PKTDRV_size,%cx
	jcxz	PKTDRV_nothing
	xorw	%ax,%ax
	movw	%cs:PKTDRV_handle,%bx
	pushw	%cx
	pushw	%bx
	lcall	INDIRECT_PTR(%cs:PKTDRV_receiver)
	popw	%bx
	popw	%cx
	movw	%es,%ax
	orw	%di,%ax
	jz	PKTDRV_norecv
	pushw	%cs
	popw	%ds
	movw	$PKTDRV_buf,%si
	pushw	%di
	pushw	%cx
	cld
	shrw	%cx
	rep;	movsw
	jnc	PKTDRV_nobyte
	movsb
PKTDRV_nobyte:
	popw	%cx
	popw	%si
	pushw	%es
	popw	%ds
	movw	$1,%ax
	lcall	INDIRECT_PTR(%cs:PKTDRV_receiver)
PKTDRV_norecv:
	movw	$0,%cs:PKTDRV_size
PKTDRV_nothing:
	popw	%es
	popw	%ds
	popa
	iret

	.align	16,0
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
	movw	%cs:DBGload_SSSP+2,%ss
	movw	%cs:DBGload_SSSP,%sp
	mov	$0x62,%ah	/* we must get the PSP of the loaded program */
	int	$0x21
        movw	%bx,%es
	movw    %bx,%ds
	xorw    %ax,%ax
	movw    %ax,%bx
	movw    %ax,%cx
	movw    %ax,%dx
	movw    %ax,%si
	movw    %ax,%di
	movw    %ax,%bp
	sti
	ljmp	INDIRECT_PTR(%cs:CISH(DBGload_CSIP))
				/* and give control to the program */


	/* this is the paramblock, we told DOS to use for INT21 AX=4B01 */
	.align	16,0
CISH(DBGload_parblock):	.word	0
			.long	0,0,0
DBGload_SSSP:		.long	0
CISH(DBGload_CSIP):	.long	0

/* ----------------------------------------------------------------- */


	/* XMS has it's handler just after the interrupt dummy segment */
	.org	((XMSControl_SEG - BIOSSEG) << 4) + XMSControl_OFF
/* ======================= Addr = F800:0000 (F8000) */
	jmp	(.+2+3)	/* jmp short forward 3 */
	nop
	nop
	nop
	hlt		/* HLT...the current emulator trap */
	.byte	INT2F_XMS_MAGIC /* just an info byte. reserved for later */
	lret

/* ----------------------------------------------------------------- */
	.org	((Banner_SEG - BIOSSEG) << 4) + Banner_OFF
/* ======================= Addr = F800:2000 (FA000) */

	movb	$5,%al
	int	$DOSHELPER_INT
	.byte	0xb2          /* mov dl,#0   or mov dl,#0x80 */
	.globl	CISH(bios_f000_bootdrive)
CISH(bios_f000_bootdrive):
	.byte	0
	lret


/* ----------------------------------------------------------------- */
	.org	((IPX_SEG - BIOSSEG) << 4) + IPX_OFF
/* ======================= Addr = F800:3100 (FB100) */
ipx_handler:
	pushw	%ax	/* FarCallHandler removes this before returning */
	movb	$0x7a,%al
	int	$DOSHELPER_INT
	lret

	.globl	CISH(bios_IPX_FarCall)
CISH(bios_IPX_FarCall):
	pushw	%bx
	movw	$0x0a,%bx
	lcall   $IPX_SEG, $IPX_OFF
	/*.byte	0x9a		Official JES hack for call far ipx_handler */
	/*.long	0xf8003100	### This is address of bios_IPX_FarCall */
        /* Of course I can only comment out an official hack, never remove - Bart */
	popw	%bx
	lret

	.globl	CISH(bios_IPX_PopRegistersReturn)
CISH(bios_IPX_PopRegistersReturn):
	popw	%es
	popw	%ds
	popw	%bp
	popw	%di
	popw	%si
	popw	%dx
	popw	%cx
	popw	%bx
	movb	$0x7b,%al	/* Allow IPX to know ESR completed */
	int	$DOSHELPER_INT
	popw	%ax
	lret
	.globl	CISH(bios_IPX_PopRegistersIRet)
CISH(bios_IPX_PopRegistersIRet):
	popw	%es
	popw	%ds
	popw	%bp
	popw	%di
	popw	%si
	popw	%dx
	popw	%cx
	popw	%bx
	movb	$0x7b,%al
	int	$DOSHELPER_INT
	popw	%ax
	iret


/* ----------------------------------------------------------------- */

	.org	((INT16_SEG-BIOSSEG) << 4)+INT16_OFF
/* ======================= Addr = F800:3500 (FB000) */

	.globl	CISH(INT16_dummy_start)
CISH(INT16_dummy_start):			/* KEYBOARD BIOS ROUTINE        */
	pushw	%dx		/* dx needed for extended calls */
	pushw   %ds			/* save ds and bx               */
	pushw   %bx		/* next 2 lines set ds to bios data seg	*/
	movw    $BIOS_DATA,%bx
	movw	%bx,%ds
	movb    $0x00,%dl        /* clear mask for extended calls */

	cmpb	$0,%ah
	je	INT16_0			/* read key */
	cmpb	$1,%ah
	je	INT16_1			/* check key available */
	cmpb	$2,%ah
	je	INT16_2			/* get shift flags */
#if 0
	cmpb	$3,%ah
	je	INT16_3			/* set typematic rate */
	cmpb	$4,%ah
	je	INT16_4			/* adjust keyclick */
#endif
	cmpb	$5,%ah
	je	INT16_5			/* store key in buffer */
	notb	%dl				/* set mask for extended calls */
	cmpb	$0x10,%ah
	je	INT16_10		/* read key extended */
	cmpb	$0x11,%ah
	je	INT16_11		/* check key extended */
	cmpb	$0x12,%ah
	je	INT16_12		/* get shift flags extended */

INT16_ex:
	popw    %bx			/* do nothing and return	*/
	popw    %ds
	popw	%dx
	iret   

INT16_0_idle:
	sti
	pushw	%ax			/* buffer empty - 		*/
	movw    $0x1680,%ax		/* magic machine idle function  */
	int     $0x2f
	popw	%ax

INT16_0:				/* 0 => read keyboard */
INT16_10:				/* 0x10 => extended read        */
	cli				/* READ KEYBOARD		*/
	call get_key			/* get last key */
	je      INT16_0_idle		/* jump if empty		*/
	incw    %bx			/* advance buffer pointer 2 bytes*/
	incw    %bx
 	cmpw	KEYBUFFER_END,%bx	/* check for wrap around        */
	jne	INT16_0_nowrap		/* no wrap - go return to caller*/
	movw	KEYBUFFER_START,%bx	/* wrap - get buffer start	*/
INT16_0_nowrap:
	movw    %bx,KEYBUF_READ_PTR	/* save it as new pointer	*/
	sti
	jmp	INT16_ex		/* return to caller		*/

INT16_1:				/* 0x01 => check for key        */
INT16_11:				/* 0x11 => extended check       */
	cli
	call	get_key			/* READ KEYBOARD		*/
	sti
	popw    %bx			/* return to caller		*/
	popw    %ds
	popw	%dx
	lret    $2		/* can't do iret, it would change flags	*/

INT16_2:				/* RETURN SHIFT FLAG STATUS     */
INT16_12:
	movw	KEYSHIFT_FLAGS,%ax	/* get shift flags (ah=extended)*/
	movb	%ah,%bl			/* get a copy to play with	*/
	andb	$0x73,%ah		/* disallow bits 2, 3, & 7 	*/
	andb	$0x04,%bl		/* clear everything but sysreq  */
	shlb	$5,%bl			/* move it to the correct bit   */
	orb	%bl,%ah			/* set sysreq			*/
	movb	KEYBOARD_STATUS_3,%bh	/* bits 2 & 3 come from here	*/
	andb    $0xc,%bh		/* isolate those bits		*/
	orb     %bh,%ah			/* merge them with flags in ax	*/
	jmp	INT16_ex		/* return to caller		*/

INT16_5:				/* STORE KEY INTO BUFFER	*/
	movw	%cx,%ax
	cli
	call	store_key
	sti
	jc	INT16_5_full
	movb	$0,%al			/* return al=0 (success)	*/
	jmp	INT16_ex
INT16_5_full:
	movb	$1,%al			/* return al=1 (buffer full)	*/
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
	movw	KEYBUF_READ_PTR, %bx	/* get address of next char	*/
get_key_restart:	
	cmpw	KEYBUF_WRITE_PTR, %bx	/* set flag if buffer empty	*/
	je	get_key_ret		/* buffer empty			*/
	movw	(%bx), %ax		/* put char into ax		*/
	call	do_extended		/* differences for extended calls */
	jne	get_key_ret
	incw	%bx
	incw	%bx
	cmpw	KEYBUFFER_END,%bx	/* check for wrap around	*/
	jne	get_key_nowrap
	movw	KEYBUFFER_START,%bx	/* wrap - get buffer start	*/
get_key_nowrap:	
	movw	%bx,KEYBUF_READ_PTR	/* save it as new pointer	*/
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
	movb	$0xab,%dh		/* Move a value not equal to dl into dh */
	cmpb	$0,%dl			/* Ensure we actually need to do something */
	jne	do_extended_done
	cmpb	$0xe0,%al		/* is character 0xE0 ? */
	jne	do_extended_alt		/* no, continue */
	andb	%dl,%al			/* yes, mask with 0xFF or 0x00 */
	jmp	do_extended_scan
do_extended_alt:			/* Not always alt keys but nearly */
	cmpb	$0x00,%al
	jne	do_extended_slash
	cmpb	$0x01,%ah
	je	do_extended_hide
	cmpb	$0x0e,%ah
	je	do_extended_hide
	cmpb	$0x1a,%ah
	jb	do_extended_slash
	cmpb	$0x1c,%ah
	jbe	do_extended_hide
	cmpb	$0x27,%ah
	jb	do_extended_slash
	cmpb	$0x29,%ah
	jbe	do_extended_hide
	cmpb	$0x2b,%ah
	je	do_extended_hide
	cmpb	$0x33,%ah
	jb	do_extended_slash
	cmpb	$0x35,%ah
	jbe	do_extended_hide
	cmpb	$0x37,%ah
	je	do_extended_hide
	cmpb	$0x4a,%ah
	je	do_extended_hide
	cmpb	$0x4c,%ah
	je	do_extended_hide
	cmpb	$0x4e,%ah
	je	do_extended_hide
do_extended_slash:			/* Handle special cases */
	cmpw	$0xe02f,%ax		/* KEY_PAD_SLASH */
	jne	do_extended_enter
	movb	$0x35, %ah
	jmp	do_extended_done
do_extended_enter:
	cmpw	$0xe00d, %ax		/* KEY_PAD_ENTER */
	jne	do_extended_newline
	movb	$0x1c, %ah
	jmp	do_extended_done
do_extended_newline:
	cmpw	$0xe00a, %ax		/* CTRL KEY_PAD_ENTER */
	jne	do_extended_scan
	movb	$0x1c, %ah
	jmp	do_extended_done
do_extended_scan:			/* Hide new scan codes */
	cmpb	$0x85,%ah
	jb	do_extended_done
do_extended_hide:			/* Hide the rest of the extended keys */
	movb	$0x00,%dh		/* return with zero flag set */
do_extended_done:
	cmpb	%dl,%dh			/* return zero flag clear, normally */
	ret

/* SUBROUTINE: store keycode in AX in the BIOS keyboard buffer, 
 * return CF=1 if buffer is full.
 */
store_key:
	movw	KEYBUF_WRITE_PTR,%bx
	incw	%bx
	incw	%bx
	cmpw	KEYBUFFER_END,%bx
	jne	no_wrap
	movw	KEYBUFFER_START,%bx
no_wrap:
	cmpw	KEYBUF_READ_PTR,%bx
	stc
	je	keybuffer_full
	xchgw	KEYBUF_WRITE_PTR,%bx	/* ok, update write pointer	*/
	movw	%ax,(%bx)		/* and store key in buffer	*/
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
	int	$0x1c	
#endif
/* NOTE: The above int 0x1c is a compatibility fault, because
 *       the original IBM Bios calls the user *after* the timer is
 *       increased. So, I moved it down.
 *       THIS NEEDS TO BE CHECKED for side effects in dosemu !
 */
	pushw	%ds
	pushw	%ax
	xorw	%ax, %ax		/* set ax to segment 0		*/
        movw	%ax,%ds
	incl	BIOS_TIMER
	cmpw	$24, BIOS_TIMER+2	/* 24 hour check */
	jb	CISH(INT08_L1)
	cmpw	$HOUR24_ADJUST, BIOS_TIMER
	jb	CISH(INT08_L1)
	movl	$0, BIOS_TIMER
	movb	$1, BIOS_TIMER_OVERFLOW
CISH(INT08_L1):
			/* emulate 'diskette motor running */
			/* some old games rely on that --SW, --Hans */
	cmpb	$0, DISKETTE_MOTOR_TIMEOUT
	jz	CISH(INT08_L2)
	decb	DISKETTE_MOTOR_TIMEOUT
CISH(INT08_L2):
	int	$0x1c		/* call int 0x1c, per bios spec	*/
				/* must do it before EOI, but after count */
        movb    $0x20,%al
        outb    %al,$0x20		/* flag interrupt complete	*/
	popw	%ax			/* restore registers            */
	popw	%ds
	iret				/* return to interrupted code	*/
	.globl  CISH(INT08_dummy_end)
CISH(INT08_dummy_end):

/* ----------------------------------------------------------------- */   
	.org	((INT70_SEG-BIOSSEG) << 4)+INT70_OFF

	.globl	CISH(INT70_dummy_start)
/* ======================= Addr = F800:4100 (FC100) */
CISH(INT70_dummy_start):		/* RTC INTERRUPT ROUTINE	*/

	pushw	%ax
	int	$0x4a
        movb    $0x20,%al
        outb    %al,$0xa0		/* flag interrupt complete	*/
	outb	%al,$0x20
	popw	%ax			/* restore registers            */
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
	pushw	%ax
	sti
	movb	$8,%al		/*  Start Video init */
	int	$DOSHELPER_INT
	.byte	0x9a   /* call far 0xc000:3  or call far 0xe000:3 */
/* More general than just c000 or e000 ??? */	
	.globl	CISH(bios_f000_int10ptr)
CISH(bios_f000_int10ptr):
	.long	0xc0000003
	movb	$9,%al		/* Finished video init */
	int     $DOSHELPER_INT
	popw	%ax
	sti
	lret


/* This is installed after video init (helper fcn 0x9) when the internal
	mouse driver is in use.  It watches for mouse set commands and
	resets the mouse driver when it sees one. */
/* Was: Comments and bugs to David Etherton, etherton@netcom.com
 * Current Maintainer:	Eric W. Biederman <eric@biederman.org>
 */	

	.org	((INT10_WATCHER_SEG-BIOSSEG) << 4)+INT10_WATCHER_OFF
/* ======================= Addr = F800:4240 (FC240) */
WINT10:
	or	%ah,%ah
	jz	L9	/* normal mode set */
	cmpb	$0x11,%ah
	je	L9	/* character generator, possibly resize the screen */
	cmpw	$0x4F02,%ax
	jne	L10	/* svga mode set */
L9:
	pushw 	%ax	/* save everything */
	pushw	%bx
	movw	$0x33,%ax	/* mouse helper */
	movw	$0xf0,%bx	/* start video mode set */
	int	$DOSHELPER_INT
	popw	%bx
	popw	%ax

/* fake stack frame for iret: push original flags and avoid a GPF from pushf */
	movzwl	%sp,%esp	/* make sure high of esp is zero */
	CODE32  pushw	4(%esp)
	pushw	%cs
	call	L10	/* perform the actual mode set */

/* since following code doesn't affect flags, we keep current values */
	pushw	%ax	/* remember everything from int10 call */
	pushw	%bx
	movw	$0x33,%ax	/* mouse helper */
	movw	$0xf1,%bx	/* end video mode set */
	int	$DOSHELPER_INT
	popw	%bx
	popw	%ax
	lret	$2    /* keep current flags and avoid another GPF from iret */

L10:	/* chain to original handler (probably the video bios) */
	.byte 	0xea /* jump far */
	.globl	CISH(bios_f000_int10_old)
CISH(bios_f000_int10_old):
	.word	0,0

#ifdef X86_EMULATOR
	.org	((INT10_WATCHER_SEG-BIOSSEG) << 4)+(INT10_WATCHER_OFF+0x60)
/* ======================= Addr = F800:42A0 (FC2A0) */
/* the reason for this trick is that when SKIP_EMU_VBIOS is active we
 * switch to VBIOS into _true_ vm86 mode, and the iret is trapped by
 * the kernel, not the CPU emulator. Here we double the return stack
 * and fall back into an HLT at the end of the video code. Since cs==
 * f800, cpuemu gets control back at the right point -- AV
 */
/* fake stack frame for iret: push original flags and avoid a GPF from pushf */
	movzwl	%sp,%esp	/* make sure high of esp is zero */
	CODE32  pushw	4(%esp)
	pushw	%cs
	call	WINT10
	hlt
	lret	$2    /* keep current flags and avoid another GPF from iret */
#endif

/* ----------------------------------------------------------------- */   
		/* This is an int e7 used for FCB opens */
	.org	((INTE7_SEG-BIOSSEG) << 4)+INTE7_OFF
/* ======================= Addr = F800:4500 (FC500) */
	pushw	%es
	pushw	%di
	pushw	%ax
	movw	$0x120c,%ax
	int	$0x2f
	popw	%ax
	popw	%di
	popw	%es
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
	pushw	%bx
	pushw	%ax
	movb	$0x62,%ah
	int	$0x21			/* Get PSP */
	popw	%ax
/* ======================= Addr = F800:4807 (FC807) */
	.globl	CISH(DPMI_dpmi_init)
CISH(DPMI_dpmi_init):
	hlt
	popw	%bx
	lret
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
	FILL_OPCODE 256,hlt
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
	FILL_OPCODE 0x10,hlt
/* ======================= Addr = F800:491E (FC91E) */
	.globl	CISH(DPMI_mouse_callback)
CISH(DPMI_mouse_callback):
	hlt
	lret
/* ======================= Addr = F800:4920 (FC920) */
	.globl	CISH(DPMI_raw_mode_switch)
CISH(DPMI_raw_mode_switch):
	hlt
/* ======================= Addr = F800:4921 (FC921) */
	.globl	CISH(DPMI_save_restore)
CISH(DPMI_save_restore):
	hlt
	lret
/* ======================= Addr = F800:4923 (FC923) */
	.globl	CISH(DPMI_API_extension)
CISH(DPMI_API_extension):
	hlt
	lret
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
	FILL_OPCODE 32,hlt
/* ======================= Addr = F800:4949 (FC949+intno) */
	.globl	CISH(DPMI_interrupt)
CISH(DPMI_interrupt):
	FILL_OPCODE 256,hlt
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
	.word	9	/* 9 byte table */
	.byte	0xfc	/* PC AT */
	.word	0x401	/* bios revision 4 */
	.byte	0x70	/* no mca, no ebios, no wat, keybint,
			   rtc, slave 8259, no dma 3 */
	.long	0


/* COMPAS FE710-FE728	reserved */
/* COMPAS FE729		baud rate init table */
/* COMPAS FE73C-FE82D	reserved */
/* COMPAS FE82E		jmp to INT16 */
/* COMPAS FE831-FE986	reserved */
/* ----------------------------------------------------------------- */   
	.org	((INT09_SEG-BIOSSEG) << 4)+INT09_OFF
#if 0
	.globl	CISH(INT09_dummy_start)
#endif
/* ======================= Addr = FE987 */
/* COMPAS FE987		jmp to INT09 */
/* COMPAS FE98A-FEC58	reserved */
	pushw	%ax
	pushw	%bx
	pushw	%ds
	movw	$BIOS_DATA,%ax
	movw	%ax,%ds

/* BIOS keyboard intercept */

/* get the RAW scancode (used only for int15,4f) */
	inb	$0x60,%al
	
/*ERIC	* src/base/bios/bios.S (INT09_dummy_start): 
	removed spurious 'mov al,ah' between in al,0x60 and mov ah,$0x4f
	it was trashing the value read and would serve no useful purpose.
*/
	movb	$0x4f,%ah
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
	SIM_INT 0x15, n_kbd_int15_return
#else
	int	$0x15
#endif
	
	/* ignore the returned keycode, only skip the pre-translated
	   bios keycode if CF=0.
	   this is not completely accurate but hard to improve while
	   keeping a clean keyboard server design.
	 */
	jnc	kbd_done

        /* get the pre-translated bios key. */
	
	movb	%al,%ah			/* pass the scancode... */
	movb	$7,%al
	int	$DOSHELPER_INT		/* call get_bios_key helper       */
					/* returns ax=keycode or 0,       */
					/* also copies new shift state to */
					/* seg 0x40                       */

	testw	%ax,%ax
	jz	kbd_done		/* no keycode returned */

/* check for "special action" codes
 */
	cmpw	$SP_PAUSE, %ax
	je	kbd_do_pause
	cmpw	$SP_BREAK, %ax
	je	kbd_do_break
	cmpw	$SP_PRTSCR, %ax
	je	kbd_do_prtscr
	cmpw	$SP_SYSRQ_MAKE, %ax
	je	kbd_do_sysrq_make
	cmpw	$SP_SYSRQ_BREAK, %ax
	je	kbd_do_sysrq_break

	call	store_key

kbd_done:
	movb   	$0x20,%al
	outb    %al,$0x20		/* tell pic we're done 		*/
	popw	%ds
	popw	%bx
	popw	%ax			/* restore registers		*/
	iret

kbd_do_pause:
	jmp	kbd_done

kbd_do_break:				/* CTRL-BREAK pressed		*/
	xorw	%ax,%ax
	call	store_key		/* put null word into buffer	*/
	int	$0x1b			/* call BREAK interrupt		*/
	jmp	kbd_done

kbd_do_prtscr:				/* PRINT SCREEN pressed		*/
	int	$0x05
	jmp	kbd_done

kbd_do_sysrq_make:			/* Alt-SYSRQ pressed		*/
	movw	0x8500,%ax
	int	$0x15
	jmp	kbd_done

kbd_do_sysrq_break:			/* ALT-SYSRQ released		*/
	movw	0x8501,%ax
	int	$0x15
	jmp	kbd_done

#if 0
	.globl  CISH(INT09_dummy_end)
CISH(INT09_dummy_end):
#endif


/* COMPAS FEC59		jmp to INT13 FDD */
/* COMPAS FEC5C-FEF56	reserved */
/* COMPAS FEF57		jmp to INT0E */
/* COMPAS FEF5A-FEFC6	reserved */
/* ----------------------------------------------------------------- */
	.org	0xefc7
/* COMPAS FEFC7		FDD param table */
/* no one uses it, but it's there for compatibility */
	.byte	0xaf	/* b7-4=step rate b3-0=head unload time */
	.byte	0x02	/* b7=1=head load time b0=0 */
	.byte	0x25	/* motor off delay in clock ticks */
	.byte	0x02	/* bytes per sector 00=128..03=1024 */
	.byte	18	/* sectors per track */
	.byte	0x1b	/* gap between sectors */
	.byte	0xff	/* ignored */
	.byte	0x6c	/* format gap length */
	.byte	0xf6	/* format filler byte */
	.byte	0x0f	/* head settle time (ms) */
	.byte	0x08	/* motor start time (1/8") */

/* COMPAS FEFD2		jmp to INT17 */
	.org	0xefd5
/* COMPAS FEFD5-FF064	reserved */
	.byte	0xdf
	.byte	0x02
	.byte	0x25
	.byte	0x02
	.byte	63
	.byte	0x1b
	.byte	0xff
	.byte	0x54
	.byte	0xf6
	.byte	0x0f
	.byte	0x08

/* ----------------------------------------------------------------- */
	.org	0xf065
/* ======================= Addr = FF065 */
/* COMPAS FF065		jmp to INT10 */
/* COMPAS FF068-FF0A3	reserved */
	/* relocated video handler (interrupt 0x42) */
#ifndef USE_NEW_INT
	int	$0x42  /* this will be overwritten with iret, if no dualmon */
	lret	$2
#else /* USE_NEW_INT */
        /* Note:	 A conforming video-bios will redirect int 42 here if
          it doesn't find anything else.  Another fun suprise :	(
          I'll have to implement this in my video-bios. --EB  13 Jan 97 */
       jmp bios_f000+INT_OFF(0x42) /* redirect this back to our magic location */
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
	movw	$0xffff,%ax
	int	$DOSHELPER_INT
/* COMPAS FFFF5		ROM BIOS date */
	.ascii	"02/25/93"  /* our bios date */
/* COMPAS FFFFD		unused */
	hlt
/* COMPAS FFFFE		system model ID */
	.byte	0xfc   /* model byte = IBM AT */
/* COMPAS FFFFF		unused */
	hlt

	.globl  CISH(bios_f000_end)
CISH(bios_f000_end):

/*--------------------------------------------------------------------------*/
