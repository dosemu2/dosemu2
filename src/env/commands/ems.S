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
Dispatch_End:

AmountCmd = (Dispatch_End - Dispatch) / 2

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
	cmpw	$AmountCmd, %bx
	jb	1f
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
	lcall	*%cs:EMSBios
	iret

#include "xms.inc"

Init:
	movw	%cs,%es:16(%di)
	movw	$InitCodeStart,%es:14(%di)

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
	pop	%di
	pop	%es
	jne	22f
	movb	$9, %ah
	movw	$EmsAlreadyLoadedMsg, %dx
	int	$0x21
	jmp	Error

22:

# Check if Dosemu has enabled EMS support
	movb    $DOS_HELPER_EMS_HELPER, %al
	movb    $DOSEMU_EMS_DRIVER_VERSION, %ah
	movw	$EMS_HELPER_EMM_INIT, %bx
	int	$DOS_HELPER_INT
	jnc	1f

	cmpb	$EMS_ERROR_VERSION_MISMATCH, %bl
	je	.LemsSysTooOld
	cmpb	$EMS_ERROR_PFRAME_UNAVAIL, %bl
	je	.LemsFrameUnavail
	/* ems disabled */
	movb	$9, %ah
	movw	$EmsDisabledMsg, %dx
	int	$0x21
	jmp	Error
1:
	movw	%dx, EMSBios
	movw	%cx, EMSBios+2

3:
	movb    $DOS_HELPER_XMS_HELPER, %al
	movb    $XMS_HELPER_UMB_INIT, %ah
	movb    $UMB_DRIVER_VERSION, %bl
	movb    $UMB_DRIVER_EMS_SYS, %bh
	int	$DOS_HELPER_INT
	jnc	11f
	cmpb	$UMB_ERROR_VERSION_MISMATCH, %bl
	je	.LemsSysTooOld
	/* already initialized means both UMB and XMS work already */
	jmp	122f

11:
	call	HookHimem
	jnc	2f
122:
	clc
	movw	$Int2f,%es:14(%di)

2:
	movb	$0x25, %ah
	movb	$EMSint, %al
	movw	$Int67, %dx
	int	$0x21

	movb	$9, %ah
	movw	$EmsInstalledMsg, %dx
	int	$0x21
3:
	xorw	%ax, %ax
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

.LemsFrameUnavail:
	movb	$9, %ah
	movw	$EmsFrameUnavailMsg, %dx
	int	$0x21
	jmp	Error

Error:
	movw	$0, %cs:HdrAttr		# Set to block type
	movw	$0, %es:13(%di)		# Units = 0

	movw	$0,%es:14(%di)		# Break addr = cs:0000
	movw	%cs,%es:16(%di)

	movw	$0x800c, %ax		# error
	ret

DosemuTooOldMsg:
	.ascii	"ERROR: Your dosemu is too old, ems.sys not loaded.\r\n$"

EmsAlreadyLoadedMsg:
	.ascii	"WARNING: An EMS manager has already been loaded.\r\n$"

EmsInstalledMsg:
	.ascii	"dosemu EMS driver rev 0."
	.byte	DOSEMU_EMS_DRIVER_VERSION+'0'
	.ascii	" installed.\r\n$"

EmsDisabledMsg:
	.ascii	"WARNING: EMS support not enabled in dosemu.\r\n$"

EmsSysTooOldMsg:
	.ascii	"ERROR: Your ems.sys is too old, not loaded.\r\n$"

EmsFrameUnavailMsg:
	.ascii	"ERROR: EMS frame not available. umb.sys loaded?\r\n$"
