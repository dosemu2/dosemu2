;put into the public domain by Russell Nelson, nelson@crynwr.com

	public	skip_blanks
skip_blanks:
	lodsb				;skip blanks.
	cmp	al,' '
	je	skip_blanks
	cmp	al,HT
	je	skip_blanks
	dec	si
	ret

