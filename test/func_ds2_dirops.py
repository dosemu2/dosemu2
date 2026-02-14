from os.path import exists, join


def ds2_rename_common(self, fstype, testname):
    testdir = self.mkworkdir('d')

    extrad = ""

    if testname == "file":
        ename = "mfsds2r1"
        fn1 = "testa"
        fe1 = "bat"
        fn2 = "testb"
        fe2 = "bal"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
    elif testname == "file_src_missing":
        ename = "mfsds2r2"
        fn1 = "testa"
        fe1 = "bat"
        fn2 = "testb"
        fe2 = "bal"
    elif testname == "file_tgt_exists":
        ename = "mfsds2r3"
        fn1 = "testa"
        fe1 = "bat"
        fn2 = "testb"
        fe2 = "bal"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
        self.mkfile(fn2 + "." + fe2, """hello\r\n""", testdir)
    elif testname == "dir":
        ename = "mfsds2r4"
        fn1 = "testa"
        fe1 = ""
        fn2 = "testb"
        fe2 = ""
        extrad = "mkdir %s\n" % fn1
    elif testname == "dir_src_missing":
        ename = "mfsds2r5"
        fn1 = "testa"
        fe1 = ""
        fn2 = "testb"
        fe2 = ""
    elif testname == "dir_tgt_exists":
        ename = "mfsds2r6"
        fn1 = "testa"
        fe1 = ""
        fn2 = "testb"
        fe2 = ""
        extrad = "mkdir %s\nmkdir %s\n" % (fn1, fn2)

    self.mkfile("testit.bat", """\
d:
%s
c:\\%s
DIR
rem end
""" % (extrad, ename), newline="\r\n")

    # compile sources
    self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds
    push    cs
    pop     es

    mov     ax, 5600h
    mov     dx, src
    mov     di, dst
    int     21h
    jnc     prsucc

prfail:
    mov     dx, failmsg
    jmp     @1
prsucc:
    mov     dx, succmsg
@1:
    mov     ah, 9
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

src:
    db  "%s",0    ; Full path
dst:
    db  "%s",0    ; Full path

succmsg:
    db  "Rename Operation Success",13,10,'$'
failmsg:
    db  "Rename Operation Failed",13,10,'$'

""" % (fn1 + "." + fe1, fn2 + "." + fe2))

    def assertIsPresent(testdir, results, fstype, f, e, msg=None):
        if fstype == "MFS":
            self.assertTrue(exists(join(testdir, f + "." + e)), msg)
        else:
            self.assertRegex(results.upper(), r"%s( +|\.)%s" % (f.upper(), e.upper()), msg)

    def assertIsPresentDir(testdir, results, fstype, f, msg=None):
        if fstype == "MFS":
            self.assertTrue(exists(join(testdir, f)), msg)
        else:
            # 2019-06-27 11:29 <DIR>         DOSEMU
            # DOSEMU               <DIR>  06-27-19  5:33p
            # TESTB        <DIR>     8-17-20  2:03p
            self.assertRegex(results.upper(),
                r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s<DIR>\s+%s"
                r"|"
                r"%s\s+<DIR>\s+\d{1,2}-\d{1,2}-\d{2}\s+\d+:\d+[AaPp]" % (f.upper(), f.upper()), msg)

    if fstype == "MFS":
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

    if testname == "file":
        self.assertIn("Rename Operation Success", results)
        assertIsPresent(testdir, results, fstype, fn2, fe2, "File not renamed")

    elif testname == "file_src_missing":
        self.assertIn("Rename Operation Failed", results)

    elif testname == "file_tgt_exists":
        self.assertIn("Rename Operation Failed", results)

    elif testname == "dir":
        self.assertIn("Rename Operation Success", results)
        assertIsPresentDir(testdir, results, fstype, fn2, "Directory not renamed")

    elif testname == "dir_src_missing":
        self.assertIn("Rename Operation Failed", results)

    elif testname == "dir_tgt_exists":
        self.assertIn("Rename Operation Failed", results)


def ds2_delete_common(self, fstype, testname):
    testdir = self.mkworkdir('d')

    if testname == "file":
        ename = "mfsds2d1"
        fn1 = "testa"
        fe1 = "bat"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", dname=testdir)
    elif testname == "file_missing":
        ename = "mfsds2d2"
        fn1 = "testa"
        fe1 = "bat"

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

    mov     ax, 4100h
    mov     dx, src
    int     21h
    jnc     prsucc

prfail:
    mov     dx, failmsg
    jmp     @1
prsucc:
    mov     dx, succmsg
@1:
    mov     ah, 9
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

src:
    db  "%s",0    ; Full path

succmsg:
    db  "Delete Operation Success",13,10,'$'
failmsg:
    db  "Delete Operation Failed",13,10,'$'

""" % (fn1 + "." + fe1))

    def assertIsNotPresent(testdir, results, fstype, f, e, msg=None):
        if fstype == "MFS":
            self.assertFalse(exists(join(testdir, f + "." + e)), msg)
        else:
            self.assertNotRegex(results.upper(), r"%s( +|\.)%s" % (f.upper(), e.upper()))

    if fstype == "MFS":
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

    if testname == "file":
        self.assertIn("Delete Operation Success", results)
        assertIsNotPresent(testdir, results, fstype, fn1, fe1, "File not deleted")

    elif testname == "file_missing":
        self.assertIn("Delete Operation Failed", results)


def ds2_find_common(self, fstype, testname):
    testdir = self.mkworkdir('d')

    if testname == "simple":
        ename = "ds2find1"
        fn1 = "testa"
        fe1 = "bat"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
    elif testname == "missing":
        ename = "ds2find2"
        fn1 = "testa"
        fe1 = "bat"
    elif testname == "wild_one":
        ename = "ds2find3"
        fn1 = "*"
        fe1 = "in"
        for f in ["one.in", "two.in", "three.in", "four.in", "five.in",
                  "none.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_two":
        ename = "ds2find4"
        fn1 = "a*"
        fe1 = "*"
        for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                  "afive.in", "xnone.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_three":
        # To find "abc001.txt ... abc099.txt"
        ename = "ds2find5"
        fn1 = "abc0??"
        fe1 = "*"
        for f in ["abc001.txt", "abc002.txt", "abc003.txt", "abc004.txt",
                  "abc005.txt", "abc010.txt", "xbc007.txt"]:
            self.mkfile(f, """hello\r\n""", testdir)

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

    ; Get DTA -> ES:BX
    mov     ax, 2f00h
    int     21h
    push    es
    push    bx
    pop     long [pdta]

    ; FindFirst
findfirst:
    mov     ax, 4e00h
    mov     cx, 0
    mov     dx, fpatn
    int     21h
    jnc     prsucc

prfail:
    mov     dx, failmsg
    mov     ah, 9
    int     21h
    jmp     exit

prsucc:
    mov     dx, succmsg
    mov     ah, 9
    int     21h

prfilename:
    push    ds
    lds     ax, [pdta]
    add     ax, 1eh
    mov     si, ax

    push    cs
    pop     es
    mov     di, prires + 1

    mov     cx, 13
    cld

@1:
    cmp     byte [ds:si], 0
    je      @2

    movsb
    loop    @1

@2:
    mov     byte [es:di], ')'
    mov     byte [es:di + 1], 13
    mov     byte [es:di + 2], 10
    mov     byte [es:di + 3], '$'

    pop     ds
    mov     dx, prires
    mov     ah, 9
    int     21h

    ; FindNext
findnext:
    mov     ax, 4f00h
    int     21h
    jnc     prfilename

exit:
    mov     ah, 4ch
    int     21h

section .data

fpatn:
    db  "%s",0

pdta:
    dd  0

prires:
    db  "("
    times 32 db 0

succmsg:
    db  "Findfirst Operation Success",13,10,'$'
failmsg:
    db  "Findfirst Operation Failed",13,10,'$'

""" % (fn1 + "." + fe1))

    if fstype == "MFS":
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

    if testname == "simple":
        self.assertIn("Findfirst Operation Success", results)
        self.assertIn("(TESTA.BAT)", results)

    elif testname == "missing":
        self.assertIn("Findfirst Operation Failed", results)

    elif testname == "wild_one":
        self.assertIn("Findfirst Operation Success", results)
        self.assertIn("(ONE.IN)", results)
        self.assertIn("(TWO.IN)", results)
        self.assertIn("(THREE.IN)", results)
        self.assertIn("(FOUR.IN)", results)
        self.assertIn("(FIVE.IN)", results)
        self.assertNotIn("(NONE.CTL)", results)

    elif testname == "wild_two":
        self.assertIn("Findfirst Operation Success", results)
        self.assertIn("(AONE.IN)", results)
        self.assertIn("(ATWO.IN)", results)
        self.assertIn("(ATHREE.IN)", results)
        self.assertIn("(AFOUR.IN)", results)
        self.assertIn("(AFIVE.IN)", results)
        self.assertNotIn("(XNONE.CTL)", results)

    elif testname == "wild_three":
        self.assertIn("Findfirst Operation Success", results)
        self.assertIn("(ABC001.TXT)", results)
        self.assertIn("(ABC002.TXT)", results)
        self.assertIn("(ABC003.TXT)", results)
        self.assertIn("(ABC004.TXT)", results)
        self.assertIn("(ABC005.TXT)", results)
        self.assertIn("(ABC010.TXT)", results)
        self.assertNotIn("(XBC007.TXT)", results)


def ds2_find_first(self, fstype, testname):
    testdir = self.mkworkdir('d')

    ename = "ds2fndfi"

    ATTR = "0x00"

    if testname == "file_exists":
        FSPEC = r"\fileexst.ext"
    elif testname == "file_exists_as_dir":
        FSPEC = r"\fileexst.ext\somefile.ext"
    elif testname == "file_not_found":
        FSPEC = r"\Notfname.ext"
    elif testname == "no_more_files":
        FSPEC = r"\????????.??x"
    elif testname == "path_not_found_wc":
        FSPEC = r"\NotDir\????????.???"
    elif testname == "path_not_found_pl":
        FSPEC = r"\NotDir\plainfil.txt"
    elif testname == "path_exists_empty":
        FSPEC = r"\DirExist"
    elif testname == "path_exists_not_empty":
        FSPEC = r"\DirExis2"
    elif testname == "path_exists_file_not_dir":
        FSPEC = r"\DirExis2\fileexst.ext"
        ATTR = "0x10"
    elif testname == "dir_exists_pl":
        FSPEC = r"\DirExis2"
        ATTR = "0x10"
    elif testname == "dir_exists_wc":
        FSPEC = r"\Di?Exis?"
        ATTR = "0x10"
    elif testname == "dir_not_exists_pl":
        FSPEC = r"\dirNOTex"
        ATTR = "0x10"
    elif testname == "dir_not_exists_wc":
        FSPEC = r"\dirNOTex\wi??card.???"
        ATTR = "0x10"
    elif testname == "dir_not_exists_fn":
        FSPEC = r"\dirNOTex\somefile.ext"
        ATTR = "0x10"

    self.mkfile("testit.bat", """\
d:
echo hello > fileexst.ext
mkdir DirExist
mkdir DirExis2
echo hello > DirExis2\\fileexst.ext
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

    mov     ax, 4e00h
    mov     cx, %s
    mov     dx, fspec
    int     21h

    jnc     prsucc

    cmp     ax, 2
    je      fail02

    cmp     ax, 3
    je      fail03

    cmp     ax, 12h
    je      fail12

    jmp     genfail

fail02:
    mov     dx, filenotfound
    jmp     @1

fail03:
    mov     dx, pathnotfoundmsg
    jmp     @1

fail12:
    mov     dx, nomoremsg
    jmp     @1

genfail:
    mov     dx, genfailmsg
    jmp     @1

prsucc:
    mov     dx, succmsg

@1:
    mov     ah, 9
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fspec:
    db  "%s",0    ; Full path

succmsg:
    db  "FindFirst Operation Success",13,10,'$'
filenotfound:
    db  "FindFirst Operation Returned FILE_NOT_FOUND(0x02)",13,10,'$'
pathnotfoundmsg:
    db  "FindFirst Operation Returned PATH_NOT_FOUND(0x03)",13,10,'$'
nomoremsg:
    db  "FindFirst Operation Returned NO_MORE_FILES(0x12)",13,10,'$'
genfailmsg:
    db  "FindFirst Operation Returned Unexpected Errorcode",13,10,'$'

""" % (ATTR, FSPEC))

    if fstype == "MFS":
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

    if testname == "file_exists":
        self.assertIn("Operation Success", results)
    elif testname == "file_exists_as_dir":
        self.assertIn("Operation Returned PATH_NOT_FOUND(0x03)", results)
    elif testname == "file_not_found":  # Confirmed as not FILE_NOT_FOUND
        self.assertIn("Operation Returned NO_MORE_FILES(0x12)", results)
    elif testname == "no_more_files":
        self.assertIn("Operation Returned NO_MORE_FILES(0x12)", results)
    elif testname == "path_not_found_wc":
        self.assertIn("Operation Returned PATH_NOT_FOUND(0x03)", results)
    elif testname == "path_not_found_pl":
        self.assertIn("Operation Returned PATH_NOT_FOUND(0x03)", results)
    elif testname == "path_exists_empty":
        self.assertIn("Operation Returned NO_MORE_FILES(0x12)", results)
    elif testname == "path_exists_not_empty":
        self.assertIn("Operation Returned NO_MORE_FILES(0x12)", results)
    elif testname == "path_exists_file_not_dir":
        self.assertIn("Operation Success", results)
    elif testname == "dir_exists_pl":
        self.assertIn("Operation Success", results)
    elif testname == "dir_exists_wc":
        self.assertIn("Operation Success", results)
    elif testname == "dir_not_exists_pl":
        self.assertIn("Operation Returned NO_MORE_FILES(0x12)", results)
    elif testname == "dir_not_exists_wc":
        self.assertIn("Operation Returned PATH_NOT_FOUND(0x03)", results)
    elif testname == "dir_not_exists_fn":
        self.assertIn("Operation Returned PATH_NOT_FOUND(0x03)", results)


def ds2_find_mixed_wild_plain(self, fstype):
    testdir = self.mkworkdir('d')

    ename = "ds2findm"
    fsmpl = "xbc007.txt"

    for f in ["abc001.txt", "abc002.txt", "abc003.txt", "abc004.txt",
              "abc005.txt", "abc010.txt", "xbc007.txt"]:
        self.mkfile(f, """hello\r\n""", testdir)

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

    ; Get DTA -> ES:BX
    mov     ax, 2f00h
    int     21h
    push    es
    push    bx
    pop     long [pdta]

    ; First FindFirst
    mov     ax, 4e00h
    mov     cx, 0
    mov     dx, fwild
    int     21h

    ; Set alternate DTA
    mov     ax, 1a00h
    mov     dx, altdta
    int     21h

    ; Second FindFirst
    mov     ax, 4e00h
    mov     cx, 0
    mov     dx, fsmpl
    int     21h

    ; Set default DTA
    mov     ax, 1a00h
    lds     dx, [pdta]
    int     21h

    ; FindNext
    mov     ax, 4f00h
    int     21h
    jnc     prsucc

prfail:
    mov     dx, failmsg
    mov     ah, 9
    int     21h
    jmp     exit

prsucc:
    push    ds
    lds     ax, [pdta]
    add     ax, 1eh
    mov     si, ax

    push    cs
    pop     es
    mov     di, prires + 1

    mov     cx, 13
    cld

@1:
    cmp     byte [ds:si], 0
    je      @2

    movsb
    loop    @1

@2:
    mov     byte [es:di], ')'
    mov     byte [es:di + 1], 13
    mov     byte [es:di + 2], 10
    mov     byte [es:di + 3], '$'

    pop     ds
    mov     dx, succmsg
    mov     ah, 9
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fwild:
    db "a*.txt",0
fsmpl:
    db "%s",0

altdta:
    times 0x80 db 0

pdta:
    dd  0

succmsg:
    db  "Findnext Operation Success"
prires:
    db  "("
    times 32 db 0

failmsg:
    db  "Findnext Operation Failed",13,10,'$'

""" % fsmpl)

    if fstype == "MFS":
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        name = self.mkimage("12", cwd=testdir)
        config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Findnext Operation Failed", results)
    self.assertRegex(results, r"Findnext Operation Success\(ABC...\.TXT\)")
