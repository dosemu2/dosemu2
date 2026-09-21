def pit_readback_status(self):
    """Read-back command must report the current state of the OUT pin.

    Counter 2 is first driven into mode 4 and latched, which leaves the
    emulated OUT pin low, and is then reprogrammed for mode 2 with a
    count of 0xffff. Its status is read back right away: the counter is
    nowhere near its terminal count, so OUT is high on real hardware
    and bit 7 of the status byte has to be set.

    This is the sequence the CMU Debugger (dosemu2 issue #1161) runs at
    startup; it rejects any status byte with OUT low.
    """

    self.mkfile("testit.bat", """\
c:\\pitrbst
rem end
""", newline="\r\n")

    self.mkcom_with_nasm("pitrbst", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    pushf
    cli

; Mode 4 is a one-shot, so latching the count after it has expired
; leaves OUT low. This is only here to give the OUT pin a known state
; that differs from the one expected below.
    mov     al, 0b8h        ; counter 2, lobyte/hibyte, mode 4, binary
    out     43h, al
    mov     al, 0ffh
    out     42h, al
    out     42h, al
    mov     al, 080h        ; latch count of counter 2
    out     43h, al
    in      al, 42h         ; lobyte
    in      al, 42h         ; hibyte

; Now the real thing: a freshly programmed rate generator has OUT high.
    mov     al, 0b4h        ; counter 2, lobyte/hibyte, mode 2, binary
    out     43h, al
    mov     al, 0ffh
    out     42h, al         ; count = 0xffff
    out     42h, al

    mov     al, 0e8h        ; read-back, status only, counter 2
    out     43h, al
    in      al, 42h

    popf

    mov     bl, al          ; keep the status byte
    mov     ah, al
    shr     al, 4
    call    tohex
    mov     [status], al
    mov     al, ah
    and     al, 0fh
    call    tohex
    mov     [status + 1], al

    mov     ah, 9
    mov     dx, msg
    int     21h

    test    bl, 80h
    jnz     .high
    mov     dx, outlow
    jmp     .print
.high:
    mov     dx, outhigh
.print:
    mov     ah, 9
    int     21h

exit:
    mov     ax, 4c00h
    int     21h

tohex:
    cmp     al, 10
    jb      .digit
    add     al, 'A' - 10
    ret
.digit:
    add     al, '0'
    ret

section .data

msg:
    db "Status byte is ("
status:
    db "??)", 13, 10, '$'
outhigh:
    db "OUT pin is high", 13, 10, '$'
outlow:
    db "OUT pin is low", 13, 10, '$'
""")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

    self.assertIn("OUT pin is high", results)
