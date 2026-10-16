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
 * Stand-in for the Microsoft TCP/IP sockets interface, for testing
 * htget.com without MS Network Client.
 *
 * It is a TCPDRV$ character device that binds a process the way the
 * real one does and then answers the socket calls from canned data:
 * connect() succeeds, send() swallows the request and recv() hands
 * out one HTTP response and then end-of-stream.
 *
 * This checks that htget speaks the interface as it was recovered
 * from DOS_SOCK.LIB - the request block layout, the process id, the
 * entry point convention, the error reporting - and nothing about
 * the real TSR, which no free software can stand in for. It rejects
 * what the real one would reject, so that a wrong request block is a
 * failed test rather than a passed one.
 *
 * Author: Stas Sergeev
 */

#include "mstcp.h"

/* errors, as in sock_err.h */
#define EBADF		9
#define EINVAL		22
#define ENOTSOCK	108

#define TEST_SD		3	/* the descriptor we hand out */

/* as much of a request block as there can be */
#define REQ_LEN		(RB_ARGS + 32)

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

Header:
	.long	-1
	.word	0xc000		# character device, supports IOCTL
	.word	Strat
	.word	Intr
	.ascii	"TCPDRV$"
	.byte	' '

RHPtr:		.long 0
BoundPid:	.word 0		# the process that is bound to us
Sd:		.word 0		# the descriptor we handed out, or 0
Sent:		.word 0		# whether the response was picked up

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
 * The locator: binds or unbinds the calling process.
 */
Ioctl:
	pushw	%ds
	lds	%es:IOCTL_XFEROF(%di), %si	# ds:si = locator block

	movb	LOC_OP(%si), %al
	cmpb	$LOC_BIND, %al
	je	.Lbind
	cmpb	$LOC_UNBIND, %al
	je	.Lunbind
	movb	$EINVAL, LOC_STAT(%si)
	jmp	.Ldone

.Lbind:
	movw	LOC_PID(%si), %ax
	testw	%ax, %ax		# a process id is mandatory
	jz	.LbadPid
	movw	%ax, %cs:BoundPid
	movw	$Dispatch, LOC_ENTRY(%si)
	movw	%cs, LOC_ENTRY+2(%si)
	movb	$0, LOC_STAT(%si)
	jmp	.Ldone

.Lunbind:
	movw	LOC_PID(%si), %ax
	cmpw	%cs:BoundPid, %ax
	jne	.LbadPid
	movw	$0, %cs:BoundPid
	movb	$0, LOC_STAT(%si)
	jmp	.Ldone

.LbadPid:
	movb	$EINVAL, LOC_STAT(%si)

.Ldone:
	popw	%ds
	xorw	%ax, %ax
	ret

/*
 * The sockets entry point: ES:BX is the request block. The block is
 * copied in first, so that the segment registers are free afterwards.
 */
Dispatch:
	pusha
	pushw	%ds
	pushw	%es

	pushw	%es
	popw	%ds
	movw	%bx, %si
	pushw	%cs
	popw	%es
	movw	$Req, %di
	movw	$REQ_LEN, %cx
	rep	movsb
	pushw	%cs
	popw	%ds

	movw	$-1, Ret		# until a call says otherwise
	movw	$ENOTSOCK, Err

	movw	Req+RB_PID, %ax		# calls come from the bound process
	testw	%ax, %ax
	jz	.Lreply
	cmpw	BoundPid, %ax
	jne	.Lreply

	movb	Req+RB_OP, %al
	cmpb	$SO_SOCKET, %al
	je	.Lsocket
	cmpb	$SO_CONNECT, %al
	je	.Lconnect
	cmpb	$SO_SEND, %al
	je	.Lsend
	cmpb	$SO_RECV, %al
	je	.Lrecv
	cmpb	$SO_CLOSE, %al
	je	.Lclose
	movw	$EINVAL, Err
	jmp	.Lreply

.Lsocket:
	cmpw	$AF_INET, Req+RB_ARGS
	jne	.Lreply
	cmpw	$SOCK_STREAM, Req+RB_ARGS+2
	jne	.Lreply
	movw	$TEST_SD, Sd
	movw	$0, Sent
	movw	$TEST_SD, Ret
	jmp	.Lok

.Lconnect:
	call	CheckSd
	jc	.Lreply
	cmpw	$SOCKADDR_LEN, Req+RB_ARGS+6
	jne	.Lbadval
	les	Req+RB_ARGS+2, %di	# es:di = the sockaddr
	cmpw	$AF_INET, %es:(%di)
	jne	.Lbadval
	cmpw	$0, %es:2(%di)		# a port is mandatory
	je	.Lbadval
	movw	$0, Ret
	jmp	.Lok

.Lsend:
	call	CheckSd
	jc	.Lreply
	movw	Req+RB_ARGS+6, %ax	# the length is what send() returns
	testw	%ax, %ax
	jz	.Lbadval
	movw	%ax, Ret
	jmp	.Lok

.Lrecv:
	call	CheckSd
	jc	.Lreply
	cmpw	$0, Sent		# the response comes once
	jne	.Leof
	cmpw	$RESP_LEN, Req+RB_ARGS+6
	jb	.Lbadval		# a short buffer is not handled here
	les	Req+RB_ARGS+2, %di	# es:di = the caller's buffer
	movw	$Response, %si
	movw	$RESP_LEN, %cx
	rep	movsb
	movw	$1, Sent
	movw	$RESP_LEN, Ret
	jmp	.Lok

.Leof:
	movw	$0, Ret
	jmp	.Lok

.Lclose:
	call	CheckSd
	jc	.Lreply
	movw	$0, Sd
	movw	$0, Ret
	jmp	.Lok

.Lbadval:
	movw	$EINVAL, Err
	jmp	.Lreply

.Lok:
	movw	$0, Err

.Lreply:
	pushw	%cs
	popw	%ds
	les	Req+RB_RET, %di
	movw	Ret, %ax
	movw	%ax, %es:(%di)
	les	Req+RB_ERR, %di
	movw	Err, %ax
	movw	%ax, %es:(%di)

	popw	%es
	popw	%ds
	popa
	lret

/* CY unless the call is for the descriptor we handed out */
CheckSd:
	movw	Req+RB_ARGS, %ax
	testw	%ax, %ax
	jz	1f
	cmpw	Sd, %ax
	jne	1f
	clc
	ret
1:	movw	$EBADF, Err
	stc
	ret

InstalledMsg:
	.ascii	"test MS TCP/IP sockets stub installed\r\n$"

Response:
	.ascii	"HTTP/1.0 200 OK\r\n"
	.ascii	"Content-Type: text/plain\r\n"
	.ascii	"Content-Length: 12\r\n"
	.ascii	"\r\n"
	.ascii	"hello dosemu"
RESP_LEN = . - Response

	.align	2
Req:	.space	REQ_LEN, 0
Ret:	.word	0
Err:	.word	0

	.align	16
DriverEnd:
