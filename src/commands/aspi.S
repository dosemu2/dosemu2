# aspi.S stub for the Linux DOS emulator - derived from ems.S
# please forgive my assembler programming style :-) ki@kretz.co.at
#
#

.code16
.text
	.globl	_start16
_start16:

MaxCmd	=	15
cr	=	0xd
lf	=	0xa
eom	=	'$'		# DOS end-of-string character (barf)
Helpint =	0xe6		# 0xe6 is Int for Linux helpers
Aspihlp	=	0x41		# aspi helper

Header:
	.long	-1		# link to next device driver
	.word	0xC000		# attribute word for driver 
				# (char, supports IOCTL strings (it doesn't!)
	.word	Strat		# ptr to strategy routine
	.word	Intr		# ptr to interrupt service routine
	.ascii	"SCSIMGR$"	# logical-device name

# the Strat and Intr routines are entered with a "far call".  I don't
# know how to tell gas that as I would in Turbo Assembler, so I just
# explicitly "lret" at the end of their execution.  Be careful!

RHPtr:	.long	0		# ptr to request header

InitDone: .word 0		# 1 when initialisation is complete

Dispatch:
	.word	Init		# initialize driver
	.word	Dummy		# Media Check ,block only
	.word	Dummy		# Build BPB, block only
	.word	Ioctl		# Ioctl
	.word	Dummy		# read
	.word	Dummy  		# non-destructive read
	.word	Dummy		# input status
	.word	Dummy		# flush input
	.word	Dummy		# write
	.word	Dummy		# write with verify
	.word	Dummy		# output status
	.word	Dummy		# flush output
	.word	Dummy		# IOCTL output (??)
	.word	Dummy		# open device
	.word	Dummy		# close device
	.word	Dummy		# removeable media check

Strat:
	movw	%bx, %cs:RHPtr
	movw	%es, %cs:RHPtr+2
	lret

Intr:
	pushw	%ax
	pushw	%bx
	pushw	%cx
	pushw	%dx
	pushw	%ds
	pushw	%es
	pushw	%di
	pushw	%si
	pushw	%bp
	
	pushw	%cs
	popw	%ds

	les	RHPtr,%di	# let es:di = request header

	movb	%es:2(%di),%bl
	xorb	%bh,%bh
	cmpw	$MaxCmd, %bx
	jle	Intr1
	call	Error
	jmp	Intr2

Intr1:	shlw	%bx

	callw	*Dispatch(%bx)

	les	RHPtr,%di

Intr2:	orw	$0x100,%ax		# Merge done bit with status
	movw	%ax,%es:3(%di)

	popw	%bp
	popw	%si
	popw	%di
	popw	%es
	popw	%ds
	popw	%dx
	popw	%cx
	popw	%bx
	popw	%ax
	lret

Ioctl:	
	pushw	%bx
	movw	%es:14(%di),%ax
	movw 	%es:16(%di),%bx
	pushw	%ax
	pushw	%bx
	popw	%es
	popw	%di

	movw 	$DO_aspi,%ax
	movw	%ax,%es:(%di)
	pushw	%cs
	popw	%ax
	movw	%ax,%es:2(%di)
	popw	%bx
	xorw	%ax,%ax
	ret

DO_aspi:
	movw 	$Aspihlp,%ax
	int	$Helpint
	lret
Dummy:
	xorw	%ax,%ax
	ret

Error:
	movw	$0x8003,%ax
	ret

Init:
	xorw	%bx,%bx

	pushw	%cs
	popw	%ds
	movb	$Aspihlp,%al
	movb	$1,%ah		# check wether there are devices configured
	int	$Helpint
	cmp	$1,%ah
	je	Initabort	# we have no ASPI helper
	or	%ax,%ax
	jnz	Initgo
Initabort:
	mov	$9,%ah 
	mov	$OutMess,%dx
	int	$0x21
	movw	$Header, %es:14(%di)
	movw	%cs, %es:16(%di)
	jmp	Error

Initgo:
	movb	$9, %ah
	movw	$Message, %dx
	int	$0x21

	movw	$Init, %es:14(%di)
	movw	%cs, %es:16(%di)

	xorw	%ax,%ax

	ret


Message:.ascii	"[ASPI driver installed]"
	.byte	cr,lf,eom

OutMess:
	.byte	cr,lf,lf
	.ascii	"No ASPI devices configured."
	.byte	cr,lf
	.ascii	"Installation aborted."
	.byte	cr,lf,lf,eom

NoName:	.ascii	"NO NAME"
	.byte	0
