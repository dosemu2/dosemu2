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
pm_adr:	.long	0
msg0:	.ascii	"Int 0x2f, AX=0x1686 returns: 0x$"
msg1:	.ascii	"No DPMI-Server"
crlf:	.byte	0x0d, 0x0a, '$
msg2:	.ascii	"can't switch to protected mode!"
	.byte	0x0d, 0x0a, '$
msg3:	.ascii	"can't alloc dos memory!"
	.byte	0x0d, 0x0a, '$
msg4:	.ascii	"number of paragraphs needed by DPMI-Server: 0x$"
	.org	0x0300
end_of_stack:

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
	int	#0x21		!shrink memory
	mov	ax,#0x1687
	int	#0x2f
	or	ax,ax
	mov	dx,#msg1	!no dpmi-server
	jnz	bye
	mov	pm_adr,di
	mov	pm_adr+2,es
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
	jc	nodpmi

use16
	call	test_dpmi
	jmp	exit
use16

nodpmi:	mov	dx,#msg2
bye:	mov	ah,#0x09
	int	#0x21
exit:	mov	ax,#0x4c00	! bye bye
	int	#0x21
