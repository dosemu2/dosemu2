# exitemu.S, to generate exitemu.com, for dosemu 0.49, 4/9/93
# Robert Sanders, gt8134b@prism.gatech.edu
#

.code16
.text
	.globl	_start16
_start16:

#include "detect.h"

	movw $0xffff, %ax
	int $DOS_HELPER_INT         # shouldn't return, but be sure with the HLT's
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt

