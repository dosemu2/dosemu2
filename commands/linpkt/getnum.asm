;put into the public domain by Russell Nelson, nelson@crynwr.com

	public	get_number
get_number:
	mov	bp,10			;we default to 10.
	jmp	short get_number_0

	public	get_hex
get_hex:
	mov	bp,16
;get a hex number, skipping leading blanks.
;enter with si->string of digits,
;	di -> dword to store the number in.  [di] is not modified if no
;		digits are given, so it acts as the default.
;return cy if there are no digits at all.
;return nc, bx:cx = number, and store bx:cx at [di].
get_number_0:
	call	skip_blanks
	call	get_digit		;is there really a number here?
	jc	get_number_3
	xor	ah,ah
	cmp	ax,bp			;larger than our base?
	jae	get_number_3		;yes.
	or	al,al			;Does the number begin with zero?
	jne	get_number_4		;no.
	mov	bp,8			;yes - they want octal.
get_number_4:

	xor	cx,cx			;get a hex number.
	xor	bx,bx
get_number_1:
	lodsb
	cmp	al,'x'			;did they really want hex?
	je	get_number_5		;yes.
	cmp	al,'X'			;did they really want hex?
	je	get_number_5		;yes.
	call	get_digit		;convert a character into an int.
	jc	get_number_2		;not a digit (neither hex nor dec).
	xor	ah,ah
	cmp	ax,bp			;larger than our base?
	jae	get_number_2		;yes.

	push	ax			;save the new digit.

	mov	ax,bp			;multiply the low word by ten.
	mul	cx
	mov	cx,ax			;keep the low word.
	push	dx			;save the high word for later.
	mov	ax,bp
	mul	bx
	mov	bx,ax			;we keep only the low word (which is our high word)
	pop	dx
	add	bx,dx			;add the high result from earlier.

	pop	ax			;get the new digit back.
	add	cx,ax			;add the new digit in.
	adc	bx,0
	jmp	get_number_1
get_number_5:
	mov	bp,16			;change the base to hex.
	jmp	get_number_1
get_number_2:
	dec	si
	mov	[di],cx			;store the parsed number.
	mov	[di+2],bx
	clc
	jmp	short get_number_6
get_number_3:
	cmp	al,'?'			;did they ask for the default?
	stc
	jne	get_number_6		;no, return cy.
	add	si,2			;skip past the question mark.
	mov	cx,-1
	mov	bx,-1
	jmp	get_number_2		;and return the -1.
get_number_6:
	ret
