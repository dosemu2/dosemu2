signature	db	'PKT DRVR',0
signature_len	equ	$-signature

packet_int_msg	db	CR,LF
		db	"Error: <packet_int_no> should be in the range 0x60 to 0x80"
		db	'$'

verify_packet_int:
;enter with no special registers.
;exit with cy,dx-> error message if the packet int was bad,
;  or nc,zr,es:bx -> current interrupt if there is a packet driver there.
;  or nc,nz,es:bx -> current interrupt if there is no packet driver there.
	cmp	packet_int_no,60h	;make sure that the packet interrupt
	jb	verify_packet_int_bad	;  number is in range.
	cmp	packet_int_no,80h
	jbe	verify_packet_int_ok
verify_packet_int_bad:
	mov	dx,offset packet_int_msg
	stc
	ret
verify_packet_int_ok:

	mov	ah,35h			;get their packet interrupt.
	mov	al,packet_int_no
	int	21h

	lea	di,3[bx]		;see if there is already a signature
	mov	si,offset signature	;  there.
	mov	cx,signature_len
	repe	cmpsb
	clc
	ret
