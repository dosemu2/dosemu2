from re import search

from common_framework import DOSEMU_CONF_DEFAULT

BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_movss_shadow(self):
    """A load of SS blocks interrupts until the instruction after it has
    run, so that a client can set SS and ESP as a pair without ever being
    seen half way through. dosemu2 does not deliver through the processor
    - do_pm_int() builds the frame itself, at the client's own SS:ESP when
    it is already on its locked stack - so nothing but the backend keeps
    that promise. A backend that breaks it hands the host a client whose
    ESP belongs to one stack and whose SS belongs to another, which is how
    the frame ends up written outside the segment it was meant for.

    The client below spins on

        mov ss, small     ; the window opens
        mov sp, 03f0h     ; and closes
        <nops>            ; dwell here, so that hits land to be counted
        lss sp, [big]     ; back, atomically

    and hooks the protected mode interrupt the timer arrives on. On every
    tick it reads the SS:SP the host saved in the frame it pushed: SS of
    the small segment with SP already parked is a legal delivery and is
    counted as such, SS of the small segment with SP still belonging to
    the big one is a delivery inside the window.

    The dwell is what makes the test mean something: it is the control. A
    backend that never delivers on the small stack at all - the CPU
    emulator delivers only on a translated block boundary, which here is
    never inside the loop - counts nothing either way, and the test says
    so instead of passing on a measurement it did not make."""

    self.mkfile("testit.bat", BATCHFILE % 'movsswin', newline="\r\n")

    self.mkcom_with_nasm("movsswin", r"""
bits 16
cpu 386

org 100h

; A stack segment small enough that an ESP left over from another stack is
; outside it, and the SP we park there, which is inside it.
BADLIM  equ 03f0h
BADSP   equ 03f0h
STKOFF  equ 2000h
DWELL   equ 64                  ; nops spent on the small stack per turn
WANT    equ 100                 ; spin until this many ticks have landed
GUARD   equ 400000000           ; ... but not forever
TICKVEC equ 73h                 ; the line dpmi_pic_run() delivers the timer on

section .text

start:
    mov     bx, 1000h           ; leave the host some dos memory
    mov     ah, 4ah
    int     21h

    mov     ax, 1687h
    int     2fh
    test    ax, ax
    jnz     nodpmi
    mov     [pmentry], di
    mov     [pmentry + 2], es
    mov     bx, si
    test    bx, bx
    jz      .nopriv
    mov     ah, 48h
    int     21h
    jc      nomem
    mov     es, ax
    jmp     .enter
.nopriv:
    push    ds
    pop     es
.enter:
    xor     ax, ax              ; 16 bit client
    call    far [pmentry]
    jc      nodpmi

    ; ---------------- protected mode from here ----------------

    push    ds                  ; hex16 stores through es, and the mode
    pop     es                  ; switch leaves it on the dos memory block

    mov     ax, 0006h           ; our ds base
    mov     bx, ds
    int     31h
    jc      failapi
    mov     [dsbase], dx
    mov     [dsbase + 2], cx

    xor     ax, ax              ; two descriptors
    mov     cx, 2
    int     31h
    jc      failapi
    mov     [goodsel], ax
    add     ax, 8
    mov     [badsel], ax

    mov     bx, [goodsel]       ; goodsel: an alias of our whole segment
    mov     dx, [dsbase]
    mov     cx, [dsbase + 2]
    mov     ax, 0007h
    int     31h
    jc      failapi
    mov     bx, [goodsel]
    xor     cx, cx
    mov     dx, 0ffffh
    mov     ax, 0008h
    int     31h
    jc      failapi
    mov     bx, [goodsel]
    mov     cx, 00f2h
    mov     ax, 0009h
    int     31h
    jc      failapi

    mov     bx, [badsel]        ; badsel: BADLIM bytes, STKOFF into it
    mov     dx, [dsbase]
    mov     cx, [dsbase + 2]
    add     dx, STKOFF
    adc     cx, 0
    mov     ax, 0007h
    int     31h
    jc      failapi
    mov     bx, [badsel]
    xor     cx, cx
    mov     dx, BADLIM
    mov     ax, 0008h
    int     31h
    jc      failapi
    mov     bx, [badsel]
    mov     cx, 00f2h
    mov     ax, 0009h
    int     31h
    jc      failapi

    mov     ax, 0204h           ; remember the old handler
    mov     bl, TICKVEC
    int     31h
    mov     [oisel], cx
    mov     [oioff], dx

    mov     ax, 0205h           ; and hook it
    mov     bl, TICKVEC
    mov     cx, cs
    mov     dx, tickhandler
    int     31h
    jc      failapi

    cli                         ; move onto goodsel, atomically
    mov     [savesp], sp
    mov     [savess], ss
    mov     ax, [goodsel]
    mov     [goodstk + 2], ax
    mov     ax, sp
    mov     [goodstk], ax
    lss     sp, [goodstk]
    sti

    mov     ecx, GUARD
    mov     bx, [badsel]
.spin:
    mov     ss, bx              ; the window opens
    mov     sp, BADSP           ; and closes
    times DWELL nop             ; dwell on the small stack, as a control
    lss     sp, [goodstk]       ; back, atomically
    cmp     word [ticks], WANT
    jae     .spun
    dec     ecx
    jnz     .spin
.spun:
    cli
    lss     sp, [savestk]

    mov     ax, 0205h           ; unhook before talking to dos
    mov     bl, TICKVEC
    mov     cx, [oisel]
    mov     dx, [oioff]
    int     31h
    sti

    mov     ax, [inwin]
    mov     di, vwindow
    call    hex16
    mov     ax, [onbad]
    mov     di, vsmall
    call    hex16
    mov     ax, [ticks]
    mov     di, vticks
    call    hex16

    mov     ah, 9                   ; the counts always, the verdict after
    mov     dx, msgcount
    int     21h
    cmp     word [inwin], 0
    je      .ok
    mov     dx, msgcaught
    jmp     print
.ok:
    mov     dx, msgok
    jmp     print

; The 16 bit frame do_pm_int() pushes, from the handler's entry sp:
;   +0 off  +2 sel  +4 flags  +6 ip  +8 cs  +10 flags  +12 esp.lo  +14 ss
tickhandler:
    push    bp
    mov     bp, sp
    push    ax
    inc     word [ticks]
    mov     ax, [bp + 16]           ; the ss the client was interrupted on
    cmp     ax, [cs:badsel]
    jne     .done                   ; not on the small stack at all
    mov     ax, [bp + 14]
    cmp     ax, BADSP
    je      .small
    inc     word [inwin]            ; caught between mov ss and mov sp
    jmp     .done
.small:
    inc     word [onbad]
.done:
    pop     ax
    pop     bp
    jmp     far [cs:oi]             ; chain, the timer stub needs to see this

hex16:
    mov     cx, 4
.d:
    rol     ax, 4
    push    ax
    and     al, 0fh
    cmp     al, 9
    jbe     .p
    add     al, 7
.p:
    add     al, '0'
    stosb
    pop     ax
    loop    .d
    ret

nodpmi:
    mov     dx, msgnodpmi
    jmp     print
nomem:
    mov     dx, msgnomem
    jmp     print
failapi:
    mov     dx, msgapi
print:
    mov     ah, 9
    int     21h
    mov     ax, 4c00h
    int     21h

pmentry     dd 0
dsbase      dd 0
goodsel     dw 0
badsel      dw 0
ticks       dw 0
inwin       dw 0
onbad       dw 0
oi:
oioff       dw 0
oisel       dw 0
savestk:
savesp      dw 0
savess      dw 0
goodstk     dw 0, 0

msgcount    db "window=0x"
vwindow     db "????"
            db " small=0x"
vsmall      db "????"
            db " ticks=0x"
vticks      db "????", 13, 10, "$"
msgok       db "Test OK", 13, 10, "$"
msgcaught   db "FAIL: interrupt delivered between mov ss and mov sp", 13, 10, "$"
msgnodpmi   db "FAIL: no dpmi host", 13, 10, "$"
msgnomem    db "FAIL: no dos memory", 13, 10, "$"
msgapi      db "FAIL: dpmi call failed", 13, 10, "$"
""")

    results = self.runDosemu("testit.bat", config=DOSEMU_CONF_DEFAULT, timeout=90)

    self.assertNotIn("FAIL:", results)

    m = search(r"window=0x([0-9A-F]{4}) small=0x([0-9A-F]{4}) ticks=0x([0-9A-F]{4})",
               results)
    self.assertIsNotNone(m, results)
    window, small, ticks = (int(g, 16) for g in m.groups())

    self.assertNotEqual(0, ticks, "no timer interrupt reached the client")
    if small == 0:
        self.skipTest(
            "this backend delivered none of the %d interrupts on the stack "
            "under test, so it has nothing to say about the window" % ticks)
    self.assertEqual(
        0, window,
        "%d of %d interrupts were delivered between \"mov ss\" and "
        "\"mov sp\" (%d landed after it, as they should)"
        % (window, ticks, small))
