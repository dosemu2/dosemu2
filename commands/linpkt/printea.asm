	public	print_ether_addr
print_ether_addr:
	mov	cx,EADDR_LEN
print_ether_addr_0:
	push	cx
	lodsb
	mov	cl,' '			;Don't eliminate leading zeroes.
	call	byteout
	pop	cx
	cmp	cx,1
	je	print_ether_addr_1
	mov	al,':'
	call	chrout
print_ether_addr_1:
	loop	print_ether_addr_0
	ret
