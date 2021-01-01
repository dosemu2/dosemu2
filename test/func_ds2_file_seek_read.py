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

    movw    $0x4000, %%ax			# write testdata
    movw    fhndl, %%bx
    movw    $fdatalen, %%cx
    movw    $fdata, %%dx
    int     $0x21
    jc      prfailwrite
    cmpw    $fdatalen, %%ax
    jne     prnumwrite

    movw    $0x3e00, %%ax			# close file
    movw    fhndl, %%bx
    int     $0x21

    movw    $0x3d00, %%ax			# open file readonly
    movw    $fname, %%dx
    int     $0x21
    jc      prfailopen

    movw    %%ax, fhndl

    movw    $0x3f00, %%ax			# read from file to middle
    movw    fhndl, %%bx
    movw    $16, %%cx
    movw    $ftemp, %%dx
    int     $0x21
    jc      prfailread
    cmpw    $16, %%ax
    jne     prnumread

    movw    $0x%x, %%ax		    	# seek from whence to somewhere
    movw    fhndl, %%bx
    movw    $seekval, %%si
    movw    %%ds:2(%%si), %%cx
    movw    %%ds:0(%%si), %%dx
    int     $0x21
    jc      prcarryset

    movw    $0x3f00, %%ax			# read 4 chars from new position
    movw    fhndl, %%bx
    movw    $4, %%cx
    movw    $fdata2, %%dx
    int     $0x21
    jc      prfailread2
    cmpw    $4, %%ax
    jne     prnumread2

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

prfailopen:
    movw    $failopen, %%dx
    jmp     1f

prfailread:
    movw    $failread, %%dx
    jmp     2f

prnumread:
    movw    $numread, %%dx
    jmp     2f

prcarryset:
    movw    $carryset, %%dx
    jmp     2f

prfailread2:
    movw    $failread2, %%dx
    jmp     2f

prnumread2:
    movw    $numread2, %%dx
    jmp     2f

prsucc:
    movb    $')',  (fdata2 + 4)
    movb    $'\r', (fdata2 + 5)
    movb    $'\n', (fdata2 + 6)
    movb    $'$',  (fdata2 + 7)
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

fname:
    .asciz  "%s"

fhndl:
    .word   0

fdata:
    .ascii  "%s"
fdatalen = (. - fdata)

success:
    .ascii  "Operation Success("
fdata2:
    .space  64
ftemp:
    .space  64
failcreate:
    .ascii  "Create Operation Failed\r\n$"
failwrite:
    .ascii  "Write Operation Failed\r\n$"
numwrite:
    .ascii  "Write Incorrect Length\r\n$"
failopen:
    .ascii  "Open Operation Failed\r\n$"
failread:
    .ascii  "Read Operation Failed\r\n$"
numread:
    .ascii  "Read Not 16 Chars\r\n$"
carryset:
    .ascii  "Seek Carry Set\r\n$"
failread2:
    .ascii  "Read2 Operation Failed\r\n$"
numread2:
    .ascii  "Read2 Not 4 Chars\r\n$"

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
