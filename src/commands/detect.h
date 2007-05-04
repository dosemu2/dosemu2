/*
 * (C) Copyright 1992, ..., 2007 the "DOSEMU-Development-Team".
 *
 * for details see file COPYING.DOSEMU in the DOSEMU distribution
 */

/*
 * This is file detect.h
 * Used for DOSEMU supplied DOS-tools to detect DOSEMU
 * For gas code also some rudimentary runtime system is supported
 * ( argc, argv, print, printnumber ..., exit )
 *
 * Copyright (C) under GPL, The DOSEMU Team 1997-2001
 * Author: Hans Lermen@fgan.de
 */

#ifndef DOSEMU_DETECT_H
#define DOSEMU_DETECT_H

#define DOSEMU_BIOS_DATE		"02/25/93"
#define DOSEMU_MAGIC			"$DOSEMU$"

#if defined(__GNUC__) && !defined(__ASSEMBLER__)
#define DOSEMU_BIOS_DATE_LOCATION	0x000FFFF5
#define DOSEMU_MAGIC_LOCATION		0x000FFFE0
#define FAR
#else
#define DOSEMU_BIOS_DATE_LOCATION	0xF000FFF5
#define DOSEMU_MAGIC_LOCATION		0xF000FFE0
#define FAR far
#endif

#if defined(__ASSEMBLER__)

/* this for assembler code */

# assuming we have a *.com file (CS=DS=SS=ES)
# assuming we are included just at the start point of the program

#ifndef SEGES
#  define SEGES .byte 0x26;
#endif

	cld
	pushw	%es
	les	magicptr,%si
	SEGES lodsl
	cmpl	exp_magic, %eax
	jne	check_old
	SEGES lodsl
	cmpl	exp_magic+4, %eax
	jne	check_old
	SEGES lodsl
	movl	%eax, real_version	# save dosemu version
	popw	%es
	jmp	ok_start_label

check_old:
	les	dateptr,%si
	SEGES lodsl
	cmpl	exp_date,%eax
	SEGES lodsl
	pop	%es
	jne	check_failed
	cmpl	exp_date+4,%eax
	jne	check_failed
	jmp	ok_start_label

check_failed:
	movw	$2,std_handle # print to STDERR
	leaw	exit1msg,%si
print_and_exit:
	call	print
	movb	$1,%al
exit:
	movb	$0x4c,%ah
	int	$0x21

printchar:
	pushw	%ax
	pushw	%bx
#ifdef PRINT_USING_BIOS
	movb	$0x0E,%ah
	xorw	%bx,%bx
	int	$0x10
#else
	pushw	%cx
	pushw	%dx
	movw	%sp,%dx
	addw	$6,%dx		# pointing to AL on stack
	movb	$0x40,%ah
	movw	std_handle,%bx
	movw	$1,%cx		# count =1
	int	$0x21
	popw	%dx
	popw	%cx
#endif
	popw	%bx
	popw	%ax
	ret

# Print an ASCIIZ string to console
# DS:SI pointing to the string
print:
	cld
	jmp	print_entry
print_loop:
	call	printchar
print_entry:
	lodsb
	orb	%al,%al
	jnz	print_loop
	ret

hextable:
	.ascii	"0123456789ABCDEF"

printnibble:
	pushw	%ax
	pushw	%bx
	movw	%ax,%bx
	andw	$15,%bx
	movb	hextable(%bx), %al
	call	printchar
	popw	%bx
	popw	%ax
	ret
	

# Converts AX to ASCII and prints it
# AX=number
# CX=base     2,8,10,16  (e.g =10 to print decimal, =16 for hex)
# All registers preserved
printnumber:
	orw	%ax,%ax
	jnz	printnumber_0
	call	printnibble
	ret
printnumber_0:
	pushw	%ax
	pushw	%dx
	call	printnumber_
	popw	%dx
	popw	%ax
	ret
printnumber_:
	orw	%ax,%ax
	jz	printnumber_1
	xorw	%dx,%dx
	divw	%cx
	pushw	%dx
	call	printnumber_
	popw	%ax
	call	printnibble
printnumber_1:
	ret

printdecimal:
	movw	$10,%cx
	call	printnumber
	ret

printhex:
	movw	$16,%cx
	call	printnumber
	ret

real_version:	.long	0	# here we insert the DOSEMU version
dateptr:	.long	DOSEMU_BIOS_DATE_LOCATION
magicptr:	.long	DOSEMU_MAGIC_LOCATION
exp_date:	.ascii	DOSEMU_BIOS_DATE
exp_magic:	.ascii	DOSEMU_MAGIC
exit1msg:	.ascii	"This program requires DOSEMU to run, aborting"
		.byte	13,10,0
std_handle:	.word	1       #preset for STDOUT

#ifdef NEED_ARGV
ok_start_label:
	movzbw	(0x80),%cx
	jcxz	ok_start_label_1
	movw	$0x81,%si
	leaw	argv,%di
argvscan_0:
	lodsb
	orb	%al,%al
	jz	argvscan_finish
	cmpb	$13,%al
	jz	argvscan_finish
	cmpb	$32,%al
	jz	argvscan_skip
	leaw	-1(%si),%ax
	stosw
	incw	argc
	jmp	argvscan_2
argvscan_skip:
	loop	argvscan_0
argvscan_finish:
	movb	$0,-1(%si)
ok_start_label_1:
	jmp	ok_start_label_2

argvscan_2:
	lodsb
	orb	%al,%al
	jz	argvscan_finish
	cmpb	$13,%al
	jz	argvscan_finish
	cmpb	$32,%al
	jnz	argvscan_2
	movb	$0,-1(%si)
	cmpw	$16,argc
	jb	argvscan_0
	jmp	ok_start_label_2

argc:	.word	0
argv:	.word   0,0,0,0 ,0,0,0,0 ,0,0,0,0 ,0,0,0,0 ,0


# Convert asciiz decimal pointed to by DS:SI int to binary
# returns number in AX
atoi:
	push	%dx
	push	%cx
	xor	%dx,%dx
	cld
atoi_loop:
	lodsb
	orb	%al,%al
	jz	atoi_ex
	andw	$15,%ax
	movw	%dx,%cx
	shlw	$3,%dx
	addw	%cx,%dx
	addw	%cx,%dx
	addw	%ax,%dx
	jmp	atoi_loop
atoi_ex:
	movw	%dx,%ax
	popw	%cx
	popw	%dx
	ret


ok_start_label_2:
#else
ok_start_label:
#endif /* not NEED_ARGV */

# here normal program start happens


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

