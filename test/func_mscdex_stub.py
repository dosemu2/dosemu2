import re


def mscdex_stub(self):
    # With no CD-ROM drive configured dosemu2 used to leave int 2Fh AH=15h
    # alone altogether, so a client saw its own registers come back. A CD
    # game whose disc check has been patched out then believes it is talking
    # to MSCDEX anyway, never allocates the buffer it asks MSCDEX to fill
    # because the drive count stayed zero, and ends up loading the null far
    # pointer it left behind into GS. The access through it is a #GP and the
    # program dies.
    #
    # Only the install check is answered, so that is all this checks; the
    # other two calls are printed to show what an unanswered one looks like.

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""

    self.mkfile("testit.bat", """\
c:\\cdstub
rem end
""", newline="\r\n")

    self.mkcom_with_nasm("cdstub", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    ; AX=1500h - installation check, BX = drive count, CX = first drive
    mov     ax, 1500h
    xor     bx, bx
    xor     cx, cx
    int     2fh
    mov     [ndrv], bx
    mov     [fdrv], cx

    ; AX=150Bh - drive check, AX nonzero and BX=ADADh when it is ours
    mov     ax, 150bh
    mov     cx, [fdrv]
    xor     bx, bx
    int     2fh
    mov     [chk], ax
    mov     [magic], bx

    ; AX=150Ch - MSCDEX version in BX
    mov     ax, 150ch
    xor     bx, bx
    int     2fh
    mov     [vers], bx

    mov     ax, [ndrv]
    mov     di, outn
    call    hex16
    mov     ax, [fdrv]
    mov     di, outf
    call    hex16
    mov     ax, [chk]
    mov     di, outc
    call    hex16
    mov     ax, [magic]
    mov     di, outm
    call    hex16
    mov     ax, [vers]
    mov     di, outv
    call    hex16

    mov     ah, 9
    mov     dx, result
    int     21h

    mov     ax, 4c00h
    int     21h

; ax = value to print, di -> 4 chars of output
hex16:
    mov     cx, 4
.loop:
    rol     ax, 4
    push    ax
    and     al, 0fh
    add     al, '0'
    cmp     al, '9'
    jbe     .store
    add     al, 7
.store:
    mov     [di], al
    inc     di
    pop     ax
    loop    .loop
    ret

section .data

ndrv:   dw 0
fdrv:   dw 0
chk:    dw 0
magic:  dw 0
vers:   dw 0

result:
    db "NDRV("
outn:
    db "XXXX"
    db ") FDRV("
outf:
    db "XXXX"
    db ") CHK("
outc:
    db "XXXX"
    db ") MAGIC("
outm:
    db "XXXX"
    db ") VERS("
outv:
    db "XXXX"
    db ")",13,10,'$'
""")

    results = self.runDosemu("testit.bat", config=config)

    r = re.compile(r'NDRV\(([0-9A-F]{4})\) FDRV\(([0-9A-F]{4})\) '
                   r'CHK\(([0-9A-F]{4})\) MAGIC\(([0-9A-F]{4})\) '
                   r'VERS\(([0-9A-F]{4})\)')
    self.assertRegex(results, r)
    t = r.search(results)
    ndrv = int(t.group(1), 16)
    fdrv = int(t.group(2), 16)
    chk = int(t.group(3), 16)
    magic = int(t.group(4), 16)
    vers = int(t.group(5), 16)

    # no drive is configured here, so the stub is what has to answer
    self.assertGreaterEqual(ndrv, 1, results)
    self.assertLess(fdrv, 26, results)

    # and the calls it does not answer must come back untouched, not with
    # something invented: AX as passed in, BX and the version still zero
    self.assertEqual(chk, 0x150b, results)
    self.assertEqual(magic, 0, results)
    self.assertEqual(vers, 0, results)
