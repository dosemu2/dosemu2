# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# uchdir.S, to generate uchdir.com, for dosemu 0.67.16, 97/11/22
# This does a getdir on the Linux directory, not the DOS one
#             ^^^^         ^^^^^
# USAGE:
#   ugetdir
#                                            Hans Lermen@fgan.de
#

.text
.code16
	.globl	_start16
_start16:

#include "detect.h"

	movw	$1024,%cx
	movw	$buffer,%dx
	movw	$0x80,%ax	# DOS_HELPER_GETCWD
	int	$0xe6		# doshelper
	lea	buffer,%si
	call	print
	lea	linefeed,%si
	call	print
	xorw	%ax,%ax
	call	exit

linefeed:
	.byte	13,10,0
buffer:
