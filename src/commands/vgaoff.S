# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


# vgaoff.S, for dosemu 0.49, 4/16/93
# Robert Sanders, gt8134b@prism.gatech.edu
#
# this is dangerous!

.code16
.text
	.globl	_start16
_start16:

#include "detect.h"

	movb $4, %al     # card switch
	movb $0, %bl     # off
	int $0xe6
	int $0x20  # exit

# note: the VGA permissions are left on. 


