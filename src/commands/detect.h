/*
 * (C) Copyright 1992, ..., 1999 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING in the DOSEMU distribution
 */

/*
 * This is file detect.h
 * Used for DOSEMU supplied DOS-tools to detect DOSEMU
 * For as86 code also some rudimentary runtime system is supported
 * ( argc, argv, print, printnumber ..., exit )
 *
 * Copright (C) under GPL, The DOSEMU Team 1997
 * Author: Hans Lermen@fgan.de
 */

#ifndef DOSEMU_DETECT_H
#define DOSEMU_DETECT_H

#define DOSEMU_BIOS_DATE		"02/25/93"
#define DOSEMU_MAGIC			"$DOSEMU$"

#if defined(__GNUC__)
#define DOSEMU_BIOS_DATE_LOCATION	0x000FFFF5
#define DOSEMU_MAGIC_LOCATION		0x000FFFE0
#define FAR
#else
#define DOSEMU_BIOS_DATE_LOCATION	0xF000FFF5
#define DOSEMU_MAGIC_LOCATION		0xF000FFE0
#define FAR far
#endif

#if defined(__ASSEMBLER__) && defined(__AS86__)

/* this for as86 assembler code */

#define JMPL br /* 'br' on as86 does a 'jmp near label' */

; assumeing we have a *.com file (CS=DS=SS=ES)
; assuming we are included just at the start point of the programm

	cld
	push	es
	les	si,magicptr
	seg es
	lodsd
	cmp	eax,dword ptr exp_magic
	jne	check_old
	seg es
	lodsd
	cmp	eax,dword ptr exp_magic+4
	jne	check_old
	seg es
	lodsd
	mov	dword ptr real_version,eax	! save dosemu version
	pop	es
	JMPL	ok_start_label

check_old:
	les	si,dateptr
	seg es
	lodsd
	cmp	eax,dword ptr exp_date
	seg es
	lodsd
	pop	es
	jne	check_failed
	cmp	eax,dword ptr exp_date+4
	jne	check_failed
	JMPL	ok_start_label

check_failed:
	mov	word ptr std_handle,#2 ; print to STDERR
	lea	si,exit1msg
print_and_exit:
	call	print
	mov	al,#1
exit:
	mov	ah,#0x4c
	int	0x21

printchar:
	push	ax
	push	bx
#ifdef PRINT_USING_BIOS
	mov	ah,#0x0E
	xor	bx,bx
	int	0x10
#else
	push	cx
	push	dx
	mov	dx,sp
	add	dx,#6		; pointing to AL on stack
	mov	ah,#0x40
	mov	bx,word ptr std_handle
	mov	cx,#1		; count =1
	int	0x21
	pop	dx
	pop	cx
#endif
	pop	bx
	pop	ax
	ret

; Print an ASCIIZ string to console
; DS:SI pointing to the string
print:
	cld
	jmp	print_entry
print_loop:
	call	printchar
print_entry:
	lodsb
	or	al,al
	jnz	print_loop
	ret

hextable:
	.ascii	"0123456789ABCDEF"

printnibble:
	push	ax
	push	bx
	mov	bx,ax
	and	bx,#15
	mov	al,byte ptr hextable[bx]
	call	printchar
	pop	bx
	pop	ax
	ret
	

: Converts AX to ASCII and prints it
; AX=number
; CX=base     2,8,10,16  (e.g =10 to print decimal, =16 for hex)
; All registers preserved
printnumber:
	or	ax,ax
	jnz	printnumber_0
	call	printnibble
	ret
printnumber_0:
	push	ax
	push	dx
	call	printnumber_
	pop	dx
	pop	ax
	ret
printnumber_:
	or	ax,ax
	jz	printnumber_1
	xor	dx,dx
	div	cx
	push	dx
	call	printnumber_
	pop	ax
	call	printnibble
printnumber_1:
	ret

printdecimal:
	mov	cx,#10
	call	printnumber
	ret

printhex:
	mov	cx,#16
	call	printnumber
	ret

real_version:	.long	0	! here we insert the DOSEMU version
dateptr:	.long	DOSEMU_BIOS_DATE_LOCATION
magicptr:	.long	DOSEMU_MAGIC_LOCATION
exp_date:	.ascii	DOSEMU_BIOS_DATE
exp_magic:	.ascii	DOSEMU_MAGIC
exit1msg:	.ascii	"This program requires DOSEMU to run, aborting"
		db	13,10,0
std_handle:	dw	1       ;preset for STDOUT

#ifdef NEED_ARGV
ok_start_label:
	movzx	cx,byte ptr [0x80]
	jcxz	ok_start_label_1
	mov	si,#0x81
	lea	di,argv
argvscan_0:
	lodsb
	or	al,al
	jz	argvscan_finish
	cmp	al,#13
	jz	argvscan_finish
	cmp	al,#32
	jz	argvscan_skip
	lea	ax,[si-1]
	stosw
	inc	word ptr argc
	jmp	argvscan_2
argvscan_skip:
	loop	argvscan_0
argvscan_finish:
	mov	byte ptr [si-1],#0
ok_start_label_1:
	JMPL	ok_start_label_2

argvscan_2:
	lodsb
	or	al,al
	jz	argvscan_finish
	cmp	al,#13
	jz	argvscan_finish
	cmp	al,#32
	jnz	argvscan_2
	mov	byte ptr[si-1],#0
	cmp	word ptr argc,#16
	jb	argvscan_0
	JMPL	ok_start_label_2

argc:	.word	0
argv:	.word   0,0,0,0 ,0,0,0,0 ,0,0,0,0 ,0,0,0,0 ,0


; Convert asciiz decimal pointed to by DS:SI int to binary
; returns number in AX
atoi:
	push	dx
	push	cx
	xor	dx,dx
	cld
atoi_loop:
	lodsb
	or	al,al
	jz	atoi_ex
	and	ax,#15
	mov	cx,dx
	shl	dx,#3
	add	dx,cx
	add	dx,cx
	add	dx,ax
	jmp	atoi_loop
atoi_ex:
	mov	ax,dx
	pop	cx
	pop	dx
	ret


ok_start_label_2:
#else
ok_start_label:
#endif /* not NEED_ARGV */

; here normal program start happens


#else

struct dosemu_detect {
    union {
	char magic_[8];
	unsigned long magic[2];
    } u;
    unsigned long version;
};

static int old_dosemu_compatibility(void)
{
    static unsigned long FAR *given = (void FAR *) DOSEMU_BIOS_DATE_LOCATION;
    static unsigned long *expected = (unsigned long *)DOSEMU_BIOS_DATE;

    return (given[0] == expected[0] && given[1] == expected[1]);
}

static unsigned long is_dosemu(void)
{
    static struct dosemu_detect FAR *given = (void FAR *) DOSEMU_MAGIC_LOCATION;
    static struct dosemu_detect expected = {DOSEMU_MAGIC};

    if (   (expected.u.magic[0] == given->u.magic[0])
           && (expected.u.magic[1] == given->u.magic[1]) )
                    return given->version;
    return old_dosemu_compatibility();
}

#endif /* not __ASSEMBLER__ */

#endif /* DOSEMU_DETECT_H */

