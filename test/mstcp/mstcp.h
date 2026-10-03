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
 * The Microsoft TCP/IP sockets interface of MS Network Client and
 * LAN Manager, as far as it is needed to make socket calls.
 *
 * There appears to be no published specification: the numbers below
 * were recovered from DOS_SOCK.LIB of the Microsoft TCP/IP Sockets
 * Development Kit 1.0, see README for which routine each one comes
 * from.
 */

#ifndef MSTCP_H
#define MSTCP_H

/*
 * The character device of the MS TCP/IP protocol driver. An IOCTL read
 * of a 25-byte locator block on it binds a process to the sockets TSR.
 */
#define MSTCP_DEVICE_NAME	"TCPDRV$"

#define LOC_LEN		25	/* size of the locator block */
#define LOC_BIND	2	/* bind, returns the sockets entry point */
#define LOC_UNBIND	3	/* unbind, on the way out */

/* fields of the locator block */
#define LOC_OP		0	/* byte: LOC_BIND or LOC_UNBIND */
#define LOC_STAT	1	/* byte: zero on success */
#define LOC_PID		2	/* word: the PSP of the calling process */
#define LOC_ENTRY	4	/* dword: the entry point, on LOC_BIND */

/*
 * Fields of the request block that every socket call goes through.
 * The block is passed to the entry point in ES:BX.
 */
#define RB_OP		0	/* byte: the SO_* opcode */
#define RB_ERR		1	/* dword: far pointer to the errno word */
#define RB_RET		5	/* dword: far pointer to the result word */
#define RB_PID		9	/* word: the PSP of the calling process */
#define RB_ARGS		11	/* the arguments of the individual call */

/* opcodes of the socket calls */
#define SO_CLOSE	0x02	/* word s */
#define SO_CONNECT	0x04	/* word s, far name, word namelen */
#define SO_RECV		0x0b	/* recvfrom, with a flavour byte of 2 */
#define SO_SEND		0x0d	/* sendto, with a flavour byte of 0 */
#define SO_SOCKET	0x10	/* word domain, word type, word protocol */

/* as in the SDK headers */
#define AF_INET		2
#define SOCK_STREAM	1
#define SOCKADDR_LEN	16

#define REQBUF_LEN	512
#define RECVBUF_LEN	1024

#endif /* MSTCP_H */
