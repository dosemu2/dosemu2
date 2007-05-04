# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# uchdir.S, to generate uchdir.com, for dosemu 0.67.16, 97/11/22
# This does a chdir on the Linux directory, not the DOS one
#             ^^^^         ^^^^^
# USAGE:
#   uchdir newpath
#                                            Hans Lermen@fgan.de
#

.text
.code16
	.globl	_start16
_start16:

#define NEED_ARGV
#include "detect.h"

	cmpw	$1,argc
	je	ok_1
	lea	abortmsg,%si
	call	print_and_exit
abortmsg:
	.ascii	"Run chdir newpath"
	.byte	13,10,0

ok_1:
	movw	$0x81,%ax # chdir
	movw	argv,%dx  # es:dx pointing to path
	int	$0xe6	  # doshelper
	call	exit
