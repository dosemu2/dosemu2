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
 * UMB hooks
 *
 * Author: Stas Sergeev
 *
 */

#include "memory.h"
#include "doshelpers.h"
#include "xms.h"

.code16
.text
	.globl	_start16
_start16:

LEN	=	0
UNITS	=	1
CMD	=	2
STAT	=	3

Header:
	.long	-1		# link to next device driver
HdrAttr:
	.word	0xC000		# attribute word for driver
				# (char, supports IOCTL strings (it doesn't!)
	.word	Strat		# ptr to strategy routine
	.word	Intr		# ptr to interrupt service routine
UMBStr:
	.ascii	"UMBXXXX0"	# logical-device name

RHPtr:		.long 0		# ptr to request header

OldXMSCall:	.long 0

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
	.word	Dummy		# output till busy
	.word	Dummy		# ??
	.word	Dummy		# ??
	.word	Dummy		# ioctl
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

#include "xms.inc"

Init:
	movw	%cs,%es:16(%di)
	movw	$InitCodeStart,%es:14(%di)

	movb    $DOS_HELPER_DOSEMU_CHECK, %al
	int	$DOS_HELPER_INT
	cmpw	$3493, %cx
	jb	.LdosemuTooOld
	movb    $DOS_HELPER_XMS_HELPER, %al
	movb    $XMS_HELPER_UMB_INIT, %ah
	movb    $UMB_DRIVER_VERSION, %bl
	movb    $UMB_DRIVER_UMB_SYS, %bh
	int	$DOS_HELPER_INT
	jnc	1f
	cmpb	$UMB_ERROR_VERSION_MISMATCH, %bl
	je	.LumbSysTooOld
	cmpb	$UMB_ERROR_UNKNOWN_OPTION, %bl
	je	.LunknownOption
	/* already initialized */
	movb	$9, %ah
	movw	$UmbAlreadyLoadedMsg, %dx
	int	$0x21
	jmp	Error

1:
	call	HookHimem

	cmpb	$XMS_EXTERNAL_DRIVER, %al
	jz	.LusingXMSExternalDriver

	cmpb	$XMS_INTERNAL_DRIVER, %al
	jz	.LusingXMSInternalDriver

	cmpb	$XMS_DISABLED_CONFIG, %al
	jz	.LerrorXMSDisabledConfig

	cmpb	$XMS_NOHOOK_HIMEM, %al
	jz	.LerrorXMSNohookHimem

	/* exit on unknown error */
	movb	$9, %ah
	movw	$UnknownResultMsg, %dx
	int	$0x21
	jmp	Error

.LusingXMSExternalDriver:
	movb	$9, %ah
	movw	$XMSExternalDriverMsg, %dx
	int	$0x21
	jmp	SuccessLoad

.LusingXMSInternalDriver:
	movb	$9, %ah
	movw	$XMSInternalDriverMsg, %dx
	int	$0x21
	jmp	SuccessNoload

.LerrorXMSDisabledConfig:
	movb	$9, %ah
	movw	$XMSInternalDisabledMsg, %dx
	int	$0x21
	jmp	Error

.LerrorXMSNohookHimem:
	movb	$9, %ah
	movw	$CantHookMsg, %dx
	int	$0x21
	jmp	Error

.LdosemuTooOld:
	movb	$9, %ah
	movw	$DosemuTooOldMsg, %dx
	int	$0x21
	jmp	Error

.LumbSysTooOld:
	movb	$9, %ah
	movw	$UmbSysTooOldMsg, %dx
	int	$0x21
	jmp	Error

.LunknownOption:
	movb	$9, %ah
	movw	$UnknownOptionMsg, %dx
	int	$0x21
	jmp	Error

Error:
	movw	$0x800c, %ax		# error
	jmp 4f

SuccessNoload:
	xorw    %ax, %ax		# okay

4:
	movw	$0, %cs:HdrAttr		# Set to block type
	movb	$0, %es:13(%di)		# Units = 0
	movw	$0,%es:14(%di)		# Break addr = cs:0000
	movw	%cs,%es:16(%di)
	ret

SuccessLoad:
	xorw 	%ax, %ax
	ret

XMSExternalDriverMsg:
	.ascii	"dosemu UMB loaded (XMS driver external).\r\n$"

XMSInternalDriverMsg:
	.ascii	"dosemu UMB not required (XMS 3.0 internal support enabled).\r\n$"

XMSInternalDisabledMsg:
	.ascii	"dosemu UMB failed (XMS internal support disabled in the config).\r\n$"

CantHookMsg:
	.ascii	"dosemu UMB failed (Unable to hook into himem.sys).\r\n"
	.ascii	"  Make sure himem.sys is loaded right before umb.sys in "
	.ascii  "your config.sys.\r\n$"

UmbAlreadyLoadedMsg:
	.ascii	"dosemu UMB failed (A UMB manager has already been loaded).\r\n$"

DosemuTooOldMsg:
	.ascii	"dosemu UMB failed (Your dosemu is too old, umb.sys not loaded).\r\n$"

UmbSysTooOldMsg:
	.ascii	"dosemu UMB failed (Your umb.sys is too old, not loaded).\r\n$"

UnknownOptionMsg:
	.ascii	"dosemu UMB failed (Unknown option in umb.sys command line).\r\n$"

UnknownResultMsg:
	.ascii	"dosemu UMB failed (Unknown result from xms.inc).\r\n$"
