# 
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
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

#include "memory.h"
#include "doshelpers.h"
#include "emm.h"

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

LEN	=	0
UNITS	=	1
CMD	=	2
STAT	=	3

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

RHPtr:		.long 0		# ptr to request header

OldXMSCall:	.long 0
OldInt2f:	.long 0
NoEMS:		.byte 0

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
	pusha
	pushw	%ds
	pushw	%es

	pushw	%cs
	popw	%ds
	les	RHPtr,%di	# let es:di = request header

	movzbw	%es:CMD(%di), %si
	movw	%si, %bx
	cmpw	$MaxCmd, %bx 
	jle	1f
	mov	$0x8003, %ax	# error
	jmp	2f

1:	shlw	%si
	callw	*Dispatch(%si)
	les	RHPtr,%di

2:	orw	$0x100,%ax	# Merge done bit with status
	mov	%ax,%es:STAT(%di)

	popw	%es
	popw	%ds
	popa
	lret

Dummy:
	movb    $DOS_HELPER_EMS_HELPER, %al
	int	$LinuxEMS
	ret

Int67:
	cmpb	$0, %cs:NoEMS
	jne	10f
	pushw	%ax
	movw	$DOS_HELPER_EMS_BIOS, %ax
	int	$LinuxEMS
10:	iret

Int2f:
	cmpw	$0x4310, %ax
	je	1f
	ljmp	*%cs:OldInt2f
1:	pushw	%cs
	popw	%es
	movw	$XMSHook, %bx
	iret

XMSHook:
	jmp 1f
	nop
	nop
	nop
1:	lcall	*%cs:OldXMSCall
	lret

InitCodeStart:		/* all the below code gets discarded after init */

HimemHook_Start:
	lcall	$XMSControl_SEG, $XMSControl_OFF
HimemHook_End:

HookHimem:
	/* check for the XMS driver */
	movw	$0x4300, %ax
	int	$0x2f
	cmpb	$0x80, %al
	je	1f
	/* no XMS driver - try to install internal driver */
	pushw	$BIOSSEG
	popw	%es
        /* version < 1.3.3.1? */
	cmpl	$0x01030301, %es:(0xffe8)
	jb	0f
	movb    $DOS_HELPER_XMS_HELPER, %al
	int	$DOS_HELPER_INT
	orb	%al, %al
	jnz	28f
	movb	$9, %ah
	movw	$XMSMsg, %dx
	int	$0x21
	jmp	1f

0:      
	movb	$9, %ah
	movw	$NoHimemMsg, %dx
	int	$0x21
	jmp	9f
28:      
	movb	$9, %ah
	movw	$NoXMSMsg, %dx
	int	$0x21
	jmp	9f
1:
	/* get entry point */
	movw	$0x4310, %ax
	int	$0x2f
	/* see if it is sjmp (0xeb) */
	cmpb	$0xeb, %es:(%bx)
	je	2f
	/* no sjmp :( */
	movb	$9, %ah
	movw	$CantHookMsg, %dx
	int	$0x21
	jmp	9f
2:
	/* see if it is our internal driver */
	movw	%es, %ax
	cmpw	$XMSControl_SEG, %ax
	je	9f
3:
	/* now install the hook for UMB */
	movw	%bx, %di
	movw	$HimemHook_Start, %si
	movw	$(HimemHook_End - HimemHook_Start), %cx
	rep	movsb
	/* save old callback address */
	movw	%bx, OldXMSCall
	movw	%ax, OldXMSCall+2
	/* install int2f hook */
	movb	$0x35, %ah
	movb	$0x2f, %al
	int	$0x21
	movw	%bx, OldInt2f
	movw	%es, OldInt2f+2
	movb	$0x25, %ah
	movb	$0x2f, %al
	movw	$Int2f, %dx
	int	$0x21
	/* all done, UMB should work */
	movb	$9, %ah
	movw	$HimemOKMsg, %dx
	int	$0x21
	xorb	%al, %al
	jmp	10f
9:
	movb	$1, %al		# report error
10:	ret

Init:
	movb    $DOS_HELPER_EMS_HELPER, %al
	movb    $DOSEMU_EMS_DRIVER_VERSION, %ah
	int	$LinuxEMS
	orb	%al, %al
	jz	1f
	movb	$1, NoEMS
	movb	$9, %ah
	movw	$NoEMSMsg, %dx
	int	$0x21
1:
	pushw	%es
	pushw	%di
	pushw	%cs
	popw	%es
	call	HookHimem
	popw	%di
	popw	%es

	movw	$InitCodeStart,%es:14(%di)
	orb	%al, %al
	jz	2f
	movw	$Int2f,%es:14(%di)
2:

	/* if both EMS and HookHimem fail, disable driver */
	andb	NoEMS, %al
	jnz	Error

	movb	$0x25, %ah
	movb	$EMSint, %al
	movw	$Int67, %dx
	int	$0x21

	movb	$1,%es:13(%di)
	movw	%cs,%es:16(%di)

	cmpb	$1, NoEMS
	je	3f
	movb	$9, %ah
	movw	$Mesage, %dx
	int	$0x21
3:
	xorw 	%ax, %ax
	ret

Error:
	movb	$0,%es:13(%di)		# No units!!
	movw	$0,%es:14(%di)		# Break addr = cs:0000
	movw	%cs,%es:16(%di)

	movw 	$2, %ax
	ret


Mesage:	.ascii	"dosemu EMS 4.0 driver installed."
	.byte	cr,lf,eom

NoHimemMsg:
	.ascii	"WARNING: himem.sys was not loaded before ems.sys!\r\n"
	.ascii  "XMS and UMB services will not be available.\r\n"
	.ascii	"Please adjust your config.sys.\r\n"
	.byte	eom
NoXMSMsg:
	.ascii	"Note: XMS disabled in the config."
	.byte	cr,lf,eom
XMSMsg:
	.ascii	"dosemu XMS 3.0 driver installed."
	.byte	cr,lf,eom
CantHookMsg:
	.ascii	"Unable to hook into himem.sys, UMB disabled.\r\n"
	.ascii	"Make sure himem.sys is loaded right before ems.sys in "
	.ascii  "your config.sys.\r\n"
	.byte	eom
HimemOKMsg:
	.ascii	"UMB support enabled."
	.byte	cr,lf,eom
DFailMsg:
	.ascii	"ERROR: dosemu refused to enable EMS support."
	.byte	cr,lf,eom
NoEMSMsg:
	.ascii	"Warning: EMS support not enabled."
	.byte	cr,lf,eom

NoName:	.ascii	"NO NAME"
	.byte	0
