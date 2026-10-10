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
 * htget for the Microsoft TCP/IP sockets interface.
 *
 * Fetches one URL over the MS TCP/IP stack of MS Network Client or
 * LAN Manager, which sits on top of an NDIS MAC driver - dosemu's own
 * one, for instance. That makes it a way to exercise the whole NDIS
 * path with real, unmodified Microsoft software above it, which the
 * loopback tests in test/ndis cannot do.
 *
 * Usage: htget <dotted-ip> [<port>] [<path>]
 *
 * The response, headers and all, goes to stdout; progress and errors
 * go to stderr. protman.dos, the MAC driver, tcpdrv.dos, tcptsr and
 * sockets.exe (or socktsr.exe) have to be loaded, see README.
 *
 * The sockets interface used here is described in README and in
 * mstcp.h; it was recovered from DOS_SOCK.LIB of the Microsoft TCP/IP
 * Sockets Development Kit 1.0, as no specification of it seems to
 * exist. 386 instructions are used in a few places, which is no
 * restriction under dosemu.
 *
 * Author: Stas Sergeev
 */

#include "mstcp.h"

.code16
.text
	.globl	_start16
_start16:
	cld
	call	ParseArgs
	jc	.LusageErr

	/*
	 * Our PSP is the process id that the sockets TSR knows us by;
	 * it goes into the locator block and into every request.
	 */
	movb	$0x62, %ah
	int	$0x21
	movw	%bx, Loc+LOC_PID
	movw	%bx, Req+RB_PID

	/* the far pointers in the request block are only known now */
	movw	%ds, Req+RB_ERR+2
	movw	%ds, Req+RB_RET+2

	call	DrvOpen
	jc	.Lfail

	call	Fetch
	pushfw
	pushw	%dx
	call	DrvClose
	popw	%dx
	popfw
	jc	.Lfail

	xorb	%al, %al
	jmp	.Lexit

.LusageErr:
	movw	$UsageMsg, %dx
.Lfail:
	call	PrintErr
	movb	$1, %al
.Lexit:
	movb	$0x4c, %ah
	int	$0x21

/*
 * Opens TCPDRV$ and binds to the sockets TSR behind it, which hands
 * back the entry point that all the socket calls then go through.
 * Returns CY and a message in DX on failure.
 */
DrvOpen:
	movw	$0x3d00, %ax		/* open, read only */
	movw	$DrvName, %dx
	int	$0x21
	jc	.LnoDriver
	movw	%ax, Handle

	movb	$LOC_BIND, Loc+LOC_OP
	call	LocRequest
	jc	.LnoBind

	movw	Loc+LOC_ENTRY, %ax
	movw	%ax, Entry
	movw	Loc+LOC_ENTRY+2, %ax
	movw	%ax, Entry+2
	orw	Entry, %ax		/* a null entry point is no entry */
	jz	.LnoBind
	clc
	ret

.LnoDriver:
	movw	$NoDriverMsg, %dx
	stc
	ret

.LnoBind:
	movw	$NoBindMsg, %dx
	pushw	%dx
	call	DrvCloseHandle
	popw	%dx
	stc
	ret

/*
 * Unbinds and closes the driver again. The TSR keeps per-process
 * state, so skipping this leaks it for as long as the TSR is loaded.
 */
DrvClose:
	movb	$LOC_UNBIND, Loc+LOC_OP
	call	LocRequest
DrvCloseHandle:
	movb	$0x3e, %ah
	movw	Handle, %bx
	int	$0x21
	ret

/*
 * Passes the locator block to TCPDRV$ via an IOCTL read.
 * Returns CY on failure or on a non-zero status in the block.
 */
LocRequest:
	movb	$0, Loc+LOC_STAT
	movw	$0x4402, %ax		/* ioctl, read from character device */
	movw	Handle, %bx
	movw	$Loc, %dx
	movw	$LOC_LEN, %cx
	int	$0x21
	jc	1f
	cmpb	$0, Loc+LOC_STAT
	je	1f
	stc
1:	ret

/*
 * socket(), connect(), send(), then recv() until the peer is done.
 * Returns CY and a message in DX on failure.
 */
Fetch:
	/* socket(AF_INET, SOCK_STREAM, 0) */
	movw	$AF_INET, Req+RB_ARGS
	movw	$SOCK_STREAM, Req+RB_ARGS+2
	movw	$0, Req+RB_ARGS+4
	movb	$SO_SOCKET, %al
	call	SockCall
	jc	.LnoSocket
	movw	%ax, Sd

	/* connect(sd, &sa, sizeof(sa)) */
	movw	Sd, %ax
	movw	%ax, Req+RB_ARGS
	movw	$SockAddr, Req+RB_ARGS+2
	movw	%ds, Req+RB_ARGS+4
	movw	$SOCKADDR_LEN, Req+RB_ARGS+6
	movb	$SO_CONNECT, %al
	call	SockCall
	jc	.LnoConnect

	movw	$ConnectedMsg, %dx
	call	PrintErr

	call	BuildRequest		/* returns the length in CX */

	/* send(sd, buf, len, 0) */
	movw	Sd, %ax
	movw	%ax, Req+RB_ARGS
	movw	$ReqBuf, Req+RB_ARGS+2
	movw	%ds, Req+RB_ARGS+4
	movw	%cx, Req+RB_ARGS+6
	movw	$0, Req+RB_ARGS+8
	movw	$0, Req+RB_ARGS+10	/* no destination address */
	movw	$0, Req+RB_ARGS+12
	movw	$SOCKADDR_LEN, Req+RB_ARGS+14
	movb	$0, Req+RB_ARGS+16	/* send, not sendto */
	movb	$SO_SEND, %al
	call	SockCall
	jc	.LnoSend

.Lrecv:
	/* recv(sd, buf, sizeof(buf), 0) */
	movw	Sd, %ax
	movw	%ax, Req+RB_ARGS
	movw	$RecvBuf, Req+RB_ARGS+2
	movw	%ds, Req+RB_ARGS+4
	movw	$RECVBUF_LEN, Req+RB_ARGS+6
	movw	$0, Req+RB_ARGS+8
	movw	$0, Req+RB_ARGS+10	/* no source address wanted */
	movw	$0, Req+RB_ARGS+12
	movw	$0, Req+RB_ARGS+14
	movw	$0, Req+RB_ARGS+16
	movb	$2, Req+RB_ARGS+18	/* recv, not recvfrom */
	movb	$SO_RECV, %al
	call	SockCall
	jc	.LnoRecv
	testw	%ax, %ax
	jz	.Ldone			/* the peer closed the connection */

	movw	%ax, %cx
	movzwl	%cx, %eax
	addl	%eax, Received
	movb	$0x40, %ah		/* write to stdout */
	movw	$1, %bx
	movw	$RecvBuf, %dx
	int	$0x21
	jc	.LnoWrite
	jmp	.Lrecv

.Ldone:
	call	PrintReceived
	call	CloseSocket
	clc
	ret

.LnoSocket:
	movw	$NoSocketMsg, %dx
	jmp	.Lerr
.LnoConnect:
	movw	$NoConnectMsg, %dx
	jmp	.LerrClose
.LnoSend:
	movw	$NoSendMsg, %dx
	jmp	.LerrClose
.LnoRecv:
	movw	$NoRecvMsg, %dx
	jmp	.LerrClose
.LnoWrite:
	movw	$0, SockErr		/* this one is not the TSR's fault */
	movw	$NoWriteMsg, %dx
.LerrClose:
	pushw	%dx
	call	CloseSocket
	popw	%dx
.Lerr:
	stc
	ret

CloseSocket:
	movw	Sd, %ax
	movw	%ax, Req+RB_ARGS
	movb	$SO_CLOSE, %al
	call	SockCall
	ret

/*
 * Issues one socket call: AL is the opcode, the arguments are already
 * in Req+RB_ARGS. Returns the value of the call in AX, CY if it is
 * negative, with its errno left in SockErr.
 */
SockCall:
	pushw	%bx
	pushw	%cx
	pushw	%dx
	pushw	%si
	pushw	%di
	pushw	%bp
	pushw	%ds
	pushw	%es

	movb	%al, Req+RB_OP
	movw	$0, SockErr
	movw	$0, RetVal
	pushw	%ds
	popw	%es
	movw	$Req, %bx
	lcall	*Entry

	popw	%es
	popw	%ds
	popw	%bp
	popw	%di
	popw	%si
	popw	%dx
	popw	%cx
	popw	%bx

	movw	RetVal, %ax
	testw	%ax, %ax
	js	1f
	clc
	ret
1:	stc
	ret

/*
 * Builds the HTTP request in ReqBuf, returns its length in CX.
 */
BuildRequest:
	movw	$ReqBuf, %di
	movw	$GetStr, %si
	call	AppendStr
	movw	Path, %si
	call	AppendStr
	movw	$ProtoStr, %si
	call	AppendStr
	movw	Host, %si
	call	AppendStr
	movw	$TailStr, %si
	call	AppendStr
	movw	%di, %cx
	subw	$ReqBuf, %cx
	ret

/* appends the NUL-terminated string at SI to DI */
AppendStr:
	lodsb
	testb	%al, %al
	jz	1f
	stosb
	jmp	AppendStr
1:	ret

/*
 * Parses the command line. Returns CY if it does not make sense.
 */
ParseArgs:
	movw	$0x81, %si
	call	NextToken
	jc	.LbadArgs
	movw	%si, Host		/* doubles as the Host: header */
	call	ParseIp
	jc	.LbadArgs

	movw	%di, %si
	call	NextToken
	jc	.LnoMore
	call	ParseNum
	jc	.LbadArgs
	cmpb	$0, (%si)		/* the port is all there is */
	jne	.LbadArgs
	xchgb	%al, %ah		/* ports are in network order */
	movw	%ax, SockPort

	movw	%di, %si
	call	NextToken
	jc	.LnoMore
	movw	%si, Path

.LnoMore:
	movw	$AF_INET, SockFamily
	clc
	ret

.LbadArgs:
	stc
	ret

/*
 * Finds the next token: returns it NUL-terminated at SI, with DI just
 * past its terminator, or CY if the command line is exhausted.
 */
NextToken:
1:	cmpb	$' ', (%si)
	je	2f
	cmpb	$9, (%si)
	jne	3f
2:	incw	%si
	jmp	1b
3:	cmpb	$13, (%si)
	je	.LnoToken
	movw	%si, %di
4:	cmpb	$' ', (%di)
	je	5f
	cmpb	$9, (%di)
	je	5f
	cmpb	$13, (%di)
	je	5f
	incw	%di
	jmp	4b
5:	movb	$0, (%di)
	incw	%di
	clc
	ret

.LnoToken:
	stc
	ret

/*
 * Parses the dotted quad at SI into the address in SockAddr.
 */
ParseIp:
	pushw	%si
	pushw	%bx
	movw	$SockIp, %bx
	movw	$4, %bp
1:	call	ParseNum
	jc	.LbadIp
	cmpw	$255, %ax
	ja	.LbadIp
	movb	%al, (%bx)
	incw	%bx
	decw	%bp
	jz	2f
	cmpb	$'.', (%si)
	jne	.LbadIp
	incw	%si
	jmp	1b
2:	cmpb	$0, (%si)		/* nothing may follow the last octet */
	jne	.LbadIp
	popw	%bx
	popw	%si
	clc
	ret

.LbadIp:
	popw	%bx
	popw	%si
	stc
	ret

/*
 * Parses the decimal number at SI into AX, leaving SI on the first
 * character that is not a digit. CY if there is no digit at all or
 * the number does not fit in a word.
 */
ParseNum:
	pushw	%cx
	pushw	%dx
	xorw	%ax, %ax
	xorw	%cx, %cx		/* CH stays zero: CX is the digit */
	xorw	%dx, %dx
	movw	$0, NumDigits
1:	movb	(%si), %cl
	cmpb	$'0', %cl
	jb	2f
	cmpb	$'9', %cl
	ja	2f
	subb	$'0', %cl
	pushw	%cx
	movw	$10, %cx
	mulw	%cx			/* DX:AX = AX * 10 */
	popw	%cx
	testw	%dx, %dx
	jnz	.LbadNum
	addw	%cx, %ax
	jc	.LbadNum
	incw	%si
	incw	NumDigits
	jmp	1b
2:	cmpw	$0, NumDigits
	je	.LbadNum
	popw	%dx
	popw	%cx
	clc
	ret

.LbadNum:
	popw	%dx
	popw	%cx
	stc
	ret

/*
 * Prints the message in DX on stderr, followed by the
 * errno of the last socket call if there is one.
 */
PrintErr:
	movw	%dx, %si
	call	PrintStr
	cmpw	$0, SockErr
	je	1f
	movw	$ErrnoMsg, %si
	call	PrintStr
	movzwl	SockErr, %eax
	call	PrintDec
1:	movw	$CrLf, %si
	call	PrintStr
	ret

PrintReceived:
	movw	$ReceivedMsg, %si
	call	PrintStr
	movl	Received, %eax
	call	PrintDec
	movw	$BytesMsg, %si
	call	PrintStr
	movw	$CrLf, %si
	call	PrintStr
	ret

/*
 * Writes the NUL-terminated string at SI to stderr. NUL rather than
 * the usual '$', because the messages name devices like TCPDRV$.
 */
PrintStr:
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx
	pushw	%di
	movw	%si, %di
1:	cmpb	$0, (%di)
	je	2f
	incw	%di
	jmp	1b
2:	movw	%di, %cx
	subw	%si, %cx
	jcxz	3f
	movb	$0x40, %ah
	movw	$2, %bx
	movw	%si, %dx
	int	$0x21
3:	popw	%di
	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	ret

/* writes EAX as a decimal number to stderr */
PrintDec:
	pushw	%si
	pushw	%di
	movw	$NumBuf+11, %di
	movb	$0, (%di)
	movl	$10, %ecx
1:	decw	%di
	xorl	%edx, %edx
	divl	%ecx
	addb	$'0', %dl
	movb	%dl, (%di)
	testl	%eax, %eax
	jnz	1b
	movw	%di, %si
	call	PrintStr
	popw	%di
	popw	%si
	ret

/************************************************************************/

DrvName:
	.asciz	MSTCP_DEVICE_NAME

/* the locator block, exchanged with TCPDRV$ by an IOCTL read */
Loc:
	.space	LOC_LEN, 0

Entry:		.long	0		/* the sockets entry point */
Handle:		.word	0
Sd:		.word	0
Received:	.long	0
NumDigits:	.word	0

/* the request block that every socket call goes through */
Req:
	.byte	0			/* RB_OP: opcode */
	.word	SockErr			/* RB_ERR: far pointer to errno */
	.word	0
	.word	RetVal			/* RB_RET: far pointer to the result */
	.word	0
	.word	0			/* RB_PID: our PSP */
	.space	32, 0			/* RB_ARGS: the call's arguments */

SockErr:	.word	0
RetVal:		.word	0

SockAddr:
SockFamily:	.word	0
SockPort:	.word	0x5000		/* port 80, network order */
SockIp:		.long	0
		.space	8, 0

Host:		.word	0		/* both point into the command line */
Path:		.word	DefPath
DefPath:	.asciz	"/"

GetStr:		.asciz	"GET "
ProtoStr:	.asciz	" HTTP/1.0\r\nHost: "
TailStr:	.asciz	"\r\nUser-Agent: htget (dosemu2)\r\nConnection: close\r\n\r\n"

UsageMsg:
	.asciz	"usage: htget <dotted-ip> [<port>] [<path>]"
NoDriverMsg:
	.asciz	"TCPDRV$ not found, load the MS TCP/IP stack first"
NoBindMsg:
	.asciz	"the sockets TSR did not bind us, is sockets.exe loaded?"
NoSocketMsg:
	.asciz	"socket() failed"
NoConnectMsg:
	.asciz	"connect() failed"
NoSendMsg:
	.asciz	"send() failed"
NoRecvMsg:
	.asciz	"recv() failed"
NoWriteMsg:
	.asciz	"writing to stdout failed"
ConnectedMsg:
	.asciz	"connected"
ReceivedMsg:
	.asciz	"received "
BytesMsg:
	.asciz	" bytes"
ErrnoMsg:
	.asciz	", errno "
CrLf:
	.asciz	"\r\n"

NumBuf:		.space	12, 0
ReqBuf:		.space	REQBUF_LEN, 0
RecvBuf:	.space	RECVBUF_LEN, 0
