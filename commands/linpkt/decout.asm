;put into the public domain by Russell Nelson, nelson@crynwr.com

	public	decout
decout:
	mov	si,ax			;get the number where we want it.
	mov	di,dx
	or	ax,dx			;is the number zero?
	jne	decout_nonzero
	mov	al,'0'			;yes - easier to just print it, than
	jmp	chrout			;  to eliminate all but the last zero.
decout_nonzero:

	xor	ax,ax			;start with all zeroes in al,bx,bp
	mov	bx,ax
	mov	bp,ax

	mov	cx,32			;32 bits in two 16 bit registers.
decout_1:
	shl	si,1
	rcl	di,1
	xchg	bp,ax
	call	addbit
	xchg	bp,ax
	xchg	bx,ax
	call	addbit
	xchg	bx,ax
	adc	al,al
	daa
	loop	decout_1

	mov	cl,'0'			;prepare to eliminate leading zeroes.
	call	byteout			;output the first two.
	mov	ax,bx			;output the next four
	call	wordout			;output the next four
	mov	ax,bp
	jmp	wordout

addbit:	adc	al,al
	daa
	xchg	al,ah
	adc	al,al
	daa
	xchg	al,ah
	ret

