! 
! (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!


! eject.S for dosemu 0.53, 8/18/94
!    eject cd in drive
!

use16
.text
.data
.bss

.align 0

.globl _main           ! for ld86
_main:

       .org     0x100
#include "detect.h"

        mov al, #0x40  ! unlock door
        mov ah, #0x0C
        int     #0xe6

	mov al, #0x40  ! eject disk
        mov ah, #0x0D
	int     #0xe6

	int     #0x20  ! exit



