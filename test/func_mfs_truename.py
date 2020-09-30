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
    self.mkcom_with_gas(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds
    push    %%cs
    pop     %%es

    movw    $%s, %%ax
    movw    $%s, %%cx
    movw    $src, %%si
    movw    $dst, %%di
    int     $0x21

    movw    $128, %%cx
    movb    $0, %%al
    cld
    repne   scasb
    movb    $')', -1(%%di)
    movb    $'$', (%%di)

    jnc     prsucc

prfail:
    movw    $failmsg, %%dx
    movb    $0x9, %%ah
    int     $0x21

    jmp     exit

prsucc:
    movw    $succmsg, %%dx
    movb    $0x9, %%ah
    int     $0x21

prresult:
    movb    $0x9, %%ah
    movw    $pdst, %%dx
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

src:
    .asciz  "%s"

succmsg:
    .ascii  "Directory Operation Success\r\n$"
failmsg:
    .ascii  "Directory Operation Failed\r\n$"

pdst:
    .byte '('
dst:
    .fill 128, 1, '$'

""" % (intnum, qtype, instring.replace("\\", "\\\\")))

    results = self.runDosemu("testit.bat", config=config)

    if fstype == "VFAT":
        teardown_vfat_mounted_image(self)
        self.assertRegex(results, r"X: = .*LINUX\\FS/mnt/dosemu")

    if expected is None:
        self.assertIn("Directory Operation Failed", results)
    else:
        self.assertIn("Directory Operation Success", results)
        self.assertIn("(" + expected + ")", results)
