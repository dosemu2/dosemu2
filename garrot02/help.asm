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
TITLE help.asm

OF equ offset

PUBLIC _help, _gversion, _terms, _query
EXTRN _prnarry:proc, _print:proc, _prndec:proc, _prneol:proc
EXTRN gvers:byte, runmsg:byte, EOL:byte

ASSUME cs:_TEXT,ds:_TEXT,ss:_TEXT

_TEXT	SEGMENT	WORD PUBLIC 'CODE'


usage:
h0 db	'Usage: garrot [ -h | -DDD | -n DDD | -v | -q | -t ]',0
h1 db	'        ', ' ',0
h2 db	'        ', '-h    ',' help screen',0 
h3 db	'        ', '-DDD  ',' set garrot constant to DDD (an integer number)',0
h4 db	'        ', '-n DDD',' alternate syntax for setting the garrot constant',0
h5 db	'        ', '-v    ',' display version',0
h6 db	'        ', '-q    ',' query current settings',0
h7 db	'        ', '-t    ',' show terms of license',0

helpmsg dw OF h0, OF h1, OF h2, OF h3, OF h4, OF h5, OF h6, OF h7, 0 

_help PROC near
	push	offset helpmsg
	call	_prnarry
	pop	ax
	ret
_help ENDP

_gversion PROC near
	push	ax
	push	offset gvers
	call	_prneol
	pop	ax
	pop	ax
	ret
_gversion ENDP


t0 db ' Garrot version 0.2, Copyright (C) 1994 Thomas G. McWilliams',0
t1 db ' ',0
t2 db '  This program is free software; you can redistribute it and/or modify',0
t3 db '  it under the terms of the GNU General Public License as published by',0
t4 db '  the Free Software Foundation; either version 2 of the License, or',0
t5 db '  (at your option) any later version.',0
t6 db ' ',0
t7 db '  This program is distributed in the hope that it will be useful,',0
t8 db '  but WITHOUT ANY WARRANTY; without even the implied warranty of',0
t9 db '  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the',0
ta db '  GNU General Public License for more details.',0
tb db ' ',0
tc db '  You should have received a copy of the GNU General Public License',0
td db '  along with this program; if not, write to the Free Software',0
te db '  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.',0

termmsg dw OF t0, OF t1, OF t2, OF t3, OF t4, OF t5, OF t6, OF t7
	dw OF t8, OF t9, OF ta, OF tb, OF tc, OF td, OF te, 0 

_terms PROC near
	push	offset termmsg
	call	_prnarry
	pop	ax
	ret
_terms ENDP

levmsg	db	' at level ',0
sizmsg	db	', resident bytes = ',0

_query	PROC near
	; dx = nice
	; bx = size of resident program
	pusha
	push	bx
	push	dx
	push	offset runmsg
	call	_print
	pop	ax
	push	offset	levmsg
	call	_print
	pop	ax
	call	_prndec
	pop	ax	
	push	offset	sizmsg
	call	_print
	pop	ax
	call	_prndec
	pop	bx
	push	offset EOL
	call	_print
	pop	ax
	popa
	ret	
_query	ENDP


_TEXT	ENDS
	END

