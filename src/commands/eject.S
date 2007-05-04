# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


# eject.S for dosemu 0.53, 8/18/94
#    eject cd in drive
#
.code16
.text	
	.globl	_start16
_start16:

#include "detect.h"

        movb $0x40, %al # unlock door
        movb $0x0C, %ah
        int     $0xe6

	movb $0x40, %al # eject disk
        movb $0x0D, %ah
	int     $0xe6

	int     $0x20   # exit
