! 
! (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!


! system.S, to generate system.com, for dosemu 0.67.16, 97/11/22
! This executes a Unix command, 1st arg == command, rest args for the
! command
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

	movzx	bx, byte ptr [0x80]
	mov	byte ptr [bx+0x81],#0
	mov	dx,#0x81
	mov	ax,#0x53
	int	0xe6		; doshelper
	call	exit
