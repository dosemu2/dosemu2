# 
# (C) Copyright 1992, ..., 2004 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#

#
# generic vm86 starter code for '32bit DOS code' running  via
# coopthreads pluggin.
#
# (C) Copyright 2001, Hans Lermen <lermen@fgan.de>
#

#include <doshelpers.h>
#include "config/plugin_config.h"

.code16
.text
	.globl	_start16
_start16:

#include "detect.h"

#define stack_size		0x100
#define stack			(stack_start + stack_size)
#define psp_size		0x100

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
				# back, thread deleted
	movb	$DOS_HELPER_COMMANDS_DONE, %al
	movb	$BUILTINS_PLUGIN_VERSION, %ah
	int	$DOS_HELPER_INT

	int	$0x20

stack_start:
