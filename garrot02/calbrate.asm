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
;   MERCHAN'        'ILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;   GNU General Public License for more details.
;
;   You should have received a copy of the GNU General Public License
;   along with this program; if not, write to the Free Software
;   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
;
; author e-mail: tgm@netcom.com, thomas.mcwilliams@f615.n109.z1.fidonet.org
;
.286
TITLE calbrate.asm

PUBLIC _bogo
EXTRN _prnarry:proc, _print:proc, _prnhx:proc, _prndec:proc, nice:word
EXTRN lvlmsg, calmsg, EOL
ASSUME cs:_TEXT,ds:_TEXT,ss:_TEXT

_TEXT	SEGMENT	WORD PUBLIC 'CODE'

KONST equ 105
DBG  equ 0


_bogo	PROC near
	push	es
	push	di

; display message head
	push	offset calmsg
	call	_print
	pop	ax
; get timer
	xor	ax,ax
	mov	es,ax
	mov	di,46ch ; timer location
	mov	dx,es:di
	push	dx

; pre-sync
	mov	es:di,ax
	mov	cx,18
xz:	mov	dx,es:di
	cmp	dx,cx
	jb	xz	
	mov	ax,1680h
	int	2fh
	xor	ax,ax	
	xor	dx,dx	
	mov	es:di,dx
; sync
xy:
	mov	cx,es:di
	cmp	cx,dx
	je	xy
	add	cx,146

; calibration counting loop
b1:
	add	ax,1
	adc	dx,0
	mov	bx,es:di
	cmp	bx,cx
	jb	b1
	add	ax,8000h
	adc	dx,0
IF DBG
	push	dx ; debug !!!
ENDIF
; adjust result and set niceness level 
	mov	ax,dx
	mov	bl,KONST
	div	bl
	cmp	ah,( KONST / 2 )
	jb	x1
	inc	al
x1:	xor	ah,ah
	mov	nice,ax 

; print message tail
	push	offset EOL
	push	ax
	push	offset lvlmsg
	call	_print
	pop	dx
	call	_prndec
	pop	dx
	call	_print
	pop	dx
IF DBG 
; debug print
	call _prndec
	pop	dx
	push	offset EOL
	call _print
	pop dx

ENDIF
; clean-up
	pop	dx
	add	dx,146+20	; restore counter
	adc	word ptr es:[di+2],0
	mov	es:di,dx
	pop	di
	pop	es
	ret	
_bogo	ENDP

_TEXT	ENDS
	END

