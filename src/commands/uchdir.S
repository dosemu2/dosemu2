! uchdir.S, to generate uchdir.com, for dosemu 0.67.16, 97/11/22
! This does a chdir on the Linux directory, not the DOS one
!             ^^^^         ^^^^^
! USAGE:
!   uchdir newpath
!                                            Hans Lermen@fgan.de
!

.text
.data
.bss

.align 0

.globl _main	; for ld86
_main:

	.org	0x100	; which is were *.com files start

#define NEED_ARGV
#include "detect.h"

	cmp	argc,#1
	je	ok_1
	lea	si,abortmsg
	call	print_and_exit
abortmsg:
	.ascii	"Run chdir newpath"
	db	13,10,0

ok_1:
	mov	ax,#0x81 ; chdir
	mov	dx,argv	; es:dx pointing to path
	int	0xe6	; doshelper
	call	exit
