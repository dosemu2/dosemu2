! 
! (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!

! isemu.S, to generate isemu.com, for dosemu 0.67.16, 97/11/22
! Display current DOSEMU-version
!                                            Hans Lermen@fgan.de
!

.text
.data
.bss

.align 0

.globl _main	; for ld86
_main:

	.org	0x100	; which is were *.com files start

#include "detect.h"

	cmp	dword ptr real_version,#0
	jne	more_info
	lea	si,simpletext
	call	print
	xor	ax,ax
	call	exit

simpletext:
	.ascii	"Dosemu detected"
	db	13,10,0
txt1:	.ascii	"Dosemu version "
	db	0
txt2:	.ascii	" detected"
	db	13,10,0

printdot:
	mov	al,#0x2e	; '.'
	call	printchar
	ret

more_info:
	lea	si,txt1
	call	print
	movzx	ax,byte ptr real_version+3
	call	printdecimal
	call	printdot
	movzx	ax,byte ptr real_version+2
	call	printdecimal
	call	printdot
	movzx	ax,byte ptr real_version+1
	call	printdecimal
	cmp	byte ptr real_version,#0
	jz	more_info_1
	call	printdot
	movzx	ax,byte ptr real_version
	call	printdecimal
	call	printdot
more_info_1:
	lea	si,txt2
	call	print
	xor	ax,ax
	call	exit

