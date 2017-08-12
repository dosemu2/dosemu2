# Mach Operating System
# Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
# Copyright (c) 1991 IBM Corporation
# All Rights Reserved.
#
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation,
# and that the nema IBM not be used in advertising or publicity
# pertaining to distribution of the software without specific, written
# prior permission.
#
# CARNEGIE MELLON AND IBM ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON AND IBM DISCLAIM ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# Carnegie Mellon requests users of this software to return to
#
#  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
#
# any improvements or extensions that they make and grant Carnegie Mellon
# the rights to redistribute these changes.
#
# MACHFS.ASM  MS-DOS device driver to interface mach file system
# with the dos server's monitor.
#
# Version 1.1
#
# Gerald Malan (grm) 4/5/1991
#
# modified for the linux dos emulator by Andrew Tridgell 13/4/93
# translated into as86 form by Robert Sanders ('murrcan style date!) 4/13/93
# (I probably broke something, but it seems to work)
#

#include "doshelpers.h"
#include "redirect.h"

.code16
.text
	.globl	_start16
_start16:

MaxCmd	=	12

# Common request header offsets
LEN	=	0
UNITS	=	1
CMD	=	2
STATUS	=	3

# Init only request header offsets
ENDOFS	=	14
ENDSEG	=	16


Header:
	.long	-1			# link to next device driver
HdrAttr:
	.word   0b1000000000000000	# attribute word for char driver
	.word	Strat			# ptr to strategy routine
	.word	Intr			# ptr to interrupt service routine
	.ascii	"EMUFS$  "		# logical-device name

.align 2

RHPtr:	.long	0			# ptr to request header

Dispatch:
	.word	Init			# initialize driver
	.word	Dummy
	.word	Dummy
	.word	Dummy
	.word	Dummy			# read
	.word	Dummy
	.word	Dummy
	.word	Dummy
	.word	Dummy			# write
	.word	Dummy			# write with verify
	.word	Dummy
	.word	Dummy
	.word	Dummy

Strat:
	movw	%bx, %cs:RHPtr
	movw	%es, %cs:RHPtr+2
	lret

Intr:
	pusha
	pushw	%ds
	pushw	%es

	pushw	%cs
	popw	%ds

	les	RHPtr,%di		# let es:di = request header

	movb	%es:CMD(%di),%bl
	xorb	%bh,%bh
	cmpw	$MaxCmd,%bx
	jle	1f
	movw	$0x8003,%ax		# Set error
	jmp	2f

1:	shlw	%bx

	call	*Dispatch(%bx)

	les	RHPtr,%di

2:	or	$0x100,%ax		# Merge done bit with status
	movw	%ax,%es:STATUS(%di)

	popw	%es
	popw	%ds
	popa
	lret

Dummy:
# Tell the caller we don't support this command. For example if we do
# COPY AUTOEXEC.BAT EMUFS$
# the user gets to the see the 'Abort, Retry, Fail' message.
	movw	$0x8003,%ax
	ret

InitCode:	# All data and code below will be discarded after Init

MFSini:
	pushw %es
	pushw %di

	pushw %cs
	popw %ds

	movw	$0x3000, %ax
	int	$0x21
	mov	%al, major
	mov	%ah, minor

	movw	$0x5200, %ax
	int	$0x21
	# Result in ES:BX

	movw	$0x5d06, %ax
	int	$0x21
	# Result in DS:SI
	movw	%cx, %cs:sdasize

	mov	%cs:major, %ah
	cmp	$3, %ah
	jne	.LisPC40
	mov	%cs:minor, %ah
	cmp	$9, %ah
	ja	.LisPC31
	movw	%cs:sdasize, %cx
	cmpw	$SDASIZE_CQ30, %cx
	je	.LisCQ30

.LisPC30:
	movw	$REDVER_PC30, %cx
	jmp	1f
.LisPC31:
	movw	$REDVER_PC31, %cx
	jmp	1f
.LisPC40:
	movw	$REDVER_PC40, %cx
	jmp	1f
.LisCQ30:
	movw	$REDVER_CQ30, %cx

1:
	pushw	%bx
	popw	%dx
	movw	$DOS_SUBHELPER_MFS_REDIR_INIT, %bx
	movw    $DOS_HELPER_MFS_HELPER, %ax
	int	$DOS_HELPER_INT

	popw %di
	popw %es
	ret

.macro unrev inum err
	movb $0x35,%ah
	movb $\inum,%al
	int $0x21
	movw %bx,%di
	movb $DOS_HELPER_REVECT_HELPER,%al
	movb $DOS_SUBHELPER_RVC_UNREVECT,%bl
	movb $\inum,%ah
	movb $0,%bh
	int $DOS_HELPER_INT
	jc \err
	movw %si,%dx
	movb $0x25,%ah
	movb $\inum,%al
	int $0x21
.endm

Init:
	movb $DOS_HELPER_REVECT_HELPER,%al
	movb $DOSEMU_EMUFS_DRIVER_VERSION,%ah
	movw $DOS_SUBHELPER_RVC_VERSION_CHECK,%bx
	int $DOS_HELPER_INT
	jc .LtooOld

	movw $DOS_HELPER_MFS_HELPER, %ax
	movw $DOS_SUBHELPER_MFS_EMUFS_INIT, %bx
	int $DOS_HELPER_INT
	jc .LisLoaded

.LgoodToInstall:
	# es:di points to request header, see Intr
	pushw %ds
	pushw %es
	pushw %di
	# unrevect int2f and int21
	unrev 0x2f 1f
	unrev 0x21 1f
1:
	popw %di
	popw %es
	popw %ds

	call	MFSini

	pushw	%cs
	popw	%ds
	movw	$mfsMsg, %dx
	cmp	$1, %ax				# Check the result from MFSini
	je	2f
	movw	$rvcMsg, %dx
2:
	movb	$9, %ah				# Display the banner
	int	$0x21

	movw	$InitCode, %es:ENDOFS(%di)	# End of resident driver
	movw	%cs, %es:ENDSEG(%di)

	xorw %ax,%ax
	ret

.LtooOld:
	pushw %cs
	popw %ds
	movb	$9, %ah
	movw	$oldMsg, %dx
	int	$0x21
	jmp	Error

.LisLoaded:
	pushw %cs
	popw %ds
	movb	$9, %ah
	movw	$isLoadedMsg, %dx
	int	$0x21
#	jmp	Error

Error:
	movw	$0, %cs:HdrAttr			# Set to block type
	movw	$0, %es:13(%di)			# Units = 0

	movw	$0, %es:ENDOFS(%di)		# Break addr = cs:0000
	movw	%cs, %es:ENDSEG(%di)

	movw	$0x8003,%ax			# Set error
	ret

major:	.byte	0
minor:	.byte	0
sdasize: .word  0

oldMsg: .ascii	"emufs.sys is too old, please update.\r\n"
	.ascii	"Installation aborted.\r\n$"

isLoadedMsg:
	.ascii	"emufs.sys has already been loaded.\r\n$"

mfsMsg: .ascii "EMUFS host file and print access available\r\n$"

rvcMsg: .ascii "EMUFS revectoring only\r\n$"

