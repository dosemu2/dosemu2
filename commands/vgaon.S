! vgaon.S, for dosemu 0.49, 4/16/93
! Robert Sanders, gt8134b@prism.gatech.edu
!
! $Date: 1993/11/12 12:41:27 $
! $Source: /home/src/dosemu0.49pl3/commands/RCS/vgaon.S,v $
! $Revision: 1.1 $
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
	mov al, #3     ! dos helper I/O perms
	mov bx, #0x70  ! starting port
	mov cx, #2     ! # of ports
	stc            ! set means get I/Operms
	int #0xe5
	mov al, #4
	mov bl, #1
	int #0xe5
	mov al, #3     ! dos helper I/O perms
	mov bx, #0x70  ! starting port
	mov cx, #2     ! # of ports
	clc            ! clear means release I/Operms
	int #0xe5
	int #0x20  ! exit

! note: the VGA permissions are left on. 


