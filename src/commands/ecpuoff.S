# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# turn CPU emulator OFF
# Alberto Vignani, vignani@tin.it
#

.text
.code16
	.globl	_start16
_start16:

#include "detect.h"

	mov $0x91, %al
	mov $0, %bx
	int $0xe6  # disable bootdisk
	int $0x20  # exit
