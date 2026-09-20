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


def memory_jemm_windows(self):
    # With JEMM the client gets one array of 24 windows, high enough that
    # 640k of DOS memory is still its own and low enough to leave our BIOS
    # alone.  The windows are real memory whichever view of memory the
    # client asked for, and 'SM'/'sm' report the previous state.

    self.mkcom_with_nasm('jemmwnd', r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 4100h               ; page frame segment
    int     67h
    or      ah, ah
    jnz     noems
    mov     [frame], bx
    mov     si, mframe
    call    puts
    mov     ax, bx
    call    puthex16
    call    crlf

    mov     ax, 5801h               ; how many windows are there
    int     67h
    or      ah, ah
    jnz     noems
    mov     si, mpages
    call    puts
    mov     ax, cx
    call    puthex16
    call    crlf

    mov     ax, 4300h               ; two logical pages
    mov     bx, 2
    int     67h
    or      ah, ah
    jnz     noems
    mov     [handle], dx            ; the handle comes back in dx

    mov     ax, [frame]             ; window 2 is the one over 0xa000,
    add     ax, 800h                ; where the video memory would be
    mov     [wnd], ax

    mov     ax, 4402h               ; logical page 0 into window 2
    xor     bx, bx
    int     67h
    or      ah, ah
    jnz     nomap

    mov     es, [wnd]               ; it is memory of our own now
    xor     di, di
    mov     si, marker
    mov     cx, 8
    rep     movsb

    mov     ds, [cs:wnd]            ; and it reads back as we left it
    xor     si, si
    push    cs
    pop     es
    mov     di, buf
    mov     cx, 8
    rep     movsb
    push    cs
    pop     ds

    mov     si, mems
    call    puts
    mov     si, buf
    mov     cx, 8
.p:
    lodsb
    call    putc
    loop    .p
    call    crlf

    mov     ax, [frame]             ; window 22 is the one over 0xf000,
    add     ax, 5800h               ; where our read-only ROM would be
    mov     [wnd], ax

    mov     ax, 4416h               ; logical page 1 into window 22
    mov     bx, 1
    int     67h
    or      ah, ah
    jnz     nomap

    mov     es, [wnd]               ; it is memory of our own here too
    xor     di, di
    mov     si, marker
    mov     cx, 8
    rep     movsb

    mov     ds, [cs:wnd]
    xor     si, si
    push    cs
    pop     es
    mov     di, buf
    mov     cx, 8
    rep     movsb
    push    cs
    pop     ds

    mov     si, mrom
    call    puts
    mov     si, buf
    mov     cx, 8
.q:
    lodsb
    call    putc
    loop    .q
    call    crlf

    mov     si, mstate              ; 'SM' and 'sm' answer with the state
    call    puts                    ; they found, so this reads 0110
    mov     bx, 534Dh
    call    jemm
    mov     bx, 534Dh
    call    jemm
    mov     bx, 736Dh
    call    jemm
    mov     bx, 736Dh
    call    jemm
    call    crlf

    mov     ax, 4500h
    mov     dx, [handle]
    int     67h
    jmp     done

noems:
    mov     si, mnoems
    call    puts
    jmp     done
nomap:
    mov     si, mnomap
    call    puts
done:
    mov     ax, 4C00h
    int     21h

jemm:                               ; call BX, print the previous state
    mov     ax, 1209h
    int     15h
    mov     al, bl
    add     al, '0'
    call    putc
    ret

putc:
    push    ax
    push    dx
    mov     dl, al
    mov     ah, 2
    int     21h
    pop     dx
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

puthex16:
    push    ax
    push    bx
    mov     bx, ax
    mov     al, bh
    call    puthex8
    mov     al, bl
    call    puthex8
    pop     bx
    pop     ax
    ret

section .data

handle  dw 0
frame   dw 0
wnd     dw 0
marker  db 'JEMMOK!!'
buf     times 8 db '?'

mframe  db 'FRAME=',0
mpages  db 'PAGES=',0
mems    db 'EMS=',0
mrom    db 'ROM=',0
mstate  db 'STATE=',0
mnoems  db 'NOEMS',13,10,0
mnomap  db 'NOMAP',13,10,0
""")

    self.mkfile("testit.bat", """\
c:\\jemmwnd
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ems = (8192)
$_jemm = (on)
""")

    self.assertNotIn("NOEMS", results)
    self.assertNotIn("NOMAP", results)

    # 24 windows, ending below the lowmem heap and our BIOS
    self.assertIn("FRAME=9800", results)
    self.assertIn("PAGES=0018", results)

    # the window is EMS memory, not the video memory it sits over
    self.assertIn("EMS=JEMMOK!!", results)

    # the window over our f000 ROM is the client's memory, not read-only
    self.assertIn("ROM=JEMMOK!!", results)

    # each switch reports the state it found
    self.assertIn("STATE=0110", results)


def memory_jemm_xms(self):
    # Under JEMM the window array covers the whole upper memory area, so
    # there is no room left for UMBs.  That must not cost DOS its XMS: one
    # driver installs both, and it used to give up on the pair of them.

    self.mkcom_with_nasm('jemmxms', r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 4300h               ; is an XMS driver there?
    int     2Fh
    cmp     al, 80h
    mov     dx, mnoxms
    jne     .say

    mov     ax, 4310h               ; its entry point
    int     2Fh
    mov     [entry], bx
    mov     [entry + 2], es

    mov     ah, 8                   ; query free extended memory
    xor     bl, bl
    call    far [entry]
    or      bl, bl
    mov     dx, mnofree
    jnz     .say
    or      ax, ax                  ; largest free block, in kbytes
    mov     dx, mnofree             ; mov leaves the flags alone
    jz      .say
    mov     dx, mxms

.say:
    mov     ah, 9
    int     21h

    mov     ax, 4C00h
    int     21h

section .data

entry   dd 0
mxms    db 'XMSOK',13,10,'$'
mnoxms  db 'NOXMS',13,10,'$'
mnofree db 'NOFREE',13,10,'$'
""")

    self.mkfile("testit.bat", """\
c:\\jemmxms
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ems = (8192)
$_jemm = (on)
""")

    self.assertNotIn("NOXMS", results)
    self.assertNotIn("NOFREE", results)
    self.assertIn("XMSOK", results)
