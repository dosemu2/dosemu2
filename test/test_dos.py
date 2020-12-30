#!/usr/bin/python3

import inspect
import unittest

import re

from datetime import datetime
from difflib import unified_diff
from os import statvfs, uname, utime, rename, environ, access, R_OK, W_OK
from os.path import exists, isdir, join
from pathlib import Path
from shutil import copy
from subprocess import call, check_call, CalledProcessError, DEVNULL, TimeoutExpired
from sys import argv, exit, modules
from time import mktime

from common_framework import (BaseTestCase, main, mkstring,
                              IPROMPT, KNOWNFAIL, UNSUPPORTED)

from func_cpu_trap_flag import cpu_trap_flag
from func_ds2_file_seek_tell import ds2_file_seek_tell
from func_ds2_file_seek_read import ds2_file_seek_read
from func_ds2_set_fattrs import ds2_set_fattrs
from func_ds3_lock_two_handles import ds3_lock_two_handles
from func_ds3_lock_readlckd import ds3_lock_readlckd
from func_ds3_lock_readonly import ds3_lock_readonly
from func_ds3_lock_twice import ds3_lock_twice
from func_ds3_lock_writable import ds3_lock_writable
from func_ds3_share_open_access import ds3_share_open_access
from func_ds3_share_open_twice import ds3_share_open_twice

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
            Path(testdir / testname).mkdir()
            if operation == "DeleteNotEmpty":
                self.mkfile("DirNotEm.pty", """hello\r\n""", join(testdir, testname))
        elif operation == "Chdir":
            ename += "dh"
            if nametype == "SFN":
                intnum = "0x3b00"  # chdir
                cwdnum = "0x4700"  # getcwd
            else:
                intnum = "0x713b"
                cwdnum = "0x7147"
            Path(testdir / testname).mkdir()
        else:
            self.fail("Incorrect argument")

        self.mkfile("testit.bat", """\
d:
c:\\%s
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

        testdir = self.mkworkdir('d')

        if nametype == "SFN":
            cwdnum = "0x4700"  # getcwd
        else:
            cwdnum = "0x7147"

        Path(testdir / PRGFIL_LFN).mkdir()

        self.mkfile("testit.bat", """\
d:
cd %s
c:\\%s
rem end
""" % (PRGFIL_SFN, ename), newline="\r\n")

        # compile sources
        self.mkcom_with_gas(ename, r"""
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
        testdir = self.mkworkdir('d')

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

        config += """$_lfn_support = (%s)\n""" % confsw

        self.mkfile("testit.bat", """\
d:
c:\\%s
rem end
""" % ename, newline="\r\n")

        # compile sources

        self.mkcom_with_gas(ename, r"""
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

        testdir = self.mkworkdir('d')
        self.mkfile("shrtname.txt", """hello\r\n""", testdir)
        self.mkfile("long file name.txt", """hello\r\n""", testdir)
        Path(testdir / PRGFIL_LFN).mkdir()

        self.mkfile("testit.bat", """\
%s
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

        testdata = mkstring(32)

        self.mkfile("testit.bat", """\
d:
echo %s > test.fil
c:\\%s
DIR
rem end
""" % (testdata, ename), newline="\r\n")

        # compile sources
        self.mkcom_with_gas(ename, r"""
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

        testdata = mkstring(32)

        self.mkfile("testit.bat", """\
d:
echo %s > test.fil
c:\\%s
DIR
rem end
""" % (testdata, ename), newline="\r\n")

        # compile sources
        self.mkcom_with_gas(ename, r"""
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

        testdata = mkstring(32)

        self.mkfile("testit.bat", """\
d:
c:\\%s
DIR
type test.fil
rem end
""" % ename, newline="\r\n")

        # compile sources
        self.mkcom_with_gas(ename, r"""
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
        self.mkcom_with_gas(ename, r"""
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
        self.mkcom_with_gas(ename, r"""
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
        self.mkcom_with_gas(ename, r"""
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
        testdir = self.mkworkdir('d')

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
""" % self.mkimage("12", cwd=testdir)

        testdata = mkstring(32)

        self.mkfile("testit.bat", """\
d:
echo %s > test.fil
c:\\%s
DIR
rem end
""" % (testdata, ename), newline="\r\n")

        # compile sources
        self.mkcom_with_gas(ename, r"""
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
        testdir = self.mkworkdir('d')

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
""" % self.mkimage("12", cwd=testdir)

        testdata = mkstring(32)

        self.mkfile("testit.bat", """\
d:
echo %s > test.fil
c:\\%s
DIR
rem end
""" % (testdata, ename), newline="\r\n")

        # compile sources
        self.mkcom_with_gas(ename, r"""
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

    def test_fat_ds2_file_seek_set_read(self):
        """FAT DOSv2 file seek set read"""
        ds2_file_seek_read(self, "FAT", "SET")

    def test_mfs_ds2_file_seek_set_read(self):
        """MFS DOSv2 file seek set read"""
        ds2_file_seek_read(self, "MFS", "SET")

    def test_fat_ds2_file_seek_cur_read(self):
        """FAT DOSv2 file seek current read"""
        ds2_file_seek_read(self, "FAT", "CUR")

    def test_mfs_ds2_file_seek_cur_read(self):
        """MFS DOSv2 file seek current read"""
        ds2_file_seek_read(self, "MFS", "CUR")

    def test_fat_ds2_file_seek_end_read(self):
        """FAT DOSv2 file seek end read"""
        ds2_file_seek_read(self, "FAT", "END")

    def test_mfs_ds2_file_seek_end_read(self):
        """MFS DOSv2 file seek end read"""
        ds2_file_seek_read(self, "MFS", "END")

    def test_fat_ds2_file_seek_tell_end_back(self):
        """FAT DOSv2 file seek tell end back"""
        ds2_file_seek_tell(self, "FAT", "ENDBCKSML")

    def test_mfs_ds2_file_seek_tell_end_back(self):
        """MFS DOSv2 file seek tell end back"""
        ds2_file_seek_tell(self, "MFS", "ENDBCKSML")

    def test_fat_ds2_file_seek_tell_end_back_large(self):
        """FAT DOSv2 file seek tell end back large"""
        ds2_file_seek_tell(self, "FAT", "ENDBCKLRG")

    def test_mfs_ds2_file_seek_tell_end_back_large(self):
        """MFS DOSv2 file seek tell end back large"""
        ds2_file_seek_tell(self, "MFS", "ENDBCKLRG")

    def test_fat_ds2_file_seek_tell_end_forward(self):
        """FAT DOSv2 file seek tell end forward"""
        ds2_file_seek_tell(self, "FAT", "ENDFWDSML")

    def test_mfs_ds2_file_seek_tell_end_forward(self):
        """MFS DOSv2 file seek tell end forward"""
        ds2_file_seek_tell(self, "MFS", "ENDFWDSML")

    def test_fat_ds2_file_seek_tell_end_forward_large(self):
        """FAT DOSv2 file seek tell end forward large"""
        ds2_file_seek_tell(self, "FAT", "ENDFWDLRG")

    def test_mfs_ds2_file_seek_tell_end_forward_large(self):
        """MFS DOSv2 file seek tell end forward large"""
        ds2_file_seek_tell(self, "MFS", "ENDFWDLRG")

    def _test_ds2_rename_common(self, fstype, testname):
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
        self.mkcom_with_gas(ename, r"""
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
        self.mkcom_with_gas(ename, r"""
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
        self.mkcom_with_gas(ename, r"""
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
        testdir = self.mkworkdir('d')

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
        self.mkcom_with_gas(ename, r"""
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
        self.mkcom_with_gas(ename, r"""
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
            name = self.mkimage("12", cwd=testdir)
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

        self.mkfile("testit.bat", """\
c:\\%s %s
rem end
""" % (ename, cmdline), newline="\r\n")

        # compile sources
        self.mkcom_with_gas(ename, r"""
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
        with open(self.logfiles['log'][0], "r") as f:
            for line in f:
                if "system type is" in line:
                    systypeline = line
                    break

        self.assertIn(self.systype, systypeline)

    def _test_memory_dpmi_ecm(self, name):
        ename = "%s.com" % name
        edir = self.topdir / "test" / "ecm" / "dpmitest"

        call(["make", "--quiet", "-C", str(edir), ename])
        copy(edir / ename, self.workdir)

        self.mkfile("testit.bat", """\
c:\\%s
rem end
""" % name, newline="\r\n")

        return self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

    def test_memory_dpmi_ecm_alloc(self):
        """Memory DPMI (ECM) alloc"""
        results = self._test_memory_dpmi_ecm("dpmialoc")
# About to Execute : dpmialoc.com
# Protected mode breakpoint at 0221h.
# 32-bit code segment breakpoint at 0233h.
# Return from child process at 0403h.

# Welcome in 32-bit protected mode.
# DPMI allocation at 02121000h.
# DPMI allocation entrypoint at 00EFh:00000000h.
# Real mode procedure called at 00EFh:00000044h.
# DPMI allocation exit at 00EFh:0000006Ch.
# Hello from DPMI memory section!
# Calling real mode procedure which called callback successful.
# Child process terminated okay, back in real mode.

        self.assertRegex(results, r"Protected mode breakpoint at")
        self.assertRegex(results, r"32-bit code segment breakpoint at")
        self.assertRegex(results, r"Return from child process at")
        self.assertRegex(results, r"Welcome in 32-bit protected mode")
        self.assertRegex(results, r"Hello from DPMI memory section!")
        self.assertRegex(results, r"Calling real mode procedure which called callback successful")
        self.assertRegex(results, r"Child process terminated okay, back in real mode")
        self.assertNotIn("fail", results)

    def test_memory_dpmi_ecm_mini(self):
        """Memory DPMI (ECM) mini"""
        results = self._test_memory_dpmi_ecm("dpmimini")

# About to Execute : dpmimini.com
# Protected mode breakpoint at 015Ah.
# 32-bit code segment breakpoint at 016Ch.
#
# Welcome in 32-bit protected mode.

        self.assertRegex(results, r"Protected mode breakpoint at")
        self.assertRegex(results, r"32-bit code segment breakpoint at")
        self.assertRegex(results, r"Welcome in 32-bit protected mode")
        self.assertNotIn("fail", results)

    def test_memory_dpmi_ecm_modeswitch(self):
        """Memory DPMI (ECM) Mode Switch"""
        results = self._test_memory_dpmi_ecm("dpmims")

# About to Execute : dpmims.com
# Protected mode breakpoint at 015Eh.
#
# Welcome in protected mode.
# Mode-switched to real mode.
# In protected mode again.

        self.assertRegex(results, r"Protected mode breakpoint at")
        self.assertRegex(results, r"Welcome in protected mode")
        self.assertRegex(results, r"Mode-switched to real mode")
        self.assertRegex(results, r"In protected mode again")
        self.assertNotIn("fail", results)

    def test_memory_dpmi_ecm_psp(self):
        """Memory DPMI (ECM) psp"""
        results = self._test_memory_dpmi_ecm("dpmipsp")

# About to Execute : dpmipsp.com
# Protected mode breakpoint at 0221h.
# 32-bit code segment breakpoint at 0233h.
# Real mode procedure called at 0275h.
# Return from child process at 02FCh.
#
# Welcome in 32-bit protected mode.
# Calling real mode procedure which called callback successful.
# Child process terminated okay, back in real mode.

        self.assertRegex(results, r"Protected mode breakpoint at")
        self.assertRegex(results, r"32-bit code segment breakpoint at")
        self.assertRegex(results, r"Real mode procedure called at")
        self.assertRegex(results, r"Return from child process at")
        self.assertRegex(results, r"Welcome in 32-bit protected mode")
        self.assertRegex(results, r"Calling real mode procedure which called callback successful")
        self.assertRegex(results, r"Child process terminated okay, back in real mode")
        self.assertNotIn("fail", results)

    def _test_memory_dpmi_japheth(self, switch):
        self.unTarOrSkip("VARIOUS.tar", [
            ("dpmihxrt218.exe", "65fda018f4422c39dbf36860aac2c537cfee466b"),
        ])
        rename(self.workdir / "dpmihxrt218.exe", self.workdir / "dpmi.exe")

        self.mkfile("testit.bat", """\
c:\\dpmi %s
rem end
""" % switch, newline="\r\n")

        return self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""", timeout=60)

    def test_memory_dpmi_japheth(self):
        """Memory DPMI (Japheth) ''"""
        results = self._test_memory_dpmi_japheth("")

#Cpu is in V86-mode
#Int 15h, ax=e801h, extended memory: 8256 kB
#int 15h, ax=e820h, available memory at 000100000, size 8256 kB
#XMS v3.0 host found, largest free block: 8192 kB
#No VCPI host found
#DPMI v0.90 host found, cpu: 05, support of 32-bit clients: 1
#entry initial switch to protected-mode: F000:F500
#size task-specific memory: 230 paragraphs
#now in protected mode, client CS/SS/DS/FS/GS: A7/AF/AF/0/0
#Eflags=206, ES (=PSP): B7 (environment: BF, parent PSP segm: 431)
#GDTR: 17.A308100 IDTR: 7FF.A308200 LDTR: 0 TR: 0
#DPMI version flags: 5
#master/slave PICs base: 08/70
#state save protected-mode: 97:1, real-mode: F000:F514
#size state save buffer: 52 bytes
#raw jump to real-mode: 97:0, protected-mode: F000:F513
#largest free/lockable memory block (kB): 131004/131004
#free unlocked (=virtual) memory (kB): 131004
#total/free address space (kB): 32768/32768
#total/free physical memory (kB): 131072/131004
#Coprocessor status: 4D
#vendor: DOSEMU Version 2.0, version: 0.90
#capabilities: 78
#vendor 'MS-DOS' API entry: 97:130
#'MS-DOS' API, ax=100h (get LDT selector): E7

        self.assertRegex(results, r"DPMI v0.90 host found, cpu: \d+, support of 32-bit clients: 1")
        self.assertRegex(results, r"DPMI version flags: 5")
        self.assertRegex(results, r"vendor: DOSEMU Version 2.0, version: 0.90")
        self.assertRegex(results, r"capabilities: 78")

    def test_memory_dpmi_japheth_c(self):
        """Memory DPMI (Japheth) '-c'"""
        results = self._test_memory_dpmi_japheth("-c")
        self.assertRegex(results, r"time.*CLI/STI: \d+ ms")
        self.assertRegex(results, r"time.*interrupts via DPMI: \d+ ms")

    def test_memory_dpmi_japheth_i(self):
        """Memory DPMI (Japheth) '-i'"""
        results = self._test_memory_dpmi_japheth("-i")
        self.assertRegex(results, r"time.*IN 21: \d+ ms")

    def test_memory_dpmi_japheth_m(self):
        """Memory DPMI (Japheth) '-m'"""
        results = self._test_memory_dpmi_japheth("-m")
        t = re.search(r"alloc largest mem block \(size=(\d+) kB\) returned handle [0-9a-fA-F]+, base \d+", results)
        self.assertIsNotNone(t, "Unable to parse memory block size")
        size = int(t.group(1))
        self.assertGreater(size, 128000, results)

    def test_memory_dpmi_japheth_r(self):
        """Memory DPMI (Japheth) '-r'"""
        results = self._test_memory_dpmi_japheth("-r")
        self.assertRegex(results, r"time.*INT 69h: \d+ ms")
        self.assertRegex(results, r"time.*INT 31h, AX=0300h \(Sim INT 69h\): \d+ ms")
        self.assertRegex(results, r"time.*real-mode callback: \d+ ms")
        self.assertRegex(results, r"time.*raw mode switches PM->RM->PM: \d+ ms")

    def test_memory_dpmi_japheth_t(self):
        """Memory DPMI (Japheth) '-t'"""
        results = self._test_memory_dpmi_japheth("-t")

#C:\>c:\dpmi -t
#allocated rm callback FF10:214, rmcs=AF:20E4
#calling rm proc [53A:8B6], rm cx=1
#  inside rm proc, ss:sp=7A8:1FC, cx=1
#  calling rm callback FF10:214
#    inside rm callback, ss:esp=9F:EFF4, ds:esi=F7:1FC
#    es:edi=AF:20E4, rm ss:sp=7A8:1FC, rm cx=1
#    calling rm proc [53A:8B6]
#      inside rm proc, ss:sp=7A8:1F8, cx=2
#      calling rm callback FF10:214
#        inside rm callback, ss:esp=9F:EFE0, ds:esi=F7:1F8
#        es:edi=AF:20E4, rm ss:sp=7A8:1F8, rm cx=2
#        exiting
#      back in rm proc, ss:sp=7A8:1F8; exiting
#    back in rm callback, rm ss:sp=7A8:1FC, rm cx=2; exiting
#  back in rm proc, ss:sp=7A8:1FC; exiting
#back in protected-mode, rm ss:sp=7A8:200, rm cx=2

        self.assertRegex(results, re.compile(r"^allocated rm callback", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^  inside rm proc", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^    inside rm callback", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^      inside rm proc", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^        inside rm callback", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^      back in rm proc", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^    back in rm callback", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^  back in rm proc", re.MULTILINE))
        self.assertRegex(results, re.compile(r"^back in protected-mode", re.MULTILINE))

    def test_memory_ems_borland(self):
        """Memory EMS (Borland)"""

        self.unTarOrSkip("VARIOUS.tar", [
            ("emstest.com", "d0a07e97905492a5cb9d742513cefeb36d09886d"),
        ])

        self.mkfile("testit.bat", """\
c:\\emstest
rem end
""", newline="\r\n")

        interactions = [
            ("Esc to abort.*:", "\n"),
            ("Esc to abort.*:", "\n"),
        ]

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""", interactions=interactions)

        pt1start = results.index("  PART ONE")
        pt2start = results.index("  PART TWO")
        pt3start = results.index("  PART THREE")

        pt1 = results[pt1start:pt2start-1]
        pt2 = results[pt2start:pt3start-1]
        pt3 = results[pt3start:-1]

#  PART ONE - EMS DETECTION
#  ------------------------
#
#Expanded Memory Manager version 64.
#
#INT 67 Handler Address: C102:007B
#Window Segment Address: E000
#Total no. of Pages    :  536 (8576k)
#
#Process ID   Pages allocated
#----------------------------
#   0000         24 ( 384k)
#   0001          4 (  64k)
#   Free        508 (8128k)
#----------------------------
#Press Esc to abort or any other key to continue:

        self.assertRegex(pt1, r"Expanded Memory Manager version \d+")
        self.assertRegex(pt1, r"INT 67 Handler Address: [0-9A-F]{4}:[0-9A-F]{4}")
        self.assertIn("Window Segment Address: E000", pt1)
        self.assertRegex(pt1, r"Free\s+\d+ \(\d+k\)")

#  PART TWO - BASIC EMS FUNCTIONS
#  ------------------------------
#
#     Allocating 128 pages EMS memory: OK.
#             EMS handle (Process ID): 0002
#      Saving page map and map window: OK.
#              Initializing 128 pages: OK.
#                  Checking 128 pages: OK.
#                  Restoring page map: OK.
#  De-allocating 128 pages EMS memory: OK.
#Press Esc to abort or any other key to continue:

        self.assertIn("Allocating 128 pages EMS memory: OK", pt2)
        self.assertRegex(pt2, r"EMS handle \(Process ID\): \d+")
        self.assertIn("Saving page map and map window: OK", pt2)
        self.assertRegex(pt2, r"Initializing 128 pages: ([\s\d\x08]{6})+OK")
        self.assertRegex(pt2, r"Checking 128 pages: ([\s\d\x08]{6})+OK")
        self.assertIn("Restoring page map: OK", pt2)
        self.assertIn("De-allocating 128 pages EMS memory: OK", pt2)


#  PART THREE - EMS I/O FUNCTIONS
#  ------------------------------
#
#     Allocating 128 pages EMS memory: OK.
#             EMS handle (Process ID): 0002
#      Saving page map and map window: OK.
#              Initializing 128 pages: OK.
#      Creating temp file EMSTEST.$$$: OK.
# 250 random EMS I/O with disk access: OK.
#       Erasing temp file EMSTEST.$$$: OK.
#                  Restoring page map: OK.
#  De-allocating 128 pages EMS memory: OK.

        self.assertIn("Allocating 128 pages EMS memory: OK", pt3)
        self.assertRegex(pt3, r"EMS handle \(Process ID\): \d+")
        self.assertIn("Saving page map and map window: OK", pt3)
        self.assertRegex(pt3, r"Initializing 128 pages: ([\s\d\x08]{6})+OK")
        self.assertIn("Creating temp file EMSTEST.$$$: OK", pt3)
        self.assertRegex(pt3, r"250 random EMS I/O with disk access: ([\s\d/\x08]{20})+OK")
        self.assertIn("Erasing temp file EMSTEST.$$$: OK", pt3)
        self.assertIn("Restoring page map: OK", pt3)
        self.assertIn("De-allocating 128 pages EMS memory: OK", pt3)

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
        self.mkfile(self.confsys, """\
DOS=UMB,HIGH
lastdrive=Z
files=40
stacks=0,0
buffers=10
device=a:\\dosemu\\emufs.sys
device=a:\\dosemu\\umb.sys
devicehigh=a:\\dosemu\\ems.sys
devicehigh=a:\\dosemu\\cdrom.sys
install=a:\\dosemu\\emufs.com
shell=command.com /e:1024 /k %s
""" % self.autoexec, newline="\r\n")

        self.mkfile(self.autoexec, """\
prompt $P$G
path a:\\bin;a:\\gnu;a:\\dosemu
system -s DOSEMU_VERSION
@echo %s
""" % IPROMPT, newline="\r\n")

        results = self.runDosemu("version.bat", config="""\
$_hdimage = ""
$_floppy_a = "dXXXXs/c:fiveinch_360"
$_bootdrive = "a"
""")

        self.assertIn(self.version, results)

    def test_three_drives_vfs(self):
        """Three vfs directories configured"""
        # C exists as part of standard test
        self.mkworkdir('d')
        self.mkworkdir('e')

        results = self.runDosemu("version.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 dXXXXs/e:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertIn(self.version, results)   # Just to check we booted

    def _test_fat_img_d_writable(self, fat):
        self.mkfile("testit.bat", """\
D:
mkdir test
echo hello > hello.txt
DIR
rem end
""", newline="\r\n")

        testdir = self.mkworkdir('d')

        name = self.mkimage(fat, cwd=testdir)

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
        self.mkfile("testit.bat", "lredir\r\nrem end\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

# C:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE

        self.assertRegex(results, r"C: = .*LINUX\\FS")

    def test_mfs_lredir_command(self):
        """MFS lredir command redirection"""
        self.mkfile("testit.bat", """\
lredir X: LINUX\\FS\\tmp
lredir
rem end
""", newline="\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "/tmp"
""")

# A:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE
# X: = LINUX\FS\tmp\        attrib = READ/WRITE

        self.assertRegex(results, r"X: = .*LINUX\\FS\\tmp")

    def test_mfs_lredir_command_no_perm(self):
        """MFS lredir command redirection permission fail"""
        self.mkfile("testit.bat", """\
lredir X: LINUX\\FS\\tmp
lredir
rem end
""", newline="\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

# A:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE
# X: = LINUX\FS\tmp\        attrib = READ/WRITE

        self.assertRegex(results, r"Error 5 \(access denied\) while redirecting drive X:")

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

        testdir = self.mkworkdir('d')

        self.mkfile("testit.bat", """\
%s
d:
c:\\mfsfind
rem end
""" % disablelfn, newline="\r\n")

        for name in testnames:
            self.mkfile(name, "some test text", dname=testdir)

        # compile sources
        self.mkexe_with_djgpp("mfsfind", r"""
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
        testdir = self.mkworkdir('d')

        self.mkfile("testit.bat", """\
%s
d:
c:\\mfsread %s %s
rem end
""" % (disablelfn, testname, testdata), newline="\r\n")

        self.mkfile(testname, testdata, dname=testdir)

        # compile sources
        self.mkexe_with_djgpp("mfsread", r"""
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
        testdir = self.mkworkdir('d')

        self.mkfile("testit.bat", """\
%s
d:
c:\\%s %s %s
rem end
""" % (disablelfn, ename, testname, testdata), newline="\r\n")

        if operation != "create" and operation != "createreadonly":
            self.mkfile(testname, testprfx, dname=testdir)

        # compile sources
        self.mkexe_with_djgpp(ename, r"""
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
        if not fstype in ["MFS", "FAT"]:
            self.fail("Incorrect argument")

        self.mkfile("testit.bat", """\
c:\\lfnvinfo D:\\
rem end
""", newline="\r\n")

        testdir = self.mkworkdir('d')
        self.mkfile("foo.dat", "some content", dname=testdir)

        name = self.mkimage("16", cwd=testdir)

        # compile sources
        self.mkexe_with_djgpp("lfnvinfo", r"""
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

        self.mkfile("testit.bat", """\
c:\\fat32dif %s
rem end
""" % path, newline="\r\n")

        # compile sources
        self.mkexe_with_djgpp("fat32dif", r"""\
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

        self.assertNotIn("Call failed", results)

        fsinfo = statvfs(self.workdir)
        lfs_total = fsinfo.f_blocks * fsinfo.f_bsize
        lfs_avail = fsinfo.f_bavail * fsinfo.f_bsize

        t = re.search(r'total_bytes\((\d+)\)', results)
        self.assertIsNotNone(t, "Unable to parse 'total_bytes'")
        dfs_total = int(t.group(1))
        a = re.search(r'avail_bytes\((\d+)\)', results)
        self.assertIsNotNone(a, "Unable to parse 'avail_bytes'")
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

        self.mkfile("testit.bat", """\
c:\\int21dif %s
rem end
""" % path, newline="\r\n")

        # compile sources
        self.mkexe_with_djgpp("int21dif", r"""\
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

        self.assertNotIn("Call failed", results)

        fsinfo = statvfs(self.workdir)
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

        self.mkfile("testit.bat", """\
lredir X: \\\\linux\\fs%s
c:\\lfnfilei X:\\%s
rem end
""" % (dpath, fpath), newline="\r\n")

        # compile sources
        self.mkexe_with_djgpp("lfnfilei", r"""\
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
$_lredir_paths = "/tmp"
""")

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
        testdir = self.mkworkdir('d')

        self.mkfile("testit.bat", """\
d:
c:\\getftime %s
rem end
""" % tstype, newline="\r\n")

        # compile sources
        self.mkexe_with_djgpp("getftime", r"""

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

        def settime(fn, Y, M, D, h, m, s):
            date = datetime(year=Y, month=M, day=D,
                            hour=h, minute=m, second=s)
            modTime = mktime(date.timetuple())
            utime(join(testdir, fname), (modTime, modTime))

        YEARS = range(1980, 2099 + 1) if tstype == "DATE" else []
        for i in YEARS:
            fname = "%08d.YR" % i
            self.mkfile(fname, "some content", dname=testdir)
            settime(fname, i, 1, 1, 0, 0, 0)

        MONTHS = range(1, 12 + 1) if tstype == "DATE" else []
        for i in MONTHS:
            fname = "%08d.MN" % i
            self.mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, i, 1, 0, 0, 0)

        DAYS = range(1, 31 + 1) if tstype == "DATE" else []
        for i in DAYS:
            fname = "%08d.DY" % i
            self.mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, i, 0, 0, 0)

        HOURS = range(0, 23 + 1) if tstype == "TIME" else []
        for i in HOURS:
            fname = "%08d.HR" % i
            self.mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, 1, i, 0, 0)

        MINUTES = range(0, 59 + 1) if tstype == "TIME" else []
        for i in MINUTES:
            fname = "%08d.MI" % i
            self.mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, 1, 0, i, 0)

        SECONDS = range(0, 59 + 1, 2) if tstype == "TIME" else []
        for i in SECONDS:
            fname = "%08d.SC" % i
            self.mkfile(fname, "some content", dname=testdir)
            settime(fname, 1980, 1, 1, 0, 0, i)

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
        testdir = self.mkworkdir('d')

        self.mkfile("testit.bat", """\
d:
c:\\setftime
rem end
""", newline="\r\n")

        # compile sources
        self.mkexe_with_djgpp("setftime", r"""
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

        self.assertNotIn("FAIL:", results)

    def test_mfs_ds2_set_ftime(self):
        """MFS DOSv2 set file time"""
        self._test_ds2_set_ftime("MFS")

    def test_fat_ds2_set_ftime(self):
        """FAT DOSv2 set file time"""
        self._test_ds2_set_ftime("FAT")

    def xtest_mfs_ds2_set_fattrs(self):
        """MFS DOSv2 set file attrs"""
        tests = ('RDONLY', 'HIDDEN', 'SYSTEM') # Broken for now
        for t in tests:
            with self.subTest(t=t):
                ds2_set_fattrs(self, "MFS", t)

    def test_fat_ds2_set_fattrs(self):
        """FAT DOSv2 set file attrs"""
        tests = ('RDONLY', 'HIDDEN', 'SYSTEM')
        for t in tests:
            with self.subTest(t=t):
                ds2_set_fattrs(self, "FAT", t)

    def test_mfs_ds3_lock_readonly(self):
        """MFS DOSv3 lock file readonly"""
        ds3_lock_readonly(self, "MFS")

    def test_fat_ds3_lock_readonly(self):
        """FAT DOSv3 lock file readonly"""
        ds3_lock_readonly(self, "FAT")

    def test_mfs_ds3_lock_readlckd(self):
        """MFS DOSv3 lock file read locked"""
        ds3_lock_readlckd(self, "MFS")

    def test_fat_ds3_lock_readlckd(self):
        """FAT DOSv3 lock file read locked"""
        ds3_lock_readlckd(self, "FAT")

    def test_mfs_ds3_lock_two_handles(self):
        """MFS DOSv3 lock file lock with two handles"""
        ds3_lock_two_handles(self, "MFS")

    def test_fat_ds3_lock_two_handles(self):
        """FAT DOSv3 lock file lock with two handles"""
        ds3_lock_two_handles(self, "FAT")

    def test_mfs_ds3_lock_twice(self):
        """MFS DOSv3 lock file twice"""
        ds3_lock_twice(self, "MFS")

    def test_fat_ds3_lock_twice(self):
        """FAT DOSv3 lock file twice"""
        ds3_lock_twice(self, "FAT")

    def test_mfs_ds3_lock_writable(self):
        """MFS DOSv3 lock file writable"""
        ds3_lock_writable(self, "MFS")

    def test_fat_ds3_lock_writable(self):
        """FAT DOSv3 lock file writable"""
        ds3_lock_writable(self, "FAT")

    def test_mfs_ds3_share_open_twice(self):
        """MFS DOSv3 share open twice"""
        ds3_share_open_twice(self, "MFS")

    def test_fat_ds3_share_open_twice(self):
        """FAT DOSv3 share open twice"""
        ds3_share_open_twice(self, "FAT")

    def test_mfs_ds3_share_open_delete_ds2(self):
        """MFS DOSv3 share open delete DOSv2"""
        ds3_share_open_access(self, "MFS", "DELPTH")

    def test_fat_ds3_share_open_delete_ds2(self):
        """FAT DOSv3 share open delete DOSv2"""
        ds3_share_open_access(self, "FAT", "DELPTH")

    def test_mfs_ds3_share_open_delete_fcb(self):
        """MFS DOSv3 share open delete FCB"""
        ds3_share_open_access(self, "MFS", "DELFCB")

    def test_fat_ds3_share_open_delete_fcb(self):
        """FAT DOSv3 share open delete FCB"""
        ds3_share_open_access(self, "FAT", "DELFCB")

    def test_mfs_ds3_share_open_rename_ds2(self):
        """MFS DOSv3 share open rename DOSv2"""
        ds3_share_open_access(self, "MFS", "RENPTH")

    def test_fat_ds3_share_open_rename_ds2(self):
        """FAT DOSv3 share open rename DOSv2"""
        ds3_share_open_access(self, "FAT", "RENPTH")

    def test_mfs_ds3_share_open_rename_fcb(self):
        """MFS DOSv3 share open rename FCB"""
        ds3_share_open_access(self, "MFS", "RENFCB")

    def test_fat_ds3_share_open_rename_fcb(self):
        """FAT DOSv3 share open rename FCB"""
        ds3_share_open_access(self, "FAT", "RENFCB")

    def test_mfs_ds3_share_open_setfattrs(self):
        """MFS DOSv3 share open set file attrs DOSv2"""
        ds3_share_open_access(self, "MFS", "SETATT")

    def test_fat_ds3_share_open_setfattrs(self):
        """FAT DOSv3 share open set file attrs DOSv2"""
        ds3_share_open_access(self, "FAT", "SETATT")

    def _test_cpu(self, cpu_vm, cpu_vm_dpmi, cpu_emu):
        edir = self.topdir / "test" / "cpu"

        try:
            check_call(['make', '-C', str(edir), 'all'], stdout=DEVNULL, stderr=DEVNULL)
        except CalledProcessError as e:
            self.skipTest("Unable to build test binaries '%s'" % e)
        copy(edir / "dosbin.exe", self.workdir / "dosbin.exe")

        reffile = "reffile.log"
        dosfile = "dosfile.log"

        # reference file
        try:
            with open(self.topdir / reffile, "w") as f:
               check_call([str(edir / 'native32'), '--common-tests'],
                            stdout=f, stderr=DEVNULL)
        except CalledProcessError as e:
            self.skipTest("Host reference file error '%s'" % e)

        refoutput = []
        try:
            with open(self.topdir / reffile, "r") as f:
                refoutput = f.readlines()
        except FileNotFoundError as e:
            self.fail("Could not open reference file error '%s'" % e)

        # output from dos under test
        self.mkfile("testit.bat", """\
dosbin --common-tests > %s
rem end
""" % dosfile, newline="\r\n")

        self.runDosemu("testit.bat", timeout=20, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "%s"
$_cpu_vm_dpmi = "%s"
$_cpuemu = (%i)
$_ignore_djgpp_null_derefs = (off)
"""%(cpu_vm, cpu_vm_dpmi, cpu_emu))

        dosoutput = []
        try:
            with open(self.workdir / dosfile, "r") as f:
                dosoutput = f.readlines()
        except FileNotFoundError:
            self.fail("DOS output file not found")

        # Compare DOS output to reference file
        if dosoutput != refoutput:
            diff = unified_diff(refoutput, dosoutput, fromfile=reffile, tofile=dosfile)
            self.fail('differences detected\n' + ''.join(list(diff)))

    def test_cpu_1_vm86native(self):
        """CPU test: native vm86 + native DPMI (i386 only)"""
        if uname()[4] == 'x86_64':
            self.skipTest("x86_64 doesn't support native vm86()")
        self._test_cpu("vm86", "native", 0)

    def test_cpu_2_jitnative(self):
        """CPU test: JIT vm86 + native DPMI"""
        self._test_cpu("emulated", "native", 0)

    def test_cpu_jitkvm(self):
        """CPU test: JIT vm86 + KVM DPMI"""
        self._test_cpu("emulated", "kvm", 0)

    def test_cpu_simnative(self):
        """CPU test: simulated vm86 + native DPMI"""
        self._test_cpu("emulated", "native", 1)

    def test_cpu_simkvm(self):
        """CPU test: simulated vm86 + KVM DPMI"""
        self._test_cpu("emulated", "kvm", 1)

    def test_cpu_kvmnative(self):
        """CPU test: KVM vm86 + native DPMI"""
        self._test_cpu("kvm", "native", 0)

    def test_cpu_kvm(self):
        """CPU test: KVM vm86 + KVM DPMI"""
        if not access("/dev/kvm", W_OK|R_OK):
            self.skipTest("Emulation fallback fails for full KVM")
        self._test_cpu("kvm", "kvm", 0)

    def test_cpu_kvmjit(self):
        """CPU test: KVM vm86 + JIT DPMI"""
        self._test_cpu("kvm", "emulated", 0)

    def test_cpu_kvmsim(self):
        """CPU test: KVM vm86 + simulated DPMI"""
        self._test_cpu("kvm", "emulated", 1)

    def test_cpu_jit(self):
        """CPU test: JIT vm86 + JIT DPMI"""
        self._test_cpu("emulated", "emulated", 0)

    def test_cpu_sim(self):
        """CPU test: simulated vm86 + simulated DPMI"""
        self._test_cpu("emulated", "emulated", 1)

    def test_cpu_trap_flag_emulated(self):
        """CPU Trap Flag emulated"""
        cpu_trap_flag(self, 'emulated')

    def test_cpu_trap_flag_kvm(self):
        """CPU Trap Flag KVM"""
        cpu_trap_flag(self, 'kvm')

    def xtest_libi86_build(self):
        """libi86 build and test script"""
        if environ.get("SKIP_EXPENSIVE"):
            self.skipTest("expensive test")

        i86repo = 'https://github.com/tkchia/libi86.git'
        i86root = self.imagedir / 'i86root.git'

        call(["git", "clone", "-q", "--single-branch", "--branch=20201003", i86repo, str(i86root)],
                stdout=DEVNULL, stderr=DEVNULL)

        self.mkfile("dosemu.conf", """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""", dname=self.imagedir, mode="a")

        dose = self.topdir / "bin" / "dosemu"
        opts = '-f {0}/dosemu.conf -n --Fimagedir {0}'.format(self.imagedir)

        build = i86root / "build-xxxx"
        build.mkdir()

        if environ.get("CC"):
            del environ["CC"]

        # logfiles
        #  - dosemu log not applicable as libi86 test suite invokes dosemu multiple times
        #  - expect log is not present as it's run non-interactively
        #  - libi86 test suite has its own log file called 'testsuite.log'
        #    which contains configure, build, and test
        del self.logfiles['log']
        del self.logfiles['xpt']
        self.logfiles['suite'] = (str(build / "tests" / "testsuite.log"), "testsuite.log")

        check_call(['../configure', '--host=ia16-elf', '--disable-elks-libc'],
                        cwd=build, env=environ, stdout=DEVNULL, stderr=DEVNULL)

        check_call(['make'], cwd=build, env=environ, stdout=DEVNULL, stderr=DEVNULL)

        try:
            environ["TESTSUITEFLAGS"] = "--x-test-underlying --x-with-dosemu=%s --x-with-dosemu-options=\"%s\"" % (dose, opts)
            starttime = datetime.utcnow()
            check_call(['make', 'check'],
                    cwd=build, env=environ, timeout=600, stdout=DEVNULL, stderr=DEVNULL)
            self.duration = datetime.utcnow() - starttime
        except CalledProcessError:
            raise self.failureException("Test error") from None
        except TimeoutExpired:
            raise self.failureException("Test timeout") from None

    def test_pcmos_build(self):
        """PC-MOS build script"""
        if environ.get("SKIP_EXPENSIVE"):
            self.skipTest("expensive test")

        mosrepo = 'https://github.com/roelandjansen/pcmos386v501.git'
        mosroot = self.imagedir / 'pcmos.git'

        call(["git", "clone", "-q", "--depth=1", mosrepo, str(mosroot)])

        outfiles = [mosroot / 'SOURCES/src/latest' / x for x in [
            '$286n.sys', '$386.sys', '$all.sys', '$arnet.sys',
            '$charge.sys', '$ems.sys', '$gizmo.sys', '$kbbe.sys',
            '$kbcf.sys', '$kbdk.sys', '$kbfr.sys', '$kbgr.sys',
            '$kbit.sys', '$kbla.sys', '$kbnl.sys', '$kbno.sys',
            '$kbpo.sys', '$kbsf.sys', '$kbsg.sys', '$kbsp.sys',
            '$kbsv.sys', '$kbuk.sys', 'minbrdpc.sys', 'mosdd7f.sys',
            '$$mos.sys', '$mouse.sys', '$netbios.sys', '$pipe.sys',
            '$ramdisk.sys', '$serial.sys', '$$shell.sys']]

        for outfile in outfiles:
            outfile.unlink(missing_ok=True)

        # Run the equivalent of the MOSROOT/build.sh script from MOSROOT
        # Note:
        #     We have to avoid runDosemu() as this test is non-interactive
        args = ["-K", r".:SOURCES\src", "-E", "MAKEMOS.BAT", r"path=%D\bin;%O"]
        results = self.runDosemuCmdline(args, cwd=str(mosroot), timeout=300, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")
        if results == 'Timeout':
            raise self.failureException("Timeout:\n")

        missing = []
        for outfile in outfiles:
            if not outfile.exists():
                missing.append(str(outfile.relative_to(mosroot)))
        if len(missing):
            msg = "Output file(s) missing %s\n" % str(missing)
            raise self.failureException(msg)


class DRDOS701TestCase(OurTestCase, unittest.TestCase):
    # OpenDOS 7.01

    priority = 3

    @classmethod
    def setUpClass(cls):
        super(DRDOS701TestCase, cls).setUpClass()
        cls.version = "Caldera OpenDOS 7.01"
        cls.prettyname = "DR-DOS-7.01"
        cls.files = [
            ("ibmbio.com", "61211eb63329a67fdd9d336271f06e1bfdab2b6f"),
            ("ibmdos.com", "52e71c8e9d74100f138071acaecdef4a79b67d3c"),
            ("command.com", "4bc38f973b622509aedad8c6af0eca59a2d90fca"),
            ("share.exe", "10f2c0e2cabe98617faa017806c80476b3b6c1e1"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.autoexec = "dautoemu.bat"
        cls.confsys = "dconfig.sys"
        cls.bootblocks = [
            ("boot-306-4-17.blk", "1151ab9a3429163ac3ddf55b88d81359cb6975e5"),
            ("boot-615-4-17.blk", "a18ee96e63e384b766bafc4ff936e4087c31bf59"),
            ("boot-900-15-17.blk", "2ea4ea747f6e62a8ea46f14f6c9af1ad6fd0126b"),
        ]
        cls.images = [
            ("boot-floppy.img", "d38fb2dba30185ce510cf3366bd71a1cbc2635da"),
        ]
        cls.actions = {
            "test_fat_ds3_share_open_setfattrs": KNOWNFAIL,
            "test_fat_fcb_rename_simple": KNOWNFAIL,
            "test_fat_fcb_rename_wild_1": KNOWNFAIL,
            "test_fat_fcb_rename_wild_2": KNOWNFAIL,
            "test_fat_fcb_rename_wild_3": KNOWNFAIL,
            "test_fat_fcb_rename_wild_4": KNOWNFAIL,
            "test_mfs_fcb_rename_simple": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_1": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_2": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_3": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_4": KNOWNFAIL,
            "test_mfs_sfn_truename": KNOWNFAIL,
            "test_floppy_vfs": KNOWNFAIL,
            "test_pcmos_build": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        with open(join("src/bindist", self.autoexec), "r") as f:
            contents = f.read()
            self.mkfile(self.autoexec, re.sub(r"[Dd]:\\", r"c:\\", contents), newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands/dosemu")

        # Use the (almost) standard shipped config
        with open(join("src/bindist", self.confsys), "r") as f:
            contents = f.read()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

    def setUpDosVersion(self):
        self.mkfile("version.bat", "ver\r\nrem end\r\n")


class FRDOS120TestCase(OurTestCase, unittest.TestCase):

    priority = 3

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
        cls.autoexec = "fdautoem.bat"
        cls.confsys = "fdconfig.sys"
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
            "test_mfs_lredir_command": KNOWNFAIL,
            "test_mfs_lredir_command_no_perm": KNOWNFAIL,
            "test_lfn_file_info_mfs_6GiB": KNOWNFAIL,
            "test_lfn_file_info_mfs_1MiB": KNOWNFAIL,
            "test_fat_ds3_lock_writable": KNOWNFAIL,
            "test_fat_ds3_lock_readlckd": KNOWNFAIL,
            "test_fat_ds3_lock_two_handles": KNOWNFAIL,
            "test_fat_ds3_share_open_twice": KNOWNFAIL,
            "test_fat_ds3_share_open_delete_ds2": KNOWNFAIL,
            "test_fat_ds3_share_open_delete_fcb": KNOWNFAIL,
            "test_fat_ds3_share_open_rename_ds2": KNOWNFAIL,
            "test_fat_ds3_share_open_rename_fcb": KNOWNFAIL,
            "test_mfs_ds3_share_open_rename_fcb": KNOWNFAIL,
            "test_fat_ds3_share_open_setfattrs": KNOWNFAIL,
            "test_create_new_psp": KNOWNFAIL,
            "test_pcmos_build": KNOWNFAIL,
            "test_libi86_build": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        with open(join("src/bindist", self.autoexec), "r") as f:
            contents = f.read()
            self.mkfile(self.autoexec, re.sub(r"[Dd]:\\", r"c:\\", contents), newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands/dosemu")

        # Use the (almost) standard shipped config
        with open(join("src/bindist/c", self.confsys), "r") as f:
            contents = f.read()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")


class MSDOS622TestCase(OurTestCase, unittest.TestCase):

    priority = 2

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
            ("dos/himem.sys", "fb41fbc1c4bdd8652d445055508bc8265bc64aea"),
        ]
        cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
        cls.autoexec = "autoemu.bat"
        cls.bootblocks = [
            ("boot-306-4-17.blk", "d40c24ef5f5f9fd6ef28c29240786c70477a0b06"),
            ("boot-615-4-17.blk", "7fc96777727072471dbaab6f817c8d13262260d2"),
            ("boot-900-15-17.blk", "2a0ca1b87b82013fd417542a5ac28e965fb13e7a"),
        ]
        cls.images = [
            ("boot-floppy.img", "14b8310910bf19d6e375298f3b06da7ffdec9932"),
        ]

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        with open(join("src/bindist", self.autoexec), "r") as f:
            contents = f.read()
            self.mkfile(self.autoexec, re.sub(r"[Dd]:\\", r"c:\\", contents), newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands/dosemu")

        # Use the (almost) standard shipped config
        with open(join("src/bindist/c", self.confsys), "r") as f:
            contents = f.read()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

    def setUpDosVersion(self):
        self.mkfile("version.bat", "ver\r\nrem end\r\n")


class PPDOSGITTestCase(OurTestCase, unittest.TestCase):

    priority = 1

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
        cls.autoexec = "fdppauto.bat"
        cls.confsys = "fdppconf.sys"

        cls.setUpClassPost()

    def setUpDosConfig(self):
        # Use the (almost) standard shipped config
        with open(join("src/bindist", self.confsys), "r") as f:
            contents = f.read()
            contents = re.sub(r"SWITCHES=#0", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")


if __name__ == '__main__':

    tests = [t[0] for t in
            inspect.getmembers(OurTestCase, predicate=inspect.isfunction)
            if t[0].startswith("test")]

    xtests = [t[0] for t in
            inspect.getmembers(OurTestCase, predicate=inspect.isfunction)
            if t[0].startswith("xtest")]

    cases = [c[0] for c in
            inspect.getmembers(modules[__name__], predicate=inspect.isclass)
            if issubclass(c[1], OurTestCase) and c[0] != "OurTestCase"]

    def explode(n):
        if n in tests:
            return [c + "." + n for c in cases]
        if n in xtests:
            return [c + "." + n for c in cases]
        if n in cases:
            return [n,]
        p = n.split('.')
        if p and p[0] in cases and (p[1] in tests or p[1] in xtests):
            return [n,]
        return []

    if len(argv) > 1:
        if argv[1] == "--help":
            print("Usage: %s [--help | --list-cases | --list-tests] | [TestCase[.testname] ...]" % argv[0])
            exit(0)
        elif argv[1] == "--list-cases":
            for m in cases:
                print(str(m))
            exit(0)
        elif argv[1] == "--list-tests":
            for m in tests:
                print(str(m))
            exit(0)
        else:
            a = []
            for b in [explode(x) for x in argv[1:]]:
                a.extend(b)
            if not len(a):
                print("No tests found, was your testcase or testname incorrect? See --help")
                exit(1)
            argv = [argv[0],] + a
            main(argv)

    main()
