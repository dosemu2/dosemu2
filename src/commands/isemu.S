# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# isemu.S, to generate isemu.com, for dosemu 0.67.16, 97/11/22
# Display current DOSEMU-version
#                                            Hans Lermen@fgan.de
#

.text
.code16
	.globl	_start16
_start16:

#include "detect.h"

	cmpl	$0,real_version
	jne	more_info
	lea	simpletext,%si
	call	print
	xorw	%ax,%ax
	call	exit

simpletext:
	.ascii	"Dosemu detected"
	.byte	13,10,0
txt1:	.ascii	"Dosemu version "
	.byte	0
txt2:	.ascii	" detected"
	.byte	13,10,0

printdot:
	movb	$'.',%al
	call	printchar
	ret

more_info:
	lea	txt1,%si
	call	print
	movzbw	real_version+3,%ax
	call	printdecimal
	call	printdot
	movzbw	real_version+2,%ax
	call	printdecimal
	call	printdot
	movzbw	real_version+1,%ax
	call	printdecimal
	cmpb	$0,real_version
	jz	more_info_1
	call	printdot
	movzbw	real_version,%ax
	call	printdecimal
	call	printdot
more_info_1:
	lea	txt2,%si
	call	print
	xorw	%ax,%ax
	call	exit

