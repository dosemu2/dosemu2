! 
! All modifications in this file to the original code are
! (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!


! ems.S for the Linux DOS emulator
!   provide a V86-mode ISR and device driver for EMS emulation
!
! License on original linux.asm:
!
! #Mach Operating System
! #Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
! #Copyright (c) 1991 IBM Corporation 
! #All Rights Reserved.
! #
! #Permission to use, copy, modify and distribute this software and its
! #documentation is hereby granted, provided that both the copyright
! #notice and this permission notice appear in all copies of the
! #software, derivative works or modified versions, and any portions
! #thereof, and that both notices appear in supporting documentation,
! #and that the nema IBM not be used in advertising or publicity 
! #pertaining to distribution of the software without specific, written
! #prior permission.
! # 
! #CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
! #CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
! #ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
! # 
! #Carnegie Mellon requests users of this software to return to
! #
! # Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
! # School of Computer Science
! # Carnegie Mellon University
! # Pittsburgh PA 15213-3890
! #
! #any improvements or extensions that they make and grant Carnegie Mellon
! #the rights to redistribute these changes.
! # 
! #MACHFS.ASM  MS-DOS device driver to interface mach file system
! #with the dos server's monitor.
! #
! #Version 1.1
! #
! #Gerald Malan (grm) 4/5/1991
!
! modified for the linux dos emulator by Andrew Tridgell 13/4/93
! translated into as86 form by Robert Sanders ('murrcan style date!) 4/13/93
! (I probably broke something, but it seems to work)
!
! copied from emufs.S to make ems.S  -  7/19/93
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
LinuxEMS =	0xe6		! 0xe6 is Int for Linux EMS server
EMSint	=	0x67
vecoff   = 	(EMSint * 4)   ! mem loc for the interrupt vector
vecseg	=	(vecoff + 2)

.globl _main
_main:

Header:
	.long	-1		! link to next device driver
	.word	0xC000		! attribute word for driver 
				! (char, supports IOCTL strings (it doesn't!)
	.word	Strat		! ptr to strategy routine
	.word	Intr		! ptr to interrupt service routine
	.ascii	"EMMXXXX0"	! logical-device name

! the Strat and Intr routines are entered with a "far call".  I don't
! know how to tell as86 that as I would in Turbo Assembler, so I just
! explicitly "retf" at the end of their execution.  Be careful!

RHPtr:	.long	0		! ptr to request header

InitDone: .word 0		! 1 when initialisation is complete

Dispatch:

	.word	Init		! initialize driver
	.word	Dummy		! Media Check ,block only
	.word	Dummy		! Build BPB, block only
	.word	Dummy		! Ioctl
	.word	Dummy		! read
	.word	Dummy  		! non-destructive read
	.word	Dummy		! input status
	.word	Dummy		! flush input
	.word	Dummy		! write
	.word	Dummy		! write with verify
	.word	Dummy		! output status
	.word	Dummy		! flush output
	.word	Dummy		! IOCTL output (??)
! if DOS 3.0 or newer...
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

Dummy:
	call MFSini
	les	di, [RHPtr]
	seg es
	mov	bl, [di+2]  ! get request number
	xor	bh,bh

	mov     al, #0x21
	int	LinuxEMS

	xor	ax,ax
	ret


Int67:
	push	ax
	mov	ax, #0x22
	int	LinuxEMS
	iret

Read:
	call MFSini
	mov	bx,#0x101
	xor	ax,ax
	ret


Write:
	call MFSini
	xor	ax,ax
	ret

Error:
	mov	ax,#0x8003
	ret

MFSini:
	push es
	push di

	push cs
	pop ds
	mov ax,[InitDone]
	cmp ax,#0
	jne AlreadyDone

	mov	ax, #0x3000
	int	0x21
	push	ax

	mov	ah, #0x52
	int	0x21

	mov	ax, #0x5d06
	int	0x21

	pop	cx
	push bx
	pop dx

	push	di
	push	es
	push	ax
	mov	ax, 0
	mov	es, ax
	mov	di, #vecoff
	seg	es
	mov	[di], #Int67
	seg	es
	mov	[di+2], cs
	pop	ax
	pop	es
	pop	di

	push cs
	pop ds
	mov ax,#1
	mov [InitDone],ax

AlreadyDone:
	pop di
	pop es
	ret


Init:
	xor	bx,bx

	cmp	ax, #0
	jne	MFSin

MFSout:
	push cs
	pop ds
	mov	ah,  #9
	mov	dx,  #OutMess
	int	0x21

	seg es 
	movb	[di+13],#0		! No units!!
	seg es
	mov	[di+20],cs
	seg es
	mov	[di+14],#0		!Break addr = cs:0000
	seg es
	mov	[di+16],cs

	ret

MFSin:
	call	MFSini

	seg es
	movb	al, [di+22]
	add	al, #0x41

	push	cs
	pop	ds

	mov	ah, #9
	mov	dx, #Mesage
	int	0x21

	seg es
	mov	[di+14], #Init
	seg es
	mov	[di+16], cs

	xor ax,ax

	ret


Mesage:	.ascii	"[dosemu EMS 4.0 driver installed]"
	.byte	cr,lf,eom

OutMess:
	.byte	cr,lf,lf
	.ascii	"EMS Server not responding."
	.byte	cr,lf
	.ascii	"Installation aborted."
	.byte	cr,lf,lf,eom

NoName:	.ascii	"NO NAME"
	.byte	0

	end
