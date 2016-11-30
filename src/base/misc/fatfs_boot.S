#
# (C) Copyright 1992, ..., 2014 the "DOSEMU-Development-Team".
#
# for details see file COPYING in the DOSEMU distribution
#


# Boot sector for DOSEMU directly bootable redirected Unix directory
#
# Have a look at fatfs.c before modifying anything!!!

#include "doshelpers.h"

.code16
.text
	.globl	boot_prog
boot_prog:

boot_ofs        = 0x7c00

_io             = 0
_ax             = 4
_bx             = 6
_cx             = 8
_dx             = 0x0a
_si             = 0x0c
_di             = 0x0e
_bp             = 0x10
read_cnt        = 0x12
drive           = 0x13
read_tabs       = 0x14

main:
	cli
	cld
	xorw	%ax,%ax
	movw	%ax,%ss
	movw	$boot_ofs,%sp
	sti
	movw	%ax,%ds
	movw	%ax,%es
	movw	$((boot_txfr - main) + 0x7c40),%bp
	leaw	read_tabs(%bp),%si
	cmpb	read_cnt(%bp), %al
	je	main_30
main_10:
	decb	read_cnt(%bp)
	js	main_20
	movb	drive(%bp),%dl
	movb	$0x42,%ah
	int	$0x13
	jb	main_30
	addw	$0x10,%si
	jmp	main_10
main_20:
	movw	_ax(%bp),%ax
	movw	_bx(%bp),%bx
	movw	_cx(%bp),%cx
	movw	_dx(%bp),%dx
	movw	_si(%bp),%si
	movw	_di(%bp),%di
	pushw	%dx
	pushw	%ax
	pushl	0x1e * 4
	pushw	$0
	pushw	$0x1e * 4
	pushl	_io(%bp)
	movw	_bp(%bp),%bp
	lret
main_30:
	mov	$((boot_message - main) + 0x7c40),%si
write_msg:
	lodsb
	orb	%al,%al
	jnz	write_msg_20
write_msg_10:
	movb	$0,%ah
	int	$0x16
	movw	$0xffff,%ax
	int	$DOS_HELPER_INT
	jmp	write_msg_10
write_msg_20:
	movb	$0x0e,%ah
	movw	$7,%bx
	int	$0x10
	jmp	write_msg

	.align 2
	.globl	boot_txfr
boot_txfr:
	.word	0x0    /* start ofs */
	.word	0x0    /* start seg */
	.word	0x0    /* ax */
	.word	0x0    /* bx */
	.word	0x0    /* cx */
	.word	0x0    /* dx */
	.word	0x0    /* si */
	.word	0x0    /* di */
	.word	0x0    /* bp */
	.byte	0x0    /* number of list items */
	.byte	0x0    /* drive */
	.fill	0x10*4 /* sizeof(struct ibm_ms_diskaddr_pkt) * list length */

	.align 4
/*
 * Currently this sequence is ignored in fatfs.c as we are unable to trigger
 * MS-DOS 7.10 to print it to confirm it's actually required. After checking
 * over 40 different boot blocks I cannot find this sequence occuring so I
 * doubt the actual values are significant. If subsequently we find that a
 * failing MS-DOS 7+ boot is printing our failure message with five bytes
 * missing off the front we can investigate adjusting the string address for
 * MS-DOS 7+ in fatfs.c and reenable this if necessary.
 *
 * 5 magic bytes (for IO.SYS compatibility) :-)
 *
	.byte	4, 3, 2, 1, 13
 */
	.globl	boot_message
boot_message:
	.asciz "Default fatfs_boot.S message\r\n"

	.globl	boot_prog_end
boot_prog_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
