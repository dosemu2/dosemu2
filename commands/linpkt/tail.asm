;  This code has been modified by Jason Gorden
;  gorden@jegnixa.hsc.missouri.edu to be used with the Linux DOSEMU
;  virtual packet driver.

;   PC/FTP Packet Driver source, conforming to version 1.05 of the spec
;   Updated to version 1.08 Feb. 17, 1989.
;   Copyright 1988-1992 Russell Nelson

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

code	segment word public
	assume	cs:code, ds:code

	extrn	phd_dioa: byte
	extrn	phd_environ: word
	extrn	flagbyte: byte
	extrn	receive_ptr: dword
	extrn	rcv_isr: word

	include	printnum.asm
	include	decout.asm
	include	digout.asm
	include	chrout.asm

end_tail_1	label	byte		; end of the delayed init driver

;usage_msg is of the form "usage: driver [-d -n] <packet_int_no> <args>"
	extrn	usage_msg: byte

;copyright_msg is of the form:
;"Packet driver for the foobar",CR,LF
;"Portions Copyright 19xx, J. Random Hacker".
	extrn	copyright_msg: byte

copyleft_msg	label	byte
 db "Packet driver skeleton copyright 1988-92, Crynwr Software.",CR,LF
 db "This program is free software; see the file COPYING for details.",CR,LF
 db "NO WARRANTY; see the file COPYING for details.",CR,LF
 db CR,LF
crlf_msg	db	CR,LF,'$'

no_resident_msg	label	byte
 db CR,LF,"*** Packet driver failed to initialize the board ***",CR,LF,'$'

;parse_args should parse the arguments.
;called with ds:si -> immediately after the packet_int_no.
;	extrn	parse_args: near
;not needed!

;print_parameters should print the arguments.
;	extrn	print_parameters: near
;not needed

	extrn	our_isr: near, their_isr: dword
	extrn	packet_int_no: byte
	extrn	is_at: byte, sys_features: byte
	extrn	int_no: byte
	extrn	driver_class: byte

location_msg	db	"Packet driver is at segment ",'$'

packet_int_no_name	db	"Packet interrupt number ",'$'
eaddr_msg	db	"My Ethernet address is ",'$'
aaddr_msg	db	"My ARCnet address is ",'$'

already_msg	db	CR,LF,"There is already a packet driver at ",'$'
int_msg		db	CR,LF
		db	"Error: <int_no> should be between 0 and "
int_msg_num	label	word
		db	"15 inclusive", '$'

our_address	db	EADDR_LEN dup(?)
	public	etopen_diagn
etopen_diagn	db	0		; errorlevel from etopen if set

;get the address of the interface.
;enter with es:di -> place to get the address, cx = size of address buffer.
;exit with nc, cx = actual size of address, or cy if buffer not big enough.
	extrn	get_address: near

already_error:
	mov	dx,offset already_msg
	mov	di,offset packet_int_no
	call	print_number
	mov	ax,4c05h		; give errorlevel 5
	int	21h

usage_error:
	mov	dx,offset usage_msg
	public	error
error:
	mov	ah,9
	int	21h
	mov	ax,4c0ah		; give errorlevel 10
	int	21h

	include	timeout.asm

	public	start_1
start_1:
	cld

	mov	dx,offset copyright_msg
	mov	ah,9
	int	21h

	mov	dx,offset copyleft_msg
	mov	ah,9
	int	21h

;print the location we were loaded at.
	mov	dx,offset location_msg
	mov	ah,9
	int	21h

	mov	ax,cs			;print cs as a word.
	call	wordout

	mov	al,60h
	mov	packet_int_no,al

	mov	dx,offset crlf_msg
	mov	ah,9
	int	21h

	call	take_packet_int

	push	ds
	pop	es
	mov	di,offset our_address
	mov	cx,16
	call	get_address

	mov	dx,offset eaddr_msg
	mov	ah,9
	int	21h

	mov	si,offset our_address
	call	print_ether_addr

	mov	dx,offset crlf_msg	;can't depend on DOS to newline for us.
	mov	ah,9
	int	21h

delayed_open_1:

;	mov	ah,49h			;free our environment, because
;	mov	es,phd_environ		;  we won't need it.
;	int	21h

	mov	bx,1			;get the stdout handle.
	mov	ah,3eh			;close it in case they redirected it.
	int	21h

;	pop	dx			;get their ending address.
	add	dx,0fh			;round up to next highest paragraph.
	mov	cl,4
	shr	dx,cl
	mov dx,255		;About 4k
	mov	ah,31h			;terminate, stay resident.
	mov	al,etopen_diagn		; errorlevel (0 - 9, just diagnostics)
	int	21h

some_call:
	ret


take_packet_int:
;	mov	ah,35h			;remember their packet interrupt.
;	int	21h
;	mov	their_isr.offs,bx
;	mov	their_isr.segm,es

	mov	al,60h
	mov	ah,25h			;install our packet interrupt
	mov	dx,offset our_isr
	int	21h

	mov	al,61h
	mov	ah,25h			;install our packet interrupt
	mov	dx,offset rcv_isr
	int	21h
	ret

	include	verifypi.asm
	include	getnum.asm
	include	getdig.asm
	include	skipblk.asm
	include	printea.asm

code	ends

	end
	end
clude	verifypi.asm
	include	getnum.asm
	include	getdig.asm
	include	skipblk.asm
	include	printea.asm

code	ends

	end
num.asm
	include	getdig.asm
	include	skipblk.asm
	include	printea.asm

code	ends

	end

nd











	end













	end
































0e5h

	pop	ds
	pop	cx
	pop	bx
	pop	ax
	iret
;pkt_size dw	60h



take_packet_int:
;	mov	ah,35h			;remember their packet interrupt.
;	int	21h
;	mov	their_isr.offs,bx
;	mov	their_isr.segm,es

	mov	al,60h
	mov	ah,25h			;install our packet interrupt
	mov	dx,offset our_isr
	int	21h

	mov	al,61h
	mov	ah,25h			;install our packet interrupt
	mov	dx,offset rcv_isr
	int	21h
	ret

	include	verifypi.asm
	include	getnum.asm
	include	getdig.asm
	include	skipblk.asm
	include	printea.asm

code	ends

	end
	end
clude	verifypi.asm
	include	getnum.asm
	include	getdig.asm
	include	skipblk.asm
	include	printea.asm

code	ends

	end
num.asm
	include	getdig.asm
	include	skipblk.asm
	include	printea.asm

code	ends

	end

nd











	end













	end
































