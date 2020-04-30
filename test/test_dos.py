import unittest

import re

from datetime import datetime
from glob import glob
from os import (makedirs, statvfs, listdir, symlink, uname, remove,
                utime, access, R_OK, W_OK)
from os.path import exists, isdir, join
from time import mktime

from common_framework import (MyTestRunner, BaseTestCase,
                              mkfile, mkexe, mkcom, mkstring,
                              SKIP, KNOWNFAIL, UNSUPPORTED)

SYSTYPE_DRDOS_ENHANCED = "Enhanced DR-DOS"
SYSTYPE_DRDOS_ORIGINAL = "Original DR-DOS"
SYSTYPE_DRDOS_OLD = "Old DR-DOS"
SYSTYPE_PCDOS_NEW = "New PC-DOS"
SYSTYPE_PCDOS_OLD = "Old PC-DOS"
SYSTYPE_MSDOS_NEW = "New MS-DOS"
SYSTYPE_MSDOS_INTERMEDIATE = "Newer MS-DOS"
SYSTYPE_MSDOS_OLD = "Old MS-DOS"
SYSTYPE_MSDOS_NEC = "NEC MS-DOS"
SYSTYPE_FRDOS_OLD = "Old FreeDOS"
SYSTYPE_FRDOS_NEW = "FreeDOS"
SYSTYPE_FDPP = "FDPP"

PRGFIL_SFN = "PROGR~-I"
PRGFIL_LFN = "Program Files"


class OurTestCase(BaseTestCase):

    pname = "test_dos"

    # Tests using assembler

    def _test_mfs_directory_common(self, nametype, operation):
        if nametype == "LFN":
            ename = "mfslfn"
            testname = "test very long directory"
        elif nametype == "SFN":
            ename = "mfssfn"
            testname = "testdir"
        else:
            self.fail("Incorrect argument")

        testdir = "test-imagedir/dXXXXs/d"

        cwdnum = "0x0"

        if operation == "Create":
            ename += "dc"
            if nametype == "SFN":
                intnum = "0x3900"  # create
            else:
                intnum = "0x7139"
            makedirs(testdir)
        elif operation in ["Delete", "DeleteNotEmpty"]:
            ename += "dd"
            if nametype == "SFN":
                intnum = "0x3a00"  # delete
            else:
                intnum = "0x713a"
            makedirs(join(testdir, testname))
            if operation == "DeleteNotEmpty":
                mkfile("DirNotEm.pty", """hello\r\n""", join(testdir, testname))
        elif operation == "Chdir":
            ename += "dh"
            if nametype == "SFN":
                intnum = "0x3b00"  # chdir
                cwdnum = "0x4700"  # getcwd
            else:
                intnum = "0x713b"
                cwdnum = "0x7147"
            makedirs(join(testdir, testname))
        else:
            self.fail("Incorrect argument")

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $%s, %%ax
    movw    $dname, %%dx
    int     $0x21

    jnc     prsucc

prfail:
    movw    $failmsg, %%dx
    jmp     1f
prsucc:
    movw    $succmsg, %%dx
1:
    movb    $0x9, %%ah
    int     $0x21

    movw    $%s, %%ax
    cmpw    $0x7147, %%ax
    je      prcwd
    cmpw    $0x4700, %%ax
    je      prcwd

exit:
    movb    $0x4c, %%ah
    int     $0x21

prcwd:
# get cwd
    movb    $0, %%dl
    movw    $curdir, %%si
    int     $0x21

    push    %%ds
    pop     %%es
    movw    %%si, %%di

    movw    $128, %%cx
    movb    $0, %%al
    cld
    repne   scasb
    movb    $')', -1(%%di)
    movb    $'$', (%%di)

    movb    $0x9, %%ah
    movw    $pcurdir, %%dx
    int     $0x21

    jmp     exit

dname:
    .asciz  "%s"

succmsg:
    .ascii  "Directory Operation Success\r\n$"
failmsg:
    .ascii  "Directory Operation Failed\r\n$"

pcurdir:
    .byte '('
curdir:
    .fill 128, 1, '$'

""" % (intnum, cwdnum, testname))

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        # test to see if the directory intnum made it through to linux
        if operation == "Create":
            self.assertIn("Directory Operation Success", results)
            self.assertTrue(isdir(join(testdir, testname)), "Directory not created")
        elif operation == "Delete":
            self.assertIn("Directory Operation Success", results)
            self.assertFalse(isdir(join(testdir, testname)), "Directory not deleted")
        elif operation == "DeleteNotEmpty":
            self.assertIn("Directory Operation Failed", results)
            self.assertTrue(isdir(join(testdir, testname)), "Directory incorrectly deleted")
        elif operation == "Chdir":
            self.assertIn("Directory Operation Success", results)
            if nametype == "SFN":
                self.assertIn("(" + testname.upper() + ")", results)
            else:
                self.assertIn("(" + testname + ")", results)

    def test_mfs_sfn_directory_create(self):
        """MFS SFN directory create"""
        self._test_mfs_directory_common("SFN", "Create")

    def test_mfs_sfn_directory_delete(self):
        """MFS SFN directory delete"""
        self._test_mfs_directory_common("SFN", "Delete")

    def test_mfs_sfn_directory_delete_not_empty(self):
        """MFS SFN directory delete not empty"""
        self._test_mfs_directory_common("SFN", "DeleteNotEmpty")

    def test_mfs_sfn_directory_chdir(self):
        """MFS SFN directory change current"""
        self._test_mfs_directory_common("SFN", "Chdir")

    def test_mfs_lfn_directory_create(self):
        """MFS LFN directory create"""
        self._test_mfs_directory_common("LFN", "Create")

    def test_mfs_lfn_directory_delete(self):
        """MFS LFN directory delete"""
        self._test_mfs_directory_common("LFN", "Delete")

    def test_mfs_lfn_directory_delete_not_empty(self):
        """MFS LFN directory delete not empty"""
        self._test_mfs_directory_common("LFN", "DeleteNotEmpty")

    def test_mfs_lfn_directory_chdir(self):
        """MFS LFN directory change current"""
        self._test_mfs_directory_common("LFN", "Chdir")

    def _test_mfs_get_current_directory(self, nametype):
        if nametype == "LFN":
            ename = "mfslfngd"
            testname = PRGFIL_LFN
        elif nametype == "SFN":
            ename = "mfssfngd"
            testname = PRGFIL_SFN
        else:
            self.fail("Incorrect argument")

        testdir = "test-imagedir/dXXXXs/d"

        if nametype == "SFN":
            cwdnum = "0x4700"  # getcwd
        else:
            cwdnum = "0x7147"

        makedirs(join(testdir, PRGFIL_LFN))

        mkfile("testit.bat", """\
d:\r
cd %s\r
c:\\%s\r
rem end\r
""" % (PRGFIL_SFN, ename))

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

# get cwd
    movw    $%s, %%ax
    movb    $0, %%dl
    movw    $curdir, %%si
    int     $0x21

    push    %%ds
    pop     %%es
    movw    %%si, %%di

    movw    $128, %%cx
    movb    $0, %%al
    cld
    repne   scasb
    movb    $')', -1(%%di)
    movb    $'$', (%%di)

    movb    $0x9, %%ah
    movw    $pcurdir, %%dx
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

pcurdir:
    .byte '('
curdir:
    .fill 128, 1, '$'

""" % cwdnum)

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

            if nametype == "SFN":
                self.assertIn("(" + testname.upper() + ")", results)
            else:
                self.assertIn("(" + testname + ")", results)

    def test_mfs_sfn_get_current_directory(self):
        """MFS SFN get current directory"""
        self._test_mfs_get_current_directory("SFN")

    def test_mfs_lfn_get_current_directory(self):
        """MFS LFN get current directory"""
        self._test_mfs_get_current_directory("LFN")

    def _test_lfn_support(self, fstype, confsw):
        ename = "lfnsuppt"
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

        if fstype == "MFS":
            config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
        else:       # FAT
            config = """\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        config += """$_lfn_support = (%s)\n""" % confsw

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
rem end\r
""" % ename)

        # compile sources

        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %cs
    pop     %ds

# Get current drive and store its letter in fspath
    movw    $0x1900, %ax
    int     $0x21
    addb    $'A', %al
    movb    %al, fspath

# Get Volume info
#    Windows95 - LONG FILENAME - GET VOLUME INFORMATION
#
#    Call:
#      AX = 71A0h
#      DS:DX -> ASCIZ root name (e.g. "C:\")
#      ES:DI -> buffer for file system name
#      CX = size of ES:DI buffer
#
#    Return:
#      CF clear if successful
#        AX destroyed (0000h and 0200h seen)
#        BX = file system flags (see #01783)
#        CX = maximum length of file name [usually 255]
#        DX = maximum length of path [usually 260]
#        ES:DI buffer filled (ASCIZ, e.g. "FAT","NTFS","CDFS")
#
#      CF set on error
#        AX = error code
#          7100h if function not supported

    movw    $0x71a0, %ax
    movw    $fspath, %dx  # ds:dx
    movw    $fstype, %di  # es:di
    movw    $fstypelen, %cx
    stc
    int     $0x21

    jc      chkfail

    cmpb    $'$', fstype
    je      prnofstype

prsuccess:
    movw    $fstype, %di
    movw    fstypelen, %cx
    movb    $0, %al
    cld
    repne   scasb
    movb    $')', -1(%di)
    movb    $'\r',  (%di)
    movb    $'\n', 1(%di)
    movb    $'$',  2(%di)
    movw    $success, %dx
    jmp     exit

prnofstype:
    movw    $nofstype, %dx
    jmp     exit

prnotsupported:
    movw    $notsupported, %dx
    jmp     exit

prcarryset:
    movw    $carryset, %dx
    jmp     exit

chkfail:
    cmpw    $0x7100, %ax
    jne     prcarryset

    jmp     prnotsupported

exit:
    movb    $0x9, %ah
    int     $0x21

    movb    $0x4c, %ah
    int $0x21

carryset:
    .ascii  "Carry Set\r\n$"
notsupported:
    .ascii  "Not Supported(AX=0x7100)\r\n$"
nofstype:
    .ascii  "Carry Not Set But No Filesystem Type\r\n$"
success:
    .ascii  "Operation Success("
fstype:
    .fill 32, 1, '$'
fstypelen = (. - fstype)
successend:
    .space 4
fspath:
    .asciz "?:\\"

""")

        results = self.runDosemu("testit.bat", config=config)

        if fstype == "MFS":
            if confsw == "on":
                self.assertIn("Operation Success(%s)" % fstype, results)
            else:
                self.assertIn("Not Supported(AX=0x7100)", results)
        else:    # FAT
                self.assertIn("Not Supported(AX=0x7100)", results)

    def test_lfn_mfs_support_on(self):
        """LFN MFS Support On"""
        self._test_lfn_support("MFS", "on")

    def test_lfn_fat_support_on(self):
        """LFN FAT Support On"""
        self._test_lfn_support("FAT", "on")

    def test_lfn_mfs_support_off(self):
        """LFN MFS Support Off"""
        self._test_lfn_support("MFS", "off")

    def test_lfn_fat_support_off(self):
        """LFN FAT Support Off"""
        self._test_lfn_support("FAT", "off")

    def _test_mfs_truename(self, nametype, instring, expected):
        if nametype == "LFN0":
            ename = "mfslfntn"
            intnum = "0x7160"
            qtype = "0"
        elif nametype == "LFN1":
            ename = "mfslfntn"
            intnum = "0x7160"
            qtype = "1"
        elif nametype == "LFN2":
            ename = "mfslfntn"
            intnum = "0x7160"
            qtype = "2"
        elif nametype == "SFN":
            ename = "mfssfntn"
            intnum = "0x6000"
            qtype = "0"
        else:
            self.fail("Incorrect argument")

        testdir = "test-imagedir/dXXXXs/d"
        if not exists(testdir):
            makedirs(testdir)
            mkfile("shrtname.txt", """hello\r\n""", testdir)
            mkfile("long file name.txt", """hello\r\n""", testdir)
        if not exists(join(testdir, PRGFIL_LFN)):
            makedirs(join(testdir, PRGFIL_LFN))

        mkfile("testit.bat", """\
%s\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
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

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        if expected is None:
            self.assertIn("Directory Operation Failed", results)
        else:
            self.assertIn("Directory Operation Success", results)
            self.assertIn("(" + expected + ")", results)

    def test_mfs_sfn_truename(self):
        """MFS SFN Truename"""
        tests = (
            ("SFN", "testname", "C:\\TESTNAME"),
            ("SFN", "d:\\shrtname.txt", "D:\\SHRTNAME.TXT"),
            ("SFN", "d:\\testname", "D:\\TESTNAME"),
            ("SFN", "aux", "C:/AUX"),
            ("SFN", "d:testname", "D:\\TESTNAME"),
# FAIL            ("SFN", "d:\\fakedir\\testname", None),
        )
        for t in tests:
            with self.subTest(t=t):
                self._test_mfs_truename(*t)

    def test_mfs_lfn_truename(self):
        """MFS LFN Truename"""
        tests = (
# NOTE: not sure that the output should be UPCASED.
            ("LFN0", "very long testname", "C:\\VERY LONG TESTNAME"),
            ("LFN0", "d:\\verylongtestname.txt", "D:\\VERYLONGTESTNAME.TXT"),
            ("LFN0", "d:\\very long testname", "D:\\VERY LONG TESTNAME"),
            ("LFN0", "aux", "C:/AUX"),
            ("LFN0", "d:very long testname", "D:\\VERY LONG TESTNAME"),
# FAIL             ("LFN0", "d:\\fakedir\\very long testname", None),

            ("LFN0", "D:\\" + PRGFIL_SFN, "D:\\" + PRGFIL_SFN),
            ("LFN1", "D:\\" + PRGFIL_SFN, "D:\\" + PRGFIL_SFN),
            ("LFN2", "D:\\" + PRGFIL_SFN, "D:\\" + PRGFIL_LFN),
        )
        for t in tests:
            with self.subTest(t=t):
                self._test_mfs_truename(*t)

    def _test_fcb_read(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

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
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        testdata = mkstring(32)

        mkfile("testit.bat", """\
d:\r
echo %s > test.fil\r
c:\\%s\r
DIR\r
rem end\r
""" % (testdata, ename))

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds


    movw    $0x0f00, %%ax			# open file
    movw    $fcb, %%dx
    int     $0x21
    cmpb    $0, %%al
    jne     prfailopen

    movw    $0x1400, %%ax			# read from file
    movw    $fcb, %%dx
    int     $0x21
    cmpb    $03, %%al				# partial read
    jne     prfailread

    jmp     prsucc

prfailopen:
    movw    $failopen, %%dx
    jmp     1f

prfailread:
    movw    $0x1000, %%ax			# close file
    movw    $fcb, %%dx
    int     $0x21
    movw    $failread, %%dx
    jmp     1f

prsucc:
    movw    $succstart, %%dx
    movb    $0x9, %%ah
    int     $0x21

    movw    $0x2f00, %%ax			# get DTA address in ES:BX
    int     $0x21

    movb    $'$', %%es:%d(%%bx)		# terminate
    push    %%es
    pop     %%ds
    movw    %%bx, %%dx
    movb    $0x9, %%ah
    int     $0x21

    movw    $0x1000, %%ax			# close file
    movw    $fcb, %%dx
    int     $0x21

    push    %%cs
    pop     %%ds
    movw    $succend, %%dx

1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

fcb:
    .byte   0          # 0 default drive
fn1:
    .ascii  "% -8s"    # 8 bytes
fe1:
    .ascii  "% -3s"    # 3 bytes
wk1:
    .space  24

succstart:
    .ascii  "Operation Success($"
succend:
    .ascii  ")\r\n$"
failopen:
    .ascii  "Open Operation Failed\r\n$"
failread:
    .ascii  "Read Operation Failed\r\n$"

""" % (len(testdata), "test", "fil"))

        results = self.runDosemu("testit.bat", config=fcbreadconfig)

        self.assertNotIn("Operation Failed", results)
        self.assertIn("Operation Success(%s)" % testdata, results)

    def test_fat_fcb_read(self):
        """FAT FCB file read simple"""
        self._test_fcb_read("FAT")

    def test_mfs_fcb_read(self):
        """MFS FCB file read simple"""
        self._test_fcb_read("MFS")

    def _test_fcb_read_alt_dta(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

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
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        testdata = mkstring(32)

        mkfile("testit.bat", """\
d:\r
echo %s > test.fil\r
c:\\%s\r
DIR\r
rem end\r
""" % (testdata, ename))

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x1a00, %%ax			# set DTA
    movw    $altdta, %%dx
    int     $0x21

    movw    $0x2f00, %%ax			# get DTA address in ES:BX
    int     $0x21
    movw    %%cs, %%ax
    movw    %%es, %%dx
    cmpw    %%ax, %%dx
    jne     prfaildtaset
    cmpw    $altdta, %%bx
    jne     prfaildtaset

    movw    $0x0f00, %%ax			# open file
    movw    $fcb, %%dx
    int     $0x21
    cmpb    $0, %%al
    jne     prfailopen

    movw    $0x1400, %%ax			# read from file
    movw    $fcb, %%dx
    int     $0x21
    cmpb    $03, %%al				# partial read
    jne     prfailread

    jmp     prsucc

prfaildtaset:
    movw    $faildtaset, %%dx
    jmp     1f

prfailopen:
    movw    $failopen, %%dx
    jmp     1f

prfailread:
    movw    $0x1000, %%ax			# close file
    movw    $fcb, %%dx
    int     $0x21
    movw    $failread, %%dx
    jmp     1f

prsucc:
    movw    $succstart, %%dx
    movb    $0x9, %%ah
    int     $0x21

    movw    $0x2f00, %%ax			# get DTA address in ES:BX
    int     $0x21

    movb    $'$', %%es:%d(%%bx)		# terminate
    push    %%es
    pop     %%ds
    movw    %%bx, %%dx
    movb    $0x9, %%ah
    int     $0x21

    movw    $0x1000, %%ax			# close file
    movw    $fcb, %%dx
    int     $0x21

    push    %%cs
    pop     %%ds
    movw    $succend, %%dx

1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

fcb:
    .byte   0          # 0 default drive
fn1:
    .ascii  "% -8s"    # 8 bytes
fe1:
    .ascii  "% -3s"    # 3 bytes
wk1:
    .space  24

succstart:
    .ascii  "Operation Success($"
succend:
    .ascii  ")\r\n$"
faildtaset:
    .ascii  "Set DTA Operation Failed\r\n$"
failopen:
    .ascii  "Open Operation Failed\r\n$"
failread:
    .ascii  "Read Operation Failed\r\n$"

altdta:
    .space  128

""" % (len(testdata), "test", "fil"))

        results = self.runDosemu("testit.bat", config=config)

        self.assertNotIn("Operation Failed", results)
        self.assertIn("Operation Success(%s)" % testdata, results)

    def test_fat_fcb_read_alt_dta(self):
        """FAT FCB file read alternate DTA"""
        self._test_fcb_read_alt_dta("FAT")

    def test_mfs_fcb_read_alt_dta(self):
        """MFS FCB file read alternate DTA"""
        self._test_fcb_read_alt_dta("MFS")

    def _test_fcb_write(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

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
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        testdata = mkstring(32)

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
type test.fil\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push %%cs
    popw %%ds

    movw    $0x1600, %%ax           # create file
    movw    $fcb, %%dx
    int     $0x21
    cmpb    $0, %%al
    jne     prfailopen

    movw    $data, %%si             # copy data to DTA
    movw    $0x2f00, %%ax           # get DTA address in ES:BX
    int     $0x21
    movw    %%bx, %%di
    movw    $DATALEN, %%cx
    cld
    repnz movsb

    movw    $0x1500, %%ax           # write to file
    movw    $fcb, %%dx
    movw    $DATALEN, flrs          # only the significant part
    int     $0x21
    cmpb    $0, %%al
    jne     prfailwrite

    movw    $donewrite, %%dx
    jmp     2f

prfailwrite:
    movw    $failwrite, %%dx
    jmp     2f

prfailopen:
    movw    $failopen, %%dx
    jmp     1f

2:
    movw    $0x1000, %%ax           # close file
    push    %%dx
    movw    $fcb, %%dx
    int     $0x21
    popw    %%dx

1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

data:
    .ascii  "Operation Success(%s)\r\n"
dend:
DATALEN = (dend - data)
    .ascii  "$"   # for printing

fcb:
    .byte   0          # 0 default drive
fn1:
    .ascii  "% -8s"    # 8 bytes
fe1:
    .ascii  "% -3s"    # 3 bytes
fcbn:
    .word 0
flrs:
    .word 0
ffsz:
    .long 0
fdlw:
    .word 0
ftlw:
    .word 0
res8:
    .space 8
fcbr:
    .byte 0
frrn:
    .long 0

failopen:
    .ascii  "Open Operation Failed\r\n$"
failwrite:
    .ascii  "Write Operation Failed\r\n$"
donewrite:
    .ascii  "Write Operation Done\r\n$"

""" % (testdata, "test", "fil"))

        results = self.runDosemu("testit.bat", config=fcbreadconfig)

        self.assertNotIn("Operation Failed", results)
        self.assertIn("Operation Success(%s)" % testdata, results)

    def test_fat_fcb_write(self):
        """FAT FCB file write simple"""
        self._test_fcb_write("FAT")

    def test_mfs_fcb_write(self):
        """MFS FCB file write simple"""
        self._test_fcb_write("MFS")

    def _test_fcb_rename_common(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        if testname == "simple":
            ename = "mfsfcbr1"
            fn1 = "testa"
            fe1 = "bat"
            fn2 = "testb"
            fe2 = "bal"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
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
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
            mkfile(fn2 + "." + fe2, """hello\r\n""", testdir)
        elif testname == "wild_one":
            ename = "mfsfcbr4"
            fn1 = "*"
            fe1 = "in"
            fn2 = "*"
            fe2 = "out"
            for f in ["one.in", "two.in", "three.in", "four.in", "five.in",
                      "none.ctl"]:
                mkfile(f, """hello\r\n""", testdir)
        elif testname == "wild_two":
            ename = "mfsfcbr5"
            fn1 = "a*"
            fe1 = "*"
            fn2 = "b*"
            fe2 = "out"
            for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                      "afive.in", "xnone.ctl"]:
                mkfile(f, """hello\r\n""", testdir)
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
                mkfile(f, """hello\r\n""", testdir)
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
                mkfile(f, """hello\r\n""", testdir)

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x1700, %%ax
    movw    $fcb, %%dx
    int     $0x21

    cmpb    $0, %%al
    je      prsucc

prfail:
    movw    $failmsg, %%dx
    jmp     1f
prsucc:
    movw    $succmsg, %%dx
1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

fcb:
    .byte   0       # 0 default drive
fn1:
    .ascii  "% -8s"    # 8 bytes
fe1:
    .ascii  "% -3s"    # 3 bytes
wk1:
    .space  5
fn2:
    .ascii  "% -8s"    # 8 bytes
fe2:
    .ascii  "% -3s"    # 3 bytes
wk2:
    .space  16

succmsg:
    .ascii  "Rename Operation Success$"
failmsg:
    .ascii  "Rename Operation Failed$"

""" % (fn1, fe1, fn2, fe2))

        def assertIsPresent(testdir, results, fstype, f, e, msg=None):
            if fstype == "MFS":
                self.assertTrue(exists(join(testdir, f + "." + e)), msg)
            else:
                self.assertRegex(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()))

        if fstype == "MFS":
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
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

    def test_fat_fcb_rename_simple(self):
        """FAT FCB file rename simple"""
        self._test_fcb_rename_common("FAT", "simple")

    def test_mfs_fcb_rename_simple(self):
        """MFS FCB file rename simple"""
        self._test_fcb_rename_common("MFS", "simple")

    def test_fat_fcb_rename_source_missing(self):
        """FAT FCB file rename source missing"""
        self._test_fcb_rename_common("FAT", "source_missing")

    def test_mfs_fcb_rename_source_missing(self):
        """MFS FCB file rename source missing"""
        self._test_fcb_rename_common("MFS", "source_missing")

    def test_fat_fcb_rename_target_exists(self):
        """FAT FCB file rename target exists"""
        self._test_fcb_rename_common("FAT", "target_exists")

    def test_mfs_fcb_rename_target_exists(self):
        """MFS FCB file rename target exists"""
        self._test_fcb_rename_common("MFS", "target_exists")

    def test_fat_fcb_rename_wild_1(self):
        """FAT FCB file rename wildcard one"""
        self._test_fcb_rename_common("FAT", "wild_one")

    def test_mfs_fcb_rename_wild_1(self):
        """MFS FCB file rename wildcard one"""
        self._test_fcb_rename_common("MFS", "wild_one")

    def test_fat_fcb_rename_wild_2(self):
        """FAT FCB file rename wildcard two"""
        self._test_fcb_rename_common("FAT", "wild_two")

    def test_mfs_fcb_rename_wild_2(self):
        """MFS FCB file rename wildcard two"""
        self._test_fcb_rename_common("MFS", "wild_two")

    def test_fat_fcb_rename_wild_3(self):
        """FAT FCB file rename wildcard three"""
        self._test_fcb_rename_common("FAT", "wild_three")

    def test_mfs_fcb_rename_wild_3(self):
        """MFS FCB file rename wildcard three"""
        self._test_fcb_rename_common("MFS", "wild_three")

    def test_fat_fcb_rename_wild_4(self):
        """FAT FCB file rename wildcard four"""
        self._test_fcb_rename_common("FAT", "wild_four")

    def test_mfs_fcb_rename_wild_4(self):
        """MFS FCB file rename wildcard four"""
        self._test_fcb_rename_common("MFS", "wild_four")

    def _test_fcb_delete_common(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        if testname == "simple":
            ename = "fcbdel1"
            fn1 = "testa"
            fe1 = "bat"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
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
                mkfile(f, """hello\r\n""", testdir)
        elif testname == "wild_two":
            ename = "fcbdel4"
            fn1 = "a*"
            fe1 = "*"
            for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                      "afive.in", "xnone.ctl"]:
                mkfile(f, """hello\r\n""", testdir)
        elif testname == "wild_three":
            # To delete "abc001.txt ... abc099.txt"
            ename = "fcbdel5"
            fn1 = "abc0??"
            fe1 = "*"
            for f in ["abc001.txt", "abc002.txt", "abc003.txt", "abc004.txt",
                      "abc005.txt", "abc010.txt", "xbc007.txt"]:
                mkfile(f, """hello\r\n""", testdir)

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x1300, %%ax
    movw    $fcb, %%dx
    int     $0x21

    cmpb    $0, %%al
    je      prsucc

prfail:
    movw    $failmsg, %%dx
    jmp     1f
prsucc:
    movw    $succmsg, %%dx
1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

fcb:
    .byte   0       # 0 default drive
fn1:
    .ascii  "% -8s"    # 8 bytes
fe1:
    .ascii  "% -3s"    # 3 bytes
wk1:
    .space  25

succmsg:
    .ascii  "Delete Operation Success$"
failmsg:
    .ascii  "Delete Operation Failed$"

""" % (fn1, fe1))

        def assertIsPresent(testdir, results, fstype, f, e, msg=None):
            if fstype == "MFS":
                self.assertTrue(exists(join(testdir, f + "." + e)), msg)
            else:
                self.assertRegex(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()))

        def assertIsNotPresent(testdir, results, fstype, f, e, msg=None):
            if fstype == "MFS":
                self.assertFalse(exists(join(testdir, f + "." + e)), msg)
            else:
                self.assertNotRegex(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()))

        if fstype == "MFS":
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
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

    def test_fat_fcb_delete_simple(self):
        """FAT FCB file delete simple"""
        self._test_fcb_delete_common("FAT", "simple")

    def test_mfs_fcb_delete_simple(self):
        """MFS FCB file delete simple"""
        self._test_fcb_delete_common("MFS", "simple")

    def test_fat_fcb_delete_missing(self):
        """FAT FCB file delete missing"""
        self._test_fcb_delete_common("FAT", "missing")

    def test_mfs_fcb_delete_missing(self):
        """MFS FCB file delete missing"""
        self._test_fcb_delete_common("MFS", "missing")

    def test_fat_fcb_delete_wild_1(self):
        """FAT FCB file delete wildcard one"""
        self._test_fcb_delete_common("FAT", "wild_one")

    def test_mfs_fcb_delete_wild_1(self):
        """MFS FCB file delete wildcard one"""
        self._test_fcb_delete_common("MFS", "wild_one")

    def test_fat_fcb_delete_wild_2(self):
        """FAT FCB file delete wildcard two"""
        self._test_fcb_delete_common("FAT", "wild_two")

    def test_mfs_fcb_delete_wild_2(self):
        """MFS FCB file delete wildcard two"""
        self._test_fcb_delete_common("MFS", "wild_two")

    def test_fat_fcb_delete_wild_3(self):
        """FAT FCB file delete wildcard three"""
        self._test_fcb_delete_common("FAT", "wild_three")

    def test_mfs_fcb_delete_wild_3(self):
        """MFS FCB file delete wildcard three"""
        self._test_fcb_delete_common("MFS", "wild_three")

    def _test_fcb_find_common(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        if testname == "simple":
            ename = "fcbfind1"
            fn1 = "testa"
            fe1 = "bat"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
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
                mkfile(f, """hello\r\n""", testdir)
        elif testname == "wild_two":
            ename = "fcbfind4"
            fn1 = "a*"
            fe1 = "*"
            for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                      "afive.in", "xnone.ctl"]:
                mkfile(f, """hello\r\n""", testdir)
        elif testname == "wild_three":
            # To find "abc001.txt ... abc099.txt"
            ename = "fcbfind5"
            fn1 = "abc0??"
            fe1 = "*"
            for f in ["abc001.txt", "abc002.txt", "abc003.txt", "abc004.txt",
                      "abc005.txt", "abc010.txt", "xbc007.txt"]:
                mkfile(f, """hello\r\n""", testdir)

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    # Get DTA -> ES:BX
    movw    $0x2f00, %%ax
    int     $0x21
    pushw   %%es
    pushw   %%bx
    popl    pdta

    # FindFirst
findfirst:
    movw    $0x1100, %%ax
    movw    $fcb, %%dx
    int     $0x21

    cmpb    $0, %%al
    je      prsucc

prfail:
    movw    $failmsg, %%dx
    movb    $0x9, %%ah
    int     $0x21
    jmp     exit

prsucc:
    movw    $succmsg, %%dx
    movb    $0x9, %%ah
    int     $0x21

prfilename:
    push    %%ds
    lds     pdta, %%si
    inc     %%si

    push    %%cs
    pop     %%es
    movw    $prires, %%di
    inc     %%di

    movw    $11, %%cx
    cld
    repne   movsb

    pop     %%ds
    movw    $prires, %%dx
    movb    $0x9, %%ah
    int     $0x21

    # FindNext
findnext:
    movw    $0x1200, %%ax
    movw    $fcb, %%dx
    int     $0x21

    cmpb    $0, %%al
    je      prfilename

exit:
    movb    $0x4c, %%ah
    int     $0x21

fcb:
    .byte   0       # 0 default drive
fn1:
    .ascii  "% -8s"    # 8 bytes
fe1:
    .ascii  "% -3s"    # 3 bytes
wk1:
    .space  25

pdta:
    .long   0

prires:
    .ascii  "("
fname:
    .space  8, 20
fext:
    .space  3, 20
    .ascii  ")\r\n$"

succmsg:
    .ascii  "Find Operation Success\r\n$"
failmsg:
    .ascii  "Find Operation Failed\r\n$"

""" % (fn1, fe1))

        if fstype == "MFS":
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
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

    def test_fat_fcb_find_simple(self):
        """FAT FCB file find simple"""
        self._test_fcb_find_common("FAT", "simple")

    def test_mfs_fcb_find_simple(self):
        """MFS FCB file find simple"""
        self._test_fcb_find_common("MFS", "simple")

    def test_fat_fcb_find_missing(self):
        """FAT FCB file find missing"""
        self._test_fcb_find_common("FAT", "missing")

    def test_mfs_fcb_find_missing(self):
        """MFS FCB file find missing"""
        self._test_fcb_find_common("MFS", "missing")

    def test_fat_fcb_find_wild_1(self):
        """FAT FCB file find wildcard one"""
        self._test_fcb_find_common("FAT", "wild_one")

    def test_mfs_fcb_find_wild_1(self):
        """MFS FCB file find wildcard one"""
        self._test_fcb_find_common("MFS", "wild_one")

    def test_fat_fcb_find_wild_2(self):
        """FAT FCB file find wildcard two"""
        self._test_fcb_find_common("FAT", "wild_two")

    def test_mfs_fcb_find_wild_2(self):
        """MFS FCB file find wildcard two"""
        self._test_fcb_find_common("MFS", "wild_two")

    def test_fat_fcb_find_wild_3(self):
        """FAT FCB file find wildcard three"""
        self._test_fcb_find_common("FAT", "wild_three")

    def test_mfs_fcb_find_wild_3(self):
        """MFS FCB file find wildcard three"""
        self._test_fcb_find_common("MFS", "wild_three")

    def _test_ds2_read_eof(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

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
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        testdata = mkstring(32)

        mkfile("testit.bat", """\
d:\r
echo %s > test.fil\r
c:\\%s\r
DIR\r
rem end\r
""" % (testdata, ename))

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x3d00, %%ax			# open file readonly
    movw    $fname, %%dx
    int     $0x21
    jc      prfailopen

    movw    %%ax, fhndl

    movw    $0x3f00, %%ax			# read from file, should be partial (35)
    movw    fhndl, %%bx
    movw    $64, %%cx
    movw    $fdata, %%dx
    int     $0x21
    jc      prfailread
    cmpw    $35, %%ax
    jne     prnumread

    movw    $0x3f00, %%ax			# read from file again to get EOF
    movw    fhndl, %%bx
    movw    $64, %%cx
    movw    $fdata, %%dx
    int     $0x21
    jc      prcarryset
    cmpw    $0, %%ax
    jne     praxnotzero

    jmp     prsucc

prfailopen:
    movw    $failopen, %%dx
    jmp     1f

prfailread:
    movw    $failread, %%dx
    jmp     2f

prnumread:
    movw    $numread, %%dx
    jmp     2f

praxnotzero:
    movw    $axnotzero, %%dx
    jmp     2f

prcarryset:
    movw    $carryset, %%dx
    jmp     2f

prsucc:
    movb    $')',  (fdata + 32)
    movb    $'\r', (fdata + 33)
    movb    $'\n', (fdata + 34)
    movb    $'$',  (fdata + 35)
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

fname:
    .asciz  "%s"

fhndl:
    .word   0

success:
    .ascii  "Operation Success("
fdata:
    .space  64
failopen:
    .ascii  "Open Operation Failed\r\n$"
failread:
    .ascii  "Read Operation Failed\r\n$"
numread:
    .ascii  "Partial Read Not 35 Chars\r\n$"
carryset:
    .ascii  "Carry Set at EOF\r\n$"
axnotzero:
    .ascii  "AX Not Zero at EOF\r\n$"

""" % "test.fil")

        results = self.runDosemu("testit.bat", config=config)

        self.assertNotIn("Operation Failed", results)
        self.assertNotIn("Partial Read Not 35 Chars", results)
        self.assertNotIn("Carry Set at EOF", results)
        self.assertNotIn("AX Not Zero at EOF", results)
        self.assertIn("Operation Success(%s)" % testdata, results)

    def test_fat_ds2_read_eof(self):
        """FAT DOSv2 file read EOF"""
        self._test_ds2_read_eof("FAT")

    def test_mfs_ds2_read_eof(self):
        """MFS DOSv2 file read EOF"""
        self._test_ds2_read_eof("MFS")

    def _test_ds2_read_alt_dta(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

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
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        testdata = mkstring(32)

        mkfile("testit.bat", """\
d:\r
echo %s > test.fil\r
c:\\%s\r
DIR\r
rem end\r
""" % (testdata, ename))

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x1a00, %%ax			# set DTA
    movw    $altdta, %%dx
    int     $0x21

    movw    $0x2f00, %%ax			# get DTA address in ES:BX
    int     $0x21
    movw    %%cs, %%ax
    movw    %%es, %%dx
    cmpw    %%ax, %%dx
    jne     prfaildtaset
    cmpw    $altdta, %%bx
    jne     prfaildtaset

    movw    $0x3d00, %%ax			# open file readonly
    movw    $fname, %%dx
    int     $0x21
    jc      prfailopen

    movw    %%ax, fhndl

    movw    $0x3f00, %%ax			# read from file, should be partial (35)
    movw    fhndl, %%bx
    movw    $64, %%cx
    movw    $fdata, %%dx
    int     $0x21
    jc      prfailread
    cmpw    $35, %%ax
    jne     prnumread

    jmp     prsucc

prfaildtaset:
    movw    $faildtaset, %%dx
    jmp     1f

prfailopen:
    movw    $failopen, %%dx
    jmp     1f

prfailread:
    movw    $failread, %%dx
    jmp     2f

prnumread:
    movw    $numread, %%dx
    jmp     2f

prsucc:
    movb    $')',  (fdata + 32)
    movb    $'\r', (fdata + 33)
    movb    $'\n', (fdata + 34)
    movb    $'$',  (fdata + 35)
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

fname:
    .asciz  "%s"

fhndl:
    .word   0

success:
    .ascii  "Operation Success("
fdata:
    .space  64
faildtaset:
    .ascii  "Set DTA Operation Failed\r\n$"
failopen:
    .ascii  "Open Operation Failed\r\n$"
failread:
    .ascii  "Read Operation Failed\r\n$"
numread:
    .ascii  "Partial Read Not 35 Chars\r\n$"

altdta:
    .space  128

""" % "test.fil")

        results = self.runDosemu("testit.bat", config=config)

        self.assertNotIn("Operation Failed", results)
        self.assertNotIn("Partial Read Not 35 Chars", results)
        self.assertIn("Operation Success(%s)" % testdata, results)

    def test_fat_ds2_read_alt_dta(self):
        """FAT DOSv2 file read alternate DTA"""
        self._test_ds2_read_alt_dta("FAT")

    def test_mfs_ds2_read_alt_dta(self):
        """MFS DOSv2 file read alternate DTA"""
        self._test_ds2_read_alt_dta("MFS")

    def _test_ds2_file_seek_read(self, fstype, whence, value, expected):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

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
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        testdata = "0123456789abcdefFEDCBA9876543210"

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        if whence == "START":
            SCMD = "$0x4200"
        elif whence == "CURRENT":
            SCMD = "$0x4201"
        elif whence == "END":
            SCMD = "$0x4202"
        else:
            self.fail("Incorrect whence parameter")

        SVAL = str(value)

        # compile sources
        mkcom(ename, r"""
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

    movw    %s, %%ax		    	# seek from whence to somewhere
    movw    fhndl, %%bx
    movw    $sval, %%si
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

sval:
    .long   %s

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

""" % (SCMD, SVAL, "test.fil", testdata))

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

    def test_fat_ds2_file_seek_0_read(self):
        """FAT DOSv2 file seek start read"""
        self._test_ds2_file_seek_read("FAT", "START", 3, "3456")

    def test_mfs_ds2_file_seek_0_read(self):
        """MFS DOSv2 file seek start read"""
        self._test_ds2_file_seek_read("MFS", "START", 3, "3456")

    def test_fat_ds2_file_seek_1_read(self):
        """FAT DOSv2 file seek current read"""
        self._test_ds2_file_seek_read("FAT", "CURRENT", 3, "CBA9")

    def test_mfs_ds2_file_seek_1_read(self):
        """MFS DOSv2 file seek current read"""
        self._test_ds2_file_seek_read("MFS", "CURRENT", 3, "CBA9")

    def test_fat_ds2_file_seek_2_read(self):
        """FAT DOSv2 file seek end read"""
        self._test_ds2_file_seek_read("FAT", "END", -4, "3210")

    def test_mfs_ds2_file_seek_2_read(self):
        """MFS DOSv2 file seek end read"""
        self._test_ds2_file_seek_read("MFS", "END", -4, "3210")

    def _test_ds2_file_seek_position(self, fstype, whence, value, expected):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)

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
""" % self.mkimage("12", "", bootblk=False, cwd=testdir)

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        if whence == "START":
            SCMD = "$0x4200"
        elif whence == "CURRENT":
            SCMD = "$0x4201"
        elif whence == "END":
            SCMD = "$0x4202"
        else:
            self.fail("Incorrect whence parameter")

        SVAL = str(value)
        FVAL = str(expected)

        # compile sources
        mkcom(ename, r"""
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

    movw    %s, %%ax		    	# seek from whence to somewhere
    movw    fhndl, %%bx
    movw    $sval, %%si
    movw    %%ds:2(%%si), %%cx
    movw    %%ds:0(%%si), %%dx
    int     $0x21
    jc      prcarryset

    shll    $16, %%edx              # compare the resultant position
    movw    %%ax, %%dx
    movl    fval, %%ecx
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

sval:
    .long   %s

fval:
    .long   %s

fname:
    .asciz  "%s"

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

""" % (SCMD, SVAL, FVAL, "test.fil"))

        results = self.runDosemu("testit.bat", config=config)

        self.assertNotIn("Create Operation Failed", results)
        self.assertNotIn("Write Operation Failed", results)
        self.assertNotIn("Write Incorrect Length", results)
        self.assertNotIn("Seek Carry Set", results)
        self.assertNotIn("File Position Incorrect", results)
        self.assertIn("Operation Success", results)

    def test_fat_ds2_file_seek_2_position_back(self):
        """FAT DOSv2 file seek end position back"""
        self._test_ds2_file_seek_position("FAT", "END", -7, 2097145)

    def test_mfs_ds2_file_seek_2_position_back(self):
        """MFS DOSv2 file seek end position back"""
        self._test_ds2_file_seek_position("MFS", "END", -7, 2097145)

    def test_fat_ds2_file_seek_2_position_back_large(self):
        """FAT DOSv2 file seek end position back large"""
        self._test_ds2_file_seek_position("FAT", "END", -0x10000, 2031616)

    def test_mfs_ds2_file_seek_2_position_back_large(self):
        """MFS DOSv2 file seek end position back large"""
        self._test_ds2_file_seek_position("MFS", "END", -0x10000, 2031616)

    def test_fat_ds2_file_seek_2_position_forward(self):
        """FAT DOSv2 file seek end position forward"""
        self._test_ds2_file_seek_position("FAT", "END", 7, 2097159)

    def test_mfs_ds2_file_seek_2_position_forward(self):
        """MFS DOSv2 file seek end position forward"""
        self._test_ds2_file_seek_position("MFS", "END", 7, 2097159)

    def test_fat_ds2_file_seek_2_position_forward_large(self):
        """FAT DOSv2 file seek end position forward large"""
        self._test_ds2_file_seek_position("FAT", "END", 0x10000, 2162688)

    def test_mfs_ds2_file_seek_2_position_forward_large(self):
        """MFS DOSv2 file seek end position forward large"""
        self._test_ds2_file_seek_position("MFS", "END", 0x10000, 2162688)

    def _test_ds2_rename_common(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        extrad = ""

        if testname == "file":
            ename = "mfsds2r1"
            fn1 = "testa"
            fe1 = "bat"
            fn2 = "testb"
            fe2 = "bal"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
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
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
            mkfile(fn2 + "." + fe2, """hello\r\n""", testdir)
        elif testname == "dir":
            ename = "mfsds2r4"
            fn1 = "testa"
            fe1 = ""
            fn2 = "testb"
            fe2 = ""
            extrad = "mkdir %s\r\n" % fn1
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
            extrad = "mkdir %s\r\nmkdir %s\r\n" % (fn1, fn2)

        mkfile("testit.bat", """\
d:\r
%s
c:\\%s\r
DIR\r
rem end\r
""" % (extrad, ename))

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds
    push    %%cs
    pop     %%es

    movw    $0x5600, %%ax
    movw    $src, %%dx
    movw    $dst, %%di
    int     $0x21
    jnc     prsucc

prfail:
    movw    $failmsg, %%dx
    jmp     1f
prsucc:
    movw    $succmsg, %%dx
1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

src:
    .asciz  "%s"    # Full path
dst:
    .asciz  "%s"    # Full path

succmsg:
    .ascii  "Rename Operation Success$"
failmsg:
    .ascii  "Rename Operation Failed$"

""" % (fn1 + "." + fe1, fn2 + "." + fe2))

        def assertIsPresent(testdir, results, fstype, f, e, msg=None):
            if fstype == "MFS":
                self.assertTrue(exists(join(testdir, f + "." + e)), msg)
            else:
                self.assertRegex(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()), msg)

        def assertIsPresentDir(testdir, results, fstype, f, msg=None):
            if fstype == "MFS":
                self.assertTrue(exists(join(testdir, f)), msg)
            else:
                # 2019-06-27 11:29 <DIR>         DOSEMU
                # DOSEMU               <DIR>  06-27-19  5:33p
                self.assertRegex(results.upper(),
                    r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s<DIR>\s+%s"
                    r"|"
                    r"%s\s+<DIR>\s+\d{2}-\d{2}-\d{2}\s+\d+:\d+[AaPp]" % (f.upper(), f.upper()), msg)

        if fstype == "MFS":
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
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

    def test_fat_ds2_rename_file(self):
        """FAT DOSv2 rename file"""
        self._test_ds2_rename_common("FAT", "file")

    def test_mfs_ds2_rename_file(self):
        """MFS DOSv2 rename file"""
        self._test_ds2_rename_common("MFS", "file")

    def test_fat_ds2_rename_file_src_missing(self):
        """FAT DOSv2 rename file src missing"""
        self._test_ds2_rename_common("FAT", "file_src_missing")

    def test_mfs_ds2_rename_file_src_missing(self):
        """MFS DOSv2 rename file src missing"""
        self._test_ds2_rename_common("MFS", "file_src_missing")

    def test_fat_ds2_rename_file_tgt_exists(self):
        """FAT DOSv2 rename file tgt exists"""
        self._test_ds2_rename_common("FAT", "file_tgt_exists")

    def test_mfs_ds2_rename_file_tgt_exists(self):
        """MFS DOSv2 rename file tgt exists"""
        self._test_ds2_rename_common("MFS", "file_tgt_exists")

    def test_fat_ds2_rename_dir(self):
        """FAT DOSv2 rename dir"""
        self._test_ds2_rename_common("FAT", "dir")

    def test_mfs_ds2_rename_dir(self):
        """MFS DOSv2 rename dir"""
        self._test_ds2_rename_common("MFS", "dir")

    def test_fat_ds2_rename_dir_src_missing(self):
        """FAT DOSv2 rename dir src missing"""
        self._test_ds2_rename_common("FAT", "dir_src_missing")

    def test_mfs_ds2_rename_dir_src_missing(self):
        """MFS DOSv2 rename dir src missing"""
        self._test_ds2_rename_common("MFS", "dir_src_missing")

    def test_fat_ds2_rename_dir_tgt_exists(self):
        """FAT DOSv2 rename dir tgt exists"""
        self._test_ds2_rename_common("FAT", "dir_tgt_exists")

    def test_mfs_ds2_rename_dir_tgt_exists(self):
        """MFS DOSv2 rename dir tgt exists"""
        self._test_ds2_rename_common("MFS", "dir_tgt_exists")

    def _test_ds2_delete_common(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        if testname == "file":
            ename = "mfsds2d1"
            fn1 = "testa"
            fe1 = "bat"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
        elif testname == "file_missing":
            ename = "mfsds2d2"
            fn1 = "testa"
            fe1 = "bat"

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x4100, %%ax
    movw    $src, %%dx
    int     $0x21
    jnc     prsucc

prfail:
    movw    $failmsg, %%dx
    jmp     1f
prsucc:
    movw    $succmsg, %%dx
1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

src:
    .asciz  "%s"    # Full path

succmsg:
    .ascii  "Delete Operation Success$"
failmsg:
    .ascii  "Delete Operation Failed$"

""" % (fn1 + "." + fe1))

        def assertIsNotPresent(testdir, results, fstype, f, e, msg=None):
            if fstype == "MFS":
                self.assertFalse(exists(join(testdir, f + "." + e)), msg)
            else:
                self.assertNotRegex(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()))

        if fstype == "MFS":
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

        if testname == "file":
            self.assertIn("Delete Operation Success", results)
            assertIsNotPresent(testdir, results, fstype, fn1, fe1, "File not deleted")

        elif testname == "file_missing":
            self.assertIn("Delete Operation Failed", results)

    def test_fat_ds2_delete_file(self):
        """FAT DOSv2 delete file"""
        self._test_ds2_delete_common("FAT", "file")

    def test_mfs_ds2_delete_file(self):
        """MFS DOSv2 delete file"""
        self._test_ds2_delete_common("MFS", "file")

    def test_fat_ds2_delete_file_missing(self):
        """FAT DOSv2 delete file missing"""
        self._test_ds2_delete_common("FAT", "file_missing")

    def test_mfs_ds2_delete_file_missing(self):
        """MFS DOSv2 delete file missing"""
        self._test_ds2_delete_common("MFS", "file_missing")

    def _test_ds2_find_common(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        if testname == "simple":
            ename = "ds2find1"
            fn1 = "testa"
            fe1 = "bat"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
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
                mkfile(f, """hello\r\n""", testdir)
        elif testname == "wild_two":
            ename = "ds2find4"
            fn1 = "a*"
            fe1 = "*"
            for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                      "afive.in", "xnone.ctl"]:
                mkfile(f, """hello\r\n""", testdir)
        elif testname == "wild_three":
            # To find "abc001.txt ... abc099.txt"
            ename = "ds2find5"
            fn1 = "abc0??"
            fe1 = "*"
            for f in ["abc001.txt", "abc002.txt", "abc003.txt", "abc004.txt",
                      "abc005.txt", "abc010.txt", "xbc007.txt"]:
                mkfile(f, """hello\r\n""", testdir)

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    # Get DTA -> ES:BX
    movw    $0x2f00, %%ax
    int     $0x21
    pushw   %%es
    pushw   %%bx
    popl    pdta

    # FindFirst
findfirst:
    movw    $0x4e00, %%ax
    movw    $0, %%cx
    movw    $fpatn, %%dx
    int     $0x21
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

prfilename:
    push    %%ds
    lds     pdta, %%ax
    addw    $0x1e, %%ax
    movw    %%ax, %%si

    push    %%cs
    pop     %%es
    movw    $(prires + 1), %%di

    movw    $13, %%cx
    cld

1:
    cmpb    $0, %%ds:(%%si)
    je     2f

    movsb
    loop    1b

2:
    movb    $')',  %%es:(%%di)
    inc     %%di
    movb    $'\r', %%es:(%%di)
    inc     %%di
    movb    $'\n', %%es:(%%di)
    inc     %%di
    movb    $'$',  %%es:(%%di)
    inc     %%di

    pop     %%ds
    movw    $prires, %%dx
    movb    $0x9, %%ah
    int     $0x21

    # FindNext
findnext:
    movw    $0x4f00, %%ax
    int     $0x21
    jnc     prfilename

exit:
    movb    $0x4c, %%ah
    int     $0x21

fpatn:
    .asciz "%s"

pdta:
    .long   0

prires:
    .ascii  "("
    .space  32

succmsg:
    .ascii  "Findfirst Operation Success\r\n$"
failmsg:
    .ascii  "Findfirst Operation Failed\r\n$"

""" % (fn1 + "." + fe1))

        if fstype == "MFS":
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
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

    def test_fat_ds2_find_simple(self):
        """FAT DOSv2 file find simple"""
        self._test_ds2_find_common("FAT", "simple")

    def test_mfs_ds2_find_simple(self):
        """MFS DOSv2 file find simple"""
        self._test_ds2_find_common("MFS", "simple")

    def test_fat_ds2_find_missing(self):
        """FAT DOSv2 file find missing"""
        self._test_ds2_find_common("FAT", "missing")

    def test_mfs_ds2_find_missing(self):
        """MFS DOSv2 file find missing"""
        self._test_ds2_find_common("MFS", "missing")

    def test_fat_ds2_find_wild_1(self):
        """FAT DOSv2 file find wildcard one"""
        self._test_ds2_find_common("FAT", "wild_one")

    def test_mfs_ds2_find_wild_1(self):
        """MFS DOSv2 file find wildcard one"""
        self._test_ds2_find_common("MFS", "wild_one")

    def test_fat_ds2_find_wild_2(self):
        """FAT DOSv2 file find wildcard two"""
        self._test_ds2_find_common("FAT", "wild_two")

    def test_mfs_ds2_find_wild_2(self):
        """MFS DOSv2 file find wildcard two"""
        self._test_ds2_find_common("MFS", "wild_two")

    def test_fat_ds2_find_wild_3(self):
        """FAT DOSv2 file find wildcard three"""
        self._test_ds2_find_common("FAT", "wild_three")

    def test_mfs_ds2_find_wild_3(self):
        """MFS DOSv2 file find wildcard three"""
        self._test_ds2_find_common("MFS", "wild_three")

    def _test_ds2_find_first(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)
        ename = "ds2fndfi"

        ATTR = "$0x00"

        if testname == "file_exists":
            FSPEC = r"\\fileexst.ext"
        elif testname == "file_exists_as_dir":
            FSPEC = r"\\fileexst.ext\\somefile.ext"
        elif testname == "file_not_found":
            FSPEC = r"\\Notfname.ext"
        elif testname == "no_more_files":
            FSPEC = r"\\????????.??x"
        elif testname == "path_not_found_wc":
            FSPEC = r"\\NotDir\\????????.???"
        elif testname == "path_not_found_pl":
            FSPEC = r"\\NotDir\\plainfil.txt"
        elif testname == "path_exists_empty":
            FSPEC = r"\\DirExist"
        elif testname == "path_exists_not_empty":
            FSPEC = r"\\DirExis2"
        elif testname == "path_exists_file_not_dir":
            FSPEC = r"\\DirExis2\\fileexst.ext"
            ATTR = "$0x10"
        elif testname == "dir_exists_pl":
            FSPEC = r"\\DirExis2"
            ATTR = "$0x10"
        elif testname == "dir_exists_wc":
            FSPEC = r"\\Di?Exis?"
            ATTR = "$0x10"
        elif testname == "dir_not_exists_pl":
            FSPEC = r"\\dirNOTex"
            ATTR = "$0x10"
        elif testname == "dir_not_exists_wc":
            FSPEC = r"\\dirNOTex\\wi??card.???"
            ATTR = "$0x10"
        elif testname == "dir_not_exists_fn":
            FSPEC = r"\\dirNOTex\\somefile.ext"
            ATTR = "$0x10"

        mkfile("testit.bat", """\
d:\r
echo hello > fileexst.ext\r
mkdir DirExist\r
mkdir DirExis2\r
echo hello > DirExis2\\fileexst.ext\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    movw    $0x4e00, %%ax
    movw    %s, %%cx
    movw    $fspec, %%dx
    int     $0x21

    jnc     prsucc

    cmpw    $0x02, %%ax
    je      fail02

    cmpw    $0x03, %%ax
    je      fail03

    cmpw    $0x12, %%ax
    je      fail12

    jmp     genfail

fail02:
    movw    $filenotfound, %%dx
    jmp     1f

fail03:
    movw    $pathnotfoundmsg, %%dx
    jmp     1f

fail12:
    movw    $nomoremsg, %%dx
    jmp     1f

genfail:
    movw    $genfailmsg, %%dx
    jmp     1f

prsucc:
    movw    $succmsg, %%dx

1:
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

fspec:
    .asciz  "%s"    # Full path

succmsg:
    .ascii  "FindFirst Operation Success\r\n$"
filenotfound:
    .ascii  "FindFirst Operation Returned FILE_NOT_FOUND(0x02)\r\n$"
pathnotfoundmsg:
    .ascii  "FindFirst Operation Returned PATH_NOT_FOUND(0x03)\r\n$"
nomoremsg:
    .ascii  "FindFirst Operation Returned NO_MORE_FILES(0x12)\r\n$"
genfailmsg:
    .ascii  "FindFirst Operation Returned Unexpected Errorcode\r\n$"

""" % (ATTR, FSPEC))

        if fstype == "MFS":
            results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
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

    def test_fat_ds2_findfirst_file_exists(self):
        """FAT DOSv2 findfirst file exists"""
        self._test_ds2_find_first("FAT", "file_exists")

    def test_mfs_ds2_findfirst_file_exists(self):
        """MFS DOSv2 findfirst file exists"""
        self._test_ds2_find_first("MFS", "file_exists")

    def test_fat_ds2_findfirst_file_exists_as_dir(self):
        """FAT DOSv2 findfirst file exists as dir"""
        self._test_ds2_find_first("FAT", "file_exists_as_dir")

    def test_mfs_ds2_findfirst_file_exists_as_dir(self):
        """MFS DOSv2 findfirst file exists as dir"""
        self._test_ds2_find_first("MFS", "file_exists_as_dir")

    def test_fat_ds2_findfirst_file_not_found(self):
        """FAT DOSv2 findfirst file not found"""
        self._test_ds2_find_first("FAT", "file_not_found")

    def test_mfs_ds2_findfirst_file_not_found(self):
        """MFS DOSv2 findfirst file not found"""
        self._test_ds2_find_first("MFS", "file_not_found")

    def test_fat_ds2_findfirst_no_more_files(self):
        """FAT DOSv2 findfirst no more files"""
        self._test_ds2_find_first("FAT", "no_more_files")

    def test_mfs_ds2_findfirst_no_more_files(self):
        """MFS DOSv2 findfirst no more files"""
        self._test_ds2_find_first("MFS", "no_more_files")

    def test_fat_ds2_findfirst_path_not_found_wc(self):
        """FAT DOSv2 findfirst path not found wildcard"""
        self._test_ds2_find_first("FAT", "path_not_found_wc")

    def test_mfs_ds2_findfirst_path_not_found_wc(self):
        """MFS DOSv2 findfirst path not found wildcard"""
        self._test_ds2_find_first("MFS", "path_not_found_wc")

    def test_fat_ds2_findfirst_path_not_found_pl(self):
        """FAT DOSv2 findfirst path not found plain"""
        self._test_ds2_find_first("FAT", "path_not_found_pl")

    def test_mfs_ds2_findfirst_path_not_found_pl(self):
        """MFS DOSv2 findfirst path not found plain"""
        self._test_ds2_find_first("MFS", "path_not_found_pl")

    def test_fat_ds2_findfirst_path_exists_empty(self):
        """FAT DOSv2 findfirst path exists empty"""
        self._test_ds2_find_first("FAT", "path_exists_empty")

    def test_mfs_ds2_findfirst_path_exists_empty(self):
        """MFS DOSv2 findfirst path exists empty"""
        self._test_ds2_find_first("MFS", "path_exists_empty")

    def test_fat_ds2_findfirst_path_exists_file_not_dir(self):
        """FAT DOSv2 findfirst path exists file not dir"""
        self._test_ds2_find_first("FAT", "path_exists_file_not_dir")

    def test_mfs_ds2_findfirst_path_exists_file_not_dir(self):
        """MFS DOSv2 findfirst path exists file not dir"""
        self._test_ds2_find_first("MFS", "path_exists_file_not_dir")

    def test_fat_ds2_findfirst_path_exists_not_empty(self):
        """FAT DOSv2 findfirst path exists not empty"""
        self._test_ds2_find_first("FAT", "path_exists_not_empty")

    def test_mfs_ds2_findfirst_path_exists_not_empty(self):
        """MFS DOSv2 findfirst path exists not empty"""
        self._test_ds2_find_first("MFS", "path_exists_not_empty")

    def test_fat_ds2_findfirst_dir_exists_pl(self):
        """FAT DOSv2 findfirst dir exists plain"""
        self._test_ds2_find_first("FAT", "dir_exists_pl")

    def test_mfs_ds2_findfirst_dir_exists_pl(self):
        """MFS DOSv2 findfirst dir exists plain"""
        self._test_ds2_find_first("MFS", "dir_exists_pl")

    def test_fat_ds2_findfirst_dir_exists_wc(self):
        """FAT DOSv2 findfirst dir exists wildcard"""
        self._test_ds2_find_first("FAT", "dir_exists_wc")

    def test_mfs_ds2_findfirst_dir_exists_wc(self):
        """MFS DOSv2 findfirst dir exists wildcard"""
        self._test_ds2_find_first("MFS", "dir_exists_wc")

    def test_fat_ds2_findfirst_dir_not_exists_pl(self):
        """FAT DOSv2 findfirst dir not exists plain"""
        self._test_ds2_find_first("FAT", "dir_not_exists_pl")

    def test_mfs_ds2_findfirst_dir_not_exists_pl(self):
        """MFS DOSv2 findfirst dir not exists plain"""
        self._test_ds2_find_first("MFS", "dir_not_exists_pl")

    def test_fat_ds2_findfirst_dir_not_exists_wc(self):
        """FAT DOSv2 findfirst dir not exists wildcard"""
        self._test_ds2_find_first("FAT", "dir_not_exists_wc")

    def test_mfs_ds2_findfirst_dir_not_exists_wc(self):
        """MFS DOSv2 findfirst dir not exists wildcard"""
        self._test_ds2_find_first("MFS", "dir_not_exists_wc")

    def test_fat_ds2_findfirst_dir_not_exists_fn(self):
        """FAT DOSv2 findfirst dir not exists filename"""
        self._test_ds2_find_first("FAT", "dir_not_exists_fn")

    def test_mfs_ds2_findfirst_dir_not_exists_fn(self):
        """MFS DOSv2 findfirst dir not exists filename"""
        self._test_ds2_find_first("MFS", "dir_not_exists_fn")

    def _test_ds2_find_mixed_wild_plain(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        ename = "ds2findm"
        fsmpl = "xbc007.txt"

        for f in ["abc001.txt", "abc002.txt", "abc003.txt", "abc004.txt",
                  "abc005.txt", "abc010.txt", "xbc007.txt"]:
            mkfile(f, """hello\r\n""", testdir)

        mkfile("testit.bat", """\
d:\r
c:\\%s\r
DIR\r
rem end\r
""" % ename)

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

    push    %%cs
    pop     %%ds

    # Get DTA -> ES:BX
    movw    $0x2f00, %%ax
    int     $0x21
    pushw   %%es
    pushw   %%bx
    popl    pdta

    # First FindFirst
    movw    $0x4e00, %%ax
    movw    $0, %%cx
    movw    $fwild, %%dx
    int     $0x21

    # Set alternate DTA
    movw    $0x1a00, %%ax
    movw    $altdta, %%dx
    int     $0x21

    # Second FindFirst
    movw    $0x4e00, %%ax
    movw    $0, %%cx
    movw    $fsmpl, %%dx
    int     $0x21

    # Set default DTA
    movw    $0x1a00, %%ax
    lds     pdta, %%dx
    int     $0x21

    # FindNext
    movw    $0x4f00, %%ax
    int     $0x21
    jnc     prsucc

prfail:
    movw    $failmsg, %%dx
    movb    $0x9, %%ah
    int     $0x21
    jmp     exit

prsucc:
    push    %%ds
    lds     pdta, %%ax
    addw    $0x1e, %%ax
    movw    %%ax, %%si

    push    %%cs
    pop     %%es
    movw    $(prires + 1), %%di

    movw    $13, %%cx
    cld

1:
    cmpb    $0, %%ds:(%%si)
    je     2f

    movsb
    loop    1b

2:
    movb    $')',  %%es:(%%di)
    inc     %%di
    movb    $'\r', %%es:(%%di)
    inc     %%di
    movb    $'\n', %%es:(%%di)
    inc     %%di
    movb    $'$',  %%es:(%%di)
    inc     %%di

    pop     %%ds
    movw    $succmsg, %%dx
    movb    $0x9, %%ah
    int     $0x21

exit:
    movb    $0x4c, %%ah
    int     $0x21

fwild:
    .asciz "a*.txt"
fsmpl:
    .asciz "%s"

altdta:
    .space  0x80

pdta:
    .long   0

succmsg:
    .ascii  "Findnext Operation Success"
prires:
    .ascii  "("
    .space  32

failmsg:
    .ascii  "Findnext Operation Failed\r\n$"

""" % fsmpl)

        if fstype == "MFS":
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

        results = self.runDosemu("testit.bat", config=config)

        self.assertNotIn("Findnext Operation Failed", results)
        self.assertRegex(results, r"Findnext Operation Success\(ABC...\.TXT\)")

    def test_fat_ds2_find_mixed_wild_plain(self):
        """FAT DOSv2 findnext intermixed wild plain"""
        self._test_ds2_find_mixed_wild_plain("FAT")

    def test_mfs_ds2_find_mixed_wild_plain(self):
        """MFS DOSv2 findnext intermixed wild plain"""
        self._test_ds2_find_mixed_wild_plain("MFS")

    def test_create_new_psp(self):
        """Create New PSP"""
        ename = "getnwpsp"
        cmdline = "COMMAND TAIL TEST"

        mkfile("testit.bat", """\
c:\\%s %s\r
rem end\r
""" % (ename, cmdline))

        # compile sources
        mkcom(ename, r"""
.text
.code16

    .globl  _start16
_start16:

# designate target segment
    push    %%cs
    pop     %%ax
    addw    $0x0200, %%ax
    movw    %%ax, %%es

# create PSP in memory
    movw    %%es, %%dx
    movw    $0x2600, %%ax
    int     $0x21

# see if the exit field is set
    cmpw    $0x20cd, %%es:(0x0000)
    jne     extfail

# see if the parent PSP is zero
    cmpw    $0x0000, %%es:(0x0016)
    je      pntzero

# see if the parent PSP points to a PSP
    push    %%es
    pushw   %%es:(0x0016)
    pop     %%es
    cmpw    $0x20cd, %%es:(0x0000)
    pop     %%es
    jne     pntnpsp

# see if the 'INT 21,RETF' is there
    cmpw    $0x21cd, %%es:(0x0050)
    jne     int21ms
    cmpb    $0xcb, %%es:(0x0052)
    jne     int21ms

# see if the cmdtail is there
    movzx   %%es:(0x0080), %%cx
    cmpw    $%d, %%cx
    jne     cmdlngth

    inc     %%cx
    mov     $cmdline, %%si
    mov     $0x0081, %%di
    cld
    repe    cmpsb
    jne     cmdtail

success:
    movw    $successmsg, %%dx
    jmp     exit

extfail:
    movw    $extfailmsg, %%dx
    jmp     exit

pntzero:
    movw    $pntzeromsg, %%dx
    jmp     exit

pntnpsp:
    movw    $pntnpspmsg, %%dx
    jmp     exit

int21ms:
    movw    $int21msmsg, %%dx
    jmp     exit

cmdlngth:
    movw    $cmdlngthmsg, %%dx
    jmp     exit

cmdtail:
    movw    $cmdtailmsg, %%dx
    jmp     exit

exit:
    movb    $0x9, %%ah
    int     $0x21

    movb    $0x4c, %%ah
    int     $0x21

extfailmsg:
    .ascii  "PSP exit field not set to CD20\r\n$"

pntzeromsg:
    .ascii  "PSP parent is zero\r\n$"

pntnpspmsg:
    .ascii  "PSP parent doesn't point to a PSP\r\n$"

int21msmsg:
    .ascii  "PSP is missing INT21, RETF\r\n$"

cmdlngthmsg:
    .ascii  "PSP has incorrect command length\r\n$"

cmdtailmsg:
    .ascii  "PSP has incorrect command tail\r\n$"

successmsg:
    .ascii  "PSP structure okay\r\n$"

# 05 20 54 45 53 54 0d
cmdline:
    .ascii " %s\r"

""" % (1 + len(cmdline), cmdline))

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertIn("PSP structure okay", results)

# Tests using neiher compiler nor assembler

    def test_systype(self):
        """SysType"""
        self.runDosemu("version.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_debug = "-D+d"
""")

        # read the logfile
        systypeline = "Not found in logfile"
        with open(self.logname, "r") as f:
            for line in f:
                if "system type is" in line:
                    systypeline = line
                    break

        self.assertIn(self.systype, systypeline)

    def test_floppy_img(self):
        """Floppy image file"""
        # Note: image must have
        # dosemu directory
        # autoexec.bat
        # version.bat

        self.unTarImageOrSkip("boot-floppy.img")

        results = self.runDosemu("version.bat", config="""\
$_hdimage = ""
$_floppy_a = "boot-floppy.img"
$_bootdrive = "a"
""")

        self.assertIn(self.version, results)

    def test_floppy_vfs(self):
        """Floppy vfs directory"""
        mkfile(self.autoexec, """\
prompt $P$G\r
path a:\\bin;a:\\gnu;a:\\dosemu\r
system -s DOSEMU_VERSION\r
system -e\r
""")

        results = self.runDosemu("version.bat", config="""\
$_hdimage = ""
$_floppy_a = "dXXXXs/c:fiveinch_360"
$_bootdrive = "a"
""")

        self.assertIn(self.version, results)

    def test_three_drives_vfs(self):
        """Three vfs directories configured"""
        # C exists as part of standard test
        makedirs("test-imagedir/dXXXXs/d")
        makedirs("test-imagedir/dXXXXs/e")

        results = self.runDosemu("version.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 dXXXXs/e:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertIn(self.version, results)   # Just to check we booted

    def _test_fat_img_d_writable(self, fat):
        mkfile("testit.bat", """\
D:\r
mkdir test\r
echo hello > hello.txt\r
DIR\r
rem end\r
""")

        name = self.mkimage(fat, [("testit.bat", "0")], bootblk=False)

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
""" % name)

        # Std DOS format
        # TEST         <DIR>
        # HELLO    TXT 8
        #
        # ComCom32 format
        # 2019-06-28 22:29 <DIR>         TEST
        # 2019-06-28 22:29             8 HELLO.TXT
        self.assertRegex(results,
                r"TEST[\t ]+<DIR>"
                r"|"
                r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s<DIR>\s+TEST")
        self.assertRegex(results,
                r"HELLO[\t ]+TXT[\t ]+8"
                r"|"
                r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s+8\s+HELLO.TXT")

    def test_fat12_img_d_writable(self):
        """FAT12 image file D writable"""
        self._test_fat_img_d_writable("12")

    def test_fat16_img_d_writable(self):
        """FAT16 image file D writable"""
        self._test_fat_img_d_writable("16")

    def test_mfs_lredir_auto_hdc(self):
        """MFS lredir auto C drive redirection"""
        mkfile("testit.bat", "lredir\r\nrem end\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

# C:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertRegex(results, r"C: = .*LINUX\\FS")

    def test_mfs_lredir_command(self):
        """MFS lredir command redirection"""
        mkfile("testit.bat", """\
lredir X: LINUX\\FS\\bin\r
lredir\r
rem end\r
""")
        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

# A:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE
# X: = LINUX\FS\bin\        attrib = READ/WRITE

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertRegex(results, r"X: = .*LINUX\\FS\\bin")

# Tests using the DJGPP DOS compiler

    def _test_mfs_file_find(self, nametype):
        if nametype == "LFN":
            testnames = [
                "verylongfilename.txt",
                "space embedded filename.txt",
                "shrtname.longextension"
            ]
            disablelfn = ""
        elif nametype == "SFN":
            testnames = [
                "SHRTNAME.TXT",
                "HELLO~1F.JAV",  # fake a mangled name
                "1.C"
            ]
            disablelfn = "set LFN=n"
        else:
            self.fail("Incorrect argument")

        testdir = "test-imagedir/dXXXXs/d"

        mkfile("testit.bat", """\
%s\r
d:\r
c:\\mfsfind\r
rem end\r
""" % disablelfn)

        makedirs(testdir)
        for name in testnames:
            mkfile(name, "some test text", dname=testdir)

        # compile sources
        mkexe("mfsfind", r"""
#include <dir.h>
#include <stdio.h>

int main(void) {
  struct ffblk f;

  int done = findfirst("*.*", &f, FA_HIDDEN | FA_SYSTEM);
  while (!done) {
    printf("%10u %2u:%02u:%02u %2u/%02u/%4u %s\n", f.ff_fsize,
           (f.ff_ftime >> 11) & 0x1f, (f.ff_ftime >> 5) & 0x3f,
           (f.ff_ftime & 0x1f) * 2, (f.ff_fdate >> 5) & 0x0f,
           (f.ff_fdate & 0x1f), ((f.ff_fdate >> 9) & 0x7f) + 1980, f.ff_name);
    done = findnext(&f);
  }
  return 0;
}
""")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        for name in testnames:
            self.assertIn(name, results)

    def test_mfs_lfn_file_find(self):
        """MFS LFN file find"""
        self._test_mfs_file_find("LFN")

    def test_mfs_sfn_file_find(self):
        """MFS SFN file find"""
        self._test_mfs_file_find("SFN")

    def _test_mfs_file_read(self, nametype):
        if nametype == "LFN":
            testname = "verylongname.txt"
            disablelfn = ""
        elif nametype == "SFN":
            testname = "shrtname.txt"
            disablelfn = "set LFN=n"
        else:
            self.fail("Incorrect argument")

        testdata = mkstring(128)
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("testit.bat", """\
%s\r
d:\r
c:\\mfsread %s %s\r
rem end\r
""" % (disablelfn, testname, testdata))

        makedirs(testdir)
        mkfile(testname, testdata, dname=testdir)

        # compile sources
        mkexe("mfsread", r"""
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  char b[512];
  int f, size;

  if (argc < 1) {
    printf("missing filename argument\n");
    return 3;
  }

  f = open(argv[1], O_RDONLY | O_TEXT);
  if (f < 0) {
    printf("open failed\n");
    return 2;
  }

  size = read(f, b, sizeof(b));
  if (size < 0) {
    printf("read failed\n");
    return 1;
  }

  write(1, b, size);
  close(f);
  return 0;
}
""")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertIn(testdata, results)

    def test_mfs_lfn_file_read(self):
        """MFS LFN file read"""
        self._test_mfs_file_read("LFN")

    def test_mfs_sfn_file_read(self):
        """MFS SFN file read"""
        self._test_mfs_file_read("SFN")

    def _test_mfs_file_write(self, nametype, operation):
        if nametype == "LFN":
            ename = "mfslfn"
            testname = "verylongname.txt"
            disablelfn = ""
        elif nametype == "SFN":
            ename = "mfssfn"
            testname = "shrtname.txt"
            disablelfn = "set LFN=n"
        else:
            self.fail("Incorrect argument")

        if operation == "create":
            ename += "wc"
            testprfx = ""
            openflags = "O_WRONLY | O_CREAT | O_TEXT"
            mode = ", 0222"
        elif operation == "createreadonly":
            ename += "wk"
            testprfx = ""
            openflags = "O_WRONLY | O_CREAT | O_TEXT"
            mode = ", 0444"
        elif operation == "truncate":
            ename += "wt"
            testprfx = "dummy data"
            openflags = "O_WRONLY | O_CREAT | O_TRUNC | O_TEXT"
            mode = ", 0222"
        elif operation == "append":
            ename += "wa"
            testprfx = "Original Data"
            openflags = "O_RDWR | O_APPEND | O_TEXT"
            mode = ""
        else:
            self.fail("Incorrect argument")

        testdata = mkstring(64)   # need to be fairly short to pass as arg
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("testit.bat", """\
%s\r
d:\r
c:\\%s %s %s\r
rem end\r
""" % (disablelfn, ename, testname, testdata))

        makedirs(testdir)
        if operation != "create" and operation != "createreadonly":
            mkfile(testname, testprfx, dname=testdir)

        # compile sources
        mkexe(ename, r"""
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int f, size;

  if (argc < 2) {
    printf("missing filename argument\n");
    return 4;
  }

  if (argc < 3) {
    printf("missing data argument\n");
    return 3;
  }

  f = open(argv[1], %s %s);
  if (f < 0) {
    printf("open failed\n");
    return 2;
  }

  size = write(f, argv[2], strlen(argv[2]));
  if (size < strlen(argv[2])) {
    printf("write failed\n");
    return 1;
  }

  close(f);
  return 0;
}
""" % (openflags, mode))

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertNotIn("open failed", results)

        try:
            with open(join(testdir, testname), "r") as f:
                filedata = f.read()
                if operation == "truncate":
                    self.assertNotIn(testprfx, filedata)
                elif operation == "append":
                    self.assertIn(testprfx + testdata, filedata)
                self.assertIn(testdata, filedata)
        except IOError:
            self.fail("File not created/opened")

    def test_mfs_lfn_file_create(self):
        """MFS LFN file create"""
        self._test_mfs_file_write("LFN", "create")

    def test_mfs_sfn_file_create(self):
        """MFS SFN file create"""
        self._test_mfs_file_write("SFN", "create")

    def test_mfs_lfn_file_create_readonly(self):
        """MFS LFN file create readonly"""
        self._test_mfs_file_write("LFN", "createreadonly")

    def test_mfs_sfn_file_create_readonly(self):
        """MFS SFN file create readonly"""
        self._test_mfs_file_write("SFN", "createreadonly")

    def test_mfs_lfn_file_truncate(self):
        """MFS LFN file truncate"""
        self._test_mfs_file_write("LFN", "truncate")

    def test_mfs_sfn_file_truncate(self):
        """MFS SFN file truncate"""
        self._test_mfs_file_write("SFN", "truncate")

    def test_mfs_lfn_file_append(self):
        """MFS LFN file append"""
        self._test_mfs_file_write("LFN", "append")

    def test_mfs_sfn_file_append(self):
        """MFS SFN file append"""
        self._test_mfs_file_write("SFN", "append")

    def _test_lfn_volume_info(self, fstype):
        if fstype == "MFS":
            drive = "C:\\"
        elif fstype == "FAT":
            drive = "D:\\"
        else:
            self.fail("Incorrect argument")

        mkfile("testit.bat", """\
c:\\lfnvinfo %s\r
rem end\r
""" % drive)

        # C exists as part of standard test
        makedirs("test-imagedir/dXXXXs/d")

        name = self.mkimage("FAT16", [("testit.bat", "0")], bootblk=False)

        # compile sources
        mkexe("lfnvinfo", r"""
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char *argv[]) {
  int max_file_len, max_path_len;
  char fsystype[32];
  unsigned rval;

  if (argc < 1) {
    printf("missing volume argument e.g. 'C:\\'\n");
    return 1;
  }

  rval = _get_volume_info(argv[1], &max_file_len, &max_path_len, fsystype);
  if (rval == 0 && errno) {
    printf("ERRNO(%d)\r\n", errno);
    return 2;
  }
  if (rval == _FILESYS_UNKNOWN) {
    printf("FILESYS_UNKNOWN(%d)\r\n", errno);
    return 3;
  }

  printf("FSTYPE(%s), FILELEN(%d), PATHLEN(%d), BITS(0x%04x)\r\n",
          fsystype, max_file_len, max_path_len, rval);

  return 0;
}
""")

        if fstype == "MFS":
            hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
        elif fstype == "FAT":
            hdimage = "dXXXXs/c:hdtype1 %s" % name

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "%s +1"
$_floppy_a = ""
""" % hdimage)

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        if fstype == "MFS":
            self.assertIn("FSTYPE(MFS)", results)
        elif fstype == "FAT":
            self.assertIn("ERRNO(27)", results)

    def test_lfn_volume_info_mfs(self):
        """LFN volume info on MFS"""
        self._test_lfn_volume_info("MFS")

    def test_lfn_volume_info_fat_img(self):
        """LFN volume info on FAT(img)"""
        self._test_lfn_volume_info("FAT")

    def test_fat32_disk_info(self):
        """FAT32 disk info"""

        path = "C:\\"

        mkfile("testit.bat", """\
c:\\fat32dif %s\r
rem end\r
""" % path)

        # compile sources
        mkexe("fat32dif", r"""\
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct dinfo {
  uint16_t size;
  uint16_t version; // (0000h)
  uint32_t spc;
  uint32_t bps;
  uint32_t avail_clusters;
  uint32_t total_clusters;
  uint32_t avail_sectors;
  uint32_t total_sectors;
  uint32_t avail_units;
  uint32_t total_units;
  char reserved[8];
};

#define MAXPATH 128

int main(int argc, char *argv[]) {
  struct dinfo df;
  uint8_t carry;
  uint16_t ax;
  int len;

  if (argc < 2) {
    printf("path argument missing e.g. 'C:\\'\n");
    return 3;
  }

  len = strlen(argv[1]) + 1;
  if (len > MAXPATH) {
    printf("path argument too long\n");
    return 2;
  }

  /*
    AX = 7303h
    DS:DX -> ASCIZ string for drive ("C:\" or "\\SERVER\Share")
    ES:DI -> buffer for extended free space structure (see #01789)
    CX = length of buffer for extended free space

    Return:
    CF clear if successful
    ES:DI buffer filled
    CF set on error
    AX = error code
   */

  asm volatile("stc\n"
               "int $0x21\n"
               "setc %0\n"
               : "=r"(carry), "=a"(ax)
               : "a"(0x7303), "d"(argv[1]), "D"(&df), "c"(sizeof(df))
               : "cc", "memory");

  if (carry) {
    printf("Call failed (CARRY), AX = 0x%04x\n", ax);
    return 1;
  }

  /* See if we have valid data */
  if (df.size > sizeof(df)) {
    printf("Call failed (Struct invalid), size = 0x%04x, version 0x%04x\n", df.size, df.version);
    return 1;
  }

  printf("size                0x%04x\n", df.size);
  printf("version             0x%04x\n", df.version);
  printf("spc                 0x%08lx\n", df.spc);
  printf("bps                 0x%08lx\n", df.bps);
  printf("avail_clusters      0x%08lx\n", df.avail_clusters);
  printf("total_clusters      0x%08lx\n", df.total_clusters);
  printf("avail_sectors       0x%08lx\n", df.avail_sectors);
  printf("total_sectors       0x%08lx\n", df.total_sectors);
  printf("avail_units         0x%08lx\n", df.avail_units);
  printf("total_units         0x%08lx\n", df.total_units);

  printf("avail_bytes(%llu)\n",
         (unsigned long long)df.spc * (unsigned long long)df.bps * (unsigned long long)df.avail_clusters);
  printf("total_bytes(%llu)\n",
         (unsigned long long)df.spc * (unsigned long long)df.bps * (unsigned long long)df.total_clusters);
  return 0;
}
""")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertNotIn("Call failed", results)

        fsinfo = statvfs("test-imagedir/dXXXXs/c")
        lfs_total = fsinfo.f_blocks * fsinfo.f_bsize
        lfs_avail = fsinfo.f_bavail * fsinfo.f_bsize

        t = re.search(r'total_bytes\((\d+)\)', results)
        dfs_total = int(t.group(1))
        a = re.search(r'avail_bytes\((\d+)\)', results)
        dfs_avail = int(a.group(1))

# see if we are within 5% of the values obtained from Linux
        msg = "total dos %d, linux %d" % (dfs_total, lfs_total)
        self.assertLessEqual(dfs_total, lfs_total * 1.05, msg)
        self.assertGreaterEqual(dfs_total, lfs_total * 0.95, msg)

        msg = "avail dos %d, linux %d" % (dfs_avail, lfs_avail)
        self.assertLessEqual(dfs_avail, lfs_avail * 1.05, msg)
        self.assertGreaterEqual(dfs_avail, lfs_avail * 0.95, msg)

    def test_int21_disk_info(self):
        """INT21 disk info"""

        path = "C:\\"

        mkfile("testit.bat", """\
c:\\int21dif %s\r
rem end\r
""" % path)

        # compile sources
        mkexe("int21dif", r"""\
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct dinfo {
  uint16_t spc;
  uint16_t avail_clusters;
  uint16_t bps;
  uint16_t total_clusters;
};

#define MAXPATH 128

int main(int argc, char *argv[]) {
  struct dinfo df;
  uint8_t carry;
  uint16_t ax, bx, cx, dx;
  int len;

  if (argc < 2) {
    printf("path argument missing e.g. 'C:\\'\n");
    return 3;
  }

  len = strlen(argv[1]) + 1;
  if (len > MAXPATH) {
    printf("path argument too long\n");
    return 2;
  }

  if (argv[1][0] && argv[1][1] == ':') {
    if (argv[1][0] == 'a' || argv[1][0] == 'A')
      dx = 1;
    else if (argv[1][0] == 'b' || argv[1][0] == 'B')
      dx = 2;
    else if (argv[1][0] == 'c' || argv[1][0] == 'C')
      dx = 3;
    else if (argv[1][0] == 'd' || argv[1][0] == 'D')
      dx = 4;
    else {
      printf("Drive out of range\n");
      return 2;
    }
  } else {
    printf("Drive used is default\n");
    dx = 0; // default drive
  }

  /*
  AH = 36h
  DL = drive number (00h = default, 01h = A:, etc)

  Return:
    AX = FFFFh if invalid drive
  else
    AX = sectors per cluster
    BX = number of free clusters
    CX = bytes per sector
    DX = total clusters on drive
   */

  asm volatile("stc\n"
               "int $0x21\n"
               : "=a"(ax), "=b"(bx), "=c"(cx), "=d"(dx)
               : "a"(0x3600), "d"(dx)
               : "cc", "memory");

  if (ax == 0xffff) {
    printf("Call failed, AX = 0x%04x\n", ax);
    return 1;
  }

  printf("spc                 0x%04x\n", ax);
  printf("avail_clusters      0x%04x\n", bx);
  printf("bps                 0x%04x\n", cx);
  printf("total_clusters      0x%04x\n", dx);

  printf("avail_bytes(%llu)\n",
         (unsigned long long)ax * (unsigned long long)cx * (unsigned long long)bx);
  printf("total_bytes(%llu)\n",
         (unsigned long long)ax * (unsigned long long)cx * (unsigned long long)dx);
  return 0;
}
""")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertNotIn("Call failed", results)

        fsinfo = statvfs("test-imagedir/dXXXXs/c")
        lfs_total = fsinfo.f_blocks * fsinfo.f_bsize
        lfs_avail = fsinfo.f_bavail * fsinfo.f_bsize

        t = re.search(r'total_bytes\((\d+)\)', results)
        dfs_total = int(t.group(1))
        a = re.search(r'avail_bytes\((\d+)\)', results)
        dfs_avail = int(a.group(1))

# see if we are within 5% of the values obtained from Linux
        if lfs_total > 2147450880:
            lfs_total = 2147450880
        if lfs_avail > 2147450880:
            lfs_avail = 2147450880
        msg = "total dos %d, linux %d" % (dfs_total, lfs_total)
        self.assertLessEqual(dfs_total, lfs_total * 1.05, msg)
        self.assertGreaterEqual(dfs_total, lfs_total * 0.95, msg)

        msg = "avail dos %d, linux %d" % (dfs_avail, lfs_avail)
        self.assertLessEqual(dfs_avail, lfs_avail * 1.05, msg)
        self.assertGreaterEqual(dfs_avail, lfs_avail * 0.95, msg)

    def _test_lfn_file_info_mfs(self, fsize):

        # Note: this needs to be somewhere writable, but not where a fatfs
        # will be generated else the sparse file will be copied to full size
        dpath = "/tmp"
        fpath = "lfnfilei.tst"

        mkfile("testit.bat", """\
lredir X: \\\\linux\\fs%s\r
c:\\lfnfilei X:\\%s\r
rem end\r
""" % (dpath, fpath))

        # compile sources
        mkexe("lfnfilei", r"""\
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

struct finfo {
  uint32_t fattr;    // 00h    DWORD   file attributes
  uint64_t ctime;    // 04h    QWORD   creation time (0 = unsupported)
  uint64_t atime;    // 0Ch    QWORD   last access time (0 = unsupported)
  uint64_t wtime;    // 14h    QWORD   last write time
  uint32_t sernum;   // 1Ch    DWORD   volume serial number
  uint32_t fsize_hi; // 20h    DWORD   high 32 bits of file size
  uint32_t fsize_lo; // 24h    DWORD   low 32 bits of file size
  uint32_t numlinks; // 28h    DWORD   number of links to file
  uint32_t filid_hi; // 2Ch    DWORD   unique file identifier (high 32 bits)
  uint32_t filid_lo; // 30h    DWORD   unique file identifier (low 32 bits)
} __attribute__((packed));

int main(int argc, char *argv[]) {
  struct finfo fi;
  uint8_t carry;
  uint16_t ax;
  int len;
  int fd;

  if (argc < 2) {
    printf("Error: file argument missing e.g. 'C:\\test.fil'\n");
    return 3;
  }

  len = strlen(argv[1]) + 1;
  if (len > MAXPATH) {
    printf("Error: path argument too long\n");
    return 2;
  }

  fd = open(argv[1], O_RDONLY | O_TEXT);
  if (fd < 0) {
    printf("Error: open failed\n");
    return 2;
  }

  memset(&fi, 0, sizeof fi);

  /*
    Windows95 - LONG FILENAME - GET FILE INFO BY HANDLE

    AX = 71A6h
    BX = file handle
    DS:DX -> buffer for file information (see #01784)
    CF set

    Return:
    CF clear if successful
    file information record filled
    CF set on error
    AX = error code
    7100h if function not supported
   */

  asm volatile("stc\n"
               "int $0x21\n"
               "setc %0\n"
               : "=r"(carry), "=a"(ax)
               : "a"(0x71a6), "b"(fd), "d"(&fi)
               : "cc", "memory");

  if (carry) {
    printf("Error: call failed (CARRY), AX = 0x%04x\n", ax);
    close(fd);
    return 1;
  }

  printf("sizeof struct is 0x%02x\n", sizeof fi);
  printf("\n");

  printf("fattr               0x%08lx\n", fi.fattr);
  printf("ctime       0x%016llx\n",       fi.ctime);
  printf("atime       0x%016llx\n",       fi.atime);
  printf("wtime       0x%016llx\n",       fi.wtime);
  printf("sernum              0x%08lx\n", fi.sernum);
  printf("fsize_hi            0x%08lx\n", fi.fsize_hi);
  printf("fsize_lo            0x%08lx\n", fi.fsize_lo);
  printf("numlinks            0x%08lx\n", fi.numlinks);
  printf("filid_hi            0x%08lx\n", fi.filid_hi);
  printf("filid_lo            0x%08lx\n", fi.filid_lo);

  close(fd);
  return 0;
}
""")

        # Make sparse file
        with open(join(dpath, fpath), "w") as f:
            f.truncate(fsize)

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        # Check the obvious fields
        self.assertNotIn("Error: ", results)

        t = re.search(r'fsize_hi.*0x(\d+)', results)
        fsize_hi = int(t.group(1), 16)
        t = re.search(r'fsize_lo.*0x(\d+)', results)
        fsize_lo = int(t.group(1), 16)
        t = re.search(r'numlinks.*0x(\d+)', results)
        numlinks = int(t.group(1), 16)

        self.assertEqual(fsize_hi, fsize >> 32)
        self.assertEqual(fsize_lo, fsize & 0xffffffff)
        self.assertEqual(numlinks, 1)

    def test_lfn_file_info_mfs_1MiB(self):
        """LFN file info on MFS (1 MiB)"""
        self._test_lfn_file_info_mfs(1024 * 1024)

    def test_lfn_file_info_mfs_6GiB(self):
        """LFN file info on MFS (6 GiB)"""
        self._test_lfn_file_info_mfs(1024 * 1024 * 1024 * 6)

    def _test_ds2_get_ftime(self, fstype, tstype):
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("testit.bat", """\
d:\r
c:\\getftime %s\r
rem end\r
""" % tstype)

        # compile sources
        mkexe("getftime", r"""

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

int gettest(const char *fname) {

  int handle;
  int ret;
  unsigned int date, time;
  unsigned int year, month, day, hour, minute, second;

  ret = _dos_open(fname, O_RDONLY, &handle);
  if (ret != 0) {
    printf("FAIL: File '%s' not opened\n", fname);
    return -1;
  }
  _dos_getftime(handle, &date, &time);
  _dos_close(handle);

  year = ((date >> 9) & 0x7f) + 1980;
  month = (date >> 5) & 0x0f;
  day = (date & 0x1f);

  hour = (time >> 11) & 0x1f;
  minute = (time >> 5) & 0x3f;
  second = (time & 0x1f) * 2;

  // 2018-08-13 10:10:51
  printf("%s: %04u-%02u-%02u %02u:%02u:%02u\n",
      fname, year, month, day, hour, minute, second);

  return 0;
}

int main(int argc, char *argv[]) {
  unsigned int val;
  int ret;
  char fname[13];

  if (argc < 2) {
    printf("FAIL: missing argument\n");
    return -2;
  }

  if (strcmp(argv[1], "DATE") == 0) {

    // Year
    for (val = 1980; val <= 2099; val++) {
      sprintf(fname, "%08u.YR", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Month
    for (val = 1; val <= 12; val++) {
      sprintf(fname, "%08u.MN", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Day
    for (val = 1; val <= 31; val++) {
      sprintf(fname, "%08u.DY", val);
      if (gettest(fname) < 0)
        return -1;
    }

  } else if (strcmp(argv[1], "TIME") == 0) {

    // Hour
    for (val = 0; val <= 23; val++) {
      sprintf(fname, "%08u.HR", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Minute
    for (val = 0; val <= 59; val++) {
      sprintf(fname, "%08u.MI", val);
      if (gettest(fname) < 0)
        return -1;
    }

    // Seconds
    for (val = 0; val <= 59; val += 2) {
      sprintf(fname, "%08u.SC", val);
      if (gettest(fname) < 0)
        return -1;
    }

  } else {
    printf("FAIL: argument not DATE or TIME\n");
    return -2;
  }

  return 0;
}
""")

        try:
            makedirs(testdir)
        except FileExistsError:
            for f in glob(join(testdir, '0000????.??')):
                remove(f)

        def settime(fn, Y, M, D, h, m, s):
            date = datetime(year=Y, month=M, day=D,
                            hour=h, minute=m, second=s)
            modTime = mktime(date.timetuple())
            utime(join(testdir, fname), (modTime, modTime))

        YEARS = range(1980, 2099 + 1) if tstype == "DATE" else []
        for i in YEARS:
            fname = "%08d.YR" % i
            mkfile(fname, "some content", dname=testdir)
            settime(fname, i, 1, 1, 0, 0, 0)

        MONTHS = range(1, 12 + 1) if tstype == "DATE" else []
        for i in MONTHS:
            fname = "%08d.MN" % i
            mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, i, 1, 0, 0, 0)

        DAYS = range(1, 31 + 1) if tstype == "DATE" else []
        for i in DAYS:
            fname = "%08d.DY" % i
            mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, i, 0, 0, 0)

        HOURS = range(0, 23 + 1) if tstype == "TIME" else []
        for i in HOURS:
            fname = "%08d.HR" % i
            mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, 1, i, 0, 0)

        MINUTES = range(0, 59 + 1) if tstype == "TIME" else []
        for i in MINUTES:
            fname = "%08d.MI" % i
            mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, 1, 0, i, 0)

        SECONDS = range(0, 59 + 1, 2) if tstype == "TIME" else []
        for i in SECONDS:
            fname = "%08d.SC" % i
            mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, 1, 0, 0, i)

        if fstype == "MFS":
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

        results = self.runDosemu("testit.bat", config=config)

        for i in YEARS:
            self.assertIn("%08d.YR: %04d-01-01 00:00:00" % (i, i), results)
        for i in MONTHS:
            self.assertIn("%08d.MN: 1980-%02d-01 00:00:00" % (i, i), results)
        for i in DAYS:
            self.assertIn("%08d.DY: 1980-01-%02d 00:00:00" % (i, i), results)
        for i in HOURS:
            self.assertIn("%08d.HR: 1980-01-01 %02d:00:00" % (i, i), results)
        for i in MINUTES:
            self.assertIn("%08d.MI: 1980-01-01 00:%02d:00" % (i, i), results)
        for i in SECONDS:
            self.assertIn("%08d.SC: 1980-01-01 00:00:%02d" % (i, i), results)

        self.assertNotIn("FAIL:", results)

    def test_mfs_ds2_get_ftime(self):
        """MFS DOSv2 get file time"""
        self._test_ds2_get_ftime("MFS", "DATE")
        self._test_ds2_get_ftime("MFS", "TIME")

    def test_fat_ds2_get_ftime(self):
        """FAT DOSv2 get file time"""
        # Note: we need to split this test as FAT cannot have enough files
        #       in the root directory, and mkimage can't store them in a
        #       subdirectory.
        self._test_ds2_get_ftime("FAT", "DATE")
        self._test_ds2_get_ftime("FAT", "TIME")

    def _test_ds2_set_ftime(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("testit.bat", """\
d:\r
c:\\setftime\r
rem end\r
""")

        # compile sources
        mkexe("setftime", r"""
/* Most of this was copied from DJGPP docs at
   http://www.delorie.com/djgpp/doc/libc/libc_181.html */

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <stdio.h>

#define FNAME "FOO.DAT"

int settest(unsigned int year, unsigned int month, unsigned int day,
            unsigned int hour, unsigned int minute, unsigned int second) {

  int handle;
  struct find_t f;
  int ret;
  unsigned int date, time;
  unsigned int _year, _month, _day, _hour, _minute, _second;

  _dos_creat(FNAME, _A_NORMAL, &handle);
  date = ((year - 1980) << 9) | (month << 5) | day;
  time = (hour << 11) | (minute << 5) | (second / 2);
  _dos_setftime(handle, date, time);
  _dos_close(handle);

  ret = _dos_findfirst(FNAME, _A_NORMAL, &f);
  if (ret != 0) {
    printf("FAIL: File '%s' not found\n", FNAME);
    return -1;
  }

  _year = ((f.wr_date >> 9) & 0x7f) + 1980;
  _month = (f.wr_date >> 5) & 0x0f;
  _day = (f.wr_date & 0x1f);

  _hour = (f.wr_time >> 11) & 0x1f;
  _minute = (f.wr_time >> 5) & 0x3f;
  _second = (f.wr_time & 0x1f) * 2;

  second &= ~1; // DOS has 2 second resolution so round down the target

  if ((year != _year) || (month != _month) || (day != _day) ||
      (hour != _hour) || (minute != _minute) || (second != _second)) {
    printf("FAIL: mismatch year(%u -> %u), month(%u -> %u), day(%u -> %u)\n",
           year, _year, month, _month, day, _day);
    printf("               hour(%u -> %u), minute(%u -> %u), second(%u -> %u)\n",
           hour, _hour, minute, _minute, second, _second);
    return -2;
  }

  return 0;
}

int main(void) {
  unsigned int val;
  int ret;

  // Year
  for (val = 1980; val <= 2099; val++) {
    if (settest(val, 1, 1, 0, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: years matched\n");

  // Month
  for (val = 1; val <= 12; val++) {
    if (settest(1980, val, 1, 0, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: months matched\n");

  // Day
  for (val = 1; val <= 31; val++) {
    if (settest(1980, 1, val, 0, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: days matched\n");

  // Hour
  for (val = 0; val <= 23; val++) {
    if (settest(1980, 1, 1, val, 0, 0) < 0)
      return -1;
  }
  printf("OKAY: hours matched\n");

  // Minute
  for (val = 0; val <= 59; val++) {
    if (settest(1980, 1, 1, 0, val, 0) < 0)
      return -1;
  }
  printf("OKAY: minutes matched\n");

  // Seconds
  for (val = 0; val <= 59; val++) {
    if (settest(1980, 1, 1, 0, 0, val) < 0)
      return -1;
  }
  printf("OKAY: seconds matched\n");

  printf("PASS: Set values are retrieved and matched\n");
  return 0;
}
""")

        makedirs(testdir)

        if fstype == "MFS":
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

        results = self.runDosemu("testit.bat", config=config)

        self.assertNotIn("FAIL:", results)

    def test_mfs_ds2_set_ftime(self):
        """MFS DOSv2 set file time"""
        self._test_ds2_set_ftime("MFS")

    def test_fat_ds2_set_ftime(self):
        """FAT DOSv2 set file time"""
        self._test_ds2_set_ftime("FAT")

    def _test_ds3_lock_twice(self, fstype):
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("testit.bat", """\
d:\r
%s\r
c:\\lcktwice primary\r
rem end\r
""" % ("rem Internal share" if self.version == "FDPP kernel" else "c:\\share"))

        # compile sources
        mkexe("lcktwice", r"""
/* Most of this was copied from DJGPP docs at
   http://www.delorie.com/djgpp/doc/libc/libc_181.html */

#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <string.h>

#define FNAME "FOO.DAT"

int main(int argc, char *argv[]) {

  int handle;
  int ret;
  int primary = -1;

  if (argc < 2) {
    printf("FAIL: Missing argument (primary|secondary)\n");
    return -2;
  }
  if (strcmp(argv[1], "primary") == 0)
    primary = 1;
  if (strcmp(argv[1], "secondary") == 0)
    primary = 0;
  if (primary < 0) {
    printf("FAIL: Invalid argument (primary|secondary)\n");
    return -2;
  }

  if (primary) {
    int rc;
    ret = _dos_creat(FNAME, _A_NORMAL, &handle);
    if (ret == 0)
      _dos_write(handle, "hello\n", 6, &rc);  // something to lock on
  } else {
    ret = _dos_open(FNAME, O_RDWR, &handle);
  }
  if (ret != 0) {
    printf("FAIL: %s: File '%s' not created or opened\n", argv[1], FNAME);
    return -1;
  }

  ret = _dos_lock(handle, 0,  1); // just the first byte
  if (ret != 0) {
    if (primary) {
      printf("FAIL: %s: Could not get lock on file '%s'\n", argv[1], FNAME);
      _dos_close(handle);
      return -1;
    } else {
      printf("PASS: %s: Could not get lock on file '%s'\n", argv[1], FNAME);
      _dos_close(handle);
      return 0;
    }
  } else {
    if (primary) {
      printf("OKAY: %s: Acquired lock on file '%s'\n", argv[1], FNAME);

      // Now start second copy
      spawnlp(P_WAIT, argv[0], argv[0], "secondary", NULL);

    } else {
      printf("FAIL: %s: Acquired lock on file '%s'\n", argv[1], FNAME);
      _dos_unlock(handle, 0,  1);
      _dos_close(handle);
      return -1;
    }
  }

  _dos_unlock(handle, 0,  1);
  _dos_close(handle);
  return 0;
}
""")

        makedirs(testdir)

        if fstype == "MFS":
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name

        results = self.runDosemu("testit.bat", config=config)

        self.assertNotIn("FAIL:", results)
        self.assertIn("PASS:", results)

    def test_mfs_ds3_lock_twice(self):
        """MFS DOSv3 lock file twice"""
        self._test_ds3_lock_twice("MFS")

    def test_fat_ds3_lock_twice(self):
        """FAT DOSv3 lock file twice"""
        self._test_ds3_lock_twice("FAT")

    def _test_ds3_share_open_twice_run_all(self, fstype, tests):
        testdir = "test-imagedir/dXXXXs/d"

        share = "rem Internal share" if self.version == "FDPP kernel" else "c:\\share"

        tfile = "set LFN=n\r\n" + "d:\r\n" + share + "\r\n"
        for t in tests:
            tfile += ("c:\\sharopen primary %s %s %s %s %s\r\n" % t)
        tfile += "rem tests complete\r\n"
        tfile += "rem end\r\n"

        mkfile("testit.bat", tfile)

        # compile sources
        mkexe("sharopen", r"""
#include <dos.h>
#include <dir.h>
#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <share.h>
#include <stdio.h>
#include <string.h>

#define FNAME "FOO.DAT"

unsigned short sharemode(const char *s) {
  if (strcmp(s, "SH_COMPAT") == 0)
    return SH_COMPAT;

  if (strcmp(s, "SH_DENYRW") == 0)
    return SH_DENYRW;

  if (strcmp(s, "SH_DENYWR") == 0)
    return SH_DENYWR;

  if (strcmp(s, "SH_DENYRD") == 0)
    return SH_DENYRD;

  if (strcmp(s, "SH_DENYNO") == 0)
    return SH_DENYNO;

  return 0xff;
}

unsigned short openmode(const char *s) {
  if (strcmp(s, "RW") == 0)
    return O_RDWR;
  if (s[0] == 'R')
    return O_RDONLY;
  if (s[0] == 'W')
    return O_WRONLY;
  return 0xff;
}

int main(int argc, char *argv[]) {
  int handle;
  int ret;
  int primary = -1;
  unsigned short prismode, priomode, secsmode, secomode;

  if (argc < 7) {
    printf("FAIL: Missing arguments (primary|secondary) prismode priomode secsmode secomode expected\n");
    return -2;
  }
  if (strcmp(argv[1], "primary") == 0)
    primary = 1;
  if (strcmp(argv[1], "secondary") == 0)
    primary = 0;
  if (primary < 0) {
    printf("FAIL: Invalid argument (primary|secondary)\n");
    return -2;
  }

  prismode = sharemode(argv[2]);
  if (prismode == 0xff) {
    printf("FAIL: Invalid argument prismode '%s'\n", argv[2]);
    return -2;
  }
  priomode = openmode(argv[3]);
  if (priomode == 0xff) {
    printf("FAIL: Invalid argument priomode '%s'\n", argv[3]);
    return -2;
  }
  secsmode = sharemode(argv[4]);
  if (secsmode == 0xff) {
    printf("FAIL: Invalid argument secsmode '%s'\n", argv[4]);
    return -2;
  }
  secomode = openmode(argv[5]);
  if (secomode == 0xff) {
    printf("FAIL: Invalid argument secomode '%s'\n", argv[5]);
    return -2;
  }

  // expected result is argv[6]

  // Print results in consistent format
  // FAIL:("SH_DENYNO", "RW", "SH_DENYNO", "RW", "ALLOW")[primary denied]

  if (primary) {
    unsigned short mode = prismode | priomode;
    ret = _dos_open(FNAME, mode, &handle);
    if (ret != 0) {
      printf("FAIL:('%s', '%s', '%s', '%s', '%s')[primary denied]\n",
          argv[2], argv[3], argv[4], argv[5], argv[6]);
      return -1;
    }
//    printf("INFO: primary: File was opened with mode 0x%04x\n", mode);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", argv[2], argv[3], argv[4], argv[5], argv[6], NULL);

  } else { // secondary
    unsigned short mode = secsmode | secomode;
    ret = _dos_open(FNAME, mode, &handle);
    if (ret != 0) {
      if (strcmp(argv[6], "DENY") == 0 || strcmp(argv[6], "INT24") == 0) {
        printf("PASS:('%s', '%s', '%s', '%s', '%s')[secondary denied]\n",
            argv[2], argv[3], argv[4], argv[5], argv[6]);
      } else {
        printf("FAIL:('%s', '%s', '%s', '%s', '%s')[secondary denied]\n",
            argv[2], argv[3], argv[4], argv[5], argv[6]);
      }
      return -1;
    }
    if (strcmp(argv[6], "ALLOW") == 0) {
      printf("PASS:('%s', '%s', '%s', '%s', '%s')[secondary allowed]\n",
          argv[2], argv[3], argv[4], argv[5], argv[6]);
    } else {
      printf("FAIL:('%s', '%s', '%s', '%s', '%s')[secondary allowed]\n",
          argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
  }

  _dos_close(handle);
  return 0;
}
""")

        makedirs(testdir)
        mkfile("FOO.DAT", "some data", dname=testdir)

        config = """\
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "native"
$_cpu_emu = "vm86"
$_floppy_a = ""
"""

        if fstype == "MFS":
            config += """$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"\r\n"""
        else:       # FAT
            files = [(x, 0) for x in listdir(testdir)]
            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            config += """$_hdimage = "dXXXXs/c:hdtype1 %s +1"\r\n""" % name

        return self.runDosemu("testit.bat", config=config, timeout=60)

#
# (Table 01403)
# Values of DOS 2-6.22 file sharing behavior:
#          |     Second and subsequent Opens
# First    |Compat  Deny   Deny   Deny   Deny
# Open     |        All    Write  Read   None
#          |R W RW R W RW R W RW R W RW R W RW
# - - - - -| - - - - - - - - - - - - - - - - -
# Compat R |Y Y Y  N N N  1 N N  N N N  1 N N.
#        W |Y Y Y  N N N  N N N  N N N  N N N.
#        RW|Y Y Y  N N N  N N N  N N N  N N N
# - - - - -|
# Deny   R |C C C  N N N  N N N  N N N  N N N
# All    W |C C C  N N N  N N N  N N N  N N N.
#        RW|C C C  N N N  N N N  N N N  N N N
# - - - - -|
# Deny   R |2 C C  N N N  Y N N  N N N  Y N N
# Write  W |C C C  N N N  N N N  Y N N  Y N N.
#        RW|C C C  N N N  N N N  N N N  Y N N
# - - - - -|
# Deny   R |C C C  N N N  N Y N  N N N  N Y N
# Read   W |C C C  N N N  N N N  N Y N  N Y N.
#        RW|C C C  N N N  N N N  N N N  N Y N
# - - - - -|
# Deny   R |2 C C  N N N  Y Y Y  N N N  Y Y Y
# None   W |C C C  N N N  N N N  Y Y Y  Y Y Y.
#        RW|C C C  N N N  N N N  N N N  Y Y Y

# Legend:
# Y = open succeeds, N = open fails with error code 05h.
# C = open fails, INT 24 generated.
# 1 = open succeeds if file read-only, else fails with error code.
# 2 = open succeeds if file read-only, else fails with INT 24

    OPENTESTS = (
# Compat R |Y Y Y  N N N  1 N N  N N N  1 N N.
        ("SH_COMPAT", "R" , "SH_COMPAT", "R" , "ALLOW"),
        ("SH_COMPAT", "R" , "SH_COMPAT", "W" , "ALLOW"),
        ("SH_COMPAT", "R" , "SH_COMPAT", "RW", "ALLOW"),
        ("SH_COMPAT", "R" , "SH_DENYRW", "R" , "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYRW", "W" , "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYRW", "RW", "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYWR", "R" , "DENY"), # File RO success, W fails
        ("SH_COMPAT", "R" , "SH_DENYWR", "W" , "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYWR", "RW", "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYRD", "R" , "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYRD", "W" , "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYRD", "RW", "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYNO", "R" , "DENY"), # File RO success, W fails
        ("SH_COMPAT", "R" , "SH_DENYNO", "W" , "DENY"),
        ("SH_COMPAT", "R" , "SH_DENYNO", "RW", "DENY"),

#        W |Y Y Y  N N N  N N N  N N N  N N N.
        ("SH_COMPAT", "W" , "SH_COMPAT", "R" , "ALLOW"),
        ("SH_COMPAT", "W" , "SH_COMPAT", "W" , "ALLOW"),
        ("SH_COMPAT", "W" , "SH_COMPAT", "RW", "ALLOW"),
        ("SH_COMPAT", "W" , "SH_DENYRW", "R" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYRW", "W" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYRW", "RW", "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYWR", "R" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYWR", "W" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYWR", "RW", "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYRD", "R" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYRD", "W" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYRD", "RW", "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYNO", "R" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYNO", "W" , "DENY"),
        ("SH_COMPAT", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|Y Y Y  N N N  N N N  N N N  N N N
        ("SH_COMPAT", "RW", "SH_COMPAT", "R" , "ALLOW"),
        ("SH_COMPAT", "RW", "SH_COMPAT", "W" , "ALLOW"),
        ("SH_COMPAT", "RW", "SH_COMPAT", "RW", "ALLOW"),
        ("SH_COMPAT", "RW", "SH_DENYRW", "R" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYRW", "W" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYRW", "RW", "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYWR", "R" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYWR", "W" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYWR", "RW", "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYRD", "R" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYRD", "W" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYRD", "RW", "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYNO", "R" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYNO", "W" , "DENY"),
        ("SH_COMPAT", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |C C C  N N N  N N N  N N N  N N N
        ("SH_DENYRW", "R" , "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYRW", "R" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYRW", "R" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYRW", "R" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYNO", "R" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYNO", "W" , "DENY"),
        ("SH_DENYRW", "R" , "SH_DENYNO", "RW", "DENY"),

# All    W |C C C  N N N  N N N  N N N  N N N.
        ("SH_DENYRW", "W" , "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYRW", "W" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYRW", "W" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYRW", "W" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYNO", "R" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYNO", "W" , "DENY"),
        ("SH_DENYRW", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|C C C  N N N  N N N  N N N  N N N
        ("SH_DENYRW", "RW", "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYRW", "RW", "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYRW", "RW", "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYRW", "RW", "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYNO", "R" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYNO", "W" , "DENY"),
        ("SH_DENYRW", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |2 C C  N N N  Y N N  N N N  Y N N
        ("SH_DENYWR", "R" , "SH_COMPAT", "R" , "INT24"), # File RO success, W fails INT24
        ("SH_DENYWR", "R" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYWR", "R" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYWR", "R" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYWR", "R" , "ALLOW"),
        ("SH_DENYWR", "R" , "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYNO", "R" , "ALLOW"),
        ("SH_DENYWR", "R" , "SH_DENYNO", "W" , "DENY"),
        ("SH_DENYWR", "R" , "SH_DENYNO", "RW", "DENY"),

# Write  W |C C C  N N N  N N N  Y N N  Y N N.
        ("SH_DENYWR", "W" , "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYWR", "W" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYWR", "W" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYWR", "W" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYRD", "R" , "ALLOW"),
        ("SH_DENYWR", "W" , "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYNO", "R" , "ALLOW"),
        ("SH_DENYWR", "W" , "SH_DENYNO", "W" , "DENY"),
        ("SH_DENYWR", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|C C C  N N N  N N N  N N N  Y N N
        ("SH_DENYWR", "RW", "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYWR", "RW", "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYWR", "RW", "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYWR", "RW", "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYNO", "R" , "ALLOW"),
        ("SH_DENYWR", "RW", "SH_DENYNO", "W" , "DENY"),
        ("SH_DENYWR", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |C C C  N N N  N Y N  N N N  N Y N
        ("SH_DENYRD", "R" , "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYRD", "R" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYRD", "R" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYRD", "R" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYWR", "W" , "ALLOW"),
        ("SH_DENYRD", "R" , "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYNO", "R" , "DENY"),
        ("SH_DENYRD", "R" , "SH_DENYNO", "W" , "ALLOW"),
        ("SH_DENYRD", "R" , "SH_DENYNO", "RW", "DENY"),

# Read   W |C C C  N N N  N N N  N Y N  N Y N.
        ("SH_DENYRD", "W" , "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYRD", "W" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYRD", "W" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYRD", "W" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYRD", "W" , "ALLOW"),
        ("SH_DENYRD", "W" , "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYNO", "R" , "DENY"),
        ("SH_DENYRD", "W" , "SH_DENYNO", "W" , "ALLOW"),
        ("SH_DENYRD", "W" , "SH_DENYNO", "RW", "DENY"),

#        RW|C C C  N N N  N N N  N N N  N Y N
        ("SH_DENYRD", "RW", "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYRD", "RW", "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYRD", "RW", "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYRD", "RW", "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYNO", "R" , "DENY"),
        ("SH_DENYRD", "RW", "SH_DENYNO", "W" , "ALLOW"),
        ("SH_DENYRD", "RW", "SH_DENYNO", "RW", "DENY"),

# Deny   R |2 C C  N N N  Y Y Y  N N N  Y Y Y
        ("SH_DENYNO", "R" , "SH_COMPAT", "R" , "INT24"), # 2
        ("SH_DENYNO", "R" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYNO", "R" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYNO", "R" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYNO", "R" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYNO", "R" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYNO", "R" , "SH_DENYWR", "R" , "ALLOW"),
        ("SH_DENYNO", "R" , "SH_DENYWR", "W" , "ALLOW"),
        ("SH_DENYNO", "R" , "SH_DENYWR", "RW", "ALLOW"),
        ("SH_DENYNO", "R" , "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYNO", "R" , "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYNO", "R" , "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYNO", "R" , "SH_DENYNO", "R" , "ALLOW"),
        ("SH_DENYNO", "R" , "SH_DENYNO", "W" , "ALLOW"),
        ("SH_DENYNO", "R" , "SH_DENYNO", "RW", "ALLOW"),

# None   W |C C C  N N N  N N N  Y Y Y  Y Y Y.
        ("SH_DENYNO", "W" , "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYNO", "W" , "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYNO", "W" , "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYNO", "W" , "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYNO", "W" , "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYNO", "W" , "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYNO", "W" , "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYNO", "W" , "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYNO", "W" , "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYNO", "W" , "SH_DENYRD", "R" , "ALLOW"),
        ("SH_DENYNO", "W" , "SH_DENYRD", "W" , "ALLOW"),
        ("SH_DENYNO", "W" , "SH_DENYRD", "RW", "ALLOW"),
        ("SH_DENYNO", "W" , "SH_DENYNO", "R" , "ALLOW"),
        ("SH_DENYNO", "W" , "SH_DENYNO", "W" , "ALLOW"),
        ("SH_DENYNO", "W" , "SH_DENYNO", "RW", "ALLOW"),

#        RW|C C C  N N N  N N N  N N N  Y Y Y
        ("SH_DENYNO", "RW", "SH_COMPAT", "R" , "INT24"),
        ("SH_DENYNO", "RW", "SH_COMPAT", "W" , "INT24"),
        ("SH_DENYNO", "RW", "SH_COMPAT", "RW", "INT24"),
        ("SH_DENYNO", "RW", "SH_DENYRW", "R" , "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYRW", "W" , "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYRW", "RW", "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYWR", "R" , "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYWR", "W" , "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYWR", "RW", "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYRD", "R" , "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYRD", "W" , "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYRD", "RW", "DENY"),
        ("SH_DENYNO", "RW", "SH_DENYNO", "R" , "ALLOW"),
        ("SH_DENYNO", "RW", "SH_DENYNO", "W" , "ALLOW"),
        ("SH_DENYNO", "RW", "SH_DENYNO", "RW", "ALLOW"),
    )

    def _test_ds3_share_open_twice_check_single_result(self, results, t):
        m = re.search("FAIL:\('%s', '%s', '%s', '%s', '%s'\)\[.*\]" % t, results)
        if m:
            self.fail(msg=m.group(0))

    def _test_ds3_share_open_twice(self, fstype):
        results = self._test_ds3_share_open_twice_run_all(fstype, self.OPENTESTS)
        for t in self.OPENTESTS:
            with self.subTest(t=t):
                self._test_ds3_share_open_twice_check_single_result(results, t)
        self.assertIn("rem tests complete", results)

    def xtest_mfs_ds3_share_open_twice(self):
        """MFS DOSv3 share open twice"""
        self._test_ds3_share_open_twice("MFS")

    def test_fat_ds3_share_open_twice(self):
        """FAT DOSv3 share open twice"""
        self._test_ds3_share_open_twice("FAT")

    def _test_cpu(self, cpu_vm, cpu_vm_dpmi, cpu_emu):
        testdir = "test-imagedir/dXXXXs/d"
        makedirs(testdir)
        mkfile("testit.bat", """\
test > test.log\r
rem end\r
""")

        symlink("../../../src/tests/test-i386.exe",
                "test-imagedir/dXXXXs/c/test.exe")

        results = self.runDosemu("testit.bat", timeout=20, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "%s"
$_cpu_vm_dpmi = "%s"
$_cpu_emu = "%s"
$_ignore_djgpp_null_derefs = (off)
"""%(cpu_vm, cpu_vm_dpmi, cpu_emu))

        with open("test-imagedir/dXXXXs/c/test.log") as f:
            lines = list(f)
            self.assertEqual(len(lines), 5056)
            # compare or copy to reference file
            try:
                with open("test-i386.log") as g:
                    self.maxDiff = None
                    self.assertEqual(list(g), lines)
            except IOError:
                # copy as reference file
                with open("test-i386.log", "w") as g:
                    g.write("".join(lines))

    def test_cpu_1_vm86native(self):
        """CPU test: native vm86 + native DPMI (i386 only)"""
        if uname()[4] == 'x86_64':
            self.skipTest("x86_64 doesn't support native vm86()")
        self._test_cpu("vm86", "native", "off")

    def test_cpu_2_jitnative(self):
        """CPU test: JIT vm86 + native DPMI"""
        self._test_cpu("emulated", "native", "vm86")

    def test_cpu_simnative(self):
        """CPU test: simulated vm86 + native DPMI"""
        self._test_cpu("emulated", "native", "vm86sim")

    def test_cpu_kvmnative(self):
        """CPU test: KVM vm86 + native DPMI"""
        self._test_cpu("kvm", "native", "off")

    def test_cpu_kvm(self):
        """CPU test: KVM vm86 + KVM DPMI"""
        if not access("/dev/kvm", W_OK|R_OK):
            self.skipTest("Emulation fallback fails for full KVM")
        self._test_cpu("kvm", "kvm", "off")

    def test_cpu_jit(self):
        """CPU test: JIT vm86 + JIT DPMI"""
        self._test_cpu("emulated", "emulated", "full")

    def test_cpu_sim(self):
        """CPU test: simulated vm86 + simulated DPMI"""
        self._test_cpu("emulated", "emulated", "fullsim")


class FRDOS120TestCase(OurTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(FRDOS120TestCase, cls).setUpClass()
        cls.version = "FreeDOS kernel 2042"
        cls.prettyname = "FR-DOS-1.20"
        cls.files = [
            ("kernel.sys", "0709f4e7146a8ad9b8acb33fe3fed0f6da9cc6e0"),
            ("command.com", "0733db7babadd73a1b98e8983c83b96eacef4e68"),
            ("share.com", "cadc29d49115cb3a250f90921cca345e7c427464"),
        ]
        cls.systype = SYSTYPE_FRDOS_NEW
        cls.bootblocks = [
            ("boot-302-4-17.blk", "8b5cfda502e59b067d1e34e993486440cad1d4f7"),
            ("boot-603-4-17.blk", "5c89a0c9c20ba9d581d8bf6969fda88df8ab2d45"),
            ("boot-900-15-17.blk", "523f699a79edde098fceee398b15711fac56a807"),
        ]
        cls.images = [
            ("boot-floppy.img", "c3faba3620c578b6e42a6ef26554cfc9d2ee3258"),
        ]
        cls.actions = {
            "test_fat_fcb_rename_target_exists": KNOWNFAIL,
            "test_fat_fcb_rename_source_missing": KNOWNFAIL,
            "test_fat_fcb_rename_wild_1": KNOWNFAIL,
            "test_fat_fcb_rename_wild_2": KNOWNFAIL,
            "test_fat_fcb_rename_wild_3": KNOWNFAIL,
            "test_mfs_fcb_rename_target_exists": KNOWNFAIL,
            "test_mfs_fcb_rename_source_missing": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_1": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_2": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_3": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_4": KNOWNFAIL,
            "test_fat_fcb_find_wild_1": KNOWNFAIL,
            "test_fat_fcb_find_wild_2": KNOWNFAIL,
            "test_fat_fcb_find_wild_3": KNOWNFAIL,
            "test_mfs_fcb_find_wild_1": KNOWNFAIL,
            "test_mfs_fcb_find_wild_2": KNOWNFAIL,
            "test_mfs_fcb_find_wild_3": KNOWNFAIL,
            "test_fat_ds3_share_open_twice": KNOWNFAIL,
            "test_create_new_psp": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUp(self):
        super(FRDOS120TestCase, self).setUp()

        mkfile("version.bat", "ver /r\r\nrem end\r\n")


class MSDOS622TestCase(OurTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS622TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 6.22"
        cls.prettyname = "MS-DOS-6.22"
        cls.files = [
            ("io.sys", "d697961ca6edaf9a1aafe8b7eb949597506f7f95"),
            ("msdos.sys", "d6a5f54006e69c4407e56677cd77b82395acb60a"),
            ("command.com", "c2179d2abfa241edd388ab875cfabbac89fec44d"),
            ("share.exe", "9e7385cfa91a012638520e89b9884e4ce616d131"),
        ]
        cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
        cls.bootblocks = [
            ("boot-306-4-17.blk", "d40c24ef5f5f9fd6ef28c29240786c70477a0b06"),
            ("boot-615-4-17.blk", "7fc96777727072471dbaab6f817c8d13262260d2"),
            ("boot-900-15-17.blk", "2a0ca1b87b82013fd417542a5ac28e965fb13e7a"),
        ]
        cls.images = [
            ("boot-floppy.img", "14b8310910bf19d6e375298f3b06da7ffdec9932"),
        ]

        cls.setUpClassPost()


class PPDOSGITTestCase(OurTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PPDOSGITTestCase, cls).setUpClass()
        cls.version = "FDPP kernel"
        cls.prettyname = "PP-DOS-GIT"
        cls.actions = {
            "test_floppy_img": UNSUPPORTED,
            "test_floppy_vfs": UNSUPPORTED,
        }

        # Use the default files that FDPP installed
        cls.tarfile = ""

        cls.systype = SYSTYPE_FDPP
#        cls.autoexec = "fdppauto.bat"
        cls.confsys = "fdppconf.sys"

        cls.setUpClassPost()

    def setUp(self):
        super(PPDOSGITTestCase, self).setUp()

        mkfile("version.bat", "ver /r\r\nrem end\r\n")


if __name__ == '__main__':
    unittest.main(testRunner=MyTestRunner, verbosity=2)
