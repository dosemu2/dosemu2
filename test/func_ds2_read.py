
def ds2_read_eof(self, fstype):
    testdir = self.mkworkdir('d')

    ename = "ds2rdeof"

    if fstype == "MFS":
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % self.mkimage("12", cwd=testdir)

    testdata = self.mkstring(32)

    self.mkfile("testit.bat", """\
d:
echo %s > test.fil
c:\\%s
DIR
rem end
""" % (testdata, ename), newline="\r\n")

    # compile sources
    self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 3d00h			; open file readonly
    mov     dx, fname
    int     21h
    jc      prfailopen

    mov     word [fhndl], ax

    mov     ax, 3f00h			; read from file, should be partial (35)
    mov     bx, word [fhndl]
    mov     cx, 64
    mov     dx, fdata
    int     21h
    jc      prfailread
    cmp     ax, 35
    jne     prnumread

    mov     ax, 3f00h			; read from file again to get EOF
    mov     bx, word [fhndl]
    mov     cx, 64
    mov     dx, fdata
    int     21h
    jc      prcarryset
    cmp     ax, 0
    jne     praxnotzero

    jmp     prsucc

prfailopen:
    mov     dx, failopen
    jmp     @1

prfailread:
    mov     dx, failread
    jmp     @2

prnumread:
    mov     dx, numread
    jmp     @2

praxnotzero:
    mov     dx, axnotzero
    jmp     @2

prcarryset:
    mov     dx, carryset
    jmp     @2

prsucc:
    mov     byte [fdata + 32], ')'
    mov     byte [fdata + 33], 13
    mov     byte [fdata + 34], 10
    mov     byte [fdata + 35], '$'
    mov     dx, success
    jmp     @2

@2:
    mov     ax, 3e00h			; close file
    mov     bx, word [fhndl]
    int     21h

@1:
    mov     ah, 9               ; print string
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fname:
    db  "%s",0

fhndl:
    dw  0

success:
    db  "Operation Success("
fdata:
    times 64 db 0
failopen:
    db  "Open Operation Failed",13,10,'$'
failread:
    db  "Read Operation Failed",13,10,'$'
numread:
    db  "Partial Read Not 35 Chars",13,10,'$'
carryset:
    db  "Carry Set at EOF",13,10,'$'
axnotzero:
    db  "AX Not Zero at EOF",13,10,'$'

""" % "test.fil")

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Operation Failed", results)
    self.assertNotIn("Partial Read Not 35 Chars", results)
    self.assertNotIn("Carry Set at EOF", results)
    self.assertNotIn("AX Not Zero at EOF", results)
    self.assertIn("Operation Success(%s)" % testdata, results)


def ds2_read_alt_dta(self, fstype):
    testdir = self.mkworkdir('d')

    ename = "ds2radta"

    if fstype == "MFS":
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % self.mkimage("12", cwd=testdir)

    testdata = self.mkstring(32)

    self.mkfile("testit.bat", """\
d:
echo %s > test.fil
c:\\%s
DIR
rem end
""" % (testdata, ename), newline="\r\n")

    # compile sources
    self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 1a00h			; set DTA
    mov     dx, altdta
    int     21h

    mov     ax, 2f00h			; get DTA address in ES:BX
    int     21h
    mov     ax, cs
    mov     dx, es
    cmp     dx, ax
    jne     prfaildtaset
    cmp     bx, altdta
    jne     prfaildtaset

    mov     ax, 3d00h			; open file readonly
    mov     dx, fname
    int     21h
    jc      prfailopen

    mov     word [fhndl], ax

    mov     ax, 3f00h			; read from file, should be partial (35)
    mov     bx, word [fhndl]
    mov     cx, 64
    mov     dx, fdata
    int     21h
    jc      prfailread
    cmp     ax, 35
    jne     prnumread

    jmp     prsucc

prfaildtaset:
    mov     dx, faildtaset
    jmp     @1

prfailopen:
    mov     dx, failopen
    jmp     @1

prfailread:
    mov     dx, failread
    jmp     @2

prnumread:
    mov     dx, numread
    jmp     @2

prsucc:
    mov     byte [fdata + 32], ')'
    mov     byte [fdata + 33], 13
    mov     byte [fdata + 34], 10
    mov     byte [fdata + 35], '$'
    mov     dx, success
    jmp     @2

@2:
    mov     ax, 3e00h			; close file
    mov     bx, word [fhndl]
    int     21h

@1:
    mov     ah, 9               ; print string
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fname:
    db  "%s",0

fhndl:
    dw  0

success:
    db  "Operation Success("
fdata:
    times 64 db 0
faildtaset:
    db  "Set DTA Operation Failed",13,10,'$'
failopen:
    db  "Open Operation Failed",13,10,'$'
failread:
    db  "Read Operation Failed",13,10,'$'
numread:
    db  "Partial Read Not 35 Chars",13,10,'$'

altdta:
    times 128 db 0

""" % "test.fil")

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Operation Failed", results)
    self.assertNotIn("Partial Read Not 35 Chars", results)
    self.assertIn("Operation Success(%s)" % testdata, results)

