/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Common Device Driver Skeleton
 *
 * Author: Andrew Bird
 */

// Common
LEN	=	0
UNIT	=	1
CMD	=	2
STATUS	=	3

// Init function 0
HSIZE	=	12
NUNITS	=	13
ENDOFS	=	14
ENDSEG	=	16
DATOFS	=	18
DATSEG	=	20
FIRSTD	=	22

.macro CREATE_COMMON devattr devname tabname
	.long	-1		# link to next device driver
HdrAttr:
	.word	\devattr	# attribute word for driver
				# (char, supports IOCTL strings (it doesn't!)
	.word	Strat		# ptr to strategy routine
	.word	Intr		# ptr to interrupt service routine
HdrStr:
	.ascii	"\devname"	# logical-device name

.LRHPtr:.long 0			# ptr to request header

Strat:
	mov	%bx, %cs:.LRHPtr
	mov	%es, %cs:.LRHPtr+2
	lret

Intr:
	pusha
	pushw	%ds
	pushw	%es

	pushw	%cs
	popw	%ds
	les	.LRHPtr,%di	# let es:di = request header

	movzbw	%es:CMD(%di), %si
	movw	%si, %bx
	cmpw	$((\tabname\()_end - \tabname) / 2), %bx
	jb	1f
	mov	$0x8003, %ax	# error
	jmp	2f

1:	shlw	%si
	callw	*\tabname(%si)
	les	.LRHPtr,%di

2:	orw	$0x100,%ax	# Merge done bit with status
	mov	%ax,%es:STATUS(%di)

	popw	%es
	popw	%ds
	popa
	lret
.endm
