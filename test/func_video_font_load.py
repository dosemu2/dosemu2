def video_font_load(self):
    self.mkbat_testit("fontload")

    # The tests run with dumb video, where vgaemu used to be left
    # uninitialised, so a BIOS font call dereferenced a NULL plane.  The
    # probe does not stop at making the calls: it asks the BIOS and the
    # CRTC afterwards what the font in force is, so a call that went
    # nowhere is caught as well as one that died.
    self.mkcom_with_nasm("fontload", r"""

bits 16
cpu 386

org 100h

section .text

    mov     ax, 1101h       ; load the 8x14 ROM font into block 0
    xor     bl, bl
    int     10h

    mov     ax, 1102h       ; and the 8x8 one into block 1
    mov     bl, 1
    int     10h

    mov     ah, 9
    mov     dx, loaded
    int     21h

    mov     ax, 1111h       ; 8x14 again, this time reprogramming the CRTC
    xor     bl, bl
    int     10h
    mov     dx, after14
    call    report

    mov     ax, 1112h       ; and 8x8, which gives 43 rows
    xor     bl, bl
    int     10h
    mov     dx, after8
    call    report

    mov     ax, 4c00h
    int     21h

; What the BIOS says about the font now in force, and what the CRTC was
; told: the height out of the BIOS data area, and the maximum scan line
; out of the CRTC itself.
report:
    mov     ah, 9
    int     21h

    push    ds
    mov     ax, 40h
    mov     ds, ax
    mov     al, [85h]       ; BIOS_FONT_HEIGHT
    pop     ds
    call    puthex

    mov     ah, 9
    mov     dx, scan
    int     21h
    mov     dx, 3d4h
    mov     al, 9           ; maximum scan line
    out     dx, al
    inc     dx
    in      al, dx
    and     al, 1fh
    call    puthex

    mov     ah, 9
    mov     dx, crlf
    int     21h
    ret

puthex:                     ; al as two hex digits
    push    ax
    shr     al, 4
    call    .digit
    pop     ax
    and     al, 0fh
    call    .digit
    ret
.digit:
    add     al, '0'
    cmp     al, '9'
    jbe     .put
    add     al, 7
.put:
    mov     dl, al
    mov     ah, 2
    int     21h
    ret

loaded  db  "FONTS LOADED", 13, 10, '$'
after14 db  "8X14: HEIGHT $"
after8  db  "8X8: HEIGHT $"
scan    db  " SCAN $"
crlf    db  13, 10, '$'
""")

    results = self.runDosemu("testit.bat")

    self.assertIn("FONTS LOADED", results)

    # Both calls reprogram for the height they loaded, so the BIOS data
    # area and the CRTC have to agree with it afterwards.
    self.assertIn("8X14: HEIGHT 0E SCAN 0D", results)
    self.assertIn("8X8: HEIGHT 08 SCAN 07", results)
