!
! fossil.S: FOSSIL serial driver emulator for dosemu.
! 
! Copyright (C) 1995 by Pasi Eronen.
!
! The code in this module is free software; you can redistribute it
! and/or modify it under the terms of the GNU General Public License
! as published by the Free Software Foundation; either version 2 of 
! the License, or (at your option) any later version.
!
				       
.text
.data
.bss

.align 0

.globl _main             
_main:  
        
        .org     0x100
start:
        br      check_init

! -------------------------------------------------------------------------

DOSEMU_HELPER_DOSEMU_CHECK = 0x00
DOSEMU_HELPER_INT = 0xe6
DOSEMU_MAGIC = 0xaa55

DOSEMU_HELPER_SERIAL = 0x29
DOSEMU_HELPER_SERIAL_CHECK = 0
DOSEMU_HELPER_SERIAL_INSTALL = 1

FOSSIL_MAGIC = 0x1954
FOSSIL_MAX_FUNCTION = 0x1b

! -------------------------------------------------------------------------

old14:
        .long   -1                
fossil_id_string:
        .ascii  "dosemu FOSSIL emulator"
	.byte   0

int14:
        jmp     int14a
	.byte   0,0,0,0
	.word   FOSSIL_MAGIC          
	.byte   FOSSIL_MAX_FUNCTION   
	.byte   0
int14a:
        ! in TASM this is "jmp far cs:old14"
        .byte   0x2e,0xff,0x2e
 	.word   old14

! -------------------------------------------------------------------------

init:
        push    cs
        pop     ds

        ! show banner
        mov     dx,#banner_txt
	mov     ah,#0x09
	int     0x21

#if 0
        ! check that dosemu is installed.
	mov     al,#DOSEMU_HELPER_DOSEMU_CHECK
        int     DOSEMU_HELPER_INT
	cmp     ax,#DOSEMU_MAGIC
	je      dosemu_found
no_dosemu:
        mov     dx,#no_dosemu_txt
	jmp     complain_and_exit
#endif
	
        ! check if we are already installed.
dosemu_found:
	mov     al,#DOSEMU_HELPER_SERIAL
	mov     ah,#DOSEMU_HELPER_SERIAL_CHECK
	int     DOSEMU_HELPER_INT
	cmp     ax,#0
	je      not_yet_installed
	cmp     ax,#1
	je      already_installed
	jmp     no_fossil_support
already_installed:
        mov     dx,#already_installed_txt
	jmp     complain_and_exit
no_fossil_support:
        mov     dx,#no_fossil_support_txt
	jmp     complain_and_exit
	
not_yet_installed:
        ! get old interrupt vector and save it
        mov     ah,#0x35
        mov     al,#0x14
        int     0x21
        mov     [old14],bx      
	mov     [old14+2],es
        ! set new interrupt vector (ds already points to cs)
        mov     ah,#0x25
        mov     al,#0x14
        mov     dx,#int14
        int     0x21
	
	! notify dosemu module
        mov     di,#fossil_id_string
	push    cs
	pop     es
	mov     al,#DOSEMU_HELPER_SERIAL
	mov     ah,#DOSEMU_HELPER_SERIAL_INSTALL
	int     DOSEMU_HELPER_INT
	
        ! show a message
	mov     dx,#installed_txt 
	mov     ah,#0x09
	int     0x21
        ! terminate and stay resident
        mov     ax,#0x3100
;	mov     dx,#[[init - start]/16]+17
	mov	dx,#((init - start)/16)+17
        int     0x21

complain_and_exit:
        ! display error message (at ds:dx)
	mov     ah,#0x09
	int     0x21
	! exit with return code 1
        mov     ah,#0x4c           
	mov     al,#1
	int     0x21

check_init:
#include "detect.h"
	JMPL	init

! -------------------------------------------------------------------------
	
banner_txt:
        .ascii  "dosemu FOSSIL emulator: $"
no_dosemu_txt:
        .ascii  "dosemu not found!"
	.byte   13,10
	.ascii  "$"
already_installed_txt:
        .ascii  "already installed."
	.byte   13,10
	.ascii  "$"
no_fossil_support_txt:
        .ascii  "dosemu FOSSIL support not found!"
	.byte   13,10
	.ascii  "$"
installed_txt:
        .ascii  "installed."
	.byte   13,10
	.ascii  "$"

        end
