! 
! (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!


! speed.S, to generate speed.com, for dosemu 0.67.16, 97/11/22
! Set/Get HogThreshold
!                                            Hans Lermen@fgan.de
!

.text
.data
.bss

.align 0

.globl _main	; for ld86
_main:

	.org	0x100	; which is were *.com files start

#define NEED_ARGV
#include "detect.h"

	cmp	argc,#1
	je	ok_set
	jb	ok_get
abort:
	lea	si,abortmsg
	call	print_and_exit
abortmsg:
	.ascii	"USAGE: speed [hogthreshold]"
linefeed:
	db	13,10,0
resultmsg:
	.ascii	"Current hogthreshold value = "
	db	0

ok_set:
	mov	si,argv
	call	atoi
	cmp	ax,#255
	ja	abort
	mov	bx,ax
	mov	ax,#0x12	; set HogThreshold
	int	0xe6		; doshelper
ok_get:
	lea	si,resultmsg
	call	print
	mov	ax,#0x28	; get HogThreshold
	mov	bx,#1		; bx=1 for Get Hogthreshold
	int	0xe6		; doshelper
	call	printdecimal
	lea	si,linefeed
	call	print
	mov	al,#0
	call	exit
