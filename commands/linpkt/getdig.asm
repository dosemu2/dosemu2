;put into the public domain by Russell Nelson, nelson@crynwr.com

	public	get_digit
get_digit:
;enter with al = character
;return nc, al=digit, or cy if not a digit.
	cmp	al,'0'			;decimal digit?
	jb	get_digit_1		;no.
	cmp	al,'9'			;. .?
	ja	get_digit_2		;no.
	sub	al,'0'
	clc
	ret
get_digit_2:
	cmp	al,'a'			;hex digit?
	jb	get_digit_3
	cmp	al,'f'			;hex digit?
	ja	get_digit_3
	sub	al,'a'-10
	clc
	ret
get_digit_3:
	cmp	al,'A'			;hex digit?
	jb	get_digit_1
	cmp	al,'F'			;hex digit?
	ja	get_digit_1
	sub	al,'A'-10
	clc
	ret
get_digit_1:
	stc
	ret


