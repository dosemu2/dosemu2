! vgaoff.S, for dosemu 0.49, 4/16/93
! Robert Sanders, gt8134b@prism.gatech.edu
!
! $Date: 1994/06/12 23:16:59 $
! $Source: /usr/src/dosemu0.52/commands/RCS/vgaoff.S,v $
! $Revision: 2.1 $
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


