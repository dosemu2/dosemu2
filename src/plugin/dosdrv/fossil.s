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

#include "detect.h"

# -------------------------------------------------------------------------

init:
	# get old interrupt vector to pass to Dosemu
	movb    $0x35,%ah
	movb    $0x14,%al
	int     $0x21	# ES:BX now has old ISR addr

	movb    $DOS_HELPER_SERIAL_HELPER,%al
	movb    $DOS_SUBHELPER_SERIAL_FOSSIL_INIT,%ah
	movb	$DOS_VERSION_SERIAL_FOSSIL, %cl
	int     $DOS_HELPER_INT
	jnc	.Lgoodtoload

	cmpw	$DOS_ERROR_SERIAL_ALREADY_INSTALLED, %bx
	je	.Lalreadyloaded

	cmpw	$DOS_ERROR_SERIAL_CONFIG_DISABLED, %bx
	je	.Ldisabledconfig

	cmpw	$DOS_ERROR_SERIAL_FOSSIL_VERSION, %bx
	je	.Lversionmismatch

	jmp	.Lunknownerror

.Lgoodtoload:
	# set new interrupt vector (helper returned the ISR address in DS:DX)
	movb    $0x25,%ah
	movb    $0x14,%al
	int     $0x21

	movw	$0, %ax
	movw    $installed_txt, %dx
	jmp	Done

.Lalreadyloaded:
	movw	$1, %ax
	movw    $already_txt, %dx
	jmp	Done

.Ldisabledconfig:
	movw	$1, %ax
	movw    $disabled_txt, %dx
	jmp	Done

.Lversionmismatch:
	movw	$1, %ax
	movw    $version_txt, %dx
	jmp	Done

.Lunknownerror:
	movw	$1, %ax
	movw    $unknown_txt, %dx
	jmp	Done

Done:
	pushw	%ax
	pushw	%cs
	popw    %ds
	movb    $0x09,%ah
	int     $0x21		# Show a message

	popw	%ax		# Statuc in AL and return to DOS
	mov     $0x4c,%ah
	int     $0x21

# -------------------------------------------------------------------------

installed_txt:
        .ascii  "dosemu FOSSIL emulator: installed.\r\n$"

already_txt:
        .ascii  "dosemu FOSSIL emulator: already installed.\r\n$"

disabled_txt:
        .ascii  "dosemu FOSSIL emulator: disabled in config.\r\n$"

version_txt:
        .ascii  "dosemu FOSSIL emulator: version mismatch, update FOSSIL.COM.\r\n$"

unknown_txt:
        .ascii  "dosemu FOSSIL emulator: unknown error.\r\n$"
