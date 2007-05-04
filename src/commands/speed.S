# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


# speed.S, to generate speed.com, for dosemu 0.67.16, 97/11/22
# Set/Get HogThreshold
#                                            Hans Lermen@fgan.de
#

.text
.code16
	.globl	_start16
_start16:

#define NEED_ARGV
#include "detect.h"

	cmp	$1,argc
	je	ok_set
	jb	ok_get
abort:
	lea	abortmsg,%si
	call	print_and_exit
abortmsg:
	.ascii	"USAGE: speed [hogthreshold]"
linefeed:
	.byte	13,10,0
resultmsg:
	.ascii	"Current hogthreshold value = "
	.byte	0

ok_set:
	movw	argv,%si
	call	atoi
	cmpw	$255,%ax
	ja	abort
	movw	%ax,%bx
	movw	$0x12,%ax	# set HogThreshold
	int	$0xe6		# doshelper
ok_get:
	lea	resultmsg,%si
	call	print
	movw	$0x28,%ax	# get HogThreshold
	movw	$1,%bx		# bx=1 for Get Hogthreshold
	int	$0xe6		# doshelper
	call	printdecimal
	lea	linefeed,%si
	call	print
	movb	$0,%al
	call	exit



