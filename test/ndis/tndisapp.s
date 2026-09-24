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
 * NDIS protocol stand-in, for testing dosemu's NDIS driver.
 *
 * Together with tprotman.sys this replaces the proprietary NDIS stacks
 * for testing purposes: it picks the MAC driver's characteristics table
 * up from the test protocol manager, binds to the MAC as a protocol
 * does, sets a packet filter, sends an ARP request and waits for the
 * reply to come back through an indication.
 *
 * Everything is called with the MS C "far pascal" convention that NDIS
 * mandates: arguments pushed left to right, callee removes them, the
 * result in AX, DS/SI/DI/BP preserved. The indications arrive with DS
 * undefined, so each handler loads it from the ProtDS argument.
 *
 * Usage: tndisapp [nochain]
 *   nochain - hide our ReceiveChain handler, so that the MAC has to
 *             use the ReceiveLookahead + TransferData path instead.
 *
 * Exits with errorlevel 0 on success.
 *
 * Author: Stas Sergeev
 */

#include "ndis.h"

/* private request opcode of tprotman.sys */
#define PM_TEST_GET_MODULE	0x1000

#define RXBUFSZ		1514
#define TIMEOUT_TICKS	91		/* ~5 seconds */

/* offsets in the protocol manager request block */
RB_OPCODE =	0
RB_STATUS =	2
RB_POINTER1 =	4
RB_LEN	=	14

.code16
.text
	.globl	_start16
_start16:
	jmp	Main

/*
 * Our characteristics and lower dispatch tables.
 */
	.align	2
ProtChars:
	.word	0x40			# CcSize
	.byte	NDIS_VERSION_MAJOR	# CcNdisMajor
	.byte	NDIS_VERSION_MINOR	# CcNdisMinor
	.word	0			# CcReserved
	.byte	1			# CcModMajor
	.byte	0			# CcModMinor
	.long	MFF_BIND_LOWER		# CcModFunc
	.ascii	"TPROT"			# CcName[NAME_LEN]
	.fill	NAME_LEN - 5, 1, 0
	.byte	0			# CcUprLevel
	.byte	0			# CcUprType
	.byte	UL_MAC			# CcLwrLevel
	.byte	UI_MAC			# CcLwrType
	.word	0			# CcModId
CcModDS:
	.word	0			# CcModDS, filled in at run time
	.long	0			# CcSysReq
	.long	0			# CcSChar
	.long	0			# CcSStat
	.long	0			# CcUprDisp
CcLwrDisp:
	.long	0			# CcLwrDisp, filled in at run time
	.long	0			# CcRsv0
	.long	0			# CcRsv1

	.align	2
ProtDisp:
	.long	0			# PldCCp, filled in at run time
	.long	0			# PldFlags
PldReqConfirm:
	.long	0			# RequestConfirm
PldXmitConfirm:
	.long	0			# TransmitConfirm
PldRcvLkAhead:
	.long	0			# ReceiveLookahead
PldIndComplete:
	.long	0			# IndicationComplete
PldRcvChain:
	.long	0			# ReceiveChain
PldStatInd:
	.long	0			# StatusIndication

/*
 * Run time state
 */
Handle:		.word 0			# PROTMAN$ file handle
MacChars:	.long 0			# -> MAC common characteristics
MacCharsOut:	.long 0			# what the MAC hands back on bind
MacDisp:	.long 0			# -> MAC upper dispatch table
MacSpec:	.long 0			# -> MAC specific characteristics
MacDS:		.word 0			# the MAC's data segment
RxFlag:		.byte 0			# set by the indications
RxVia:		.byte 0			# 'C' chain, 'L' lookahead
RxLen:		.word 0			# length the MAC indicated
RxCopied:	.word 0			# what TransferData copied
NoChain:	.byte 0			# "nochain" given on the command line

ReqBlock:
	.word	0			# opcode
	.word	0			# status
	.long	0			# pointer1
	.long	0			# pointer2
	.word	0			# word1

/* transmit descriptor for the ARP request */
	.align	2
TxDesc:
	.word	0			# TxImmedLen
	.long	0			# TxImmedPtr
	.word	1			# TxDataCount
	.byte	0			# TxPtrType
	.byte	0			# TxRsvd
	.word	ArpEnd - ArpFrame	# TxDataLen
TxDataPtr:
	.long	0			# TxDataPtr, filled in at run time

/* transfer descriptor used from within ReceiveLookahead */
	.align	2
TdDesc:
	.word	1			# TDDataCount
	.byte	0			# TDPtrType
	.byte	0			# TDRsvd
	.word	RXBUFSZ			# TDDataLen
TdDataPtr:
	.long	0			# TDDataPtr, filled in at run time

/* an ARP request for the slirp gateway */
	.align	2
ArpFrame:
	.byte	0xff, 0xff, 0xff, 0xff, 0xff, 0xff	# destination
ArpSrc:
	.fill	6, 1, 0					# source, our MAC
	.byte	0x08, 0x06				# ARP
	.byte	0x00, 0x01				# ethernet
	.byte	0x08, 0x00				# IPv4
	.byte	6, 4					# address sizes
	.byte	0x00, 0x01				# request
ArpSha:
	.fill	6, 1, 0					# sender MAC
	.byte	10, 0, 2, 15				# sender IP
	.fill	6, 1, 0					# target MAC
	.byte	10, 0, 2, 2				# target IP, the gateway
ArpEnd:

	.align	2
RxBuf:
	.fill	RXBUFSZ, 1, 0

/************************************************************************/

Main:
	cld
	call	ParseArgs

	movw	$BannerMsg, %dx
	call	Print

	call	GetMacChars
	jc	.Lfail
	call	Bind
	jc	.Lfail
	call	SetFilter
	jc	.Lfail
	call	SendArp
	jc	.Lfail
	call	WaitForReply
	jc	.Lfail

	call	Cleanup
	movw	$PassMsg, %dx
	call	Print
	movw	$0x4c00, %ax
	int	$0x21

.Lfail:
	call	Cleanup
	movw	$FailMsg, %dx
	call	Print
	movw	$0x4c01, %ax
	int	$0x21

/*
 * Looks for the "nochain" argument in the PSP command line.
 */
ParseArgs:
	movzbw	0x80, %cx
	jcxz	1f
	movw	$0x81, %si
2:	lodsb
	cmpb	$'n', %al
	je	3f
	cmpb	$'N', %al
	je	3f
	loop	2b
	jmp	1f
3:	movb	$1, NoChain
	movw	$NoChainMsg, %dx
	call	Print
1:	ret

/*
 * Asks the test protocol manager for the characteristics table of the
 * module that has registered with it.
 */
GetMacChars:
	movw	$0x3d02, %ax		# open PROTMAN$
	movw	$PmName, %dx
	int	$0x21
	jnc	1f
	movw	$NoProtmanMsg, %dx
	call	Print
	stc
	ret
1:	movw	%ax, Handle

	movw	$PM_TEST_GET_MODULE, ReqBlock+RB_OPCODE
	movw	$0, ReqBlock+RB_STATUS
	movw	$0x4402, %ax		# ioctl read
	movw	Handle, %bx
	movw	$ReqBlock, %dx
	movw	$RB_LEN, %cx
	int	$0x21
	pushf
	movb	$0x3e, %ah		# close
	movw	Handle, %bx
	int	$0x21
	popf
	jc	2f

	movw	ReqBlock+RB_POINTER1, %ax
	movw	%ax, MacChars
	movw	ReqBlock+RB_POINTER1+2, %bx
	movw	%bx, MacChars+2
	orw	%bx, %ax		# a NULL pointer means nobody registered
	jz	2f
	clc
	ret
2:	movw	$NoModuleMsg, %dx
	call	Print
	stc
	ret

/*
 * Fills in our tables and binds to the MAC.
 */
Bind:
	movw	%ds, CcModDS
	movw	$ProtDisp, CcLwrDisp
	movw	%ds, CcLwrDisp+2
	movw	$ProtChars, ProtDisp	# PldCCp
	movw	%ds, ProtDisp+2

	movw	$RequestConfirm, PldReqConfirm
	movw	%cs, PldReqConfirm+2
	movw	$TransmitConfirm, PldXmitConfirm
	movw	%cs, PldXmitConfirm+2
	movw	$ReceiveLookahead, PldRcvLkAhead
	movw	%cs, PldRcvLkAhead+2
	movw	$IndicationComplete, PldIndComplete
	movw	%cs, PldIndComplete+2
	movw	$StatusIndication, PldStatInd
	movw	%cs, PldStatInd+2
	cmpb	$0, NoChain
	jne	1f
	movw	$ReceiveChain, PldRcvChain
	movw	%cs, PldRcvChain+2
1:
	/* the MAC's data segment comes from its characteristics table */
	les	MacChars, %di
	movw	%es:0x22(%di), %ax	# CcModDS
	movw	%ax, MacDS

	/* SystemRequest(ProtCC, &MacCC, 0, SR_BIND, MacDS) */
	pushw	%ds			# ProtCC
	pushw	$ProtChars
	pushw	%ds			# &MacCCOut
	pushw	$MacCharsOut
	pushw	$0			# Param3
	pushw	$SR_BIND		# Opcode
	pushw	MacDS
	lcall	*%es:0x24(%di)		# CcSysReq
	orw	%ax, %ax
	jz	2f
	movw	$BindFailMsg, %dx
	call	Print
	stc
	ret

2:	/* the MAC handed us its characteristics table back */
	les	MacCharsOut, %di
	movw	%es:0x28(%di), %ax	# CcSChar
	movw	%ax, MacSpec
	movw	%es:0x2a(%di), %ax
	movw	%ax, MacSpec+2
	movw	%es:0x30(%di), %ax	# CcUprDisp
	movw	%ax, MacDisp
	movw	%es:0x32(%di), %ax
	movw	%ax, MacDisp+2

	/* take our station address from the MAC's specific chars */
	pushw	%ds
	pushw	%es
	movw	%ds, %ax
	movw	%ax, %es		# es = our data
	lds	MacSpec, %si
	addw	$20, %si		# MscPermStnAdr
	movw	$ArpSrc, %di
	movw	$6, %cx
	rep	movsb			# the ethernet header
	subw	$6, %si
	movw	$ArpSha, %di
	movw	$6, %cx
	rep	movsb			# and the ARP sender address
	popw	%es
	popw	%ds

	movw	$BoundMsg, %dx
	call	Print
	clc
	ret

/*
 * Turns the receive filter on (Param1) or off (0).
 */
SetFilter:
	movw	$(FILTER_DIRECTED | FILTER_BROADCAST), %ax
	call	DoSetFilter
	orw	%ax, %ax
	jz	1f
	movw	$FilterFailMsg, %dx
	call	Print
	stc
	ret
1:	clc
	ret

DoSetFilter:
	les	MacDisp, %di
	pushw	$0			# ProtID
	pushw	$0			# ReqHandle
	pushw	%ax			# Param1, the filter
	pushw	$0			# Param2
	pushw	$0
	pushw	$GR_SET_PACKET_FILTER
	pushw	MacDS
	lcall	*%es:4(%di)		# MudRequest
	ret

/*
 * Sends the ARP request.
 */
SendArp:
	movw	$ArpFrame, TxDataPtr
	movw	%ds, TxDataPtr+2

	les	MacDisp, %di
	pushw	$0			# ProtID
	pushw	$1			# ReqHandle
	pushw	%ds			# BufDesc
	pushw	$TxDesc
	pushw	MacDS
	lcall	*%es:8(%di)		# MudTransmitChain
	orw	%ax, %ax
	jz	1f
	movw	$SendFailMsg, %dx
	call	Print
	stc
	ret
1:	movw	$SentMsg, %dx
	call	Print
	clc
	ret

/*
 * Spins until an indication arrives, without calling into DOS, as the
 * indications can arrive at any moment.
 */
WaitForReply:
	pushw	%es
	movw	$0x40, %ax
	movw	%ax, %es
	sti
	movw	%es:0x6c, %bx		# BIOS tick count
1:	cmpb	$0, RxFlag
	jne	2f
	movw	%es:0x6c, %ax
	subw	%bx, %ax
	cmpw	$TIMEOUT_TICKS, %ax
	jb	1b
	popw	%es
	movw	$TimeoutMsg, %dx
	call	Print
	stc
	ret

2:	popw	%es
	movw	$GotMsg, %dx
	call	Print
	movb	RxVia, %al
	call	PrintChar
	movw	$LenMsg, %dx
	call	Print
	movw	RxLen, %ax
	call	PrintHexWord
	movw	$CopiedMsg, %dx
	call	Print
	movw	RxCopied, %ax
	call	PrintHexWord
	movw	$CrLfMsg, %dx
	call	Print
	clc
	ret

/*
 * Stops the reception, so that the MAC does not indicate into our
 * memory once we are gone.
 */
Cleanup:
	cmpw	$0, MacDisp+2
	je	1f
	xorw	%ax, %ax
	call	DoSetFilter
1:	ret

/************************************************************************/
/* the protocol lower dispatch entry points, far pascal                 */

/*
 * USHORT ReceiveLookahead(USHORT MACID, USHORT FrameSize,
 *                         USHORT BytesAvail, void far *Buffer,
 *                         UCHAR far *Indicate, USHORT ProtDS);
 */
ReceiveLookahead:
	pushw	%bp
	movw	%sp, %bp
	pushw	%ds
	pushw	%es
	pushw	%si
	pushw	%di

	movw	6(%bp), %ax		# ProtDS
	movw	%ax, %ds
	movw	16(%bp), %ax		# BytesAvail
	movw	%ax, RxLen

	/* TransferData(&RxCopied, 0, TdDesc, MacDS) */
	movw	$RxBuf, TdDataPtr
	movw	%ds, TdDataPtr+2
	les	MacDisp, %di
	pushw	%ds			# &BytesCopied
	pushw	$RxCopied
	pushw	$0			# FrameOfs
	pushw	%ds			# BufDesc
	pushw	$TdDesc
	pushw	MacDS
	lcall	*%es:12(%di)		# MudTransferData

	movw	RxCopied, %cx
	movb	$'L', %al
	call	CheckArpReply
	xorw	%ax, %ax		# NDIS_SUCCESS

	popw	%di
	popw	%si
	popw	%es
	popw	%ds
	popw	%bp
	lret	$16

/*
 * USHORT ReceiveChain(USHORT MACID, USHORT FrameSize, USHORT ReqHandle,
 *                     RxBufDesc far *BufDesc, UCHAR far *Indicate,
 *                     USHORT ProtDS);
 */
ReceiveChain:
	pushw	%bp
	movw	%sp, %bp
	pushw	%ds
	pushw	%es
	pushw	%si
	pushw	%di
	pushw	%cx

	movw	6(%bp), %ax		# ProtDS
	movw	%ax, %es		# our data, as the copy destination
	lds	12(%bp), %si		# ds:si = BufDesc
	movw	2(%si), %cx		# RxData[0].RxDataLen
	lds	4(%si), %si		# ds:si = the frame itself
	cmpw	$RXBUFSZ, %cx
	jbe	1f
	movw	$RXBUFSZ, %cx
1:	movw	%cx, %bx
	movw	$RxBuf, %di
	rep	movsb

	movw	%es, %ax
	movw	%ax, %ds		# back to our data
	movw	%bx, RxLen
	movw	%bx, RxCopied
	movw	%bx, %cx
	movb	$'C', %al
	call	CheckArpReply
	xorw	%ax, %ax		# NDIS_SUCCESS, we are done with it

	popw	%cx
	popw	%di
	popw	%si
	popw	%es
	popw	%ds
	popw	%bp
	lret	$16

/*
 * USHORT IndicationComplete(USHORT MACID, USHORT ProtDS);
 */
IndicationComplete:
	xorw	%ax, %ax
	lret	$4

/*
 * USHORT RequestConfirm(USHORT ProtID, USHORT MACID, USHORT ReqHandle,
 *                       USHORT Status, USHORT Request, USHORT ProtDS);
 */
RequestConfirm:
	xorw	%ax, %ax
	lret	$12

/*
 * USHORT TransmitConfirm(USHORT ProtID, USHORT MACID, USHORT ReqHandle,
 *                        USHORT Status, USHORT ProtDS);
 */
TransmitConfirm:
	xorw	%ax, %ax
	lret	$10

/*
 * USHORT StatusIndication(USHORT MACID, USHORT Param, UCHAR far *Indicate,
 *                         USHORT Opcode, USHORT ProtDS);
 */
StatusIndication:
	xorw	%ax, %ax
	lret	$12

/*
 * Accepts the frame in RxBuf only if it is the ARP reply we asked for,
 * so that a stray broadcast cannot end the test. Called from within an
 * indication with DS set to our data, AL holding the indication kind
 * and CX the number of bytes we got. Clobbers AX, BX, CX, SI, DI.
 */
CheckArpReply:
	cmpw	$(ArpEnd - ArpFrame), %cx
	jb	1f
	cmpw	$0x0608, RxBuf+12	# ARP, byte-swapped
	jne	1f
	cmpw	$0x0200, RxBuf+20	# reply, byte-swapped
	jne	1f
	movw	$RxBuf+38, %si		# target protocol address
	movw	$ArpFrame+28, %di	# our sender protocol address
	movw	$4, %cx
	pushw	%es
	pushw	%ds
	popw	%es
	repe	cmpsb
	popw	%es
	jne	1f
	movb	%al, RxVia
	movb	$1, RxFlag
1:	ret

/************************************************************************/

Print:
	movb	$9, %ah
	int	$0x21
	ret

PrintChar:
	movb	%al, %dl
	movb	$2, %ah
	int	$0x21
	ret

PrintHexWord:
	pushw	%ax
	movb	%ah, %al
	call	PrintHexByte
	popw	%ax
PrintHexByte:
	pushw	%ax
	shrb	$4, %al
	call	PrintHexNib
	popw	%ax
PrintHexNib:
	andb	$0xf, %al
	addb	$'0', %al
	cmpb	$'9', %al
	jbe	1f
	addb	$('a' - '0' - 10), %al
1:	jmp	PrintChar

PmName:
	.asciz	"PROTMAN$"

BannerMsg:
	.ascii	"NDIS test protocol\r\n$"
NoChainMsg:
	.ascii	"using the ReceiveLookahead path\r\n$"
NoProtmanMsg:
	.ascii	"FAIL: no PROTMAN$\r\n$"
NoModuleMsg:
	.ascii	"FAIL: no module registered with PROTMAN$\r\n$"
BindFailMsg:
	.ascii	"FAIL: bind rejected\r\n$"
FilterFailMsg:
	.ascii	"FAIL: SetPacketFilter rejected\r\n$"
SendFailMsg:
	.ascii	"FAIL: TransmitChain rejected\r\n$"
SentMsg:
	.ascii	"ARP request sent\r\n$"
TimeoutMsg:
	.ascii	"FAIL: no reply\r\n$"
GotMsg:
	.ascii	"got a frame via $"
BoundMsg:
	.ascii	"bound to the MAC\r\n$"
LenMsg:
	.ascii	", len 0x$"
CopiedMsg:
	.ascii	", copied 0x$"
CrLfMsg:
	.ascii	"\r\n$"
PassMsg:
	.ascii	"PASS\r\n$"
FailMsg:
	.ascii	"FAILED\r\n$"
