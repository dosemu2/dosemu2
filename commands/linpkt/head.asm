	include	defs.asm

;  This code has been modified by Jason Gorden
;  gorden@jegnixa.hsc.missouri.edu to be used with the Linux DOSEMU
;  virtual packet driver.

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

code	segment word public
	assume	cs:code, ds:code

	public	phd_environ
	org	2ch
phd_environ	dw	?

	public	phd_dioa
	org	80h
phd_dioa	label	byte

	org	100h
start:
	jmp	start_1
	extrn	start_1: near
	even				;put the stack on a word boundary.

;we use our dioa for a stack space.  Very hard usage has shown that only
;  27 bytes were being used, so 128 should be sufficient.
our_stack	label	byte

	public	packet_int_no, is_at, sys_features, flagbyte
packet_int_no	db	?,?,?,?		; interrupt to communicate.
is_at		db	0		; =1 if we're on an AT.
sys_features	db	0		; 2h = MC   40h = 2nd 8259
flagbyte	db	0
	even

functions	label	word
	dw	f_not_implemented	;0
	dw	f_driver_info		;1
	dw	f_access_type		;2
	dw	f_release_type		;3
	dw	f_send_pkt		;4
	dw	f_terminate		;5
	dw	f_get_address		;6
	dw	f_reset_interface	;7
	dw	f_stop			;8
	dw	f_not_implemented	;9
	dw	f_not_implemented	;10
	dw	f_receive_packet	;11
	dw	f_ok_received_pkt	;12
	dw	f_not_implemented	;13
	dw	f_not_implemented	;14
	dw	f_not_implemented	;15
	dw	f_not_implemented	;16
	dw	f_not_implemented	;17
	dw	f_not_implemented	;18
	dw	f_not_implemented	;19
	dw	f_set_rcv_mode		;20
	dw	f_get_rcv_mode		;21
	dw	f_not_implemented	;22
	dw	f_not_implemented	;23
	dw	f_get_statistics	;24
	dw	f_set_address		;25


	extrn	send_pkt: near
	extrn	get_address: near
	extrn	set_address: near
	extrn	terminate: near
	extrn	reset_interface: near
	extrn	xmit: near
	extrn	recv: near
	extrn	recv_exiting: near

per_handle	struc
in_use		db	0		;non-zero if this handle is in use.
packet_type	db	30 dup(0);associated packet type.
packet_type_len	dw	0		;associated packet type length.
receivero	dd	0		;receiver handler.
receiver_sig	db	8 dup(?)	;signature at the receiver handler.
class		db	?		;interface class
per_handle	ends

handles		per_handle 10 dup(<>)
end_handles	label	byte

driver_name		db	"Linpkt$",0
have_my_address	db	0		;nonzero if our address has been set.
my_address	db	69h,69h,69h,69h,69h,69h,69h,69h,69h,69h,69h
my_address_len	dw	?

rcv_mode_num	dw	3

free_handle	dw	0		; temp, a handle not in use
found_handle	dw	0	; temp, handle for our packet
	public receive_ptr
receive_ptr	dd	0		; the pkt receive service routine

statistics_list	label	dword
packets_in	dw	?,?
packets_out	dw	?,?
bytes_in	dw	?,?
bytes_out	dw	?,?
errors_in	dw	?,?
errors_out	dw	?,?
packets_dropped	dw	?,?		;dropped due to no type handler.

savess		dw	?		;saved during the stack swap.
savesp		dw	?

regs	struc				; stack offsets of incoming regs
_ES	dw	?
_DS	dw	?
_BP	dw	?
_DI	dw	?
_SI	dw	?
_DX	dw	?
_CX	dw	?
_BX	dw	?
_AX	dw	?
_IP	dw	?
_CS	dw	?
_F	dw	?			; flags, Carry flag is bit 0
regs	ends

CY	equ	0001h
EI	equ	0200h

bytes	struc				; stack offsets of incoming regs
	dw	?			; es, ds, bp, di, si are 16 bits
	dw	?
	dw	?
	dw	?
	dw	?
_DL	db	?
_DH	db	?
_CL	db	?
_CH	db	?
_BL	db	?
_BH	db	?
_AL	db	?
_AH	db	?
bytes	ends

	public	our_isr, their_isr
their_isr	dd	0		; original owner of pkt driver int

our_isr:
	jmp	our_isr_0		;the required signature.
	db	'PKT DRVR',0
our_isr_0:
	assume	ds:nothing
	push	ax
	push	bx
	push	cx
	push	dx
	push	si
	push	di
	push	bp
	push	ds
	push	es
	cld
	mov	bx,cs			;set up ds.
	mov	ds,bx
	assume	ds:code
	mov	bp,sp			;we use bp to access the original regs.
	and	_F[bp],not CY		;start by clearing the carry flag.

 our_isr_cont:

	mov	bl,ah			;jump to the correct function.
	mov	bh,0
	cmp	bx,25			;only twenty five functions right now.
	mov	dh,BAD_COMMAND		;in case we find a bad number.
	ja	our_isr_error
	add	bx,bx			;*2
	call	functions[bx]
	jnc	our_isr_return
our_isr_error:
	mov	_DH[bp],dh
	or	_F[bp],CY		;return their carry flag.
our_isr_return:
	pop	es
	pop	ds
	pop	bp
	pop	di
	pop	si
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	iret

f_not_implemented:
	mov	dh,BAD_COMMAND
	stc
	ret


f_driver_info:
;	As of 1.08, the handle is optional, so we no longer verify it.
;	call	verify_handle
	cmp	_AL[bp],0ffh		; correct calling convention?
	jne	f_driver_info_1		; ne = incorrect, fail

	jmp	default_handle
	mov	bx, _BX[bp]
	cmp	[bx].in_use,0		;if it's not in use, it's bad.
	je	default_handle
	mov	al, [bx].class
	mov	_CH[bp], al
	jmp	short got_handle

default_handle:
	mov	al,1			;Default handle num is (1)
	mov	_CH[bp],al
got_handle:

	mov	_BX[bp],1		;version
	mov	al,1
	cbw
	mov	_DX[bp],ax
	mov	_CL[bp],0		;number zero.
	mov	_DS[bp],ds		;point to our name in their ds:si
	mov	_SI[bp],offset driver_name
	mov	al,1			; Functionality 1=Basic only!
	mov	_AL[bp],al
	clc
	ret
f_driver_info_1:
	stc
	ret


f_set_rcv_mode:
	mov	dh,BAD_MODE
	stc
	ret


f_get_rcv_mode:
	call	verify_handle
	mov	ax,rcv_mode_num		;return the current receive mode.
	mov	_AX[bp],ax
	clc
	ret


f_get_statistics:
	call	verify_handle		;just in case.
	mov	_DS[bp],ds
	mov	_SI[bp],offset statistics_list
	clc
	ret

access_type_class:
	mov	dh,NO_CLASS
	stc
	ret

access_type_type:
	mov	dh,NO_TYPE
	stc
	ret

access_type_number:
	mov	dh,NO_NUMBER
	stc
	ret

access_type_bad:
	mov	dh,BAD_TYPE
	stc
	ret

;register caller of pkt TYPE
f_access_type:
	mov	bx,1				;Always give handle 1. TESTING
	mov	[bx].in_use,1		;remember that we're using it.

	mov	ax,_DI[bp]		;remember the receiver function.
	mov	receive_ptr.offs,ax
	mov	ax,_ES[bp]
	mov	receive_ptr.segm,ax

	mov	al, _AL[bp]			;Save the class
	mov	[bx].class, al
	mov	dx,2				;access type function
	mov	ax,60h				;packet helper function
	mov	di,receive_ptr.offs		;Tell dos_helper ES:DI
	mov	es,receive_ptr.segm
	int 0e6h				;dos helper interrupt
	mov	_AX[bp],bx		;return the handle to them.

	clc
	ret

f_release_type:
	call	verify_handle		;mark this handle as being unused.
	mov	[bx].in_use,0
	clc
	ret


f_send_pkt:
	mov	ax,60h					;pkt_helper function.
	mov	dx,4					;send_packet function.
	mov 	ds,_DS[bp]
	mov	si,_SI[bp]
	int	0e6h					;Call dos_helper, cx=len, es:si=buf
	ret

f_terminate:
	call	verify_handle		; must have a handle

f_terminate_1:
	mov	[bx].in_use,0		; mark handle as free
	mov	bx,offset handles	; check that all handles are free
f_terminate_2:
	cmp	[bx].in_use,0		; is this handle free?
	jne	f_terminate_4		; ne = no, so can't exit completely
	add	bx,(size per_handle)	; next handle
	cmp	bx,offset end_handles	; examined all handles?
	jb	f_terminate_2		; b = no, continue examination

	mov	al,packet_int_no	;release our_isr.
	mov	ah,25h
	push	ds
	lds	dx,their_isr
	int	21h
	pop	ds

;
; Now free our memory
;
	push	cs
	pop	es
	mov	ah,49h
	int	21h
	clc
	ret
f_terminate_4:
	mov	dh, CANT_TERMINATE
	stc
	ret


f_get_address:
	mov	cx,6			;Our address length. 6 bytes.
	mov	_CX[bp],cx		;Tell them how long our address is.
	call get_address
        mov	_ES[bp],es
	mov	_DI[bp],di
	clc
	ret

f_set_address:
	mov	dh,CANT_SET
	stc
	ret

f_reset_interface:
	clc
	ret


; Stop the packet driver doing upcalls. Also a following terminate will
; always succed (no in use handles any longer).
f_stop:
	mov	bx,offset handles
f_stop_2:
	mov	[bx].in_use,0
	add	bx,(size per_handle)	; next handle
	cmp	bx,offset end_handles
	jb	f_stop_2
	clc
	ret


verify_handle:
;Ensure that their handle is real.  If it isn't, we pop off our return
;address, and return to *their* return address with cy set.
	mov	bx,_BX[bp]		;get the handle they gave us
	cmp	bx,offset handles
	jb	verify_handle_bad	;no - must be bad.
	cmp	bx,offset end_handles
	jae	verify_handle_bad	;no - must be bad.
	cmp	[bx].in_use,0		;if it's not in use, it's bad.
	je	verify_handle_bad
	ret
verify_handle_bad:
	mov	dh,BAD_HANDLE
	add	sp,2			;pop off our return address.
	stc
	ret


f_receive_packet:
;	mov	ax,0			;allocate request.
;	mov bx,1			; The static handle (UGLY)
;	mov cx,_CX[bp]
;	stc					;with stc, flags must be an odd number
;	push	ax			; save a number that cant be flags
;	pushf				;save flags in case iret used.
;	call	receive_ptr	;ask the client for a buffer.
	; on return, flags should be at top of stack. if an IRET has been used,
	; then 0 will be at the top of the stack
;	pop	bx
;	cmp	bx,0
;	je	recv_find_4		;0 is at top of stack
;	add	sp,2
;recv_find_4:
	; Just continue here
	; ES:DI contains the buffer allocated for us from PDIPX
	mov  ax,60h
	mov  dx,26
	int  0e6h
;	mov _ES[bp],es
;	mov _DI[bp],di
;	mov _DX[bp],69h
	ret

f_ok_received_pkt:
;	mov	ax,1			;allocate request.
;	mov bx,1			; The static handle (UGLY)
;	mov cx,_CX[bp]
;	stc					;with stc, flags must be an odd number
;	push	ax			; save a number that cant be flags
;	pushf				;save flags in case iret used.
;	call	receive_ptr	;ask the client for a buffer.
	; on return, flags should be at top of stack. if an IRET has been used,
	; then 0 will be at the top of the stack
;	pop	bx
;	cmp	bx,1
;	je	recv_ok_4		;1 is at top of stack
;	add	sp,2
;recv_ok_4:
	mov _DS[bp],ds
	mov _SI[bp],ds
	mov _DX[bp],70h

	public rcv_isr
rcv_isr:
	cli
	push	ax
	push	bx
	push	cx
	push	dx
	push	di
	push	si
	push	ds
	push	es

	;Put the size in CX
	mov	ax,0CFFFh
	mov	ds,ax
	mov	bx,0eh
	mov	cx,[bx]
	push	cx
	mov	bx,1
	push	cs
	pop	ds

	mov	ax,0
	call	receive_ptr

	push	es
	push	di


	; ds is now the segment of the buffer
	push	es
	pop	ds
	; es is now the data segment
;	push	cs
;	pop	es

	mov	si,di
	; Make DS:SI point to the buffer
;	mov	di,offset receive_ptr
;	mov	bx,1

	;The buffer is updated.
	mov	ax,1
	assume ds:nothing
	call	receive_ptr

	pop	di
	pop	es


	; print out es:di too
	mov	ax,60h
	mov	dx,27
	pop     cx
	int	0e6h

	pop	es
	pop	ds
	pop	si
	pop	di
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	sti
	iret

code	ends

	end	start
