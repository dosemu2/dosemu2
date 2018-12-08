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

#include "doshelpers.h"
				
.text
.code16
	.globl	_start16
_start16:

start:
	jmp      check_init

# -------------------------------------------------------------------------

FOSSIL_MAGIC = 0x1954
FOSSIL_MAX_FUNCTION = 0x1b

# -------------------------------------------------------------------------

old14:
	.long   -1
fossil_id_string:
	.asciz  "dosemu FOSSIL emulator"

int14:
	jmp     1f
	.byte   0,0,0,0
	.word   FOSSIL_MAGIC
maxffn:
	.byte   FOSSIL_MAX_FUNCTION
	.byte   0
1:
	ljmp	*%cs:old14

# -------------------------------------------------------------------------

init:
	pushw   %cs
	popw    %ds

	# notify dosemu module
	movw    $fossil_id_string,%di
	pushw   %cs
	popw    %es
	movb    $DOS_HELPER_SERIAL_HELPER,%al
	movb    $DOS_SUBHELPER_SERIAL_TSR_INSTALL,%ah
	int     $DOS_HELPER_INT
	jnc	.Lgoodtoload

	cmpw	$DOS_ERROR_SERIAL_ALREADY_INSTALLED, %bx
	je	.Lalreadyloaded

	cmpw	$DOS_ERROR_SERIAL_CONFIG_DISABLED, %bx
	je	.Ldisabledconfig

	jmp	.Lunknownerror

.Lgoodtoload:
	# Update the maximum function supported by the Dosemu backend
	movb	%bl, %cs:maxffn

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

	# show a message
	movw    $installed_txt,%dx
	movb    $0x09,%ah
	int     $0x21

	# release the environment block
	movw	%cs:0x002c, %ax			// envptr at 0x2c within PSP
	movw	%ax, %es
	movw	$0x4900, %ax
	int     $0x21

	# terminate and stay resident
	movw    $0x3100,%ax
	movw    $((init - start) >> 4) + 17,%dx	// our code + PSP + 1
	int     $0x21

.Lalreadyloaded:
	# display error message (at ds:dx)
	movw    $already_txt, %dx
	movb    $0x09,%ah
	int     $0x21
	jmp	Error

.Ldisabledconfig:
	# display error message (at ds:dx)
	movw    $disabled_txt, %dx
	movb    $0x09,%ah
	int     $0x21
	jmp	Error

.Lunknownerror:
	# display error message (at ds:dx)
	movw    $unknown_txt, %dx
	movb    $0x09,%ah
	int     $0x21
#	jmp	Error

Error:
	# exit with return code 1
	mov     $0x4c,%ah
	mov     $1,%al
	int     $0x21

check_init:
#include "detect.h"
	jmp	init

# -------------------------------------------------------------------------

installed_txt:
        .ascii  "dosemu FOSSIL emulator: installed.\r\n$"

already_txt:
        .ascii  "dosemu FOSSIL emulator: already installed.\r\n$"

disabled_txt:
        .ascii  "dosemu FOSSIL emulator: disabled in config.\r\n$"

unknown_txt:
        .ascii  "dosemu FOSSIL emulator: unknown error.\r\n$"
