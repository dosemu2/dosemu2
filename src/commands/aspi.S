! aspi.S stub for the Linux DOS emulator - derived from ems.S
! please forgive my assembler programming style :-) ki@kretz.co.at
!
!

use16

.text
.bss
.data
.align 0

	.org	0

MaxCmd	=	15
cr	=	0xd
lf	=	0xa
eom	=	'$'		! DOS end-of-string character (barf)
Helpint =	0xe6		! 0xe6 is Int for Linux helpers
Aspihlp	=	0x41		! aspi helper

.globl _main
_main:

Header:
	.long	-1		! link to next device driver
	.word	0xC000		! attribute word for driver 
				! (char, supports IOCTL strings (it doesn't!)
	.word	Strat		! ptr to strategy routine
	.word	Intr		! ptr to interrupt service routine
	.ascii	"SCSIMGR$"	! logical-device name

! the Strat and Intr routines are entered with a "far call".  I don't
! know how to tell as86 that as I would in Turbo Assembler, so I just
! explicitly "retf" at the end of their execution.  Be careful!

RHPtr:	.long	0		! ptr to request header

InitDone: .word 0		! 1 when initialisation is complete

Dispatch:
	.word	Init		! initialize driver
	.word	Dummy		! Media Check ,block only
	.word	Dummy		! Build BPB, block only
	.word	Ioctl		! Ioctl
	.word	Dummy		! read
	.word	Dummy  		! non-destructive read
	.word	Dummy		! input status
	.word	Dummy		! flush input
	.word	Dummy		! write
	.word	Dummy		! write with verify
	.word	Dummy		! output status
	.word	Dummy		! flush output
	.word	Dummy		! IOCTL output (??)
	.word	Dummy		! open device
	.word	Dummy		! close device
	.word	Dummy		! removeable media check

Strat:
	seg cs
	mov	[RHPtr], bx
	seg cs
	mov	[RHPtr+2],es
	retf

Intr:
	push	ax
	push	bx
	push	cx
	push	dx
	push	ds
	push	es
	push	di
	push	si
	push	bp
	
	push	cs
	pop	ds

	les	di,[RHPtr]	! let es:di = request header

	seg es
	movb	bl,[di+2]
	xorb	bh,bh
	cmp	bx, #MaxCmd
	jle	Intr1
	call	Error
	jmp	Intr2

Intr1:	shl	bx,#1
	
	call	[bx+Dispatch]

	les	di,[RHPtr]

Intr2:	or	ax,#0x100		! Merge done bit with status
	seg es
	mov	[di+3],ax

	pop	bp
	pop	si
	pop	di
	pop	es
	pop	ds
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	retf

Ioctl:	
	push bx
	seg es
	mov	ax,[di+14]
	seg es
	mov 	bx,[di+16]
	push	ax
	push	bx
	pop	es
	pop	di

	mov 	ax,#DO_aspi
	seg es
	mov	[di],ax
	push	cs
	pop	ax
        seg es
	mov	[di+2],ax
	pop bx
	xor	ax,ax
	ret

DO_aspi:
	mov 	ax,#Aspihlp
	int	Helpint
	retf
Dummy:
	xor	ax,ax
	ret

Error:
	mov	ax,#0x8003
	ret

Init:
	xor	bx,bx

	push	cs
	pop	ds
	mov	al,#Aspihlp
	mov	ah,#1		! check wether there are devices configured
	int	Helpint
	cmp	ah, #1
	je	Initabort	! we have no ASPI helper
	or	ax,ax
	jnz	Initgo
Initabort:
	mov	ah, #9
	mov	dx, #OutMess
	int	0x21
	seg es
	mov	[di+14], #Header
	seg es
	mov	[di+16], cs
	jmp	Error

Initgo:
	mov	ah, #9
	mov	dx, #Message
	int	0x21

	seg es
	mov	[di+14], #Init
	seg es
	mov	[di+16], cs

	xor ax,ax

	ret


Message:.ascii	"[ASPI driver installed]"
	.byte	cr,lf,eom

OutMess:
	.byte	cr,lf,lf
	.ascii	"No ASPI devices configured."
	.byte	cr,lf
	.ascii	"Installation aborted."
	.byte	cr,lf,lf,eom

NoName:	.ascii	"NO NAME"
	.byte	0

	end
