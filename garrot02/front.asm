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

TITLE front.asm 

EXTRN _begin:near

DGROUP	GROUP	_TEXT
	ASSUME cs:DGROUP,ds:DGROUP,ss:DGROUP
_TEXT 	SEGMENT	WORD PUBLIC 'CODE'

ORG 100H

start:	jmp _begin
	nop

_TEXT 	ENDS
	END start



