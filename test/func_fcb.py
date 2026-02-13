from os.path import exists, join


def fcb_read(self, fstype):
    testdir = self.mkworkdir('d')

    if fstype == "MFS":
        ename = "mfsfcbrd"
        fcbreadconfig = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        ename = "fatfcbrd"
        fcbreadconfig = """\
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

    mov     ax, 0f00h			; open file
    mov     dx, fcb
    int     21h
    cmp     al, 0
    jne     prfailopen

    mov     ax, 1400h			; read from file
    mov     dx, fcb
    int     21h
    cmp     al, 3               ; partial read
    jne     prfailread

    jmp     prsucc

prfailopen:
    mov     dx, failopen
    jmp     @1

prfailread:
    mov     ax, 1000h			; close file
    mov     dx, fcb
    int     21h
    mov     dx, failread
    jmp     @1

prsucc:
    mov     dx, succstart
    mov     ah, 9
    int     21h

    mov     ax, 2f00h			; get DTA address in ES:BX
    int     21h

    mov     byte [es:bx+%d], '$'; terminate
    push    es
    pop     ds
    mov     dx, bx
    mov     ah, 9
    int     21h

    mov     ax, 1000h			; close file
    mov     dx, fcb
    int     21h

    push    cs
    pop     ds
    mov     dx, succend

@1:
    mov     ah, 9
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fcb:
    db  0          ; 0 default drive
fn1:
    db  "% -8s"    ; 8 bytes
fe1:
    db  "% -3s"    ; 3 bytes
wk1:
    times 24 db 0

succstart:
    db  "Operation Success($"
succend:
    db  ')',13,10,'$'
failopen:
    db  "Open Operation Failed",13,10,'$'
failread:
    db  "Read Operation Failed",13,10,'$'

""" % (len(testdata), "test", "fil"))

    results = self.runDosemu("testit.bat", config=fcbreadconfig)

    self.assertNotIn("Operation Failed", results)
    self.assertIn("Operation Success(%s)" % testdata, results)


def fcb_read_alt_dta(self, fstype):
    testdir = self.mkworkdir('d')

    ename = "fcbradta"

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

    mov     ax, 0f00h			; open file
    mov     dx, fcb
    int     21h
    cmp     al, 0
    jne     prfailopen

    mov     ax, 1400h			; read from file
    mov     dx, fcb
    int     21h
    cmp     al, 3				; partial read
    jne     prfailread

    jmp     prsucc

prfaildtaset:
    mov     dx, faildtaset
    jmp     @1

prfailopen:
    mov     dx, failopen
    jmp     @1

prfailread:
    mov     ax, 1000h			; close file
    mov     dx, fcb
    int     21h
    mov     dx, failread
    jmp     @1

prsucc:
    mov     dx, succstart
    mov     ah, 9
    int     21h

    mov     ax, 2f00h			; get DTA address in ES:BX
    int     21h

    mov     byte [es:bx+%d], '$'; terminate
    push    es
    pop     ds
    mov     dx, bx
    mov     ah, 9
    int     21h

    mov     ax, 1000h			; close file
    mov     dx, fcb
    int     21h

    push    cs
    pop     ds
    mov     dx, succend

@1:
    mov     ah, 9
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fcb:
    db  0          ; 0 default drive
fn1:
    db  "% -8s"    ; 8 bytes
fe1:
    db  "% -3s"    ; 3 bytes
wk1:
    times 24 db 0

succstart:
    db  "Operation Success($"
succend:
    db  ')',13,10,'$'
faildtaset:
    db  "Set DTA Operation Failed",13,10,'$'
failopen:
    db  "Open Operation Failed",13,10,'$'
failread:
    db  "Read Operation Failed",13,10,'$'

altdta:
    times 128 db 0

""" % (len(testdata), "test", "fil"))

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Operation Failed", results)
    self.assertIn("Operation Success(%s)" % testdata, results)


def fcb_write(self, fstype):
    testdir = self.mkworkdir('d')

    if fstype == "MFS":
        ename = "mfsfcbwr"
        fcbreadconfig = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        ename = "fatfcbwr"
        fcbreadconfig = """\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % self.mkimage("12", cwd=testdir)

    testdata = self.mkstring(32)

    self.mkfile("testit.bat", """\
d:
c:\\%s
DIR
type test.fil
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

    mov     ax, 1600h           ; create file
    mov     dx, fcb
    int     21h
    cmp     al, 0
    jne     prfailopen

    mov     si, data            ; copy data to DTA
    mov     ax, 2f00h           ; get DTA address in ES:BX
    int     21h
    mov     di, bx
    mov     cx, datalen
    cld
    repnz movsb

    mov     ax, 1500h           ; write to file
    mov     dx, fcb
    mov     word [flrs], datalen; only the significant part
    int     21h
    cmp     al , 0
    jne     prfailwrite

    mov     dx, donewrite
    jmp     @2

prfailwrite:
    mov     dx, failwrite
    jmp     @2

prfailopen:
    mov     dx, failopen
    jmp     @1

@2:
    mov     ax, 1000h           ; close file
    push    dx
    mov     dx, fcb
    int     21h
    pop     dx

@1:
    mov     ah, 9
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

data:
    db  "Operation Success(%s)",13,10,'$'
datalen equ $ - data - 1

fcb:
    db  0          ; 0 default drive
fn1:
    db  "% -8s"    ; 8 bytes
fe1:
    db  "% -3s"    ; 3 bytes
fcbn:
    dw  0
flrs:
    dw  0
ffsz:
    dd  0
fdlw:
    dw  0
ftlw:
    dw  0
res8:
    times 8 db 0
fcbr:
    db  0
frrn:
    dd  0

failopen:
    db  "Open Operation Failed",13,10,'$'
failwrite:
    db  "Write Operation Failed",13,10,'$'
donewrite:
    db  "Write Operation Done",13,10,'$'

""" % (testdata, "test", "fil"))

    results = self.runDosemu("testit.bat", config=fcbreadconfig)

    self.assertNotIn("Operation Failed", results)
    self.assertIn("Operation Success(%s)" % testdata, results)


def fcb_rename_common(self, fstype, testname):
    testdir = self.mkworkdir('d')

    if testname == "simple":
        ename = "mfsfcbr1"
        fn1 = "testa"
        fe1 = "bat"
        fn2 = "testb"
        fe2 = "bal"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
    elif testname == "source_missing":
        ename = "mfsfcbr2"
        fn1 = "testa"
        fe1 = "bat"
        fn2 = "testb"
        fe2 = "bal"
    elif testname == "target_exists":
        ename = "mfsfcbr3"
        fn1 = "testa"
        fe1 = "bat"
        fn2 = "testb"
        fe2 = "bal"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
        self.mkfile(fn2 + "." + fe2, """hello\r\n""", testdir)
    elif testname == "wild_one":
        ename = "mfsfcbr4"
        fn1 = "*"
        fe1 = "in"
        fn2 = "*"
        fe2 = "out"
        for f in ["one.in", "two.in", "three.in", "four.in", "five.in",
                  "none.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_two":
        ename = "mfsfcbr5"
        fn1 = "a*"
        fe1 = "*"
        fn2 = "b*"
        fe2 = "out"
        for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                  "afive.in", "xnone.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_three":
        # To rename "abc001.txt ... abc099.txt" to "abc601.txt....abc699.txt"
        # REN abc0??.txt ???6*.*
        ename = "mfsfcbr6"
        fn1 = "abc0??"
        fe1 = "*"
        fn2 = "???6*"
        fe2 = "*"
        for f in ["abc001.txt", "abc002.txt", "abc003.txt", "abc004.txt",
                  "abc005.txt", "abc010.txt", "xbc007.txt"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_four":
        # To rename abc001.htm to abc001.ht
        # REN abc*.htm *.??
        ename = "mfsfcbr7"
        fn1 = "abc*"
        fe1 = "htm"
        fn2 = "*"
        fe2 = "??"
        for f in ["abc001.htm", "abc002.htm", "abc003.htm", "abc004.htm",
                  "abc005.htm", "abc010.htm", "xbc007.htm"]:
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

    mov     ax, 1700h
    mov     dx, fcb
    int     21h

    cmp     al, 0
    je      prsucc

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

fcb:
    db  0          ; 0 default drive
fn1:
    db  "% -8s"    ; 8 bytes
fe1:
    db  "% -3s"    ; 3 bytes
wk1:
    times 5 db 0
fn2:
    db  "% -8s"    ; 8 bytes
fe2:
    db  "% -3s"    ; 3 bytes
wk2:
    times 16 db 0

succmsg:
    db  "Rename Operation Success",13,10,'$'
failmsg:
    db  "Rename Operation Failed",13,10,'$'

""" % (fn1, fe1, fn2, fe2))

    def assertIsPresent(testdir, results, fstype, f, e, msg=None):
        if fstype == "MFS":
            self.assertTrue(exists(join(testdir, f + "." + e)), msg)
        else:
            self.assertRegex(results.upper(), r"%s( +|\.)%s" % (f.upper(), e.upper()))

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
        self.assertIn("Rename Operation Success", results)
        assertIsPresent(testdir, results, fstype, fn2, fe2, "File not renamed")

    elif testname == "source_missing":
        self.assertIn("Rename Operation Failed", results)

    elif testname == "target_exists":
        self.assertIn("Rename Operation Failed", results)

    elif testname == "wild_one":
        self.assertIn("Rename Operation Success", results)
        assertIsPresent(testdir, results, fstype, "one", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "two", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "three", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "four", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "five", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "none", "ctl", "File incorrectly renamed")

    elif testname == "wild_two":
        self.assertIn("Rename Operation Success", results)
        assertIsPresent(testdir, results, fstype, "bone", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "btwo", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "bthree", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "bfour", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "bfive", "out", "File not renamed")
        assertIsPresent(testdir, results, fstype, "xnone", "ctl", "File incorrectly renamed")

    elif testname == "wild_three":
        self.assertIn("Rename Operation Success", results)
        assertIsPresent(testdir, results, fstype, "abc601", "txt", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc602", "txt", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc603", "txt", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc604", "txt", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc605", "txt", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc610", "txt", "File not renamed")
        assertIsPresent(testdir, results, fstype, "xbc007", "txt", "File incorrectly renamed")

    elif testname == "wild_four":
        self.assertIn("Rename Operation Success", results)
        assertIsPresent(testdir, results, fstype, "abc001", "ht", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc002", "ht", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc003", "ht", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc004", "ht", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc005", "ht", "File not renamed")
        assertIsPresent(testdir, results, fstype, "abc010", "ht", "File not renamed")
        assertIsPresent(testdir, results, fstype, "xbc007", "htm", "File incorrectly renamed")


def fcb_delete_common(self, fstype, testname):
    testdir = self.mkworkdir('d')

    if testname == "simple":
        ename = "fcbdel1"
        fn1 = "testa"
        fe1 = "bat"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
    elif testname == "missing":
        ename = "fcbdel2"
        fn1 = "testa"
        fe1 = "bat"
    elif testname == "wild_one":
        ename = "fcbdel3"
        fn1 = "*"
        fe1 = "in"
        for f in ["one.in", "two.in", "three.in", "four.in", "five.in",
                  "none.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_two":
        ename = "fcbdel4"
        fn1 = "a*"
        fe1 = "*"
        for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                  "afive.in", "xnone.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_three":
        # To delete "abc001.txt ... abc099.txt"
        ename = "fcbdel5"
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

    mov     ax, 1300h
    mov     dx, fcb
    int     21h

    cmp     al, 0
    je      prsucc

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

fcb:
    db  0       ; 0 default drive
fn1:
    db  "% -8s"    ; 8 bytes
fe1:
    db  "% -3s"    ; 3 bytes
wk1:
    times 25 db 0

succmsg:
    db  "Delete Operation Success",13,10,'$'
failmsg:
    db  "Delete Operation Failed",13,10,'$'

""" % (fn1, fe1))

    def assertIsPresent(testdir, results, fstype, f, e, msg=None):
        if fstype == "MFS":
            self.assertTrue(exists(join(testdir, f + "." + e)), msg)
        else:
            self.assertRegex(results.upper(), r"%s( +|\.)%s" % (f.upper(), e.upper()))

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

    if testname == "simple":
        self.assertIn("Delete Operation Success", results)
        assertIsNotPresent(testdir, results, fstype, fn1, fe1, "File not deleted")

    elif testname == "missing":
        self.assertIn("Delete Operation Failed", results)

    elif testname == "wild_one":
        self.assertIn("Delete Operation Success", results)
        assertIsNotPresent(testdir, results, fstype, "one", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "two", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "three", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "four", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "five", "in", "File not deleted")
        assertIsPresent(testdir, results, fstype, "none", "ctl", "File incorrectly deleted")

    elif testname == "wild_two":
        self.assertIn("Delete Operation Success", results)
        assertIsNotPresent(testdir, results, fstype, "aone", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "atwo", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "athree", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "afour", "in", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "afive", "in", "File not deleted")
        assertIsPresent(testdir, results, fstype, "xnone", "ctl", "File incorrectly deleted")

    elif testname == "wild_three":
        self.assertIn("Delete Operation Success", results)
        assertIsNotPresent(testdir, results, fstype, "abc001", "txt", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "abc002", "txt", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "abc003", "txt", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "abc004", "txt", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "abc005", "txt", "File not deleted")
        assertIsNotPresent(testdir, results, fstype, "abc010", "txt", "File not deleted")
        assertIsPresent(testdir, results, fstype, "xbc007", "txt", "File incorrectly deleted")


def fcb_find_common(self, fstype, testname):
    testdir = self.mkworkdir('d')

    if testname == "simple":
        ename = "fcbfind1"
        fn1 = "testa"
        fe1 = "bat"
        self.mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
    elif testname == "missing":
        ename = "fcbfind2"
        fn1 = "testa"
        fe1 = "bat"
    elif testname == "wild_one":
        ename = "fcbfind3"
        fn1 = "*"
        fe1 = "in"
        for f in ["one.in", "two.in", "three.in", "four.in", "five.in",
                  "none.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_two":
        ename = "fcbfind4"
        fn1 = "a*"
        fe1 = "*"
        for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                  "afive.in", "xnone.ctl"]:
            self.mkfile(f, """hello\r\n""", testdir)
    elif testname == "wild_three":
        # To find "abc001.txt ... abc099.txt"
        ename = "fcbfind5"
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
    mov     ax, 1100h
    mov     dx, fcb
    int     21h

    cmp     al, 0
    je      prsucc

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
    lds     si, [pdta]
    inc     si

    push    cs
    pop     es
    mov     di, prires
    inc     di

    mov     cx, 11
    cld
    repne   movsb

    pop     ds
    mov     dx, prires
    mov     ah, 9
    int     21h

    ; FindNext
findnext:
    mov     ax, 1200h
    mov     dx, fcb
    int     21h

    cmp     al, 0
    je      prfilename

exit:
    mov     ah, 4ch
    int     21h

section .data

fcb:
    db  0       ; 0 default drive
fn1:
    db  "% -8s"    ; 8 bytes
fe1:
    db  "% -3s"    ; 3 bytes
wk1:
    times 25 db 0

pdta:
    dd   0

prires:
    db  "("
fname:
    times 8 db 20
fext:
    times 3 db 20
    db  ')',13,10,'$'

succmsg:
    db  "Find Operation Success",13,10,'$'
failmsg:
    db  "Find Operation Failed",13,10,'$'

""" % (fn1, fe1))

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
        self.assertIn("Find Operation Success", results)
        self.assertIn("(TESTA   BAT)", results)

    elif testname == "missing":
        self.assertIn("Find Operation Failed", results)

    elif testname == "wild_one":
        self.assertIn("Find Operation Success", results)
        self.assertIn("(ONE     IN )", results)
        self.assertIn("(TWO     IN )", results)
        self.assertIn("(THREE   IN )", results)
        self.assertIn("(FOUR    IN )", results)
        self.assertIn("(FIVE    IN )", results)
        self.assertNotIn("(NONE    CTL)", results)

    elif testname == "wild_two":
        self.assertIn("Find Operation Success", results)
        self.assertIn("(AONE    IN )", results)
        self.assertIn("(ATWO    IN )", results)
        self.assertIn("(ATHREE  IN )", results)
        self.assertIn("(AFOUR   IN )", results)
        self.assertIn("(AFIVE   IN )", results)
        self.assertNotIn("(XNONE   CTL)", results)

    elif testname == "wild_three":
        self.assertIn("Find Operation Success", results)
        self.assertIn("(ABC001  TXT)", results)
        self.assertIn("(ABC002  TXT)", results)
        self.assertIn("(ABC003  TXT)", results)
        self.assertIn("(ABC004  TXT)", results)
        self.assertIn("(ABC005  TXT)", results)
        self.assertIn("(ABC010  TXT)", results)
        self.assertNotIn("(XBC007  TXT)", results)
