# 
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#


# ems.S for the Linux DOS emulator
#   provide a V86-mode ISR and device driver for EMS emulation
#
# License on original linux.asm:
#
# #Mach Operating System
# #Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
# #Copyright (c) 1991 IBM Corporation 
# #All Rights Reserved.
# #
# #Permission to use, copy, modify and distribute this software and its
# #documentation is hereby granted, provided that both the copyright
# #notice and this permission notice appear in all copies of the
# #software, derivative works or modified versions, and any portions
# #thereof, and that both notices appear in supporting documentation,
# #and that the nema IBM not be used in advertising or publicity 
# #pertaining to distribution of the software without specific, written
# #prior permission.
# # 
# #CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# #CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
# #ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
# # 
# #Carnegie Mellon requests users of this software to return to
# #
# # Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
# # School of Computer Science
# # Carnegie Mellon University
# # Pittsburgh PA 15213-3890
# #
# #any improvements or extensions that they make and grant Carnegie Mellon
# #the rights to redistribute these changes.
# # 
# #MACHFS.ASM  MS-DOS device driver to interface mach file system
# #with the dos server's monitor.
# #
# #Version 1.1
# #
# #Gerald Malan (grm) 4/5/1991
#
# modified for the linux dos emulator by Andrew Tridgell 13/4/93
# translated into as86 form by Robert Sanders ('murrcan style date!) 4/13/93
# (I probably broke something, but it seems to work)
#
# copied from emufs.S to make ems.S  -  7/19/93
#

.code16
.text
	.globl	_start16
_start16:

MaxCmd	=	15
cr	=	0xd
lf	=	0xa
eom	=	'$'		# DOS end-of-string character (barf)
LinuxEMS =	0xe6		# 0xe6 is Int for Linux EMS server
EMSint	=	0x67
vecoff   = 	(EMSint * 4)   # mem loc for the interrupt vector
vecseg	=	(vecoff + 2)

Header:
	.long	-1		# link to next device driver
	.word	0xC000		# attribute word for driver 
				# (char, supports IOCTL strings (it doesn't!)
	.word	Strat		# ptr to strategy routine
	.word	Intr		# ptr to interrupt service routine
	.ascii	"EMMXXXX0"	# logical-device name

# the Strat and Intr routines are entered with a "far call".  I don't
# know how to tell gas that as I would in Turbo Assembler, so I just
# explicitly "lret" at the end of their execution.  Be careful!

RHPtr:	.long	0		# ptr to request header

InitDone: .word 0		# 1 when initialisation is complete

Dispatch:

	.word	Init		# initialize driver
	.word	Dummy		# Media Check ,block only
	.word	Dummy		# Build BPB, block only
	.word	Dummy		# Ioctl
	.word	Dummy		# read
	.word	Dummy  		# non-destructive read
	.word	Dummy		# input status
	.word	Dummy		# flush input
	.word	Dummy		# write
	.word	Dummy		# write with verify
	.word	Dummy		# output status
	.word	Dummy		# flush output
	.word	Dummy		# IOCTL output (??)
/* if DOS 3.0 or newer... */
	.word	Dummy		# open device
	.word	Dummy		# close device
	.word	Dummy		# removeable media check

Strat:
	mov	%bx, %cs:RHPtr
	mov	%es, %cs:RHPtr+2
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
	cmpw	$MaxCmd, %bx 
	jle	Intr1
	call	Error
	jmp	Intr2

Intr1:	shlw	%bx
	
	callw	*Dispatch(%bx)

	les	RHPtr,%di

Intr2:	orw	$0x100,%ax		# Merge done bit with status
	mov	%ax,%es:3(%di)

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
	movb	%es:2(%di),%bl  # get request number
	xorb	%bh,%bh

	movb    $0x21, %al
	int	$LinuxEMS

	xorw	%ax,%ax
	ret


Int67:
	pushw	%ax
	movw	$0x22, %ax
	int	$LinuxEMS
	iret

Read:
	call MFSini
	movw	$0x101, %bx
	xorw	%ax,%ax
	ret


Write:
	call MFSini
	xorw	%ax,%ax
	ret

Error:
	mov	$0x8003,%ax
	ret

MFSini:
	pushw %es
	pushw %di

	pushw %cs
	popw %ds
	movw InitDone,%ax
	cmp $0,%ax
	jne AlreadyDone

	movw	$0x3000, %ax
	int	$0x21
	pushw	%ax

	movb	$0x52, %ah
	int	$0x21

	movw	$0x5d06, %ax
	int	$0x21

	popw	%cx
	pushw	%bx
	popw	%dx

	pushw	%di
	pushw	%es
	pushw	%ax
	movw	$0, %ax
	movw	%ax, %es
	movw	$vecoff, %di
	movw	$Int67, %es:(%di)
	movw	%cs, %es:2(%di)
	popw	%ax
	popw	%es
	popw	%di

	pushw %cs
	popw %ds
	movw $1,%ax
	movw %ax,InitDone

AlreadyDone:
	popw %di
	popw %es
	ret


Init:
	xorw	%bx,%bx

	cmpw	$0, %ax
	jne	MFSin

MFSout:
	pushw %cs
	popw %ds
	mov	$9, %ah
	mov	$OutMess, %dx
	int	$0x21

	movb	$0,%es:13(%di)		# No units!!
	movw	%cs,%es:20(%di)
	movw	$0,%es:14(%di)	#Break addr = cs:0000
	movw	%cs,%es:16(%di)

	ret

MFSin:
	call	MFSini

	movb	%es:22(%di), %al
	addb	$0x41, %al

	pushw	%cs
	popw	%ds

	movb	$9, %ah
	movw	$Mesage, %dx
	int	$0x21

	movw	$Init, %es:14(%di)
	movw	%cs, %es:16(%di)

	xorw %ax,%ax

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
