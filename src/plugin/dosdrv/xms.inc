Int2f:
	jmp 11f
	OldInt2f:       .long 0
	.word 0x424B    // signature "KB"
	.byte 0         // flag
	jmp 30f         // EB xx for compat
	.space 7        // padding
	30: lret
11:
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

#define XMS_EXTERNAL_DRIVER 0
#define XMS_INTERNAL_DRIVER 1
#define XMS_DISABLED_CONFIG 2
#define XMS_NOHOOK_HIMEM 3

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
	jc	.LdisabledConfig
	/* internal driver installed - go out with error */
	jmp	.LinternalDriver

1:
	/* get entry point */
	movw	$0xffff, %bx
	movw	$0x4310, %ax
	int	$0x2f
	/* see if the call worked at all */
	cmpw	$0xffff, %bx
	je	.LcantHook
	/* see if it is sjmp (0xeb) */
	cmpb	$0xeb, %es:(%bx)
	jne	.LcantHook
	/* save old callback address */
	movw	%bx, OldXMSCall
	movw	%es, OldXMSCall+2
	/* check if already initialized */
	movb	$0x10, %ah
	movw	$0xffff, %dx
	lcall	*%cs:OldXMSCall
	orw	%ax, %ax
	jnz	.LdisabledConfig		/* success should not happen */
	cmpb	$0x80, %bl
	jne	.LcantHook
	/* get new entry point */
	movb    $DOS_HELPER_XMS_HELPER, %al
	movb    $XMS_HELPER_GET_ENTRY_POINT, %ah
	int	$DOS_HELPER_INT
	jc	.LdisabledConfig
	/* check if the hook was initially from dosemu */
	movw	%es, %ax
	cmpw	%ax, OldXMSCall+2
	je	.LcantHook
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
.LexternalDriver:
	movb	$XMS_EXTERNAL_DRIVER, %al
	jmp	.LallDone

.LinternalDriver:
	movb	$XMS_INTERNAL_DRIVER, %al
	jmp	.LallDone

.LcantHook:
	movb	$XMS_NOHOOK_HIMEM, %al
	jmp	.LallDone

.LdisabledConfig:
	movb    $XMS_DISABLED_CONFIG, %al
#	jmp	.LallDone

.LallDone:
	popw	%di
	popw	%es
	ret
