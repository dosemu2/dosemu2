! exitemu.S, to generate exitemu.com, for dosemu 0.49, 4/9/93
! Robert Sanders, gt8134b@prism.gatech.edu
!
! $Date: 1993/11/12 12:41:27 $
! $Source: /home/src/dosemu0.49pl3/commands/RCS/exitemu.S,v $
! $Revision: 1.1 $
! $State: Exp $

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


