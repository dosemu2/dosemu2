! exitemu.S, to generate exitemu.com, for dosemu 0.49, 4/9/93
! Robert Sanders, gt8134b@prism.gatech.edu
!

.text
.data
.bss

.align 0

.globl _main              ! for ld86
_main:

      .org       0
	mov ax, #0xffff
	int #0xe5         ! shouldn't return, but be sure with the HLT's
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt


