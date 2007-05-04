# 
# Theodore Ts'o, tytso@mit.edu (original code)
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# booton.S, to generate booton.com, for dosemu 0.49, 4/9/93
#
.code16
.text
	.globl	_start16
_start16:

#include "detect.h"

	movb $0x30, %al
	movw $1, %bx
	int $0xe6  # enable bootdisk
	int $0x20  # exit
