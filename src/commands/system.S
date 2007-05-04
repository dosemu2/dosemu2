# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


# system.S, to generate system.com, for dosemu 0.67.16, 97/11/22
# This executes a Unix command, 1st arg == command, rest args for the
# command
#                                            Hans Lermen@fgan.de
#

.text
.code16
	.globl	_start16
_start16:

#include "detect.h"

	movzbw	0x80,%bx
	movb	$0,0x81(%bx)
	movw	$0x81,%dx
	movw	$0x53,%ax
	int	$0xe6		# doshelper
	call	exit
