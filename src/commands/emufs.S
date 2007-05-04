# 
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


#
# 
# Mach Operating System
# Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
# Copyright (c) 1991 IBM Corporation 
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation,
# and that the nema IBM not be used in advertising or publicity 
# pertaining to distribution of the software without specific, written
# prior permission.
# 
# CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
# 
# Carnegie Mellon requests users of this software to return to
# 
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
# 
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
# 
# MACHFS.ASM  MS-DOS device driver to interface mach file system
# with the dos server's monitor.
#
# Version 1.1
#
# Gerald Malan (grm) 4/5/1991
#
# modified for the linux dos emulator by Andrew Tridgell 13/4/93
# translated into as86 form by Robert Sanders ('murrcan style date!) 4/13/93
# (I probably broke something, but it seems to work)
#

.code16
.text
	.globl	_start16
_start16:

MaxCmd	=	12
cr	=	0xd
lf	=	0xa
eom	=	'$'		# DOS end-of-string character (barf)
Linuxfs	=	0xe6		# 0xe6 is Int for Mach Dos Server
Mivec   = 	(Linuxfs * 4)   # mem loc for the interrupt vector

Header:
	.long	-1		# link to next device driver
	.word	0		# attribute word for driver
	.word	Strat		# ptr to strategy routine
	.word	Intr		# ptr to interrupt service routine
	.ascii	"LINUX   "	# logical-device name

# the Strat and Intr routines are entered with a "far call".  I don't
# know how to tell as86 that asI would in Turbo Assembler, so Ijust
# explicitly "retf" at the end of their execution.  careful!

				# BPB from horse.mach.cs.cmu.edu
Bpb:	.word	0x200		# Bytes per Sector
	.byte	4		# Sectors per allocation unit
	.word	1		# Number of reserved sectors
	.byte	2		# Number of FATS
	.word	0x200		# Max number of root entries
	.word	0xac21		# Total number of sectors
	.byte	0xf8		# Media Desc Byte
	.word	0x2b		# Num sec per FAT
	.word	0x23		# Sectors per track
	.word	0xf		# Number of heads
	.word	0		# Number of hidden sectors
	.word	0		# High order word of hidden sectors
	.long	0		# zero
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0
	.long	0

BPBptr:	.word	Bpb

RHPtr:	.long	0		# ptr to request header

InitDone: .word 0		# 1 when initialisation is complete

Dispatch:

	.word	Init		# initialize driver
	.word	MediaChk
	.word	BuildBpb
	.word	Dummy
	.word	Read		# read
	.word	Dummy
	.word	Dummy
	.word	Dummy
	.word	Write		# write
	.word	Write		# write with verify
	.word	Dummy
	.word	Dummy
	.word	Dummy

Strat:
# was...
#	mov	word ptr cs:[RHPtr],bx
#	mov	word ptr cs:[RHPtr+2],es
	movw	%bx, %cs:RHPtr
	movw	%es, %cs:RHPtr+2
	lret


Intr:
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx
	pushw	%ds
	pushw	%es
	pushw	%di
	pushw	%si
	pushw	%bp
	
	pushw	%cs
	popw	%ds

	les	RHPtr,%di	# let es:di = request header

	movb	%es:2(%di),%bl
	xorb	%bh,%bh
	cmpw	$MaxCmd,%bx
	jle	Intr1
	call	Error
	jmp	Intr2

Intr1:	shlw	%bx
	
	call	*Dispatch(%bx)

	les	RHPtr,%di

Intr2:	or	$0x100,%ax		# Merge done bit with status
	movw	%ax,%es:3(%di)

	popw	%bp
	popw	%si
	popw	%di
	popw	%es
	popw	%ds
	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	lret

Dummy:
	call MFSini
	les	RHPtr, %di
	movb	%es:2(%di),%bl
	movb	$3, %bh

	movw    $0x20, %ax
	int	$Linuxfs

	xorw	%ax,%ax
	ret


MediaChk:
	call MFSini
# was...
#	mov	byte ptr es:[di+14],$-1	# disk has changed
#	mov	word ptr es:[di+15], offset NoName
#	mov	word ptr es:[di+17], cs	
	movb	$-1,%es:14(%di)	# disk has changed
	movw	$NoName,%es:15(%di)
	movw	%cs,%es:17(%di)
	xorw	%ax,%ax
	ret


BuildBpb:
	call MFSini
# was...
#	mov	word ptr es:[di+18],offset Bpb
#	mov	word ptr es:[di+20],cs
	movw	$Bpb,%es:18(%di)
	movw	%cs,%es:20(%di)
	xorw	%ax,%ax
	ret


Read:
	call MFSini
	movw	$0x101,%bx
	movw	$0x20,%ax
	int	$Linuxfs
	xorw	%ax,%ax
	ret


Write:
	call MFSini
	movw	$0x202,%bx
	movw	$0x20,%ax
	int	$Linuxfs
	xorw	%ax,%ax
	ret


Error:
	movw	$0x8003,%ax
	ret


MyIret:
	xorw	%ax,%ax
	iret


MFSini:
	pushw %es
	pushw %di

	pushw %cs
	popw %ds
	movw InitDone,%ax
	cmp $0,%ax
	jne AlreadyDone

	movw	$0x3000, %ax 
	xorb	%bh,%bh
	int	$0x21
	pushw	%ax

	mov	$0x52, %ah
	int	$0x21

	movw	$0x5d06, %ax
	int	$0x21

	popw	%cx
	pushw	%bx
	popw	%dx
	movw	$0x500, %bx
	movw    $0x20, %ax
	int	$Linuxfs	

	pushw %cs
	popw %ds
	movw $1,%ax
	movw %ax,InitDone

AlreadyDone:
	popw %di
	popw %es
	ret


Init:
	xorw %bx,%bx
	pushw	%si
	movw	$DirName, %si
	movb	$0x24, (%si) 
	movw    $0x20, %ax
	int	$Linuxfs
	popw	%si

	cmpw	$0, %ax
	jne	MFSin

MFSout:
	pushw %cs
	popw %ds
	movb	$9, %ah
	movw	$OutMess, %dx
	int	$0x21

# was...
#	mov	byte ptr es:[di+13],#0		# No units!!
#	mov	word ptr es:[di+18],offset BPBptr
#	mov	word ptr es:[di+20],cs
#	mov	word ptr es:[di+14],$0		#Break addr = cs:0000
#	mov	word ptr es:[di+16],cs

	movb	$0,%es:13(%di)		# No units!!
	movw	$BPBptr, %es:18(%di)
	movw	%cs, %es:20(%di)
	movw	$0, %es:14(%di)		#Break addr = cs:0000
	movw	%cs, %es:16(%di)

	ret

MFSin:
	call	MFSini

	movb	%es:22(%di),%al
	add	$0x41, %al

	pushw %di
	pushw %cs
	popw	%ds
	movw	$Mesage1, %di
	movb	%al, (%di)

	movb	$9, %ah
	movw	$Mesage, %dx
	int	$0x21
	popw %di

# was...
#	mov	byte ptr es:[di+13],$1		# Number of units
#	mov	word ptr es:[di+18],offset BPBptr
#	mov	word ptr es:[di+20],cs
#	mov	word ptr es:[di+14],offset Init
#	mov	word ptr es:[di+16],cs

# this is part of diff9
#	seg es
#	movb	[di+13],$1		# Number of units

	movw	$BPBptr, %es:18(%di)
	movw	%cs, %es:20(%di)
	movw	$Init, %es:14(%di)
	movw	%cs, %es:16(%di)

	xorw %ax,%ax

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
