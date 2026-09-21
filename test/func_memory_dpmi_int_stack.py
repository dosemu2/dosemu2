from common_framework import DOSEMU_CONF_DEFAULT

# The iret half of this rule only shows up where the host emulates the
# instruction, which the CPU emulator does not need it to.
NATIVE_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "native"\n'

BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_int_stack(self):
    """A software interrupt whose vector the client hooked itself is
    emulated by the host: it builds the iret frame on the client's own
    stack. ESP comes from the client, so it can be anything, and the frame
    used to be written at base+ESP with no regard for the segment limit -
    on real hardware that raises #SS instead.

    The client below loads SS with a small descriptor, leaves ESP past its
    limit - the state a client is in between "mov ss" and "mov esp" - and
    executes int 78h. A signature buffer sits exactly where the frame
    would land."""

    self.mkfile("testit.bat", BATCHFILE % 'intstklm', newline="\r\n")

    self.mkcom_with_nasm("intstklm", r"""
bits 16
cpu 386

org 100h

; ESP is past the limit of its own segment, which is what the host has to
; notice; the 6 byte iret frame still lands in addressable memory, so an
; unchecked host writes it out and the client survives to tell us about it.
BADESP  equ 0400h
BADLIM  equ 03f0h
; the base is picked so that base+BADESP is sig+6
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
    add     dx, SIGOFF + 6 - BADESP
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

    mov     ax, 0203h               ; set the #SS exception handler
    mov     bl, 0ch
    mov     cx, cs
    mov     dx, sshandler
    int     31h
    jc      failapi

    mov     ax, 0205h               ; set protected mode interrupt vector
    mov     bl, 78h
    mov     cx, cs
    mov     dx, inthandler
    int     31h
    jc      failapi

    mov     [savess], ss
    mov     [savesp], sp
    cli
    mov     ax, [badsel]            ; ss switched, esp not yet
    mov     ss, ax
    mov     sp, BADESP
    int     78h

; Reached if the host built the frame on the bad stack. There is no
; returning from here, the frame is past the limit of its own segment.
inthandler:
    mov     word [state], 1
    jmp     restore

; Reached if the host noticed, on the locked stack, as the hardware would
; have done it.
sshandler:
    mov     word [state], 2
    jmp     restore

restore:
    mov     ax, [savess]
    mov     ss, ax
    mov     sp, [savesp]
    sti

    cmp     word [state], 0
    je      notrap

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

pmentry     dd 0
dsbase      dd 0
badsel      dw 0
state       dw 0
savess      dw 0
savesp      dw 0

msgok       db "Test OK", 13, 10, "$"
msgclob     db "FAIL: iret frame written past the stack limit", 13, 10, "$"
msgnotrap   db "FAIL: the interrupt never arrived", 13, 10, "$"
msgnodpmi   db "FAIL: no dpmi host", 13, 10, "$"
msgnomem    db "FAIL: no dos memory", 13, 10, "$"
msgapi      db "FAIL: dpmi call failed", 13, 10, "$"

            times SIGOFF - 100h - ($ - $$) db 0
sig:        times SIGW dw 0
""")

    results = self.runDosemu("testit.bat", config=DOSEMU_CONF_DEFAULT,
                             timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)


def memory_dpmi_iret_stack(self):
    """The other half of the same rule: "sti; iret" is emulated by the
    host, which reads the frame back from the client's own SS:ESP through
    the same unchecked pointer. A client whose ESP is past the limit of
    its stack segment used to get whatever lives at base+ESP popped into
    CS:EIP, or took the host down when nothing was mapped there - real
    hardware raises #SS instead.

    The client below leaves a frame pointing at a label it should never
    reach, so that both outcomes are visible rather than only the crash."""

    self.mkfile("testit.bat", BATCHFILE % 'iretstkl', newline="\r\n")

    self.mkcom_with_nasm("iretstkl", r"""
bits 16
cpu 386

org 100h

BADESP  equ 0400h
BADLIM  equ 03f0h
; the base is picked so that base+BADESP is the frame below
FRMOFF  equ 0600h

section .text

start:
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
    add     dx, FRMOFF - BADESP
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

    mov     word [frame], wrong     ; the frame nobody should pop
    mov     [frame + 2], cs
    mov     word [frame + 4], 0202h

    mov     ax, 0203h               ; set the #SS exception handler
    mov     bl, 0ch
    mov     cx, cs
    mov     dx, sshandler
    int     31h
    jc      failapi

    mov     [savess], ss
    mov     [savesp], sp
    cli
    mov     ax, [badsel]            ; ss switched, esp not yet
    mov     ss, ax
    mov     sp, BADESP
    sti
    iret

; Reached only if the host popped the frame from past the stack limit.
wrong:
    mov     word [state], 1
    jmp     restore

; Reached if the host noticed, as the hardware would have done.
sshandler:
    mov     word [state], 2
    jmp     restore

restore:
    mov     ax, [savess]
    mov     ss, ax
    mov     sp, [savesp]
    sti

    cmp     word [state], 2
    je      .ok
    cmp     word [state], 1
    je      popped
    jmp     notrap
.ok:
    mov     dx, msgok
    jmp     print

popped:
    mov     dx, msgpop
    jmp     print
notrap:
    mov     dx, msgnotrap
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

pmentry     dd 0
dsbase      dd 0
badsel      dw 0
state       dw 0
savess      dw 0
savesp      dw 0

msgok       db "Test OK", 13, 10, "$"
msgpop      db "FAIL: iret frame popped from past the stack limit", 13, 10, "$"
msgnotrap   db "FAIL: the iret did nothing", 13, 10, "$"
msgnodpmi   db "FAIL: no dpmi host", 13, 10, "$"
msgnomem    db "FAIL: no dos memory", 13, 10, "$"
msgapi      db "FAIL: dpmi call failed", 13, 10, "$"

            times FRMOFF - 100h - ($ - $$) db 0
frame:      times 3 dw 0
""")

    results = self.runDosemu("testit.bat", config=NATIVE_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
