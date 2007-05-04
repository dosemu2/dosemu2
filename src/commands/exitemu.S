# 
# Robert Sanders, gt8134b@prism.gatech.edu (original code)
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#

# exitemu.S, to generate exitemu.com, for dosemu 0.49, 4/9/93
#

.code16
.text
	.globl	_start16
_start16:

#include "detect.h"

	movw $0xffff, %ax
	int $0xe6         # shouldn't return, but be sure with the HLT's
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt

