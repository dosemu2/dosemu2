! 
! (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!

! uchdir.S, to generate uchdir.com, for dosemu 0.67.16, 97/11/22
! This does a getdir on the Linux directory, not the DOS one
!             ^^^^         ^^^^^
! USAGE:
!   ugetdir
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

	mov	cx,#1024
	mov	dx,#buffer
	mov	ax,#0x80	; DOS_HELPER_GETCWD
	int	0xe6		; doshelper
	lea	si,buffer
	call	print
	lea	si,linefeed
	call	print
	xor	ax,ax
	call	exit

linefeed:
	db	13,10,0
buffer:
