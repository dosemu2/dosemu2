! 
! (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!


! Boot sector for DOSEMU directly bootable redirected Unix directory
!
! Have a look at fatfs.c before modifying anything!!!

.text
.data
.bss

.globl _main              ! for ld86
_main:

      .org       0

boot_ofs        equ 0x7c00

_io             equ 0
_ax             equ 4
_bx             equ 6
_cx             equ 8
_dx             equ 0x0a
_si             equ 0x0c
_di             equ 0x0e
_bp             equ 0x10
read_cnt        equ 0x12
unused_byte     equ 0x13
read_tabs       equ 0x14

	jmp	main

	.org	0x3e
tab:	.word	0
	.org	0x40
main:
	cli
	cld
	xor	ax,ax
	mov	ss,ax
	mov	sp,#boot_ofs
	sti
	mov	ds,ax
	mov	es,ax
	mov	bp, [tab + boot_ofs]
	lea	si,[bp + read_tabs]
main_10:
	dec	byte ptr [bp + read_cnt]
	js	main_20
	mov	dl,#0x80
	mov	ah,#0x42
	int	0x13
	jb	main_30
	add	si,#0x10
	jmp	main_10
main_20:
	mov	ax,[bp + _ax]
	mov	bx,[bp + _bx]
	mov	cx,[bp + _cx]
	mov	dx,[bp + _dx]
	mov	si,[bp + _si]
	mov	di,[bp + _di]
	push	dx
	push	ax
	push	dword ptr [0x1e * 4]
	push	#0
	push	#0x1e * 4
	push	dword ptr [bp + _io]
	mov	bp,[bp + _bp]
	retf
main_30:
	mov	si,#text + boot_ofs
write_msg:
	lodsb
	or	al,al
	jnz	write_msg_20
write_msg_10:
	mov	ah,#0
	int	0x16
	jmp	write_msg_10
write_msg_20:
	mov	ah,#0x0e
	mov	bx,#7
	int	0x10
	jmp	write_msg

! 5 magic bytes (for IO.SYS compatibility) :-))
	.byte	4, 3, 2, 1, 13
text:

