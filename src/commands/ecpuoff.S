! 
! (C) Copyright 1992, ..., 1998 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!

! turn CPU emulator OFF
! Alberto Vignani, vignani@mbox.vol.it
!

.text
.data
.bss

.align 0

.globl _main              ! for ld86
_main:

      .org       0x100
#include "detect.h"

	mov al, #0x91
	mov bx, #0
	int #0xe6  ! disable bootdisk
	int #0x20  ! exit
