#
# Theodore Ts'o, tytso@mit.edu (original code)
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2006 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# bootoff.S, to generate bootoff.com, for dosemu 0.49, 4/9/93
#
.code16
.text
	.globl	_start16
_start16:

#include "detect.h"

	movb $0x30, %al
	movw $0, %bx
	int $DOS_HELPER_INT  # disable bootdisk
	movb $0,%al
	call exit
