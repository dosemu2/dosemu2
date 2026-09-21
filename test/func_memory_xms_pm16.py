def memory_xms_pm16(self):
    self.mkfile("testit.bat", """\
c:\\xmspm16
rem end
""", newline="\r\n")

    # A 16-bit DPMI client calling XMS function 0Bh through the protected
    # mode entry point of int 2Fh AX=4310h. With handle 0 the upper half of
    # the address is a selector: a 16-bit client has no 32-bit offsets, so
    # the near pointer into DS that a 32-bit client passes is not available
    # to it. An upper half that is not a selector is the real mode segment
    # of the XMS spec, which the real mode driver resolves.
    self.mkcom_with_nasm("xmspm16", r"""

bits 16
cpu 386

org 100h

XMLEN	equ 16

section .text

start:
    pop     ax                  ; the word a COM file gets on its stack
    mov     bx, sp
    add     bx, 15
    shr     bx, 4
    jnz     .small
    mov     bx, 1000h           ; it was a full 64 KiB stack
.small:
    mov     ah, 4Ah
    int     21h                 ; hand back the memory we do not use
    xor     ax, ax
    xchg    ax, word [2Ch]
    mov     es, ax
    mov     ah, 49h
    int     21h                 ; free the environment, if any

    mov     bx, 8               ; a buffer addressable by segment
    mov     ah, 48h
    int     21h
    jc      nomemory
    test    al, 4               ; keep the LDT bit of the segment clear,
    jz      .haveseg            ;  so that it cannot pass for a selector
    add     ax, 4
.haveseg:
    mov     word [dosseg], ax

    mov     ax, 1687h
    int     2Fh
    test    ax, ax
    jnz     nohost
    push    es
    push    di                  ; the mode switch entry point
    test    si, si
    jz      .nomemneeded
    mov     bx, si
    mov     ah, 48h
    int     21h
    jc      nomemory
    mov     es, ax
.nomemneeded:
    mov     bp, sp
    xor     ax, ax              ; start a 16-bit client
    call    far [bp]
    jc      initfailed

                                ; 16-bit protected mode from here on
    mov     ax, 4300h
    int     2Fh
    cmp     al, 80h
    jne     noxms
    xor     ax, ax
    mov     es, ax
    mov     ax, 4310h
    int     2Fh
    mov     ax, es
    test    ax, ax
    jz      noxms
    mov     word [xmsent], bx
    mov     word [xmsent + 2], ax
    push    ds
    pop     es

    xor     ax, ax
    mov     cx, 2               ; a selector on low memory and one on
    int     31h                 ;  the buffer, to check the moves with
    jc      noselector
    mov     bx, ax
    mov     word [lowsel], bx
    add     ax, 8
    mov     word [bufsel], ax

    xor     cx, cx
    xor     dx, dx
    mov     ax, 7
    int     31h                 ; lowsel base 0
    jc      noselector
    xor     cx, cx
    mov     dx, 0FFFFh
    mov     ax, 8
    int     31h                 ; lowsel limit 64 KiB
    jc      noselector

    mov     bx, word [bufsel]
    movzx   eax, word [dosseg]
    shl     eax, 4
    mov     dx, ax
    shr     eax, 16
    mov     cx, ax
    mov     ax, 7
    int     31h                 ; bufsel base at the buffer
    jc      noselector
    xor     cx, cx
    mov     dx, 0FFFFh
    mov     ax, 8
    int     31h                 ; bufsel limit 64 KiB
    jc      noselector

samesegment:                    ; sel:ofs on both sides, our own DS
    mov     dword [xm + 0], XMLEN
    mov     word [xm + 4], 0
    mov     word [xm + 6], src
    mov     ax, ds
    mov     word [xm + 8], ax
    mov     word [xm + 10], 0
    mov     word [xm + 12], dst
    mov     word [xm + 14], ax
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     si, src
    mov     di, dst
    mov     cx, XMLEN
    repe cmpsb
    jne     .bad
    mov     dx, msg.sameok
    jmp     short .prn
.bad:
    mov     dx, msg.samebad
.prn:
    call    print

othersegment:                   ; the source is behind another selector
    mov     word [xm + 6], 0
    mov     ax, word [lowsel]
    mov     word [xm + 8], ax   ; the interrupt table, through lowsel
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     es, word [lowsel]
    xor     di, di
    mov     si, dst
    mov     cx, XMLEN
    repe cmpsb
    push    ds
    pop     es
    jne     .bad
    mov     dx, msg.otherok
    jmp     short .prn
.bad:
    mov     dx, msg.otherbad
.prn:
    call    print

realsegment:                    ; segments on both sides, which the real
                                ;  mode driver is the one to resolve
    mov     word [xm + 6], 0
    mov     word [xm + 8], 0    ; 0000:0000, the interrupt table
    mov     word [xm + 12], 0
    mov     ax, word [dosseg]
    mov     word [xm + 14], ax  ; the buffer we allocated by segment
    call    doxms
    cmp     ax, 1
    jne     .bad
    mov     bp, ds              ; ds is about to go, keep it
    mov     ax, word [bufsel]
    mov     bx, word [lowsel]
    mov     ds, ax
    mov     es, bx
    xor     si, si
    xor     di, di
    mov     cx, XMLEN
    repe cmpsb
    mov     ds, bp
    mov     es, bp
    jne     .bad
    mov     dx, msg.realok
    jmp     short .prn
.bad:
    mov     dx, msg.realbad
.prn:
    call    print

    mov     dx, msg.done
    call    print
    mov     ax, 4C00h
    int     21h

nohost:
    mov     dx, msg.nohost
    jmp     short quit
nomemory:
    mov     dx, msg.nomemory
    jmp     short quit
initfailed:
    mov     dx, msg.initfailed
    jmp     short quit
noxms:
    mov     dx, msg.noxms
    jmp     short quit
noselector:
    mov     dx, msg.noselector
quit:
    call    print
    mov     ax, 4CFFh
    int     21h

doxms:
    mov     si, xm
    mov     ah, 0Bh
    call    far word [xmsent]
    retn

print:
    mov     ah, 9
    int     21h
    retn

    align 4
xmsent:     dd 0
dosseg:     dw 0
lowsel:     dw 0
bufsel:     dw 0

    align 4
xm:                             ; the XMS move structure
    dd      0                   ; length
    dw      0                   ; source handle
    dd      0                   ; source address
    dw      0                   ; destination handle
    dd      0                   ; destination address

src:        db "0123456789ABCDEF"
dst:        times XMLEN db 0

msg:
.sameok:     db "SAME SEGMENT OK",13,10,"$"
.samebad:    db "FAIL same segment",13,10,"$"
.otherok:    db "OTHER SEGMENT OK",13,10,"$"
.otherbad:   db "FAIL other segment",13,10,"$"
.realok:     db "REAL SEGMENT OK",13,10,"$"
.realbad:    db "FAIL real segment",13,10,"$"
.done:       db "TEST DONE",13,10,"$"
.nohost:     db "FAIL no DPMI host",13,10,"$"
.nomemory:   db "FAIL no DOS memory",13,10,"$"
.initfailed: db "FAIL DPMI init",13,10,"$"
.noxms:      db "FAIL no XMS entry point",13,10,"$"
.noselector: db "FAIL no selector",13,10,"$"
""")

    results = self.runDosemu("testit.bat")

    self.assertIn("SAME SEGMENT OK", results)
    self.assertIn("OTHER SEGMENT OK", results)
    self.assertIn("REAL SEGMENT OK", results)
    self.assertIn("TEST DONE", results)
    self.assertNotIn("FAIL", results)
