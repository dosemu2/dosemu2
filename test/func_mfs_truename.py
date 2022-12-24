from os import makedirs
from os.path import join

from common_framework import (VFAT_MNTPNT,
                              setup_vfat_mounted_image, teardown_vfat_mounted_image)


def mfs_truename(self, fstype, tocreate, nametype, instring, expected):
    ename = "mfstruen"

    if fstype == "UFS":
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir, exist_ok=True)

        batchfile = """\
%s
rem end
""" % ename

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""

    elif fstype == "VFAT":
        testdir = VFAT_MNTPNT
        setup_vfat_mounted_image(self)

        batchfile = """\
lredir X: /mnt/dosemu
lredir
x:
c:\\%s
rem end
""" % ename

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "/mnt/dosemu"
"""

    else:
        self.fail("Incorrect argument")

# Make test files and directory names
    for i in tocreate:
        if i[0] == "FILE":
            with open(join(testdir, i[1]), "w") as f:
                f.write("Some data")
        elif i[0] == "DIR":
            makedirs(join(testdir, i[1]), exist_ok=True)

    if nametype == "LFN0":
        intnum = "0x7160"
        qtype = "0"
    elif nametype == "LFN1":
        intnum = "0x7160"
        qtype = "1"
    elif nametype == "LFN2":
        intnum = "0x7160"
        qtype = "2"
    elif nametype == "SFN":
        intnum = "0x6000"
        qtype = "0"
    else:
        self.fail("Incorrect argument")

    # common
    self.mkfile("testit.bat", batchfile, newline="\r\n")

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

    mov     ax, %s
    mov     cx, %s
    mov     si, src
    mov     di, dst
    int     21h

    mov     cx, 128
    mov     al, 0
    cld
    repne   scasb
    mov     byte [di-1], ')'
    mov     byte [di], '$'

    jnc     prsucc

prfail:
    mov     ah, 9
    mov     dx, failmsg
    int     21h

    jmp     exit

prsucc:
    mov     ah, 9
    mov     dx, succmsg
    int     21h

prresult:
    mov     ah, 9
    mov     dx, pdst
    int     21h

exit:
    mov     ah, 4Ch
    int     21h

section .data

src:
    db "%s", 0

succmsg:
    db  "Directory Operation Success",13,10,'$'
failmsg:
    db  "Directory Operation Failed",13,10,'$'

pdst:
    db '('
dst:
    times 128 db '$'

""" % (intnum, qtype, instring))

    results = self.runDosemu("testit.bat", config=config)

    if fstype == "VFAT":
        teardown_vfat_mounted_image(self)
        self.assertRegex(results, r"X: = .*LINUX\\FS/mnt/dosemu")

    if expected is None:
        self.assertIn("Directory Operation Failed", results)
    else:
        self.assertIn("Directory Operation Success", results)
        self.assertIn("(" + expected + ")", results)
