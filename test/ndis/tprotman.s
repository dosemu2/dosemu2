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
 */

/*
 * Minimal stand-in for PROTMAN.DOS, for testing dosemu's NDIS driver.
 *
 * It implements just enough of the protocol manager for a MAC driver to
 * come up: it hands out a canned PROTOCOL.INI memory image describing a
 * single [DOSEMU] section with DriverName=DOSNDIS$, and it remembers the
 * characteristics table of the module that registers with it. The test
 * program tndisapp.com then picks that table up through a private
 * request opcode and binds itself to the MAC as a protocol would.
 *
 * Author: Stas Sergeev
 */

#include "ndis.h"

/* private request opcode: return the registered module's chars in Pointer1 */
#define PM_TEST_GET_MODULE	0x1000

.code16
.text
	.globl	_start16
_start16:

/* offsets in the DOS request header */
CMD	=	2
STAT	=	3
INIT_UNITS =	13
INIT_BRKOF =	14
INIT_BRKSG =	16
IOCTL_XFEROF =	14
IOCTL_XFERSG =	16

/* offsets in the protocol manager request block */
RB_OPCODE =	0
RB_STATUS =	2
RB_POINTER1 =	4

Header:
	.long	-1
	.word	0xc000		# character device, supports IOCTL
	.word	Strat
	.word	Intr
	.ascii	"PROTMAN$"

RHPtr:		.long 0
ModuleChars:	.long 0		# chars of the module that registered

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
	les	RHPtr, %di

	movb	%es:CMD(%di), %al
	cmpb	$0, %al			# init
	jne	1f
	call	Init
	jmp	3f
1:	cmpb	$3, %al			# ioctl input
	jne	2f
	call	Ioctl
	jmp	3f
2:	mov	$0x8003, %ax		# unknown command

3:	les	RHPtr, %di
	orw	$0x100, %ax
	mov	%ax, %es:STAT(%di)

	popw	%es
	popw	%ds
	popa
	lret

Init:
	movb	$0, %es:INIT_UNITS(%di)
	movw	$DriverEnd, %es:INIT_BRKOF(%di)
	movw	%cs, %es:INIT_BRKSG(%di)
	movw	$InstalledMsg, %dx
	movb	$9, %ah
	int	$0x21
	xorw	%ax, %ax
	ret

/*
 * Handles a request block passed via an IOCTL read.
 */
Ioctl:
	pushw	%ds
	lds	%es:IOCTL_XFEROF(%di), %si	# ds:si = request block

	movw	RB_OPCODE(%si), %ax
	cmpw	$PM_GET_PROTOCOL_MANAGER_INFO, %ax
	je	.LgetInfo
	cmpw	$PM_REGISTER_MODULE, %ax
	je	.LregisterModule
	cmpw	$PM_TEST_GET_MODULE, %ax
	je	.LgetModule
	movw	$NDIS_ERR_INVALID_FUNCTION, RB_STATUS(%si)
	jmp	.Ldone

.LgetInfo:
	movw	$ConfigImage, RB_POINTER1(%si)
	movw	%cs, RB_POINTER1+2(%si)
	movw	$NDIS_SUCCESS, RB_STATUS(%si)
	jmp	.Ldone

.LregisterModule:
	movw	RB_POINTER1(%si), %ax
	movw	%ax, %cs:ModuleChars
	movw	RB_POINTER1+2(%si), %ax
	movw	%ax, %cs:ModuleChars+2
	movw	$NDIS_SUCCESS, RB_STATUS(%si)
	jmp	.Ldone

.LgetModule:
	movw	%cs:ModuleChars, %ax
	movw	%ax, RB_POINTER1(%si)
	movw	%cs:ModuleChars+2, %ax
	movw	%ax, RB_POINTER1+2(%si)
	movw	$NDIS_SUCCESS, RB_STATUS(%si)

.Ldone:
	popw	%ds
	xorw	%ax, %ax
	ret

InstalledMsg:
	.ascii	"test PROTMAN$ installed\r\n$"

/*
 * The canned configuration memory image: one module section with a
 * single DriverName keyword.
 */
	.align	2
ConfigImage:
	.long	0			# NextModule
	.long	0			# PrevModule
	.ascii	"DOSEMU"		# ModName[NAME_LEN]
	.fill	NAME_LEN - 6, 1, 0
KeywordEntry:
	.long	0			# NextKeywordEntry
	.long	0			# PrevKeywordEntry
	.ascii	"DRIVERNAME"		# KeyWord[NAME_LEN]
	.fill	NAME_LEN - 10, 1, 0
	.word	1			# NumParams
	.word	PARAM_TYPE_STRING	# ParamType
	.word	DrvNameEnd - DrvName	# ParamLen, including the terminator
DrvName:
	.asciz	NDIS_DEVICE_NAME
DrvNameEnd:

	.align	16
DriverEnd:
