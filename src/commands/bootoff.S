! bootoff.S, to generate bootoff.com, for dosemu 0.49, 4/9/93
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
	mov bx, #0
	int #0xe6  ! disable bootdisk
	mov al,#0
	call exit
