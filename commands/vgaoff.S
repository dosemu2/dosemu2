! vgaoff.S, for dosemu 0.49, 4/16/93
! Robert Sanders, gt8134b@prism.gatech.edu
!
! $Date: 1994/01/30 12:34:02 $
! $Source: /home/src/dosemu0.50pl1/commands/RCS/vgaoff.S,v $
! $Revision: 1.2 $
! $State: Exp $
!
! this is dangerous!

use16
.text
.data
.bss

.align 0

.globl _main              ! for ld86
_main:

      .org       0
	mov al, #4     ! card switch
	mov bl, #0     ! off
	int #0xe6
	int #0x20  ! exit

! note: the VGA permissions are left on. 


