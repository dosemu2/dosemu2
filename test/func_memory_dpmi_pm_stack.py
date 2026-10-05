from common_framework import DOSEMU_CONF_DEFAULT

# The frame under test is one the HOST builds at the client's SS:ESP, so
# the backend is pinned rather than taken from the machine. Under KVM the
# processor delivers the nested exception itself: the bad SS turns the
# divide error into a #SS, which is not a vector this client hooked, so
# the client is terminated instead - correctly, there is nothing there
# for the host to get wrong.
EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'

BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_pm_stack(self):
    """When a client is interrupted between "mov ss" and "mov esp", SS is
    already the new one while ESP still belongs to the old stack. If that
    happens inside another protected mode interrupt handler, enter_lpms()
    keeps using the client stack, and the frame it builds there used to be
    written at base+ESP with no regard for the segment limit, quietly
    corrupting whatever memory happens to live at that address.

    The client below puts itself in exactly that state on purpose. It
    divides by zero once to get its own exception handler called - that
    first delivery is what sets in_dpmi_pm_stack - and from inside the
    handler it loads SS with a small descriptor, leaves an ESP far past
    its limit, and divides by zero again. The host has to deliver the
    second exception somewhere, and a signature buffer sits exactly where
    the frame would land if it used ESP unchecked.

    Both steps are synchronous on purpose: an earlier revision entered the
    handler from a timer tick, which made the test depend on a tick
    arriving during a spin loop."""

    self.mkfile("testit.bat", BATCHFILE % 'pmstklim', newline="\r\n")

    self.mkcom_with_nasm("pmstklim", r"""
bits 16
cpu 386

org 100h

; The bad stack. ESP is past the limit of its own segment, which is what
; the host has to notice; the 20 byte exception frame still lands in
; addressable memory, so an unfixed host writes it out and the client
; survives to tell us about it.
BADESP  equ 0400h
BADLIM  equ 03f0h
; the base is picked so that base+BADESP is sig+24
SIGOFF  equ 0600h
SIGW    equ 16
PAT     equ 05a5ah

section .text

start:
    ; shrink the block, the dpmi host wants some dos memory of its own
    mov     bx, 1000h
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
    xor     ax, ax                  ; 16 bit client
    call    far [pmentry]
    jc      nodpmi

    ; ---------------- protected mode from here ----------------

    mov     ax, 0006h               ; get segment base address
    mov     bx, ds
    int     31h
    jc      failapi
    mov     [dsbase], dx
    mov     [dsbase + 2], cx

    xor     ax, ax                  ; allocate ldt descriptors
    mov     cx, 1
    int     31h
    jc      failapi
    mov     [badsel], ax

    mov     bx, [badsel]            ; set segment base address
    mov     dx, [dsbase]
    mov     cx, [dsbase + 2]
    add     dx, SIGOFF + 24 - BADESP
    adc     cx, 0
    mov     ax, 0007h
    int     31h
    jc      failapi

    mov     bx, [badsel]            ; set segment limit
    xor     cx, cx
    mov     dx, BADLIM
    mov     ax, 0008h
    int     31h
    jc      failapi

    mov     bx, [badsel]            ; 16 bit writable data, dpl 3
    mov     cx, 00f2h
    mov     ax, 0009h
    int     31h
    jc      failapi

    push    ds                      ; lay down the signature
    pop     es
    cld
    mov     di, SIGOFF
    mov     cx, SIGW
    mov     ax, PAT
    rep     stosw

    mov     si, excvecs             ; set exception handlers
.exc:
    lodsb
    cmp     al, 0ffh
    je      .exdone
    mov     bl, al
    mov     ax, 0203h
    mov     cx, cs
    mov     dx, exchandler
    int     31h
    jmp     .exc
.exdone:

    mov     word [state], 0
    mov     [savess], ss
    mov     [savesp], sp

    xor     ax, ax                  ; the first exception, on a good stack
    cwd
    div     ax
    jmp     recovery                ; only if it was never delivered

; Entered by the host through enter_lpms(). The first time it runs the
; host has just switched to its locked stack, which is what leaves
; in_dpmi_pm_stack set; the second time is the one under test.
exchandler:
    cmp     word [state], 0
    jne     .nested
    mov     word [state], 1
    mov     ax, [badsel]            ; ss switched, esp not yet
    mov     ss, ax
    mov     sp, BADESP
    xor     ax, ax                  ; and now ask for a nested frame
    cwd
    div     ax
    cli                             ; only if it was never delivered
    mov     ax, [savess]
    mov     ss, ax
    mov     sp, [savesp]
    jmp     recovery

; Reached on the bad stack if the host wrote the frame at base+ESP, on
; the locked stack if it noticed. Either way there is no returning from
; here, the frame underneath is gone.
.nested:
    mov     word [state], 2
    cli
    mov     ax, [savess]
    mov     ss, ax
    mov     sp, [savesp]

recovery:
    cli
    cmp     word [state], 2
    jne     notrap

    mov     si, SIGOFF
    mov     cx, SIGW
.check:
    lodsw
    cmp     ax, PAT
    jne     clobber
    loop    .check

    mov     dx, msgok
    jmp     print

clobber:
    mov     dx, msgclob
    jmp     print
notrap:
    mov     dx, msgnotrap
    cmp     word [state], 0
    je      print
    mov     dx, msgnonest
    jmp     print
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

excvecs     db 00h, 06h, 0dh, 0eh, 0ffh
pmentry     dd 0
dsbase      dd 0
badsel      dw 0
state       dw 0
savess      dw 0
savesp      dw 0

msgok       db "Test OK", 13, 10, "$"
msgclob     db "FAIL: exception frame written past the stack limit", 13, 10, "$"
msgnotrap   db "FAIL: no exception was delivered at all", 13, 10, "$"
msgnonest   db "FAIL: the nested exception never arrived", 13, 10, "$"
msgnodpmi   db "FAIL: no dpmi host", 13, 10, "$"
msgnomem    db "FAIL: no dos memory", 13, 10, "$"
msgapi      db "FAIL: dpmi call failed", 13, 10, "$"

            times SIGOFF - 100h - ($ - $$) db 0
sig:        times SIGW dw 0
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
