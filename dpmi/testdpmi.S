!dpmi-test-program for dos

use16
.text
.data
.bss

.align 0

.globl _main              ! for ld86
_main:

	.org	0x0100
	jmp	init
pm_adr:		.long	0
		.word	0
selector:	.word	0
selector2:	.word	0
memadr:		.long	0
memhandle:	.long	0
descriptorbuf:	.long	0, 0

RealMode_edi:	.long	0
RealMode_esi:	.long	0
RealMode_ebp:	.long	0
RealMode_esp:	.long	0
RealMode_ebx:	.long	0
RealMode_edx:	.long	0
RealMode_ecx:	.long	0
RealMode_eax:	.long	0
RealMode_flags:	.word	0
RealMode_es:	.word	0
RealMode_ds:	.word	0
RealMode_fs:	.word	0
RealMode_gs:	.word	0
RealMode_ip:	.word	0
RealMode_cs:	.word	0
RealMode_sp:	.word	0
RealMode_ss:	.word	0

msg0:	.ascii	"Int 0x2f, AX=0x1686 returns: 0x$"
msg1:	.ascii	"No DPMI-Server"
crlf:	.byte	0x0d, 0x0a, '$
msg2:	.ascii	"can't switch to protected mode!"
	.byte	0x0d, 0x0a, '$
msg3:	.ascii	"can't alloc dos memory!"
	.byte	0x0d, 0x0a, '$
msg4:	.ascii	"number of paragraphs needed by DPMI-Server: 0x$"
msg5:	.ascii	"now back in real mode!"
	.byte	0x0d, 0x0a, '$
msg6:	.ascii	"DPMI-Version: 0x$"
msg7:	.ascii	"Can't alloc descriptor"
	.byte	0x0d, 0x0a, '$
msg8:	.ascii	"Can't alloc memory"
	.byte	0x0d, 0x0a, '$
msge:	.ascii	"Leaving DPMI"
	.byte	0x0d, 0x0a, '$
vendorstring:
	.ascii	"MS-DOS"
	.byte	0
	.org	0x0400
end_of_stack:

use16
real_mode_proc:
	mov	dx,#msg5
	mov 	ah,#0x09
	int	#0x21
	retf

dl_hex_print:
	push	dx
	shr	dx,#0x04
	call	phb
	pop	dx
phb:	and	dl,#0x0f
	add	dl,#0x30
	cmp	dl,#0x39
	jle	noc
	add	dl,#0x07
noc:	mov	ah,#0x02
	int	#0x21
	ret	

dx_hex_print:
	push	dx
	mov	dl,dh
	call	dl_hex_print
	pop	dx
	jmp	dl_hex_print

crlf_print:
	mov	dx,#crlf
	mov	ah,#0x09
	int	#0x21
	ret

test_dpmi:
	pusha
	mov	dx,#msg0
	mov	ah,#0x09
	int	#0x21
	mov	ax,#0x1686
	int	#0x2f
	mov	dx,ax
	call	dx_hex_print
	call	crlf_print
	popa
	ret

init:	cli
	mov	ax,cs
	mov	ds,ax
	mov	es,ax
	mov	ss,ax
	mov	sp,#end_of_stack
	cld
	sti
	mov	ah,#0x4a
	mov	bx,#exit+0x05
	shr	bx,#0x04
	inc	bx
	int	#0x21		! shrink memory
	mov	ax,cs
	mov	RealMode_cs,ax
	mov	RealMode_ds,ax
	mov	RealMode_es,ax
	mov	RealMode_fs,ax
	mov	RealMode_gs,ax
	mov	RealMode_ip,#real_mode_proc
	mov	ax,#0x1687
	int	#0x2f
	or	ax,ax
	jnz	nodpmi
	mov	pm_adr,di
	mov	pm_adr+2,es
	pusha
	mov	dx,#msg6
	mov	ah,#0x09
	int	#0x21
	popa
	pusha
	call	dx_hex_print	! print Version number in hex
	call	crlf_print
	popa
	pusha
	mov	dx,#msg4
	mov	ah,#0x09
	int	#0x21
	mov	dx,si
	call	dx_hex_print
	call	crlf_print
	popa
	call	test_dpmi
	test	si,si
	jz	enter_pmode
	mov	bx,si
	mov	ah,#0x48	!alloc memory for private data
	int	#0x21
	mov	es,ax
	jnc	enter_pmode
	mov	dx,#msg3
	jmp	bye
enter_pmode:
	xor	ax,ax		! 16-Bit Client
	inc	ax		! No 32-Bit Client
	call	far @pm_adr
	jc	nopm

use16
	call	test_dpmi
	mov	ax,ds
	mov	es,ax
	mov	ax,#0x000b	! Get Descriptor
	mov	bx,cs
	mov	edi,#descriptorbuf
	int	#0x31
	mov	dx,descriptorbuf+6
	call	dx_hex_print
	mov	dx,descriptorbuf+4
	call	dx_hex_print
	mov	dx,descriptorbuf+2
	call	dx_hex_print
	mov	dx,descriptorbuf
	call	dx_hex_print
	call	crlf_print
	mov	di,#RealMode_edi
	mov	cx,#0
	mov	ax,#0x0301	! Call Real Mode Procedure With Far Return Frame
	int	#0x31
	mov	si,#vendorstring
	mov	ax,#0x168a	! Get Vendor API Entry Point
	int	#0x2f
	mov	dx,ax
	call	dx_hex_print
	call	crlf_print
	mov	dx,es
	call	dx_hex_print
	mov	dx,di
	call	dx_hex_print
	call	crlf_print
	xor	ax,ax		! Allocate Descriptors
	mov	selector,ax
	mov	selector2,ax
	mov	cx,#1
	int	#0x31
	mov	dx,msg7
	jc	bye
	mov	selector,ax
	mov	ax,#0x0501	! Allocate Memory Block
	mov	bx,#0x0001	! 64 Kbyte
	xor	cx,cx
	int	#0x31
	mov	dx,#msg8
	jc	bye
	mov	memadr,cx
	mov	memadr+2,bx
	mov	memhandle,si
	mov	memhandle+2,di	
	mov	ax,#0x0007	! Set Segment Base Adress
	mov	bx,selector
	mov	dx,memadr
	mov	cx,memadr+2
	int	#0x31
	jc	exit
	mov	ax,#0x00008	! Set Segment Limit
	mov	bx,selector
	xor	cx,cx
	mov	dx,#0xffff	! 64 Kbyte
	int	#0x31
	jc	exit
	mov	ax,#0x0009	! Set Access Right
	mov	bx,selector
	mov	cl,#0xfa	! Code Segment
!	xor	ch,ch		! 16-Bit Segment
	mov	ch,#0x40	! 32-Bit Segment
	int	#0x31
	mov	bx,selector
	mov	ax,#0x000a	! Create Code Segment Alias
	int	#0x31
	jc	exit
	mov	selector2,ax
	mov	ax,ds
	mov	es,ax

	mov	ax,#0x0502	! Free Memory Block
	mov	si,memhandle
	mov	di,memhandle+2
	int	#0x31
	mov	ax,#0x0001	! Free Descriptor
	mov	bx,selector
	int	#0x31
	mov	ax,#0x0001	! Free Descriptor
	mov	bx,selector2
	int	#0x31
	mov	dx,#msge
	jmp	bye
use16

nodpmi:	mov	dx,#msg1	! no dpmi-server
	jmp	bye
nopm:	mov	dx,#msg2
bye:	mov	ah,#0x09
	int	#0x21
exit:	mov	ax,#0x4c00	! bye bye
	int	#0x21
