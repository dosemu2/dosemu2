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

.code16
.text
	.globl	_start16
_start16:

#define	DOS_HELPER_COMMANDS	0xc0
#define	saved_ax		0x108
#define real_psp		0x10a
#define far_sp			0x10c
#define far_ss			0x10e
#define saved_sp		0x110
#define stack_size		0x100
#define stack			(heap_end + stack_size)
#define psp_size		0x100

	jmp	start
	.align  4,0
	.word	0xd0c0		# magic
	.word	heap		# offset of heap buffer

	.align	128,0
start:
	movw	$stack, %sp
	pushw	%ds
	pushw	%es
	movw	%ax, %cs:(saved_ax)
				# first deallocate the unused memory, ES=PSP now
	movb	$0x4a, %ah
	movw	$(stack - _start16 + psp_size), %bx
	shrw	$4, %bx
	incw	%bx
	int	$0x21
				# get the real psp, in case we
				# were loaded by an .exe
	movb	$0x62, %ah
	int	$0x21
	movw	%bx, %cs:(real_psp)
				# are we 'far' called by an .exe ?
	movw	%cs,%ax
	cmp	%ax,%bx
	je	normal_start

far_called:			# yes, far-called by .exe, have to save something
	movw	%ss,%ax
	movw	%ax, %cs:(far_ss)
	movw	%sp, %cs:(far_sp)
	movw	%cs, %ax
	movw	%ax, %ds
	movw	(saved_sp), %bx
	movw	%ax, %ss
	movw	%bx, %sp

				# ok, all is fine
continue:
	movw	$DOS_HELPER_COMMANDS, %ax
	int	$0xe6
				# back, thread deleted
	cmpw	$0, %cs:(far_ss)
	je	normal_exit
				# we have to return to .exe program
	lss	%cs:(far_sp), %sp	# restore stack
	popw	%es
	popw	%ds
	lret

normal_exit:
				# if we come here we have AL = exitcode
	movb	$0x4c, %ah
	int	$0x21		# exit

#ifdef TEST
	.align	16,0
called:
	movw	$DOS_HELPER_COMMANDS, %ax
	movb	$3, %ah
	int	$0xe6
	ret
	.align	16,0
called2:
	lret
#endif

normal_start:			# we are started as a .com file
				# check if we really are running DOSEMU
#if 0
#include "detect.h"
#endif
	jmp	continue

	.align	16,0
heap:

#ifdef TEST
	.ascii	",0x700,1,test"
#endif
	.byte	0

heap_end:
