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

! assumeing we have a *.com file (CS=DS=SS=ES)
! assuming we are included just at the start point of the programm

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
	jmp	ok_start_label_

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
	jmp	ok_start_label

check_failed:
	lea	si,exit1msg
	call	print
	mov	ax,#1
	int	0x20

! Print an ASCIIZ string to console
! DS:SI pointing to the string
print:
	mov	ax,#0x0E00
	xor	bx,bx
	jmp	print_entry
print_loop:
	int	0x10
print_entry:
	lodsb
	or	al,al
	jnz	print_loop
	ret

ok_start_label_: jmp ok_start_label ! damned as86 does simple 'jmp' short

real_version:	.long	0	! here we insert the DOSEMU version
dateptr:	.long	DOSEMU_BIOS_DATE_LOCATION
magicptr:	.long	DOSEMU_MAGIC_LOCATION
exp_date:	.ascii	DOSEMU_BIOS_DATE
exp_magic:	.ascii	DOSEMU_MAGIC
exit1msg:	.ascii	"This program requires DOSEMU to run, aborting"
		db	13,10,0

! here normal program start happens

ok_start_label:

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

