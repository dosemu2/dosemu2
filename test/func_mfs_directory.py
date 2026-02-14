
PRGFIL_SFN = "PROGR~-I"
PRGFIL_LFN = "Program Files"


def mfs_directory_common(self, nametype, operation):
    if nametype == "LFN":
        ename = "mfslfn"
        testname = "test very long directory"
    elif nametype == "SFN":
        ename = "mfssfn"
        testname = "testdir"
    else:
        raise ValueError("Incorrect argument")

    testdir = self.mkworkdir('d')

    cwdnum = "0x0"

    if operation == "Create":
        ename += "dc"
        if nametype == "SFN":
            intnum = "0x3900"  # create
        else:
            intnum = "0x7139"
    elif operation in ["Delete", "DeleteNotEmpty"]:
        ename += "dd"
        if nametype == "SFN":
            intnum = "0x3a00"  # delete
        else:
            intnum = "0x713a"
        (testdir / testname).mkdir()
        if operation == "DeleteNotEmpty":
            self.mkfile("DirNotEm.pty", """hello\r\n""", testdir / testname)
    elif operation == "Chdir":
        ename += "dh"
        if nametype == "SFN":
            intnum = "0x3b00"  # chdir
            cwdnum = "0x4700"  # getcwd
        else:
            intnum = "0x713b"
            cwdnum = "0x7147"
        (testdir / testname).mkdir()
    else:
        raise ValueError("Incorrect argument")

    self.mkfile("testit.bat", """\
d:
c:\\%s
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

mov     ax, %s
mov     dx, dname
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

mov     ax, %s
cmp     ax, 0x7147
je      prcwd
cmp     ax, 0x4700
je      prcwd

exit:
mov     ah, 4ch
int     21h

prcwd:
; get cwd
mov     dl, 0
mov     si, curdir
int     21h

push    ds
pop     es
mov     di, si

mov     cx, 128
mov     al, 0
cld
repne   scasb
mov     byte [di-1], ')'
mov     byte [di], '$'

mov     ah, 9
mov     dx, pcurdir
int     21h

jmp     exit

section .data

dname:
db  "%s",0

succmsg:
db  "Directory Operation Success",13,10,'$'
failmsg:
db  "Directory Operation Failed",13,10,'$'

pcurdir:
db '('
curdir:
times 128 db '$'

""" % (intnum, cwdnum, testname))

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

    name = testdir / testname

    # test to see if the directory intnum made it through to linux
    if operation == "Create":
        self.assertIn("Directory Operation Success", results)
        self.assertTrue(name.is_dir(), "Directory not created")
    elif operation == "Delete":
        self.assertIn("Directory Operation Success", results)
        self.assertFalse(name.is_dir(), "Directory not deleted")
    elif operation == "DeleteNotEmpty":
        self.assertIn("Directory Operation Failed", results)
        self.assertTrue(name.is_dir(), "Directory incorrectly deleted")
    elif operation == "Chdir":
        self.assertIn("Directory Operation Success", results)
        if nametype == "SFN":
            self.assertIn("(" + testname.upper() + ")", results)
        else:
            self.assertIn("(" + testname + ")", results)


def mfs_get_current_directory(self, nametype):
    if nametype == "LFN":
        ename = "mfslfngd"
        testname = PRGFIL_LFN
    elif nametype == "SFN":
        ename = "mfssfngd"
        testname = PRGFIL_SFN
    else:
        raise ValueError("Incorrect argument")

    testdir = self.mkworkdir('d')

    if nametype == "SFN":
        cwdnum = "0x4700"  # getcwd
    else:
        cwdnum = "0x7147"

    (testdir / PRGFIL_LFN).mkdir()

    self.mkfile("testit.bat", """\
d:
cd %s
c:\\%s
rem end
""" % (PRGFIL_SFN, ename), newline="\r\n")

    # compile sources
    self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

push    cs
pop     ds

; get cwd
mov     ax, %s
mov     dl, 0
mov     si, curdir
int     21h

push    ds
pop     es
mov     di, si

mov     cx, 128
mov     al, 0
cld
repne   scasb
mov     byte [di-1], ')'
mov     byte [di], '$'

mov     ah, 9
mov     dx, pcurdir
int     21h

exit:
mov     ah, 4ch
int     21h

section .data

pcurdir:
db '('
curdir:
times 128 db '$'

""" % cwdnum)

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

    if nametype == "SFN":
        self.assertIn("(" + testname.upper() + ")", results)
    else:
        self.assertIn("(" + testname + ")", results)

