version	equ	1

;  This is a port of the Crynwr packet driver skeleton to the Linux
;  DOSEMUlator by Jason Gorden, gorden@jegnixa.hsc.missouri.edu.

;  The following people have contributed to this code: David Horne, Eric
;  Henderson, and Bob Clements.

;  Copyright, 1988-1992, Russell Nelson, Crynwr Software

;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation, version 1.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

	include	defs.asm

code	segment	word public
	assume	cs:code, ds:code

	public	usage_msg
usage_msg	db	"usage: LINPKT <packet_int_no>",CR,LF,'$'

	public	copyright_msg
copyright_msg	db	"Packet driver for LINPKT, version 1.0",CR,LF
		db	"Modified By Jason Gorden gorden@jegnixa.hsc.missouri.edu."
		db	CR,LF,'$'

	extrn	set_recv_isr: near

	public	get_address

get_address:
	mov ax,60h	;pkt_helper get_address function
	mov	dx,6	;get_address function in the pkt_helper function.
	int 0e6h    ;This needs to put e-addr at ES:DI
	ret

code	ends

	end







