#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

#include <doshelpers.h>

.code16
.text
	.globl	_start16
_start16:

#define stack_size		0x100
#define stack			(stack_start + stack_size)
#define psp_size		0x100

#include "detect.h"

	movw	$stack, %sp
				# first deallocate the unused memory, ES=PSP now
	movb	$0x4a, %ah
	movw	$(stack - _start16 + psp_size), %bx
	shrw	$4, %bx
	incw	%bx
	int	$0x21

	movb	$DOS_HELPER_COMMANDS, %al
	movb	$BUILTINS_PLUGIN_VERSION, %ah
	int	$DOS_HELPER_INT
	jc	1f		// preserve error code

	movb	$DOS_HELPER_COMMANDS_DONE, %al
	movb	$BUILTINS_PLUGIN_VERSION, %ah
	int	$DOS_HELPER_INT
	movb	%bl, %al	// set ret code

1:
	movb	$0x4c, %ah
	int	$0x21

stack_start:
