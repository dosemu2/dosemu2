;   
;   Garrot, a Linux dosemu support driver, releases idle time to system.
;   Copyright (C) 1994  Thomas G. McWilliams
;
;   This program is free software; you can redistribute it and/or modify
;   it under the terms of the GNU General Public License as published by
;   the Free Software Foundation; either version 2 of the License, or
;   (at your option) any later version.
;
;   This program is distributed in the hope that it will be useful,
;   but WITHOUT ANY WARRANTY; without even the implied warranty of
;   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;
; author e-mail: tgm@netcom.com, thomas.mcwilliams@f615.n109.z1.fidonet.org
;

TITLE print.asm

.286
PUBLIC _print, _prnhx, _prnarry, _prndec, _prneol

NULL equ 0

_TEXT	SEGMENT WORD PUBLIC 'CODE'
 	ASSUME	 cs:_TEXT,ds:_TEXT,ss:_TEXT

EOL	db	13,10,0

_print	proc	near
	push	bp
	mov	bp,sp
	push	si
	push	ds
	push	cs
	pop	ds
	mov	ah,0fh
	int	10h
	xor	bl,bl
	mov	ah, 0eh 
	mov	si,word ptr [bp+4]
	jmp	short m1
m2:
	mov	 al, byte ptr [si]
	int	 10h 
	inc      si	
m1:
	cmp	byte ptr [si],0
	jne	m2
	pop	ds
	pop	si
	pop	bp
	ret	
_print	endp

	hbuf:	db 0,0,0,0, ':', 0,0,0,0, 13,10,0,0
	hxtbl:	db '0123456789abcdef!'


; this function hex prints a 32 bit integer which has been pushed onto
; the stack as two 16 bit words.

_prnhx	PROC near
	push	bp
	mov	bp,sp
	push	si
	push	dx
	push	bx
	push	ds
	push	cs
	pop	ds

	mov	ah,2		; word counter
	mov	ch,4		; nibble counter
	mov	cl,ch		; shift constant
	mov	si,offset hbuf
	mov	dx,[bp+6]	; get segment
d1:
	rol	dx,cl
	mov	bx,dx
	and	bx,0fh		
	mov	al,byte ptr cs:[bx+offset hxtbl]
	mov	[si],al
	inc	si
	dec	ch
	jnz	d1

	inc	si
	mov	dx,[bp+4]	; get offset
	mov	ch,cl
	dec	ah
	jnz	d1

	push	offset hbuf
	call	_print
	pop	ax

	pop	ds
	pop	bx
	pop	dx
	pop	si
	pop	bp	
	ret
_prnhx	ENDP

; prints a NULL terminated array of strings

_prnarry PROC near
	push	bp
	mov	bp,sp
	pusha
	mov	si,[bp+4]
top:
	mov	ax,[si]	
	cmp	ax, NULL
	je	endo
	push	ax
	call	_print
	pop	ax
	push	offset EOL
	call	_print
	pop	ax
	inc	si
	inc	si
	jmp	top
endo:
	popa
	pop	bp
	ret
_prnarry ENDP

EVEN
dtbl	dw 	10000,1000,100,10,1
dbuf	db	0,0,0,0,0,0,0
; prints a 16 bit word in decimal format
_prndec PROC near
	push 	bp
	mov	bp,sp
	pusha
	push	cs
	pop	ds
	push	cs
	pop	es
	
	mov	cx,5
	mov	si,offset dtbl
	mov	di,offset dbuf
	mov	dx,[bp+4]
tart:	
	mov	bx,[si]
	mov	ax,dx
	xor	dx,dx
	div	bx
	or	al,30h
	mov	[di],al
	inc	di
	inc	si
	inc	si
	loop	tart
	
	mov	di,offset dbuf
	mov	al,30h
	mov	cx,4	
pd1:
	cmp	al,[di]	; strip leading zeros
	jne	pd2	
	inc	di
	loop	pd1
pd2:
	push 	di
	call	_print
	pop	ax	
	popa	
	pop	bp
	ret
_prndec ENDP

; print a string with cr/lf at end
_prneol	PROC	near
	push	bp
	mov	bp,sp
	pusha
	mov	dx,[bp+4]
	push	dx
	call	_print	
	pop	ax
	push	offset EOL
	call	_print
	pop	ax
	popa
	pop	bp
	ret
_prneol 	ENDP

_TEXT 	ENDS
	END 

