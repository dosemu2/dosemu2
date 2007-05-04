# 
# (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
#
# for details see file COPYING.DOSEMU in the DOSEMU distribution
#


# Boot sector for DOSEMU directly bootable redirected Unix directory
#
# Have a look at fatfs.c before modifying anything!!!

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

tab	=	boot_ofs + 0x3e

main:
	cli
	cld
	xorw	%ax,%ax
	movw	%ax,%ss
	movw	$boot_ofs,%sp
	sti
	movw	%ax,%ds
	movw	%ax,%es
	movw	tab,%bp
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
	mov	$((text - main) + 0x7c40),%si
write_msg:
	lodsb
	orb	%al,%al
	jnz	write_msg_20
write_msg_10:
	movb	$0,%ah
	int	$0x16
	movw	$0xffff,%ax
	int	$0xe6
	jmp	write_msg_10
write_msg_20:
	movb	$0x0e,%ah
	movw	$7,%bx
	int	$0x10
	jmp	write_msg

/*# 5 magic bytes (for IO.SYS compatibility) :-))*/
	.byte	4, 3, 2, 1, 13
text:

	.globl	boot_prog_end
boot_prog_end:

#ifdef __ELF__
.section .note.GNU-stack,"",%progbits
#endif
