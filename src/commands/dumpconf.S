! 
! (C) Copyright 1992, ..., 2000 the "DOSEMU-Development-Team".
!
! for details see file COPYING in the DOSEMU distribution
!

; Author: ???
; what is this good for ??

use16

.text
.bss
.data
.align 0



	.org	0

	.globl	_main		; just to make ld86 happy
_main:

;------------------------------------------------------------------------------

Header:	dd      -1              ; link to the next driver
        dw      0x8000          ; device attribute
        dw      Strat           ; strategie routine
        dw      Intr            ; interrupt routine
	.ascii	"DUMPCONF"      ;

RHPtr:	dd	0               ; Request Header Ptr
StartPos: dw	0
WaitKey: dw	0

;------------------------------------------------------------------------------

Strat:
	seg cs
        mov     word ptr RHPtr,bx
	seg cs
        mov     word ptr RHPtr+2,es
        retf

;------------------------------------------------------------------------------

Intr:
        push    ds
        push    es
        push    si
        push    di
        push    dx
        push    cx
        push    bx
        push    ax

	seg cs
        les     di,RHPtr   ; ES:DI = request header
	seg es
        mov     bl,byte ptr [di+2]  ; command
        xor     bh,bh
        cmp     bx,#0            ; valid ?
        jne     Intr3

        call    work

Intr3:
	seg cs
	les     di,RHPtr   ; ES:DI = request header (destroyed)
	seg es
        mov     word ptr [di+0xe],#0
	seg es
        mov     word ptr [di+0x10],cs

	seg es
        mov     word ptr [di + 3],#0x8100; transfer status bit

Intr4:  pop     ax
        pop     bx
        pop     cx
        pop     dx
        pop     di
        pop     si
        pop     es
        pop     ds
        retf

;------------------------------------------------------------------------------
DumpRawChar:
        push    si
        push    ax
        push    cx
        push    dx
        mov     dl,al
        mov     ah,#2
        int     0x21
        pop     dx
        pop     cx
        pop     ax
        pop     si
        ret

DumpAsciiChar:
        push    ax
        cmp     al,#0x20
        jae     NoAdjust
        mov     al,#0x21		;'!'
NoAdjust:
        call    DumpRawChar
        pop     ax
        ret

DumpHexChar:
        push    si
        push    ax
        push    cx

        mov     ah,al
        mov     cl,#4
        shr     al,cl
        cmp     al,#9
        jbe     Add0_1
        sub     al,#10
        add     al,#0x41		;'A'
        jmp     DumpNibble1
Add0_1:
	add     al,#0x30		;'0'
DumpNibble1:
        call    DumpAsciiChar

        mov     al,ah
        and     al,#15
        cmp     al,#9
        jbe     Add0_2
        sub     al,#10
        add     al,#0x41		;'A'
        jmp     DumpNibble2
Add0_2: add     al,#0x30		;'0'
DumpNibble2:
        call    DumpAsciiChar

        mov     al,#0x30		;' '
        call    DumpAsciiChar

        pop     cx
        pop     ax
        pop     si
        ret

DumpLine:
        push    si
        push    cx
        push    si
        mov     cx,#16
DumpHex:
        lodsb
        call    DumpHexChar
        loop    DumpHex
DumpChars:
        mov     al,#0x20	;' '
        call    DumpAsciiChar
        pop     si
        mov     cx,#16
DumpChar:
        lodsb
        call    DumpAsciiChar
        loop    DumpChar

        mov     al,#13
        call    DumpRawChar
        mov     al,#10
        call    DumpRawChar

        pop     cx
        pop     si
        ret

work:
	seg es
        lds     si,dword ptr [di + 18]         ;load config.sys line
	seg cs
        mov     word ptr StartPos,si
        cld
workfind:
        lodsb                           ;load next char
        cmp     al,#13                   ;EOL ?
        je      workret
        cmp     al,#0x20	;' ', search first blank after driver name
        jne     workfind
worknext:
        lodsb                           ;load next char
        cmp     al,#13                   ;EOL ?
        je      workdone
        cmp     al,#24                   ;"!" (Read AnyKey ??)
        jne     worknext
	seg cs
        mov     word ptr WaitKey,#1
        jmp     worknext

workdone:
	seg cs
        mov     si,word ptr StartPos
        cmp     si,#64
        jae     NoPosAdjust
        mov     si,#64
NoPosAdjust:
        sub     si,#64
        call    DumpLine
        add     si,#16
        call    DumpLine
        add     si,#16
        call    DumpLine
        add     si,#16
        call    DumpLine
        add     si,#16
        call    DumpLine
        add     si,#16
        call    DumpLine
        add     si,#16
        call    DumpLine
        add     si,#16
        call    DumpLine
	seg cs
        cmp     word ptr WaitKey,#0
        je      workret
readkey:
        mov     ax,#0xC07                ;flush buffer, read unfiltered keyboard
                                        ;without echo
        int     0x21
        mov     ax,#0xC00                ;flush buffer
        int     0x21
workret:
        ret
