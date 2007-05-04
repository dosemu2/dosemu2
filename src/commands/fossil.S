# 
# All modifications in this file to the original code are
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


#
# fossil.S: FOSSIL serial driver emulator for dosemu.
# 
# Copyright (C) 1995 by Pasi Eronen.
#
# The code in this module is free software; you can redistribute it
# and/or modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2 of 
# the License, or (at your option) any later version.
#

#include "macros86.h"
				       
.text
.code16
	.globl	_start16
_start16:
        
start:
        jmp      check_init

# -------------------------------------------------------------------------

DOSEMU_HELPER_DOSEMU_CHECK = 0x00
DOSEMU_HELPER_INT = 0xe6
DOSEMU_MAGIC = 0xaa55

DOSEMU_HELPER_SERIAL = 0x29
DOSEMU_HELPER_SERIAL_CHECK = 0
DOSEMU_HELPER_SERIAL_INSTALL = 1

FOSSIL_MAGIC = 0x1954
FOSSIL_MAX_FUNCTION = 0x1b

# -------------------------------------------------------------------------

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
        # in TASM this is "jmp far cs:old14"
	ljmp	INDIRECT_PTR(%cs:old14)

# -------------------------------------------------------------------------

init:
        pushw   %cs
        popw    %ds

        # show banner
        movw    $banner_txt,%dx
	movb    $0x09,%ah
	int     $0x21

#if 0
        # check that dosemu is installed.
	movb    $DOSEMU_HELPER_DOSEMU_CHECK,%al
        int     $DOSEMU_HELPER_INT
	cmpw    $DOSEMU_MAGIC,%ax
	je      dosemu_found
no_dosemu:
        movw    $no_dosemu_txt,%dx
	jmp     complain_and_exit
#endif
	
        # check if we are already installed.
dosemu_found:
	movb    $DOSEMU_HELPER_SERIAL,%al
	movb    $DOSEMU_HELPER_SERIAL_CHECK,%ah
	int     $DOSEMU_HELPER_INT
	cmpw    $0,%ax
	je      not_yet_installed
	cmpw    $1,%ax
	je      already_installed
	jmp     no_fossil_support
already_installed:
        movw    $already_installed_txt,%dx
	jmp     complain_and_exit
no_fossil_support:
        movw    $no_fossil_support_txt,%dx
	jmp     complain_and_exit
	
not_yet_installed:
        # get old interrupt vector and save it
        movb    $0x35,%ah
        movb    $0x14,%al
        int     $0x21
        movw    %bx,old14
	movw    %es,old14+2
        # set new interrupt vector (ds already points to cs)
        movb    $0x25,%ah
        movb    $0x14,%al
        movw    $int14,%dx
        int     $0x21
	
	# notify dosemu module
        movw    $fossil_id_string,%di
	pushw   %cs
	popw    %es
	movb    $DOSEMU_HELPER_SERIAL,%al
	movb    $DOSEMU_HELPER_SERIAL_INSTALL,%ah
	int     $DOSEMU_HELPER_INT
	
        # show a message
	movw    $installed_txt,%dx
	movb    $0x09,%ah
	int     $0x21
        # terminate and stay resident
        movw    $0x3100,%ax
#	movw    $[[init - start]/16]+17,%dx
	movw	$((init - start) >> 4)+17,%dx
        int     $0x21

complain_and_exit:
        # display error message (at ds:dx)
	movb    $0x09,%ah
	int     $0x21
	# exit with return code 1
        mov     $0x4c,%ah
	mov     $1,%al
	int     $0x21

check_init:
#include "detect.h"
	jmp	init

# -------------------------------------------------------------------------
	
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
