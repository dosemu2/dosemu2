        name    DUMPCONF
        title   DUMPCONF

_TEXT   segment public  'CODE'

        assume  cs:_TEXT,ds:_TEXT,es:_TEXT

        org     0

;------------------------------------------------------------------------------

Header  dd      -1              ; link to the next driver
        dw      08000h          ; device attribute
        dw      Strat           ; strategie routine
        dw      Intr            ; interrupt routine
        db      'DUMPCONF'      ;

RHPtr   dd      ?               ; Request Header Ptr
StartPos dw     ?
WaitKey dw     0

;------------------------------------------------------------------------------

Strat   proc    far

        mov     word ptr cs:RHPtr,bx
        mov     word ptr cs:[RHPtr + 2],es
        ret

Strat   endp

;------------------------------------------------------------------------------

Intr    proc    far

        push    ds
        push    es
        push    si
        push    di
        push    dx
        push    cx
        push    bx
        push    ax

        les     di,cs:[RHPtr]   ; ES:DI = request header

        mov     bl,es:[di + 2]  ; command
        xor     bh,bh
        cmp     bx,0            ; g!ltig ?
        jne     Intr3

        call    work

Intr3:  les     di,cs:[RHPtr]   ; ES:DI = request header (destroyed)
        mov     word ptr es:[di + 0eh],0
        mov     word ptr es:[di + 10h],cs

        mov     word ptr es:[di + 3],8100h; transfer status bit

Intr4:  pop     ax
        pop     bx
        pop     cx
        pop     dx
        pop     di
        pop     si
        pop     es
        pop     ds
        ret

Intr    endp

;------------------------------------------------------------------------------
DumpRawChar     proc    near
        push    si
        push    ax
        push    cx
        push    dx
        mov     dl,al
        mov     ah,2
        int     21h
        pop     dx
        pop     cx
        pop     ax
        pop     si
        ret
DumpRawChar     endp

DumpAsciiChar   proc    near
        push    ax
        cmp     al,' '
        jae     NoAdjust
        mov     al,'!'
NoAdjust:
        call    DumpRawChar
        pop     ax
        ret
DumpAsciiChar   endp

DumpHexChar     proc    near
        push    si
        push    ax
        push    cx

        mov     ah,al
        mov     cl,4
        shr     al,cl
        cmp     al,9
        jbe     Add0_1
        sub     al,10
        add     al,'A'
        jmp     SHORT DumpNibble1
Add0_1: add     al,'0'
DumpNibble1:
        call    DumpAsciiChar

        mov     al,ah
        and     al,0Fh
        cmp     al,9
        jbe     Add0_2
        sub     al,10
        add     al,'A'
        jmp     SHORT DumpNibble2
Add0_2: add     al,'0'
DumpNibble2:
        call    DumpAsciiChar

        mov     al,' '
        call    DumpAsciiChar

        pop     cx
        pop     ax
        pop     si
        ret
DumpHexChar     endp

DumpLine proc   near
        push    si
        push    cx
        push    si
        mov     cx,16
DumpHex:
        lodsb
        call    DumpHexChar
        loop    DumpHex
DumpChars:
        mov     al,' '
        call    DumpAsciiChar
        pop     si
        mov     cx,16
DumpChar:
        lodsb
        call    DumpAsciiChar
        loop    DumpChar

        mov     al,13
        call    DumpRawChar
        mov     al,10
        call    DumpRawChar

        pop     cx
        pop     si
        ret
DumpLine endp

work    proc    near
        lds     si,es:[di + 18]         ;load config.sys line
        mov     cs:[StartPos],si
        cld
workfind:
        lodsb                           ;load next char
        cmp     al,13                   ;EOL ?
        je      workret
        cmp     al,' '                  ;search first blank after driver name
        jne     workfind
worknext:
        lodsb                           ;load next char
        cmp     al,13                   ;EOL ?
        je      workdone
        cmp     al,24                   ;"!" (Read AnyKey ??)
        jne     worknext
        mov     cs:[WaitKey],1
        jmp     SHORT worknext

workdone:
        mov     si,cs:[StartPos]
        cmp     si,64
        jae     NoPosAdjust
        mov     si,64
NoPosAdjust:
        sub     si,64
        call    DumpLine
        add     si,16
        call    DumpLine
        add     si,16
        call    DumpLine
        add     si,16
        call    DumpLine
        add     si,16
        call    DumpLine
        add     si,16
        call    DumpLine
        add     si,16
        call    DumpLine
        add     si,16
        call    DumpLine
        cmp     cs:[WaitKey],0
        je      workret
readkey:
        mov     ax,0C07h                ;flush buffer, read unfiltered keyboard
                                        ;without echo
        int     21h
        mov     ax,0C00h                ;flush buffer
        int     21h
workret:
        ret
work    endp


_TEXT   ends

        end
