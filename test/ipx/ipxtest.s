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
 * IPX exercise program, for testing dosemu's IPX over UDP relay.
 *
 * Opens a socket, broadcasts one packet tagged with its argument,
 * prints every packet that arrives and answers each one that carries
 * an upper case tag with a unicast back to its sender, tagged with
 * the same letter in lower case. That covers all four paths at once:
 * broadcast out, broadcast in, unicast out, unicast in.
 *
 * Usage: ipxtest <tag>
 *
 * It is plain DOS and plain IPX, so it runs under any IPX provider -
 * dosemu's, DOSBox's, or a real one - and the sides can be compared.
 *
 * Author: Stas Sergeev
 */

.code16
.text
	.globl	_start16
_start16:

/* offsets in the event control block */
ECB_INUSE	= 8
ECB_CC		= 9
ECB_SOCKET	= 10
ECB_IMMED	= 28
ECB_FRAGCNT	= 34
ECB_FRAGOFF	= 36
ECB_FRAGSEG	= 38
ECB_FRAGSZ	= 40
ECB_LEN		= 42

/* offsets in the IPX header */
IPX_DNET	= 6
IPX_DNODE	= 10
IPX_DSOCK	= 16
IPX_SNET	= 18
IPX_SNODE	= 22
IPX_HDR		= 30

SOCKNO		= 0x4545	/* the same either way round */
RXBUFS		= 4		/* receive ECBs; one is not enough, as
				   DOSBox hands a broadcast back to its
				   own sender and so eats one per send */
RXBUFSZ		= IPX_HDR + 16
PAYLOAD		= 8
ROUNDS		= 20		/* about five seconds in all */

	cld
	/* the tag to send is our only argument */
	movb	$'?', %al
	movw	$0x81, %si
1:	cmpb	$13, (%si)
	je	2f
	cmpb	$' ', (%si)
	jne	3f
	incw	%si
	jmp	1b
3:	movb	(%si), %al
2:	movb	%al, Tag
	movb	%al, TxBuf+IPX_HDR+4

	/* the IPX entry point, if there is an IPX driver at all */
	movw	$0x7a00, %ax
	int	$0x2f
	cmpb	$0xff, %al
	jne	.LnoIpx
	movw	%di, Entry
	movw	%es, Entry+2
	pushw	%cs
	popw	%es

	movw	$0, %bx			/* open socket */
	movb	$0, %al			/* short lived */
	movw	$SOCKNO, %dx
	lcall	*Entry
	testb	%al, %al
	jnz	.LnoSock

	movw	$0x9, %bx		/* our own address */
	movw	$MyAddr, %si
	lcall	*Entry
	movw	$NodeMsg, %si
	call	Print
	movw	$MyAddr+4, %si		/* the node follows the network */
	call	PrintNode
	call	Crlf

	call	PostListen
	call	SendBcast		/* announce ourselves, once */

	movw	$ROUNDS, Rounds
.Lloop:
	call	Wait
	call	PollRecv
	decw	Rounds
	jnz	.Lloop

	movw	$1, %bx			/* close socket */
	movw	$SOCKNO, %dx
	lcall	*Entry

	movw	$DoneMsg, %si
	call	Print
	movw	Received, %ax
	call	PrintDec
	call	Crlf
	movb	$0, %al
	jmp	.Lexit

.LnoIpx:
	movw	$NoIpxMsg, %si
	call	Print
	movb	$1, %al
	jmp	.Lexit
.LnoSock:
	movw	$NoSockMsg, %si
	call	Print
	movb	$1, %al
.Lexit:
	movb	$0x4c, %ah
	int	$0x21

/* posts every receive ECB of the pool that is not posted already */
PostListen:
	movw	$RxEcb, %si
	movw	$RxBuf, %di
	movw	$RXBUFS, %cx
1:	pushw	%cx
	cmpb	$0, RxState-RxEcb(%si)
	jne	2f
	movw	$SOCKNO, ECB_SOCKET(%si)
	movw	$1, ECB_FRAGCNT(%si)
	movw	%di, ECB_FRAGOFF(%si)
	movw	%cs, ECB_FRAGSEG(%si)
	movw	$RXBUFSZ, ECB_FRAGSZ(%si)
	movb	$1, RxState-RxEcb(%si)
	pushw	%si
	pushw	%di
	movw	$0x4, %bx		/* listen for packet */
	lcall	*Entry
	popw	%di
	popw	%si
2:	addw	$ECB_LEN, %si
	addw	$RXBUFSZ, %di
	popw	%cx
	loop	1b
	ret

/* broadcasts one packet with our tag in it */
SendBcast:
	movl	$0xffffffff, TxEcb+ECB_IMMED
	movw	$0xffff, TxEcb+ECB_IMMED+4
	movl	$0, TxBuf+IPX_DNET
	movl	$0xffffffff, TxBuf+IPX_DNODE
	movw	$0xffff, TxBuf+IPX_DNODE+4
	jmp	SendTx

/*
 * Answers the packet whose buffer is at DI with a unicast to its
 * sender, so that the specific-address path is covered too.
 */
SendReply:
	pushw	%si
	pushw	%cx
	pushw	%di
	movw	%di, %si
	addw	$IPX_SNET, %si		/* their network, then their node */
	movw	$TxBuf+IPX_DNET, %di
	movw	$10, %cx
	rep	movsb
	popw	%di
	pushw	%di
	movw	%di, %si
	addw	$IPX_SNODE, %si
	movw	$TxEcb+ECB_IMMED, %di	/* a unicast goes to the node itself */
	movw	$6, %cx
	rep	movsb
	popw	%di

	movb	Tag, %al
	orb	$0x20, %al		/* the answer carries the lower case */
	movb	%al, TxBuf+IPX_HDR+4
	call	SendTx
	movb	Tag, %al
	movb	%al, TxBuf+IPX_HDR+4
	popw	%cx
	popw	%si
	ret

/* sends TxBuf, whose destination is already set */
SendTx:
	movw	$SOCKNO, TxEcb+ECB_SOCKET
	movw	$1, TxEcb+ECB_FRAGCNT
	movw	$TxBuf, TxEcb+ECB_FRAGOFF
	movw	%cs, TxEcb+ECB_FRAGSEG
	movw	$IPX_HDR+PAYLOAD, TxEcb+ECB_FRAGSZ
	movw	$SOCKNO, TxBuf+IPX_DSOCK
	movb	$4, TxBuf+5		/* packet type: IPX */
	movw	$TxEcb, %si
	movw	$0x3, %bx		/* send packet */
	lcall	*Entry
	ret

/* prints, and answers, everything that has arrived */
PollRecv:
	movw	$0xa, %bx		/* relinquish control */
	lcall	*Entry
	movw	$RxEcb, %si
	movw	$RxBuf, %di
	movw	$RXBUFS, %cx
1:	pushw	%cx
	cmpb	$0, RxState-RxEcb(%si)
	je	2f			/* not posted, nothing to collect */
	cmpb	$0, ECB_INUSE(%si)
	jne	2f			/* still listening */
	movb	$0, RxState-RxEcb(%si)
	cmpb	$0, ECB_CC(%si)
	jne	2f			/* completed with an error */
	incw	Received

	pushw	%si			/* the ECB */
	pushw	%di			/* its buffer, which Print clobbers */
	movw	%di, %si
	pushw	%si
	movw	$RxMsg, %si
	call	Print
	popw	%si
	movb	IPX_HDR+4(%si), %al	/* their tag */
	pushw	%si
	call	PrintChar
	movw	$FromMsg, %si
	call	Print
	popw	%si
	addw	$IPX_SNODE, %si
	call	PrintNode
	call	Crlf
	popw	%di
	pushw	%di
	movb	IPX_HDR+4(%di), %al	/* only answer the first hand ones */
	cmpb	$'A', %al
	jb	3f
	cmpb	$'Z', %al
	ja	3f
	call	SendReply
3:	popw	%di
	popw	%si
2:	addw	$ECB_LEN, %si
	addw	$RXBUFSZ, %di
	popw	%cx
	loop	1b
	call	PostListen
	ret

/* waits about a fifth of a second, letting the driver run */
Wait:
	movb	$0, %ah
	int	$0x1a
	movw	%dx, %si
	addw	$4, %si
1:	movw	$0xa, %bx
	lcall	*Entry
	movb	$0, %ah
	int	$0x1a
	cmpw	%si, %dx
	jb	1b
	ret

PrintNode:
	movw	$6, %cx
1:	pushw	%cx
	movb	(%si), %al
	call	PrintHex
	incw	%si
	popw	%cx
	loop	1b
	ret

PrintHex:
	pushw	%ax
	shrb	$4, %al
	call	PrintNib
	popw	%ax
	andb	$0xf, %al
PrintNib:
	addb	$'0', %al
	cmpb	$'9', %al
	jbe	PrintChar
	addb	$7, %al
PrintChar:
	pushw	%si
	movb	%al, Ch
	movw	$Ch, %si
	movw	$1, %cx
	call	Write
	popw	%si
	ret

/* writes AX as a decimal number */
PrintDec:
	pushw	%si
	pushw	%di
	movw	$NumBuf+6, %di
	movb	$0, (%di)
	movw	$10, %cx
1:	decw	%di
	xorw	%dx, %dx
	divw	%cx
	addb	$'0', %dl
	movb	%dl, (%di)
	testw	%ax, %ax
	jnz	1b
	movw	%di, %si
	call	Print
	popw	%di
	popw	%si
	ret

Crlf:
	movw	$CrlfMsg, %si
/* writes the NUL-terminated string at SI */
Print:
	movw	%si, %di
	xorw	%cx, %cx
1:	cmpb	$0, (%di)
	je	2f
	incw	%di
	incw	%cx
	jmp	1b
2:	jcxz	3f
Write:
	pushw	%ax
	pushw	%bx
	pushw	%dx
	movb	$0x40, %ah		/* write to stdout */
	movw	$1, %bx
	movw	%si, %dx
	int	$0x21
	popw	%dx
	popw	%bx
	popw	%ax
3:	ret

Entry:		.long	0
Tag:		.byte	'?'
Ch:		.byte	0
Rounds:		.word	0
Received:	.word	0
MyAddr:		.space	10, 0
NumBuf:		.space	7, 0

NodeMsg:	.asciz	"ipxtest: node "
RxMsg:		.asciz	"rx "
FromMsg:	.asciz	" from "
DoneMsg:	.asciz	"ipxtest: done, rx "
NoIpxMsg:	.asciz	"ipxtest: no IPX driver\r\n"
NoSockMsg:	.asciz	"ipxtest: cannot open socket\r\n"
CrlfMsg:	.asciz	"\r\n"

	.align	2
TxEcb:		.space	ECB_LEN, 0
RxEcb:		.space	ECB_LEN * RXBUFS, 0
RxState:	.space	ECB_LEN * RXBUFS, 0	/* one byte of each is used */
TxBuf:		.space	IPX_HDR, 0
		.ascii	"IPX-x   "		/* the tag sits at offset 4 */
RxBuf:		.space	RXBUFSZ * RXBUFS, 0
