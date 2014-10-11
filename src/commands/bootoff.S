# bootoff.S, to generate bootoff.com, for dosemu 0.49, 4/9/93
# Theodore Ts'o, tytso@mit.edu
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
