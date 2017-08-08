/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * EMM hooks
 *
 * Author: Stas Sergeev
 *
 * Based on ems.S code by Andrew Tridgell, Robert Sanders and
 * MACHFS.ASM  MS-DOS device driver to interface mach file system.
 */

#include "memory.h"
#include "doshelpers.h"
#include "emm.h"
#include "xms.h"

.code16
.text
	.globl	_start16
_start16:

MaxCmd	=	15

EMSint	=	0x67

LEN	=	0
UNITS	=	1
CMD	=	2
STAT	=	3

EMSLEN	=	8
EMSOFS	= 	0x0a

Header:
	.long	-1		# link to next device driver
HdrAttr:
	.word	0xC000		# attribute word for driver
				# (char, supports IOCTL strings (it doesn't!)
	.word	Strat		# ptr to strategy routine
	.word	Intr		# ptr to interrupt service routine
EMSStr:
	.ascii	"EMMXXXX0"	# logical-device name

RHPtr:		.long 0		# ptr to request header

OldXMSCall:	.long 0
OldInt2f:	.long 0
EMSBios:	.long 0
NoEMS:		.byte 0
AlreadyLoaded:	.byte 0
AltEMM:		.ascii	"EMMDEMU2"

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
	.word	Dummy		# IOCTL output (?)
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
	xorw	%ax, %ax	# no error
	ret

Int67:
	cmpb	$0, %cs:NoEMS
	jne	10f
	lcall	*%cs:EMSBios
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
	.byte 0x9a	/* lcall */
	NewXMSCall:	.long 0
HimemHook_End:

HookHimem:
	/* check for the XMS driver */
	movw	$0x4300, %ax
	int	$0x2f
	cmpb	$0x80, %al
	je	1f
	/* no XMS driver - try to install internal driver */
	movb    $DOS_HELPER_XMS_HELPER, %al
	movb    $XMS_HELPER_XMS_INIT, %ah
	int	$DOS_HELPER_INT
	orb	%al, %al
	jnz	28f
	movb	$9, %ah
	movw	$XMSMsg, %dx
	int	$0x21
	/* internal driver installed - go out with error */
	jmp	9f
1:
	/* get entry point */
	movw	$0xffff, %bx
	movw	$0x4310, %ax
	int	$0x2f
	/* see if the call worked at all */
	cmpw	$0xffff, %bx
	je 27f
	/* see if it is sjmp (0xeb) */
	cmpb	$0xeb, %es:(%bx)
	jne	27f
	/* save old callback address */
	movw	%bx, OldXMSCall
	movw	%es, OldXMSCall+2
	/* check if already initialized */
	movb	$0x10, %ah
	movw	$0xffff, %dx
	lcall	*%cs:OldXMSCall
	orw	%ax, %ax
	jnz	28f		/* success should not happen */
	cmpb	$0x80, %bl
	jne	27f
	/* get new entry point */
	movb    $DOS_HELPER_XMS_HELPER, %al
	movb    $XMS_HELPER_GET_ENTRY_POINT, %ah
	int	$DOS_HELPER_INT
	orb	%al, %al
	jnz	28f
	/* check if the hook was initially from dosemu */
	movw	%es, %ax
	cmpw	%ax, OldXMSCall+2
	je	27f
	/* patch the hook with new addr */
	movw	%bx, NewXMSCall
	movw	%es, NewXMSCall+2
	/* now install the hook */
	movw	OldXMSCall, %di
	movw	OldXMSCall+2, %es
	movw	$HimemHook_Start, %si
	movw	$(HimemHook_End - HimemHook_Start), %cx
	rep	movsb
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


27:
	movb	$9, %ah
	movw	$CantHookMsg, %dx
	int	$0x21
	jmp	9f
28:
	movb	$9, %ah
	movw	$NoXMSMsg, %dx
	int	$0x21
#	jmp	9f
9:
	movb	$1, %al		# report error
10:	ret

Init:
	movb    $DOS_HELPER_DOSEMU_CHECK, %al
	int	$DOS_HELPER_INT
	cmpw	$1893, %cx
	jb	.LdosemuTooOld

# Check to see if another EMM (or instance of us) has been loaded aleady

	push	%es
	push	%di

	movb	$0x35, %ah
	movb	$EMSint, %al
	int	$0x21
	# int vector is in ES:BX, driver name is in ES:0a
	movw	$EMSOFS, %di

	push	%cs
	pop	%ds
	lea	EMSStr, %si

	movw	$EMSLEN, %cx
	cld
	repe	cmpsb

	jne	22f
	movb	$1, AlreadyLoaded
	push	%cs
	pop	%es
	lea	AltEMM, %si
	lea	EMSStr, %di
	movw	$EMSLEN, %cx
	rep	movsb

	pop	%di
	pop	%es
	movb	$9, %ah
	movw	$EmsAlreadyLoadedMsg, %dx
	int	$0x21
	jmp	1f

22:
	pop	%di
	pop	%es

# Check if Dosemu has enabled EMS support
	movb    $DOS_HELPER_EMS_HELPER, %al
	movb    $DOSEMU_EMS_DRIVER_VERSION, %ah
	movw	$0, %bx
	int	$DOS_HELPER_INT
	jnc	1f

	cmpb	$EMS_ERROR_VERSION_MISMATCH, %bl
	je	.LemsSysTooOld

.LemsDisabled:
	movb	$1, NoEMS
	movb	$9, %ah
	movw	$EmsDisabledMsg, %dx
	int	$0x21

1:
	movw	%dx, EMSBios
	movw	%cx, EMSBios+2
	pushw	%es
	pushw	%di
	pushw	%cs
	popw	%es
	call	HookHimem
	popw	%di
	popw	%es

	movw	%cs,%es:16(%di)
	movw	$InitCodeStart,%es:14(%di)
	orb	%al, %al
	jz	2f
	movw	$Int2f,%es:14(%di)
2:

	/* if both EMS and HookHimem fail, disable driver */
	movb	AlreadyLoaded, %ah
	orb	NoEMS, %ah
	andb	%ah, %al
	jnz	Error
	cmpb	$1, AlreadyLoaded
	je	3f

	movb	$0x25, %ah
	movb	$EMSint, %al
	movw	$Int67, %dx
	int	$0x21

	cmpb	$1, NoEMS
	je	3f
	movb	$9, %ah
	movw	$EmsInstalledMsg, %dx
	int	$0x21
3:
	xorw 	%ax, %ax
	ret

.LdosemuTooOld:
	movb	$9, %ah
	movw	$DosemuTooOldMsg, %dx
	int	$0x21
	jmp	Error

.LemsSysTooOld:
	movb	$9, %ah
	movw	$EmsSysTooOldMsg, %dx
	int	$0x21
	jmp	Error

Error:
	movw	$0, %cs:HdrAttr		# Set to block type
	movw	$0, %es:13(%di)		# Units = 0

	movw	$0,%es:14(%di)		# Break addr = cs:0000
	movw	%cs,%es:16(%di)

	movw	$0x8003, %ax		# error
	ret

DosemuTooOldMsg:
	.ascii	"WARNING: Your dosemu is too old, ems.sys not loaded.\r\n$"

EmsAlreadyLoadedMsg:
	.ascii	"WARNING: An EMS manager has already been loaded.\r\n$"

EmsInstalledMsg:
	.ascii	"dosemu EMS driver rev 0."
	.byte	DOSEMU_EMS_DRIVER_VERSION+'0'
	.ascii	" installed.\r\n$"

EmsDisabledMsg:
	.ascii	"WARNING: EMS support not enabled in dosemu.\r\n$"

EmsSysTooOldMsg:
	.ascii	"WARNING: Your ems.sys is too old, not loaded.\r\n$"

NoXMSMsg:
	.ascii	"Note: XMS disabled in the config.\r\n$"

XMSMsg:
	.ascii	"dosemu XMS 3.0 driver installed.\r\n$"

CantHookMsg:
	.ascii	"Unable to hook into himem.sys, UMB disabled.\r\n"
	.ascii	"Make sure himem.sys is loaded right before ems.sys in "
	.ascii  "your config.sys.\r\n$"

HimemOKMsg:
	.ascii	"UMB support enabled.\r\n$"
