! turn CPU emulator ON
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

	mov al, #0x90
	mov bx, #0
	int #0xe6  ! disable bootdisk
	int #0x20  ! exit
