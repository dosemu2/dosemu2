def video_save_pointer_table(self):
    """The BIOS keeps a Video Save Pointer Table at 40h:a8h, which leads
    to the video parameter table and to the display combination codes.
    Mode setting code and DOS task switchers reprogram the display from
    there, and with the pointer left at zero they follow the interrupt
    vectors instead."""

    self.mkfile("testit.bat", """\
c:\\vsaveptr
rem end
""", newline="\r\n")

    self.mkcom_with_nasm("vsaveptr", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

; the far pointer the BIOS data area holds
    mov     ax, 0x40
    mov     es, ax
    mov     ax, [es:0xa8]
    mov     [savptr], ax
    mov     ax, [es:0xaa]
    mov     [savptr + 2], ax

; the primary table: video parameters at +0, secondary table at +0x10
    les     bx, [savptr]
    mov     ax, [es:bx]
    mov     [vpt], ax
    mov     ax, [es:bx + 2]
    mov     [vpt + 2], ax
    mov     ax, [es:bx + 0x10]
    mov     [secnd], ax
    mov     ax, [es:bx + 0x12]
    mov     [secnd + 2], ax

; the secondary table: display combination codes at +2
    les     bx, [secnd]
    mov     ax, [es:bx + 2]
    mov     [dcc], ax
    mov     ax, [es:bx + 4]
    mov     [dcc + 2], ax

; the entry the 80x25 colour text mode is set from
    les     bx, [vpt]
    add     bx, 0x18 * 64
    mov     di, m3
    mov     al, [es:bx]         ; characters per line
    mov     [di], al
    mov     al, [es:bx + 1]     ; rows on screen - 1
    mov     [di + 1], al
    mov     al, [es:bx + 2]     ; character height
    mov     [di + 2], al
    mov     al, [es:bx + 9]     ; miscellaneous output register
    mov     [di + 3], al
    mov     al, [es:bx + 10]    ; first CRTC register
    mov     [di + 4], al

; the display combination code table header
    les     bx, [dcc]
    mov     di, dhdr
    mov     al, [es:bx]         ; number of entries
    mov     [di], al
    mov     al, [es:bx + 1]     ; version
    mov     [di + 1], al
    mov     al, [es:bx + 2]     ; maximum display code
    mov     [di + 2], al

    mov     dx, s_savptr
    mov     si, savptr
    call    putfar
    mov     dx, s_vpt
    mov     si, vpt
    call    putfar
    mov     dx, s_sec
    mov     si, secnd
    call    putfar
    mov     dx, s_dcc
    mov     si, dcc
    call    putfar

    mov     dx, s_dhdr
    call    putstr
    mov     si, dhdr
    mov     cx, 3
    call    putbytes
    mov     dx, s_m3
    call    putstr
    mov     si, m3
    mov     cx, 5
    call    putbytes

    mov     ax, 0x4c00
    int     0x21

; print the string in dx, then the far pointer at ds:si
putfar:
    call    putstr
    mov     ax, [si + 2]
    call    puthex16
    mov     dl, ':'
    call    putchar
    mov     ax, [si]
    call    puthex16
    jmp     putnl

; print cx bytes at ds:si, space separated
putbytes:
    lodsb
    push    cx
    call    puthex8
    mov     dl, ' '
    call    putchar
    pop     cx
    loop    putbytes
    jmp     putnl

putnl:
    mov     dl, 13
    call    putchar
    mov     dl, 10
    jmp     putchar

puthex16:
    push    ax
    mov     al, ah
    call    puthex8
    pop     ax
; fall through
puthex8:
    push    ax
    shr     al, 4
    call    puthex4
    pop     ax
    and     al, 0x0f
; fall through
puthex4:
    add     al, '0'
    cmp     al, '9'
    jbe     .out
    add     al, 'a' - '0' - 10
.out:
    mov     dl, al
; fall through
putchar:
    push    ax
    mov     ah, 2
    int     0x21
    pop     ax
    ret

putstr:
    push    ax
    mov     ah, 9
    int     0x21
    pop     ax
    ret

section .data

s_savptr:   db  "savptr=$"
s_vpt:      db  "vpt=$"
s_sec:      db  "sec=$"
s_dcc:      db  "dcc=$"
s_dhdr:     db  "dcchdr=$"
s_m3:       db  "mode3=$"

section .bss

savptr:     resd    1
vpt:        resd    1
secnd:      resd    1
dcc:        resd    1
m3:         resb    5
dhdr:       resb    3

""")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

    # the pointer has to lead somewhere
    self.assertNotIn("savptr=0000:0000", results)
    self.assertNotIn("vpt=0000:0000", results)
    self.assertNotIn("sec=0000:0000", results)
    self.assertNotIn("dcc=0000:0000", results)

    # and what it leads to has to be the 80x25 colour text mode: 80
    # characters per line, 25 rows of 16 scanlines, the colour value of
    # the miscellaneous output register, and the first CRTC register
    self.assertIn("mode3=50 18 10 67 5f", results)

    # version 1 of the display combination code table
    self.assertIn("dcchdr=0d 01 0c", results)
