;put into the public domain by Russell Nelson, nelson@crynwr.com

	public	print_number
print_number:
;enter with dx -> dollar terminated name of number, di ->dword.
;exit with the number printed and the cursor advanced to the next line.
	mov	ah,9			;print the name of the number.
	int	21h
	mov	al,'0'
	call	chrout
	mov	al,'x'
	call	chrout
	mov	ax,[di]			;print the number in hex.
	mov	dx,[di+2]
	call	dwordout
	mov	al,' '
	call	chrout
	mov	al,'('
	call	chrout
	mov	ax,[di]			;print the number in decimal.
	mov	dx,[di+2]
	call	decout
	mov	al,')'
	call	chrout
	mov	al,CR
	call	chrout
	mov	al,LF
	call	chrout
	ret

