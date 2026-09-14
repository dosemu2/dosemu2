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
 * DOS-resident stub of dosemu's NDIS 2.0.1 MAC driver.
 *
 * An NDIS MAC driver has to be a DOS character device that, while DOS
 * is processing its DEVICE= line, opens PROTMAN$ and registers itself
 * with it. That, and reserving the DOS memory in which dosemu builds
 * the NDIS tables, is all this stub does; the driver itself lives in
 * dosemu, see src/dosext/net/ndis.c.
 *
 * Load it from config.sys _after_ protman.dos, e.g.
 *   DEVICE=C:\NET\PROTMAN.DOS /I:C:\NET
 *   DEVICE=D:\DOSEMU\NDIS.SYS
 * and add a section with DriverName=DOSNDIS$ to PROTOCOL.INI.
 *
 * Author: Stas Sergeev
 */

#include "doshelpers.h"
#include "ndis.h"

.code16
.text
	.globl	_start16
_start16:

/* offsets in the DOS request header */
LEN	=	0
UNITS	=	1
CMD	=	2
STAT	=	3
INIT_UNITS =	13
INIT_BRKOF =	14
INIT_BRKSG =	16

/* offsets in the protocol manager request block */
RB_OPCODE =	0
RB_STATUS =	2
RB_POINTER1 =	4
RB_POINTER2 =	8
RB_WORD1 =	12
RB_LEN	=	14

Header:
	.long	-1		# link to next device driver
	.word	0x8000		# attribute word: character device
	.word	Strat		# ptr to strategy routine
	.word	Intr		# ptr to interrupt service routine
DevName:
	.ascii	NDIS_DEVICE_NAME	# logical-device name

RHPtr:		.long 0		# ptr to request header
Handle:		.word 0		# PROTMAN$ file handle
DataSeg:	.word 0		# segment of the resident data area
ResParas:	.word 0		# size of the resident data area

ReqBlock:			# protocol manager request block
	.word	0		# opcode
	.word	0		# status
	.long	0		# pointer1
	.long	0		# pointer2
	.word	0		# word1

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
	les	RHPtr, %di	# let es:di = request header

	cmpb	$0, %es:CMD(%di)	# only "init" is supported
	jne	1f
	call	Init
	jmp	2f
1:	mov	$0x8003, %ax	# unknown command

2:	les	RHPtr, %di
	orw	$0x100, %ax	# merge done bit with status
	mov	%ax, %es:STAT(%di)

	popw	%es
	popw	%ds
	popa
	lret

Init:
	/* assume failure: tell DOS to throw the driver away */
	movb	$0, %es:INIT_UNITS(%di)
	movw	$0, %es:INIT_BRKOF(%di)
	movw	%cs, %es:INIT_BRKSG(%di)

	movb	$DOS_HELPER_NDIS_HELPER, %al
	movb	$NDIS_SUBHELPER_INIT, %ah
	int	$DOS_HELPER_INT
	jc	.LnotAvailable
	movw	%cx, ResParas

	/* the resident data area starts at the next paragraph boundary */
	movw	$DataArea, %ax
	shrw	$4, %ax
	movw	%cs, %bx
	addw	%bx, %ax
	movw	%ax, DataSeg

	/* the protocol manager has to be loaded before us */
	movw	$0x3d02, %ax	# open, read/write
	movw	$PmName, %dx
	int	$0x21
	jc	.LnoProtman
	movw	%ax, Handle

	/* get the PROTOCOL.INI memory image */
	movw	$PM_GET_PROTOCOL_MANAGER_INFO, ReqBlock+RB_OPCODE
	movw	$0, ReqBlock+RB_STATUS
	call	ProtmanRequest
	jc	.LnoInfo

	/* let dosemu parse it and prepare the registration request */
	movb	$DOS_HELPER_NDIS_HELPER, %al
	movb	$NDIS_SUBHELPER_CONFIG, %ah
	pushw	%ds
	popw	%es
	movw	$ReqBlock, %bx
	movw	DataSeg, %dx
	int	$DOS_HELPER_INT
	jc	.LnoConfig

	/* register ourselves with the protocol manager */
	call	ProtmanRequest
	jc	.LnoRegister

	movb	$0x3e, %ah	# close PROTMAN$
	movw	Handle, %bx
	int	$0x21

	movb	$DOS_HELPER_NDIS_HELPER, %al
	movb	$NDIS_SUBHELPER_DONE, %ah
	int	$DOS_HELPER_INT

	movw	$InstalledMsg, %dx
	call	Print

	/* keep the driver and its data area resident */
	les	RHPtr, %di
	movw	ResParas, %ax
	shlw	$4, %ax
	addw	$DataArea, %ax
	movw	%ax, %es:INIT_BRKOF(%di)
	movw	%cs, %es:INIT_BRKSG(%di)
	xorw	%ax, %ax	# no error
	ret

.LnotAvailable:
	movw	$DisabledMsg, %dx
	cmpb	$NDIS_ERROR_ALREADY, %bl
	jne	1f
	movw	$AlreadyMsg, %dx
1:	call	Print
	jmp	Error

.LnoProtman:
	movw	$NoProtmanMsg, %dx
	call	Print
	jmp	Error

.LnoInfo:
	movw	$NoInfoMsg, %dx
	jmp	CloseError

.LnoConfig:
	movw	$NoConfigMsg, %dx
	jmp	CloseError

.LnoRegister:
	movw	$NoRegisterMsg, %dx
	jmp	CloseError

CloseError:
	call	Print
	movb	$0x3e, %ah
	movw	Handle, %bx
	int	$0x21
Error:
	movw	$0x800c, %ax	# general failure
	ret

/*
 * Passes ReqBlock to PROTMAN$ via an IOCTL read.
 * Returns CY on failure or on a non-zero status in the block.
 */
ProtmanRequest:
	movw	$0x4402, %ax	# ioctl, read from character device
	movw	Handle, %bx
	movw	$ReqBlock, %dx
	movw	$RB_LEN, %cx
	int	$0x21
	jc	1f
	cmpw	$0, ReqBlock+RB_STATUS
	je	1f
	stc
1:	ret

Print:
	movb	$9, %ah
	int	$0x21
	ret

PmName:
	.asciz	"PROTMAN$"

InstalledMsg:
	.ascii	"dosemu NDIS driver installed\r\n$"
DisabledMsg:
	.ascii	"ERROR: NDIS support is not enabled in dosemu, ndis.sys not loaded.\r\n$"
AlreadyMsg:
	.ascii	"ERROR: the dosemu NDIS driver is already loaded.\r\n$"
NoProtmanMsg:
	.ascii	"ERROR: PROTMAN$ not found, load protman.dos first.\r\n$"
NoInfoMsg:
	.ascii	"ERROR: PROTMAN$ supplied no configuration.\r\n$"
NoConfigMsg:
	.ascii	"ERROR: no PROTOCOL.INI section with DriverName="
	.ascii	NDIS_DEVICE_NAME
	.ascii	"\r\n$"
NoRegisterMsg:
	.ascii	"ERROR: PROTMAN$ rejected the driver registration.\r\n$"

	.align	16
DataArea:
