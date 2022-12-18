def ds2_file_seek_tell(self, fstype, testfunc):
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

    self.mkfile("testit.bat", """\
d:
c:\\%s
DIR
rem end
""" % ename, newline="\r\n")

    dosfunc, seekval, expected = {
        "ENDBCKSML": (0x4202,      -7, 2097145),
        "ENDBCKLRG": (0x4202,-0x10000, 2031616),
        "ENDFWDSML": (0x4202,       7, 2097159),
        "ENDFWDLRG": (0x4202, 0x10000, 2162688),
    }[testfunc]

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

    mov     cx, 1000h           ; number of 512 byte chunks

@1:
    push    cx
    mov     ax, 4000h			; write testdata
    mov     bx, word [fhndl]
    mov     cx, fdatalen
    mov     dx, fdata
    int     21h
    pop     cx
    jc      prfailwrite
    cmp     ax, fdatalen
    jne     prnumwrite
    loop @1

    mov     ax, %xh              ; seek from whence to somewhere
    mov     bx, word [fhndl]
    mov     si, seekval
    mov     cx, word [ds:si + 2]
    mov     dx, word [ds:si]
    int     21h
    jc      prcarryset

    shl     edx, 16              ; compare the resultant position
    mov     dx, ax
    mov     ecx, long [expected]
    cmp     ecx, edx
    jne     prfailcompare

    jmp     prsucc

prfailcreate:
    mov     dx, failcreate
    jmp     @3

prfailwrite:
    mov     dx, failwrite
    jmp     @2

prnumwrite:
    mov     dx, numwrite
    jmp     @2

prcarryset:
    mov     dx, carryset
    jmp     @2

prfailcompare:
    mov     dx, failcompare
    jmp     @2

prsucc:
    mov     dx, success
    jmp     @2

@2:
    mov     ax, 3e00h			; close file
    mov     bx, word [fhndl]
    int     21h

@3:
    mov     ah, 9               ; print string
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

seekval:
    dd   %d

expected:
    dd   %d

fname:
    db   "test.fil", 0

fhndl:
    dw   0

fdata:
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
    db  "0123456789abcdefFEDCBA9876543210"
fdatalen equ $ - fdata

success:
    db  "Operation Success",13,10,'$'
failcreate:
    db  "Create Operation Failed",13,10,'$'
failwrite:
    db  "Write Operation Failed",13,10,'$'
numwrite:
    db  "Write Incorrect Length",13,10,'$'
carryset:
    db  "Seek Carry Set",13,10,'$'
failcompare:
    db  "File Position Incorrect",13,10,'$'

""" % (dosfunc, seekval, expected))

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Create Operation Failed", results)
    self.assertNotIn("Write Operation Failed", results)
    self.assertNotIn("Write Incorrect Length", results)
    self.assertNotIn("Seek Carry Set", results)
    self.assertNotIn("File Position Incorrect", results)
    self.assertIn("Operation Success", results)
