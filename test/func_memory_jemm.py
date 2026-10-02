def _jemm_probe(self, name):
    # JEMM's private API lives on int 15h AX=1209h with a two-letter
    # function code in BX.  'AC' is the probe the Origin games use before
    # they make a single EMS call, and 'VE' is the version they check.
    self.mkcom_with_nasm(name, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 1209h               ; 'AC': is JEMM there?
    mov     bx, 4143h               ; nasm orders 'AC' the other way round
    int     15h
    mov     si, mac
    call    puts
    call    puthex16                ; ax
    mov     al, '/'
    call    putc
    mov     ax, bx
    call    puthex16
    mov     al, '/'
    call    putc
    mov     ax, cx
    call    puthex16
    call    crlf

    mov     ax, 1209h               ; 'VE': version
    mov     bx, 5645h
    int     15h
    mov     si, mve
    call    puts
    call    puthex16
    call    crlf

    mov     ax, 4C00h
    int     21h

putc:
    push    ax
    push    bx
    push    dx
    mov     dl, al
    mov     ah, 2
    int     21h
    pop     dx
    pop     bx
    pop     ax
    ret

puts:
    push    ax
.l:
    lodsb
    or      al, al
    jz      .e
    call    putc
    jmp     .l
.e:
    pop     ax
    ret

crlf:
    push    ax
    mov     al, 13
    call    putc
    mov     al, 10
    call    putc
    pop     ax
    ret

puthex16:
    push    ax
    push    cx
    mov     cx, ax
    mov     al, ch
    call    puthex8
    mov     al, cl
    call    puthex8
    pop     cx
    pop     ax
    ret

puthex8:
    push    ax
    push    cx
    mov     cl, al
    shr     al, 4
    call    .nyb
    mov     al, cl
    and     al, 0Fh
    call    .nyb
    pop     cx
    pop     ax
    ret
.nyb:
    cmp     al, 10
    jb      .d
    add     al, 'a'-10
    jmp     putc
.d:
    add     al, '0'
    jmp     putc

section .data

mac     db 'AC=',0
mve     db 'VE=',0
""")


def memory_jemm_api(self):
    _jemm_probe(self, 'jemmtest')

    self.mkfile("testit.bat", """\
c:\\jemmtest
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ems = (8192)
$_jemm = (on)
""")

    # AX=0 and BX=1209h is what the games test for, CX is JEMM's own
    self.assertIn("AC=0000/1209/0006", results)
    self.assertIn("VE=0436", results)


def memory_jemm_disabled(self):
    _jemm_probe(self, 'jemmoff')

    self.mkfile("testit.bat", """\
c:\\jemmoff
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ems = (8192)
""")

    # nothing answers, so the registers come back as they went in
    self.assertIn("AC=1209/4143/", results)
