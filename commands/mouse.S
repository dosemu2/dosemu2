! mouse.S for dosemu 0.53, 8/18/94
!    Just to reset int 33 away from iret!
!

use16
.text
.data
.bss

.align 0

.globl _main           ! for ld86
_main:

       .org     0
        mov al, #0x33  ! Reset mouse helper call
        int     #0xe6

	int     #0x20  ! exit

