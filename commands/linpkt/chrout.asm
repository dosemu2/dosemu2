;put into the public domain by Russell Nelson, nelson@crynwr.com

	public	chrout
chrout:
	push	ax			;print the char in al.
	push	dx
	mov	dl,al
	mov	ah,2
	int	21h
	pop	dx
	pop	ax
	ret
