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

TITLE getopt.asm

.286
PUBLIC _getopt, gentry
EXTRN  _print:proc, _prnhx:proc, dvu:byte, nice:word, start:near
EXTRN  versflg:byte, termflg:byte, queryflg:byte, helpflg:byte

ASSUME cs:_TEXT,ds:_TEXT,ss:_TEXT

_TEXT SEGMENT WORD PUBLIC 'CODE'

	argc	dw	0;
	argv	dw	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0;
	noswitch	db	'error, no switch character in ',0
	nointger	db	'error, not a number: ', 0
	nooption	db	'error, unrecognized option: ',0
	ncbuf		db	0,0
	EOL		db	13,10
	NUL		db	0
	xpct		db	0ffh

	
argify PROC near
	mov	di,offset argv
	mov	bx,0080h
	xor	cl,cl		; argc
	mov	ch,cs:[bx]	; get length of command line
	or	ch,ch
	jz	done
	inc	ch
	xor	dh,dh
	mov	dl,'-'	; switch char
	mov 	al,20h	; blank
	mov	ah,09h	; tab
m1:	inc	bx
	dec	ch
	jz	done
	
	cmp	al,cs:[bx]
	jz	m1	; skip white space
	cmp	ah,cs:[bx]
	jz	m1	; skip white space	

	; found start of non-white space
	mov	word ptr cs:[di],bx
	cmp	dl,cs:[bx] ; do we start with a switch character?
	jz	m2	; ok, found switch character

	inc	byte ptr cs:[di+1] ; flag high byte as bad option: not '-'
m2:	inc	di	; point to next slot in array
	inc	di
	inc	cl	; bump argc
	; search for white space
m3:	inc	bx
	dec	ch
	jz	done
	
	cmp	al,cs:[bx]
	jz	m4	; skip non-white space
	cmp	ah,cs:[bx]
	jnz	m3	; skip non-white space	
m4:
	mov	cs:[bx],dh ; terminate string with zero 
	jmp	m1
	
done:;	inc	bx
	mov	cs:[bx],dh ; guarantee zero termination
	xor	ch,ch
	mov	argc,cx
	inc	cx
	mov	si,offset argv
	ret
argify	ENDP


gentry:
	call _getopt
	int 20h

_getopt PROC near
	call	argify
	; cx = argc, si = offset argv
g3:
	mov	bx,cs:[si]	; get pointer to option group
	inc	si
	inc	si
g1:
	dec	cx
	jz	nomor	
	or	bh,bh
	jz	g2
	mov	bh,xpct
	or	bh,bh
	jz	g4
	xor	bh,bh
	push	bx
	push	offset noswitch ; no switch character, parse error
	jmp	exiterr
g2:
	;push	bx ; debug code
	;call	_print
	;pop	bx
	inc	bx
g4:
	mov	al,cs:[bx]	; get option character
	or	al,al
	jz	g3		; get next option group
	call	parseopt
	cmp	cx,0ffffh
	jne	g2		; get next option character
	push	ax
	push	dx
exiterr:
	call	_print  ; message at dx
	pop	dx
	call	_print  ; message at ax
	pop	ax
	push	offset EOL
	call	_print
	pop	cx
	mov	cx,0ffffh ; return non-zero in ch => error
nomor:	ret	
_getopt ENDP

parseopt PROC near
	push    si
	push	cx
	mov	ah,xpct	; expecting a numeric argument
	or	ah,ah
	jz	pp3
pp1:
	cmp	al,'n'
	jne	pp2
	xor	ah,ah
	mov	xpct,ah
	jmp	pfin
pp2:
	cmp	al,'0'
	jb	pp4
	cmp	al,'9'
	ja	pp4
pp3:
	call	atoi
	cmp	ax,0ffffh
	je	doer
	cmp	dx,0ffh
	jb	pp3a
	mov	dx,0ffh	 ; truncate nice number to 255
pp3a:
	mov	nice,dx  ; set nice number
	jmp	pfin
pp4:
	cmp	al,'d'
	jne	pp5
	mov	dvu,al
	jmp	pfin
pp5:
	cmp	al,'h'
	jne	pp6
	mov	helpflg,al
	jmp	pfin
pp6:
	cmp	al,'v'
	jne	pp7
	mov	versflg,al
	jmp	pfin
pp7:
	cmp	al,'q'
	jne	pp8
	mov	queryflg,al
	jmp	pfin
pp8:
	cmp	al,'t'
	jne	pp9
	mov	termflg,al
	jmp	pfin
pp9:
defal:
	pop	cx
	xor	cx,cx
	dec	cx
	push	cx
	mov	ncbuf,al
	mov	ax, offset ncbuf
	mov	dx,offset nooption	
	jmp	pfin
doer:
	pop	cx
	push	ax
	mov	ax,dx
	mov	dx,offset nointger

pfin:
	pop	cx
	pop	si
	ret

parseopt ENDP


; returns -1 on not a natural number 
; dx is the return register

atoi PROC near
	push	di	
	xor	dx,dx	
	mov	di,bx
	mov	al,'-'
	cmp	al,cs:[di-1]
	je	a1
	mov	al,'n'
	cmp	al,cs:[di-1]
	je	a1
	mov	ah,xpct
	or	ah,ah
	jnz	aerr
	dec	ah
	mov	xpct,ah
	mov	ah,20h
	mov	cs:[di-1],ah
a1:
	mov	al,cs:[di]
	cmp	al,0
	je	fin	
	cmp	al,'0'
	jb	aerr
	cmp	al,'9'
	ja	aerr
	and	ax,0fh	; mask out upper 12 bits, leave number only
	push	ax	; multiply dx by 10
	mov	ax,dx
	shl	dx,2
	add	dx,ax
	shl	dx,1
	pop	ax
	add	dx,ax
	inc	di
	jmp	a1

aerr:
	mov     dx,di
	dec	dx
a3:
	inc     di
	mov	al,cs:[di]
	cmp	al,0
	jne	a3
	xor	ax,ax
	dec	ax
fin:
	mov	bx,di	
	dec	bx
	pop	di
	ret

atoi 	ENDP




_TEXT	ENDS
	END start



