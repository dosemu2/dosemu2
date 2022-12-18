def ds2_file_seek_read(self, fstype, whence):
    testdir = self.mkworkdir('d')

    ename = "ds2seek"

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

    dosfunc, seekval, expected = {
        "SET": (0x4200,  3, "3456"),
        "CUR": (0x4201,  3, "CBA9"),
        "END": (0x4202, -4, "3210"),
    }[whence]

    testdata = "0123456789abcdefFEDCBA9876543210"

    self.mkfile("testit.bat", """\
d:
c:\\%s
DIR
rem end
""" % ename, newline="\r\n")

    # compile sources
    self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 3c00h			; create file
    mov     cx, 0
    mov     dx, fname
    int     21h
    jc      prfailcreate

    mov     word [fhndl], ax

    mov     ax, 4000h			; write testdata
    mov     bx, word [fhndl]
    mov     cx, fdatalen
    mov     dx, fdata
    int     21h
    jc      prfailwrite
    cmp     ax, fdatalen
    jne     prnumwrite

    mov     ax, 3e00h			; close file
    mov     bx, word [fhndl]
    int     21h

    mov     ax, 3d00h			; open file readonly
    mov     dx, fname
    int     21h
    jc      prfailopen

    mov     word [fhndl], ax

    mov     ax, 3f00h			; read from file to middle
    mov     bx, word [fhndl]
    mov     cx, 16
    mov     dx, ftemp
    int     21h
    jc      prfailread
    cmp     ax, 16
    jne     prnumread

    mov     ax, %xh		    	; seek from whence to somewhere
    mov     bx, word [fhndl]
    mov     si, seekval
    mov     cx, [ds:si + 2]
    mov     dx, [ds:si]
    int     21h
    jc      prcarryset

    mov     ax, 3f00h			; read 4 chars from new position
    mov     bx, word [fhndl]
    mov     cx, 4
    mov     dx, fdata2
    int     21h
    jc      prfailread2
    cmp     ax, 4
    jne     prnumread2

    jmp     prsucc

prfailcreate:
    mov     dx, failcreate
    jmp     @1

prfailwrite:
    mov     dx, failwrite
    jmp     @2

prnumwrite:
    mov     dx, numwrite
    jmp     @2

prfailopen:
    mov     dx, failopen
    jmp     @1

prfailread:
    mov     dx, failread
    jmp     @2

prnumread:
    mov     dx, numread
    jmp     @2

prcarryset:
    mov     dx, carryset
    jmp     @2

prfailread2:
    mov     dx, failread2
    jmp     @2

prnumread2:
    mov     dx, numread2
    jmp     @2

prsucc:
    mov     byte [fdata2 + 4], ')'
    mov     byte [fdata2 + 5], 13
    mov     byte [fdata2 + 6], 10
    mov     byte [fdata2 + 7], '$'
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

seekval:
    dd   %d

fname:
    db  "%s", 0

fhndl:
    dw   0

fdata:
    db   "%s"
fdatalen equ $ - fdata

success:
    db  "Operation Success("
fdata2:
    times 64 db 0
ftemp:
    times 64 db 0
failcreate:
    db "Create Operation Failed",13,10,'$'
failwrite:
    db "Write Operation Failed",13,10,'$'
numwrite:
    db "Write Incorrect Length",13,10,'$'
failopen:
    db  "Open Operation Failed",13,10,'$'
failread:
    db  "Read Operation Failed",13,10,'$'
numread:
    db  "Read Not 16 Chars",13,10,'$'
carryset:
    db  "Seek Carry Set",13,10,'$'
failread2:
    db  "Read2 Operation Failed",13,10,'$'
numread2:
    db  "Read2 Not 4 Chars",13,10,'$'

""" % (dosfunc, seekval, "test.fil", testdata))

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Create Operation Failed", results)
    self.assertNotIn("Write Operation Failed", results)
    self.assertNotIn("Write Incorrect Length", results)
    self.assertNotIn("Open Operation Failed", results)
    self.assertNotIn("Read Operation Failed", results)
    self.assertNotIn("Read Not 16 Chars", results)
    self.assertNotIn("Seek Carry Set", results)
    self.assertNotIn("Read2 Operation Failed", results)
    self.assertNotIn("Read2 Not 4 Chars", results)
    self.assertIn("Operation Success(%s)" % expected, results)
