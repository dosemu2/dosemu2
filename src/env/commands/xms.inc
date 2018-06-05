Int2f:
	cmpw	$0x4310, %ax
	je	1f
	ljmp	*%cs:OldInt2f
1:	pushw	%cs
	popw	%es
	movw	$XMSHook, %bx
	iret

XMSHook:
	jmp 1f
	nop
	nop
	nop
1:	lcall	*%cs:OldXMSCall
	lret

InitCodeStart:		/* all the below code gets discarded after init */

HimemHook_Start:
	.byte 0x9a	/* lcall */
	NewXMSCall:	.long 0
HimemHook_End:

HookHimem:
	pushw	%es
	pushw	%di
	pushw	%cs
	popw	%es
	/* check for the XMS driver */
	movw	$0x4300, %ax
	int	$0x2f
	cmpb	$0x80, %al
	je	1f
	/* no XMS driver - try to install internal driver */
	movb    $DOS_HELPER_XMS_HELPER, %al
	movb    $XMS_HELPER_XMS_INIT, %ah
	int	$DOS_HELPER_INT
	jc	28f
	movb	$9, %ah
	movw	$XMSMsg, %dx
	int	$0x21
	/* internal driver installed - go out with error */
	jmp	9f
1:
	/* get entry point */
	movw	$0xffff, %bx
	movw	$0x4310, %ax
	int	$0x2f
	/* see if the call worked at all */
	cmpw	$0xffff, %bx
	je 27f
	/* see if it is sjmp (0xeb) */
	cmpb	$0xeb, %es:(%bx)
	jne	27f
	/* save old callback address */
	movw	%bx, OldXMSCall
	movw	%es, OldXMSCall+2
	/* check if already initialized */
	movb	$0x10, %ah
	movw	$0xffff, %dx
	lcall	*%cs:OldXMSCall
	orw	%ax, %ax
	jnz	28f		/* success should not happen */
	cmpb	$0x80, %bl
	jne	27f
	/* get new entry point */
	movb    $DOS_HELPER_XMS_HELPER, %al
	movb    $XMS_HELPER_GET_ENTRY_POINT, %ah
	int	$DOS_HELPER_INT
	jc	28f
	/* check if the hook was initially from dosemu */
	movw	%es, %ax
	cmpw	%ax, OldXMSCall+2
	je	27f
	/* patch the hook with new addr */
	movw	%bx, NewXMSCall
	movw	%es, NewXMSCall+2
	/* now install the hook */
	movw	OldXMSCall, %di
	movw	OldXMSCall+2, %es
	movw	$HimemHook_Start, %si
	movw	$(HimemHook_End - HimemHook_Start), %cx
	rep	movsb
	/* install int2f hook */
	movb	$0x35, %ah
	movb	$0x2f, %al
	int	$0x21
	movw	%bx, OldInt2f
	movw	%es, OldInt2f+2
	movb	$0x25, %ah
	movb	$0x2f, %al
	movw	$Int2f, %dx
	int	$0x21
	/* all done, UMB should work */
	clc
	jmp	10f

27:
	movb	$9, %ah
	movw	$CantHookMsg, %dx
	int	$0x21
	jmp	9f
28:
	movb	$9, %ah
	movw	$NoXMSMsg, %dx
	int	$0x21
#	jmp	9f
9:
	stc			# report error
10:
	popw	%di
	popw	%es
	ret
