;put into the public domain by Russell Nelson, nelson@crynwr.com

;we read the timer chip's counter zero.  It runs freely, counting down
;from 65535 to zero.  Whenever the count is above the old count, the
;timer must have wrapped around.  When that happens, we count down a tick.

timeout		dw	?		;number of ticks to wait.
timeout_counter	dw	?		;old counter zero value.

set_timeout:
;enter with ax = number of ticks (36.4 ticks per second).
	inc	ax			;the first times out immediately.
	mov	cs:timeout,ax
	mov	cs:timeout_counter,0
	ret


do_timeout:
;call periodically when checking for timeout.  Returns nz if we haven't
;timed out yet.
	mov	al,0			;latch counter zero.
	out	43h,al
	in	al,40h			;read counter zero.
	mov	ah,al
	in	al,40h
	xchg	ah,al
	cmp	ax,cs:timeout_counter	;is the count higher?
	mov	cs:timeout_counter,ax
	jbe	do_timeout_1		;no.
	dec	cs:timeout		;Did we hit the timeout value yet?
	ret
do_timeout_1:
	or	sp,sp			;ensure nz.
	ret
