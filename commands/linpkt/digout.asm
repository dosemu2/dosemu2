;put into the public domain by Russell Nelson, nelson@crynwr.com

	public	dwordout, wordout, byteout, digout
dwordout:
	mov	cl,'0'			;prepare to eliminate leading zeroes.
	xchg	ax,dx			;just output 32 bits in hex.
	call	wordout			;output dx.
	xchg	ax,dx
wordout:
	push	ax
	mov	al,ah
	call	byteout
	pop	ax
byteout:
	mov	ah,al
	shr	al,1
	shr	al,1
	shr	al,1
	shr	al,1
	call	digout
	mov	al,ah
digout:
	and	al,0fh
	add	al,90h	;binary digit to ascii hex digit.
	daa
	adc	al,40h
	daa
	cmp	al,cl			;leading zero?
	je	digout_1
	mov	cl,-1			;no more leading zeros.
	jmp	chrout
digout_1:
	ret
