! exitemu.S, to generate exitemu.com, for dosemu 0.49, 4/9/93
! Robert Sanders, gt8134b@prism.gatech.edu
!
! $Date: 1994/06/12 23:16:59 $
! $Source: /home/src/dosemu0.60/commands/RCS/exitemu.S,v $
! $Revision: 2.1 $
! $State: Exp $

.text
.data
.bss

.align 0

.globl _main              ! for ld86
_main:

      .org       0
	mov ax, #0xffff
	int #0xe6         ! shouldn't return, but be sure with the HLT's
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt
	hlt


