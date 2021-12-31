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
    self.mkcom_with_gas(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x3c00, %%ax			# create file
    movw    $0, %%cx
    movw    $fname, %%dx
    int     $0x21
    jc      prfailcreate

    movw    %%ax, fhndl

    movw    $0x1000, %%cx           # number of 512 byte chunks

1:
    pushw   %%cx
    movw    $0x4000, %%ax			# write testdata
    movw    fhndl, %%bx
    movw    $fdatalen, %%cx
    movw    $fdata, %%dx
    int     $0x21
    popw    %%cx
    jc      prfailwrite
    cmpw    $fdatalen, %%ax
    jne     prnumwrite
    loop 1b

    movw    $0x%x, %%ax             # seek from whence to somewhere
    movw    fhndl, %%bx
    movw    $seekval, %%si
    movw    %%ds:2(%%si), %%cx
    movw    %%ds:0(%%si), %%dx
    int     $0x21
    jc      prcarryset

    shll    $16, %%edx              # compare the resultant position
    movw    %%ax, %%dx
    movl    expected, %%ecx
    cmpl    %%edx, %%ecx
    jne     prfailcompare

    jmp     prsucc

prfailcreate:
    movw    $failcreate, %%dx
    jmp     1f

prfailwrite:
    movw    $failwrite, %%dx
    jmp     2f

prnumwrite:
    movw    $numwrite, %%dx
    jmp     2f

prcarryset:
    movw    $carryset, %%dx
    jmp     2f

prfailcompare:
    movw    $failcompare, %%dx
    jmp     2f

prsucc:
    movw    $success, %%dx
    jmp     2f

2:
    movw    $0x3e00, %%ax			# close file
    movw    fhndl, %%bx
    int     $0x21

1:
    movb    $0x9, %%ah              # print string
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

seekval:
    .long   %d

expected:
    .long   %d

fname:
    .asciz  "test.fil"

fhndl:
    .word   0

fdata:
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
    .ascii  "0123456789abcdefFEDCBA9876543210"
fdatalen = (. - fdata)

success:
    .ascii  "Operation Success\r\n$"
failcreate:
    .ascii  "Create Operation Failed\r\n$"
failwrite:
    .ascii  "Write Operation Failed\r\n$"
numwrite:
    .ascii  "Write Incorrect Length\r\n$"
carryset:
    .ascii  "Seek Carry Set\r\n$"
failcompare:
    .ascii  "File Position Incorrect\r\n$"

""" % (dosfunc, seekval, expected))

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Create Operation Failed", results)
    self.assertNotIn("Write Operation Failed", results)
    self.assertNotIn("Write Incorrect Length", results)
    self.assertNotIn("Seek Carry Set", results)
    self.assertNotIn("File Position Incorrect", results)
    self.assertIn("Operation Success", results)
