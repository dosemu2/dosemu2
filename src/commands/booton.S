! booton.S, to generate booton.com, for dosemu 0.49, 4/9/93
! Theodore Ts'o, tytso@mit.edu
!

.text
.data
.bss

.align 0

.globl _main              ! for ld86
_main:

      .org       0x100
#include "detect.h"

	mov al, #0x30
	mov bx, #1
	int #0xe6  ! enable bootdisk
	int #0x20  ! exit
