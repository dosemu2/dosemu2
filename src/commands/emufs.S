! 
! All modifications in this file to the original code are
! (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!


!
! 
! Mach Operating System
! Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
! Copyright (c) 1991 IBM Corporation 
! All Rights Reserved.
! 
! Permission to use, copy, modify and distribute this software and its
! documentation is hereby granted, provided that both the copyright
! notice and this permission notice appear in all copies of the
! software, derivative works or modified versions, and any portions
! thereof, and that both notices appear in supporting documentation,
! and that the nema IBM not be used in advertising or publicity 
! pertaining to distribution of the software without specific, written
! prior permission.
! 
! CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
! CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
! ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
! 
! Carnegie Mellon requests users of this software to return to
! 
!  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
!  School of Computer Science
!  Carnegie Mellon University
!  Pittsburgh PA 15213-3890
! 
! any improvements or extensions that they make and grant Carnegie Mellon
! the rights to redistribute these changes.
! 
! MACHFS.ASM  MS-DOS device driver to interface mach file system
! with the dos server's monitor.
!
! Version 1.1
!
! Gerald Malan (grm) 4/5/1991
!
! modified for the linux dos emulator by Andrew Tridgell 13/4/93
! translated into as86 form by Robert Sanders ('murrcan style date!) 4/13/93
! (I probably broke something, but it seems to work)
!

use16

.text
.bss
.data
.align 0

	.org	0

MaxCmd	=	12
cr	=	0xd
lf	=	0xa
eom	=	'$'		! DOS end-of-string character (barf)
Linuxfs	=	0xe6		! 0xe6 is Int for Mach Dos Server
Mivec   = 	(Linuxfs * 4)   ! mem loc for the interrupt vector

.globl _main
_main:

Header:
	.long	-1		! link to next device driver
	.word	0		! attribute word for driver
	.word	Strat		! ptr to strategy routine
	.word	Intr		! ptr to interrupt service routine
	.ascii	"LINUX   "	! logical-device name

! the Strat and Intr routines are entered with a "far call".  I don't
! know how to tell as86 that asI would in Turbo Assembler, so Ijust
! explicitly "retf" at the end of their execution.  careful!

				! BPB from horse.mach.cs.cmu.edu
Bpb:	.word	0x200		! Bytes per Sector
	.byte	4		! Sectors per allocation unit
	.word	1		! Number of reserved sectors
	.byte	2		! Number of FATS
	.word	0x200		! Max number of root entries
	.word	0xac21		! Total number of sectors
	.byte	0xf8		! Media Desc Byte
	.word	0x2b		! Num sec per FAT
	.word	0x23		! Sectors per track
	.word	0xf		! Number of heads
	.word	0		! Number of hidden sectors
	.word	0		! High order word of hidden sectors
	.long	0		! zero
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0

BPBptr:	.word	Bpb

RHPtr:	.long	0		! ptr to request header

InitDone: .word 0		! 1 when initialisation is complete

Dispatch:

	.word	Init		! initialize driver
	.word	MediaChk
	.word	BuildBpb
	.word	Dummy
	.word	Read		! read
	.word	Dummy
	.word	Dummy
	.word	Dummy
	.word	Write		! write
	.word	Write		! write with verify
	.word	Dummy
	.word	Dummy
	.word	Dummy

Strat:
! was...
!	mov	word ptr cs:[RHPtr],bx
!	mov	word ptr cs:[RHPtr+2],es
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
	mov	bl, [di+2]
	mov	bh, #3

	mov     ax, #0x20
	int	Linuxfs

	xor	ax,ax
	ret


MediaChk:
	call MFSini
! was...
!	mov	byte ptr es:[di+14],#-1	! disk has changed
!	mov	word ptr es:[di+15], offset NoName
!	mov	word ptr es:[di+17], cs	
	seg es
	movb	[di+14],#-1	! disk has changed
	seg es
	mov	[di+15], #NoName
	seg es
	mov	[di+17], cs	
	xor	ax,ax
	ret


BuildBpb:
	call MFSini
! was...
!	mov	word ptr es:[di+18],offset Bpb
!	mov	word ptr es:[di+20],cs
	seg es
	mov	[di+18], #Bpb
	seg es
	mov	[di+20], cs
	xor	ax,ax
	ret


Read:
	call MFSini
	mov	bx,#0x101
	mov ax,#0x20
	int	Linuxfs
	xor	ax,ax
	ret


Write:
	call MFSini
	mov	bx,#0x202
	mov ax,#0x20
	int	Linuxfs
	xor	ax,ax
	ret


Error:
	mov	ax,#0x8003
	ret


MyIret:
	xor	ax,ax
	iret


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
	mov	bx, #0x500
	mov     ax,#0x20
	int	Linuxfs	

	push cs
	pop ds
	mov ax,#1
	mov [InitDone],ax

AlreadyDone:
	pop di
	pop es
	ret


Init:
	xor bx,bx
	push	si
	mov	si, #DirName
	movb	[si], #0x24
	mov     ax, #0x20
	int	Linuxfs
	pop	si

	cmp	ax, #0
	jne	MFSin

MFSout:
	push cs
	pop ds
	mov	ah,  #9
	mov	dx,  #OutMess
	int	0x21

! was...
!	mov	byte ptr es:[di+13],#0		! No units!!
!	mov	word ptr es:[di+18],offset BPBptr
!	mov	word ptr es:[di+20],cs
!	mov	word ptr es:[di+14],#0		!Break addr = cs:0000
!	mov	word ptr es:[di+16],cs

	seg es 
	movb	[di+13],#0		! No units!!
	seg es
	mov	[di+18], #BPBptr
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

	push di
	push cs
	pop	ds
	mov	di,  #Mesage1
	movb	[di],al

	mov	ah, #9
	mov	dx, #Mesage
	int	0x21
	pop di

! was...
!	mov	byte ptr es:[di+13],#1		! Number of units
!	mov	word ptr es:[di+18],offset BPBptr
!	mov	word ptr es:[di+20],cs
!	mov	word ptr es:[di+14],offset Init
!	mov	word ptr es:[di+16],cs

! this is part of diff9
!	seg es
!	movb	[di+13],#1		! Number of units

	seg es
	mov	[di+18], #BPBptr
	seg es
	mov	[di+20],cs
	seg es
	mov	[di+14], #Init
	seg es
	mov	[di+16],cs

	xor ax,ax

	ret


Mesage:	.ascii	"[Host File System] drive "
Mesage1:
	.ascii	"A: is directory "
DirName:
	.space	128
	.byte	cr,lf,eom

OutMess:
	.byte	cr,lf,lf
	.ascii	"Dosemu Server not responding."
	.byte	cr,lf
	.ascii	"Installation aborted."
	.byte	cr,lf,lf,eom

NoName:	.ascii	"NO NAME"
	.byte	0

	end
