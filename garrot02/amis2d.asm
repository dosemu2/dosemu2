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

.286
TITLE amis2d.asm 

PUBLIC int_2dh
EXTRN _print:proc,_prnhx:proc, flags:near, flagsend:near, i2dh_off:word
EXTRN nice:word, _begin:near, rezend:near

DGROUP	GROUP	_TEXT
	ASSUME cs:DGROUP,ds:DGROUP,ss:DGROUP
_TEXT 	SEGMENT	WORD PUBLIC 'CODE'


; here is where resident/non-residents comunicate

amismsg: db	'LinuxGNUGarrot  Dosemu support driver',0

int_2dh PROC near
	cmp	ah,0d3h	; 0d3h is our identity code for this driver
	jne	out2d
	cmp	al,0h	; function 00, identify
	jne	n1
	mov	al,0ffh	; in-use flag
	mov	cx,0002h ; version number
	push	cs
	pop	dx
	mov	di,offset amismsg
	iret
n1:	cmp	al,06h  ; re-nice
	jne	n2
	call	flagupdt
	xor	al,al
	iret
n2:	cmp	al,07h  ; query
	jne	n3
	mov	dx,cs:nice
	mov	bx,offset rezend
	xor	al,al
	iret
n3:	cmp	al,05h	; unimplemented AMIS flags
	ja	n4
	xor	al,al	; tell someone
	iret
n4:
out2d:
	jmp	dword ptr cs:i2dh_off  ; pass through

int_2dh ENDP

; non-resident has passed new flag settings to daemon

flagupdt PROC near
	push	ds
	push	es
	push	si
	push 	di			

	push	cs
	pop	es ; destination
	mov	di, offset flags ; destination
	mov	ds,dx  ; source	
	mov	si,bx  ; source
	mov	cx, offset flagsend
	sub	cx, offset flags
	cld
rep	movsb
	pop	di
	pop	si
	pop	es
	pop 	ds			
	ret
flagupdt ENDP

	
_TEXT 	ENDS
	END 

