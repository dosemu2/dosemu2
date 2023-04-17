#!/usr/bin/python3

import inspect
import unittest

import re

from datetime import datetime
from difflib import unified_diff
from os import statvfs, uname, utime, environ, access, R_OK, W_OK
from os.path import exists, isdir, join
from pathlib import Path
from shutil import copy
from subprocess import call, CalledProcessError, run
from sys import argv, exit, modules
from time import mktime

from common_framework import (BaseTestCase, get_test_binaries, main, mkstring,
                              IPROMPT, KNOWNFAIL, UNSUPPORTED)

from func_cpu_trap_flag import cpu_trap_flag
from func_ds2_file_seek_tell import ds2_file_seek_tell
from func_ds2_file_seek_read import ds2_file_seek_read
from func_ds2_set_fattrs import ds2_set_fattrs
from func_ds3_file_access import ds3_file_access
from func_ds3_lock_concurrent import ds3_lock_concurrent
from func_ds3_lock_two_handles import ds3_lock_two_handles
from func_ds3_lock_readlckd import ds3_lock_readlckd
from func_ds3_lock_readonly import ds3_lock_readonly
from func_ds3_lock_twice import ds3_lock_twice
from func_ds3_lock_writable import ds3_lock_writable
from func_ds3_share_open_access import ds3_share_open_access
from func_ds3_share_open_twice import ds3_share_open_twice
from func_lfn_voln_info import lfn_voln_info
from func_lfs_disk_info import lfs_disk_info
from func_lfs_file_info import lfs_file_info
from func_lfs_file_seek_tell import lfs_file_seek_tell
from func_libi86_testsuite import libi86_create_items
from func_memory_dpmi_japheth import memory_dpmi_japheth
from func_memory_ems_borland import memory_ems_borland
from func_memory_hma import (memory_hma_freespace, memory_hma_alloc, memory_hma_a20,
                             memory_hma_alloc3, memory_hma_chain)
from func_memory_uma import memory_uma_strategy
from func_memory_xms import memory_xms
from func_mfs_findfile import mfs_findfile
from func_mfs_truename import mfs_truename
from func_network import network_pktdriver_mtcp
from func_pit_mode_2 import pit_mode_2

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

    attrs = ['cputest', 'dpmitest', 'hmatest', 'nettest', 'umatest', 'xmstest']
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

        self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

; Get current drive and store its letter in fspath
    mov     ax, 1900h
    int     21h
    add     al, 'A'
    mov     byte [fspath], al

; Get Volume info
;    Windows95 - LONG FILENAME - GET VOLUME INFORMATION
;
;    Call:
;      AX = 71A0h
;      DS:DX -> ASCIZ root name (e.g. "C:\")
;      ES:DI -> buffer for file system name
;      CX = size of ES:DI buffer
;
;    Return:
;      CF clear if successful
;        AX destroyed (0000h and 0200h seen)
;        BX = file system flags (see ;01783)
;        CX = maximum length of file name [usually 255]
;        DX = maximum length of path [usually 260]
;        ES:DI buffer filled (ASCIZ, e.g. "FAT","NTFS","CDFS")
;
;      CF set on error
;        AX = error code
;          7100h if function not supported

    mov     ax, 71a0h
    mov     dx, fspath ; ds:dx
    mov     di, fstype ; es:di
    mov     cx, fstypelen
    stc
    int     21h

    jc      chkfail

    cmp     byte [fstype], '$'
    je      prnofstype

prsuccess:
    mov     di, fstype
    mov     cx, fstypelen
    mov     al, 0
    cld
    repne   scasb
    mov     byte [di-1], ')'
    mov     byte [di], 13
    mov     byte [di+1], 10
    mov     byte [di+2], '$'
    mov     dx, success
    jmp     exit

prnofstype:
    mov     dx, nofstype
    jmp     exit

prnotsupported:
    mov     dx, notsupported
    jmp     exit

prcarryset:
    mov     dx, carryset
    jmp     exit

chkfail:
    cmp     ax, 7100h
    jne     prcarryset

    jmp     prnotsupported

exit:
    mov     ah, 9
    int     21h

    mov     ah, 4ch
    int 21h

section .data

carryset:
    db  "Carry Set",13,10,'$'
notsupported:
    db  "Not Supported(AX=0x7100)",13,10,'$'
nofstype:
    db  "Carry Not Set But No Filesystem Type",13,10,'$'
success:
    db  "Operation Success("
fstype:
    times 32 db '$'
fstypelen equ $ - fstype
successend:
    times 4 db 0
fspath:
    db  "?:\", 0
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
        self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 3d00h			; open file readonly
    mov     dx, fname
    int     21h
    jc      prfailopen

    mov     word [fhndl], ax

    mov     ax, 3f00h			; read from file, should be partial (35)
    mov     bx, word [fhndl]
    mov     cx, 64
    mov     dx, fdata
    int     21h
    jc      prfailread
    cmp     ax, 35
    jne     prnumread

    mov     ax, 3f00h			; read from file again to get EOF
    mov     bx, word [fhndl]
    mov     cx, 64
    mov     dx, fdata
    int     21h
    jc      prcarryset
    cmp     ax, 0
    jne     praxnotzero

    jmp     prsucc

prfailopen:
    mov     dx, failopen
    jmp     @1

prfailread:
    mov     dx, failread
    jmp     @2

prnumread:
    mov     dx, numread
    jmp     @2

praxnotzero:
    mov     dx, axnotzero
    jmp     @2

prcarryset:
    mov     dx, carryset
    jmp     @2

prsucc:
    mov     byte [fdata + 32], ')'
    mov     byte [fdata + 33], 13
    mov     byte [fdata + 34], 10
    mov     byte [fdata + 35], '$'
    mov     dx, success
    jmp     @2

@2:
    mov     ax, 3e00h			; close file
    mov     bx, word [fhndl]
    int     21h

@1:
    mov     ah, 9               ; print string
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fname:
    db  "%s",0

fhndl:
    dw  0

success:
    db  "Operation Success("
fdata:
    times 64 db 0
failopen:
    db  "Open Operation Failed",13,10,'$'
failread:
    db  "Read Operation Failed",13,10,'$'
numread:
    db  "Partial Read Not 35 Chars",13,10,'$'
carryset:
    db  "Carry Set at EOF",13,10,'$'
axnotzero:
    db  "AX Not Zero at EOF",13,10,'$'

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

    mov     ax, 3d00h			; open file readonly
    mov     dx, fname
    int     21h
    jc      prfailopen

    mov     word [fhndl], ax

    mov     ax, 3f00h			; read from file, should be partial (35)
    mov     bx, word [fhndl]
    mov     cx, 64
    mov     dx, fdata
    int     21h
    jc      prfailread
    cmp     ax, 35
    jne     prnumread

    jmp     prsucc

prfaildtaset:
    mov     dx, faildtaset
    jmp     @1

prfailopen:
    mov     dx, failopen
    jmp     @1

prfailread:
    mov     dx, failread
    jmp     @2

prnumread:
    mov     dx, numread
    jmp     @2

prsucc:
    mov     byte [fdata + 32], ')'
    mov     byte [fdata + 33], 13
    mov     byte [fdata + 34], 10
    mov     byte [fdata + 35], '$'
    mov     dx, success
    jmp     @2

@2:
    mov     ax, 3e00h			; close file
    mov     bx, word [fhndl]
    int     21h

@1:
    mov     ah, 9               ; print string
    int     21h

exit:
    mov     ah, 4ch
    int     21h

section .data

fname:
    db  "%s",0

fhndl:
    dw  0

success:
    db  "Operation Success("
fdata:
    times 64 db 0
faildtaset:
    db  "Set DTA Operation Failed",13,10,'$'
failopen:
    db  "Open Operation Failed",13,10,'$'
failread:
    db  "Read Operation Failed",13,10,'$'
numread:
    db  "Partial Read Not 35 Chars",13,10,'$'

altdta:
    times 128 db 0

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
        self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

; designate target segment
    push    cs
    pop     ax
    add     ax, 0200h
    mov     es, ax

; create PSP in memory
    mov     dx, es
    mov     ax, 2600h
    int     21h

; see if the exit field is set
    cmp     word [es:0000], 20cdh
    jne     extfail

; see if the parent PSP is zero
    cmp     word [es:0016h], 0
    je      pntzero

; see if the parent PSP points to a PSP
    push    es
    push    word [es:0016h]
    pop     es
    cmp     word [es:0000h], 20cdh
    pop     es
    jne     pntnpsp

; see if the 'INT 21,RETF' is there
    cmp     word [es:0050h], 21cdh
    jne     int21ms
    cmp     byte [es:0052h], 0cbh
    jne     int21ms

; see if the cmdtail is there
    movzx   cx, byte [es:0080h]
    cmp     cx, %d
    jne     cmdlngth

    inc     cx
    mov     si, cmdline
    mov     di, 81h
    cld
    repe    cmpsb
    jne     cmdtail

success:
    mov     dx, successmsg
    jmp     exit

extfail:
    mov     dx, extfailmsg
    jmp     exit

pntzero:
    mov     dx, pntzeromsg
    jmp     exit

pntnpsp:
    mov     dx, pntnpspmsg
    jmp     exit

int21ms:
    mov     dx, int21msmsg
    jmp     exit

cmdlngth:
    mov     dx, cmdlngthmsg
    jmp     exit

cmdtail:
    mov     dx, cmdtailmsg
    jmp     exit

exit:
    mov     ah, 9
    int     21h

    mov     ah, 4ch
    int     21h

extfailmsg:
    db  "PSP exit field not set to CD20",13,10,'$'

pntzeromsg:
    db  "PSP parent is zero",13,10,'$'

pntnpspmsg:
    db  "PSP parent doesn't point to a PSP",13,10,'$'

int21msmsg:
    db  "PSP is missing INT21, RETF",13,10,'$'

cmdlngthmsg:
    db  "PSP has incorrect command length",13,10,'$'

cmdtailmsg:
    db  "PSP has incorrect command tail",13,10,'$'

successmsg:
    db  "PSP structure okay",13,10,'$'

; 05 20 54 45 53 54 0d
cmdline:
    db " %s",13

""" % (1 + len(cmdline), cmdline))

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertIn("PSP structure okay", results)

# Tests using neither compiler nor assembler

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

    def test_command_com_keyword_exist(self):
        """Command.com keyword exist"""
        self.mkfile("testit.bat", r"""
rem X: is a non-existent drive
if not exist X:\ANYTHING.EXE       echo 00_True
if not exist X:\NUL                echo 01_True
if not exist X:\FAKEDIR\NUL        echo 02_True

rem D: is a FAT(local) drive
D:
cd \
mkdir ISDIR
echo hello > ISDIR\EXIST.TRU
if exist D:\NUL                    echo 03_True
if not exist D:\EXIST.NOT          echo 04_True
if not exist D:\NODIR\NUL          echo 05_True
if not exist D:\NODIR\ANYTHING.EXE echo 06_True
if exist D:\ISDIR\EXIST.TRU        echo 07_True
if not exist D:\ISDIR\EXIST.NOT    echo 08_True

rem C: is an MFS(network redirected) drive
C:
cd \
mkdir ISDIR
echo hello > ISDIR\EXIST.TRU
if exist C:\NUL                    echo 09_True
if not exist C:\EXIST.NOT          echo 10_True
if not exist C:\NODIR\NUL          echo 11_True
if not exist C:\NODIR\ANYTHING.EXE echo 12_True
if exist C:\ISDIR\EXIST.TRU        echo 13_True
if not exist C:\ISDIR\EXIST.NOT    echo 14_True

rem end
""", newline="\r\n")

        testdir = self.mkworkdir('d')
        name = self.mkimage("12", cwd=testdir)

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
""" % name)

        for i in range(15):
            with self.subTest("Subtest %02d" % i):
                self.assertRegex(results, r"(?m)^%02d_True.*" % i)

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
    test_memory_dpmi_ecm_alloc.dpmitest=True

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
    test_memory_dpmi_ecm_mini.dpmitest=True

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
    test_memory_dpmi_ecm_modeswitch.dpmitest=True

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
    test_memory_dpmi_ecm_psp.dpmitest=True

    def test_memory_dpmi_japheth(self):
        """Memory DPMI (Japheth) ''"""
        memory_dpmi_japheth(self, '')
    test_memory_dpmi_japheth.dpmitest=True

    def test_memory_dpmi_japheth_c(self):
        """Memory DPMI (Japheth) '-c'"""
        memory_dpmi_japheth(self, '-c')
    test_memory_dpmi_japheth_c.dpmitest=True

    def test_memory_dpmi_japheth_d(self):
        """Memory DPMI (Japheth) '-d'"""
        memory_dpmi_japheth(self, '-d')
    test_memory_dpmi_japheth_d.dpmitest=True

    def test_memory_dpmi_japheth_e(self):
        """Memory DPMI (Japheth) '-e'"""
        memory_dpmi_japheth(self, '-e')
    test_memory_dpmi_japheth_e.dpmitest=True

    def test_memory_dpmi_japheth_i(self):
        """Memory DPMI (Japheth) '-i'"""
        memory_dpmi_japheth(self, '-i')
    test_memory_dpmi_japheth_i.dpmitest=True

    def test_memory_dpmi_japheth_m(self):
        """Memory DPMI (Japheth) '-m'"""
        memory_dpmi_japheth(self, '-m')
    test_memory_dpmi_japheth_m.dpmitest=True

    def test_memory_dpmi_japheth_r(self):
        """Memory DPMI (Japheth) '-r'"""
        memory_dpmi_japheth(self, '-r')
    test_memory_dpmi_japheth_r.dpmitest=True

    def test_memory_dpmi_japheth_t(self):
        """Memory DPMI (Japheth) '-t'"""
        memory_dpmi_japheth(self, '-t')
    test_memory_dpmi_japheth_t.dpmitest=True

    def test_memory_dpmi_japheth_z(self):
        """Memory DPMI (Japheth) '-z'"""
        memory_dpmi_japheth(self, '-z')
    test_memory_dpmi_japheth_z.dpmitest=True

    def test_memory_emm286_borland(self):
        """Memory EMM286 (Borland)"""

        self.unTarOrSkip("TEST_EMM286.tar", [
            ("tasm32.exe", "61c2ddb2c193f49dd29c083579ec7f47566278a7"),
            ("emm286.exe", "d8388a574f024d500515e4f0575958cf52939f26"),
            ("32rtm.exe", "720c8bdcb0b3b2634e95c89c56c0cc1573272cd9"),
        ])

        # Modify the config.sys
        contents = (self.workdir / self.confsys).read_text()
        contents = re.sub(r"device=(c:\\)?dosemu\\umb.sys", r"device=\1dosemu\\umb.sys /full", contents)
        contents = re.sub(r"devicehigh=(c:\\)?dosemu\\ems.sys", r"devicehigh=c:\\emm286.exe 2048", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")

        self.mkfile("testit.bat", """\
c:\\tasm32 /h
rem end
""", newline="\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        # Look for last line of output to indicate successful load/run
        # /zi,/zd,/zn    Debug info: zi=full, zd=line numbers only, zn=none
        self.assertRegex(results, r"/zi.*Debug info: zi=full")

    def test_memory_ems_borland(self):
        """Memory EMS (Borland)"""
        memory_ems_borland(self)

    def test_memory_hma_a20(self):
        """Memory HMA a20 toggle"""
        memory_hma_a20(self)
    test_memory_hma_a20.hmatest = True

    def test_memory_hma_alloc(self):
        """Memory HMA allocation"""
        memory_hma_alloc(self)
    test_memory_hma_alloc.hmatest = True

    def test_memory_hma_alloc3(self):
        """Memory HMA alloc/resize/dealloc"""
        memory_hma_alloc3(self)
    test_memory_hma_alloc3.hmatest = True

    def test_memory_hma_freespace(self):
        """Memory HMA freespace"""
        memory_hma_freespace(self)
    test_memory_hma_freespace.hmatest = True

    def test_memory_hma_chain(self):
        """Memory HMA get chain"""
        memory_hma_chain(self)
    test_memory_hma_chain.hmatest = True

    def test_memory_xms(self):
        """Memory XMS"""
        memory_xms(self)
    test_memory_xms.xmstest = True

    def test_memory_uma_strategy(self):
        """Memory UMA Strategy"""
        memory_uma_strategy(self)
    test_memory_uma_strategy.umatest = True

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

        testfil = testdir / "there.txt"
        testfil.write_text('there')

        name = self.mkimage_vbr(fat, cwd=testdir)

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
        self.assertRegex(results,
                r"THERE[\t ]+TXT[\t ]+5"
                r"|"
                r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s+5\s+THERE.TXT")

    def test_fat12_img_d_writable(self):
        """FAT12 image file D writable"""
        self._test_fat_img_d_writable("12")

    def test_fat16_img_d_writable(self):
        """FAT16 image file D writable"""
        self._test_fat_img_d_writable("16")

    def test_fat16b_img_d_writable(self):
        """FAT16B image file D writable"""
        self._test_fat_img_d_writable("16b")

    def test_fat32_img_d_writable(self):
        """FAT32 image file D writable"""
        self._test_fat_img_d_writable("32")

    def test_mfs_lredir_auto_hdc(self):
        """MFS lredir auto C drive redirection"""
        self.mkfile("testit.bat", "lredir\r\nrem end\r\n")

        results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

# C:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE

        self.assertRegex(results, r"C: = /.*")

    def test_mfs_lredir_command(self):
        """MFS lredir command redirection"""
        self.mkfile("testit.bat", """\
lredir X: /tmp
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

        self.assertRegex(results, r"X: = /tmp")

    def test_mfs_lredir_command_no_perm(self):
        """MFS lredir command redirection permission fail"""
        self.mkfile("testit.bat", """\
lredir X: /tmp
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

    def test_mfs_findfile_ufs_lfn(self):
        """MFS findfile UFS LFN"""
        tests = (
            # Type, Create, Find
            ("DIR", "Program Files", "Program Files"),
            ("FILE", "verylongfilename.txt", "verylongfilename.txt"),
            ("FILE", "verylongfilename2.txt", "verylongfilename2.txt"),
            ("FILE", "space embedded filename.txt", "space embedded filename.txt"),
            ("FILE", "MixedCaseFilename.ext", "MixedCaseFilename.ext"),
        )
        mfs_findfile(self, "UFS", "LFN", tests)

    def test_mfs_findfile_ufs_sfn(self):
        """MFS findfile UFS SFN"""
        tests = (
            # Type, Create, Find
            ("DIR", "Program Files", "PROGR~-I"),
            ("FILE", "verylongfilename.txt", "VERYL~3G.TXT"),
            ("FILE", "verylongfilename2.txt", "VERYL~2N.TXT"),
            ("FILE", "space embedded filename.txt", "SPACE~L#.TXT"),
            ("FILE", "MixedCaseFilename.ext", "MIXED~G4.EXT"),
        )
        mfs_findfile(self, "UFS", "SFN", tests)

    def test_mfs_findfile_vfat_linux_mounted_lfn(self):
        """MFS findfile VFAT Linux mounted LFN"""
        tests = (
            # Type, Create, Find
            ("DIR", "Program Files", "Program Files"),
            ("FILE", "verylongfilename.txt", "verylongfilename.txt"),
            ("FILE", "verylongfilename2.txt", "verylongfilename2.txt"),
            ("FILE", "space embedded filename.txt", "space embedded filename.txt"),
            ("FILE", "MixedCaseFilename.ext", "MixedCaseFilename.ext"),
        )
        mfs_findfile(self, "VFAT", "LFN", tests)

    def test_mfs_findfile_vfat_linux_mounted_sfn(self):
        """MFS findfile VFAT Linux mounted SFN"""
        tests = (
            # Type, Create, Find
            ("DIR", "Program Files", "PROGRA~1"),
            ("FILE", "verylongfilename.txt", "VERYLO~1.TXT"),
            ("FILE", "verylongfilename2.txt", "VERYLO~2.TXT"),
            ("FILE", "space embedded filename.txt", "SPACEE~1.TXT"),
            ("FILE", "MixedCaseFilename.ext", "MIXEDC~1.EXT"),
        )
        mfs_findfile(self, "VFAT", "SFN", tests)

    def test_mfs_truename_ufs_lfn(self):
        """MFS truename UFS LFN"""
        names_to_create = (
            ("DIR", "Program Files"),
            ("DIR", "RealDir2"),
            ("DIR", "Sub"),
            ("DIR", "Sub/RealDir"),
            ("FILE", "Sub/Very Long realName"),
            ("FILE", "Sub/verylongRealname.txt"),
            ("FILE", "Sub/RealDir/Very Long realName"),
        )
        tests = (  # Note: CurDrv == D:, CurDir == \Sub

            # These LFN 7160/0 tests are proven on Win98 and are seen
            # to have the following rules:
            # 1/ Any '..' are resolved.
            # 2/ Any '.\' are stripped.
            # 3/ If no drive specification, then default drive is prepended.
            # 4/ If not absolute path, then current directory for drive
            #    is inserted between drive and relative path.
            # 5/ All path components (except final) are upcased.
            # 6/ Final path component case is preserved from the request.
            # 7/ No path component has to exist on the filesystem or
            #    is checked against it and updated for case.

            ("LFN0", r"aux",                                      r"D:/AUX"),
            # D:\Sub exists as a directory
            ("LFN0", r"nonExist",                                 r"D:\\SUB\\nonExist"),
            ("LFN0", r"\\nonExist",                               r"D:\\nonExist"),
            ("LFN0", r"\\Sub\\nonExist",                          r"D:\\SUB\\nonExist"),
            ("LFN0", r"c:nonExist",                               r"C:\\nonExist"),
            ("LFN0", r"c:\\nonExist",                             r"C:\\nonExist"),
            ("LFN0", r"c:\\RootC\\nonExist",                      r"C:\\ROOTC\\nonExist"),
            # Both D:\RealDir2 and D:\\Sub\\RealDir exist as directories
            ("LFN0", r"d:realdir",                                r"D:\\SUB\\realdir"),
            ("LFN0", r"d:\\realdir2",                             r"D:\\realdir2"),
            ("LFN0", r"d:\\realdir2\\noNexist.TxT",               r"D:\\REALDIR2\\noNexist.TxT"),
            # D:\Sub exists as a directory
            ("LFN0", r"nonExist\\NewFile.txt",                    r"D:\\SUB\\NONEXIST\\NewFile.txt"),
            ("LFN0", r"d:nonExist\\NewFile.txt",                  r"D:\\SUB\\NONEXIST\\NewFile.txt"),
            ("LFN0", r"d:\\nonExist\\NewFile.txt",                r"D:\\NONEXIST\\NewFile.txt"),
            ("LFN0", r"..\\Sub\\RealDir\\..\\NewFile.txt",        r"D:\\SUB\\NewFile.txt"),
            # D:\Program Files exists as a directory
            ("LFN0", r"D:\\progra~1",                             r"D:\\progra~1"),
            ("LFN0", r"D:\\PROGRA~1",                             r"D:\\PROGRA~1"),
            ("LFN0", r"D:\\program files",                        r"D:\\program files"),
            ("LFN0", r"D:\\PROGRAM FILES",                        r"D:\\PROGRAM FILES"),
            ("LFN0", r"D:\\Program Files",                        r"D:\\Program Files"),
            ("LFN0", r"D:\\Program Files\\NewFile.txt",           r"D:\\PROGRAM FILES\\NewFile.txt"),
            ("LFN0", r"D:\\Program Files\\NewFile.txt",           r"D:\\PROGRAM FILES\\NewFile.txt"),
            ("LFN0", r"D:\\Program Files\\NonExist\\NewFile.txt", r"D:\\PROGRAM FILES\\NONEXIST\\NewFile.txt"),

            ("LFN1", r"d:very long realname",                     r"D:\\SUB\\VERYL~CV"),
            ("LFN1", r"d:\\very long realname",                   r"ERROR: invalid component"),
            ("LFN1", r"d:\\Sub\\VERYLONGrEALNAME.TXT",            r"D:\\SUB\\VERYL~6S.TXT"),
            ("LFN1", r"D:\\program files",                        r"D:\\PROGR~-I"),
            ("LFN1", r"D:\\PROGRAM FILES",                        r"D:\\PROGR~-I"),
            ("LFN1", r"D:\\Program Files",                        r"D:\\PROGR~-I"),

            ("LFN2", r"D:\\SUB\\VERYL~CV",                        r"D:\\Sub\\Very Long realName"),
            ("LFN2", r"D:\\SUB\\VERYL~6S.TXT",                    r"D:\\Sub\\verylongRealname.txt"),
            ("LFN2", r"D:\\progr~-i",                             r"D:\\Program Files"),
            ("LFN2", r"D:\\PROGR~-I",                             r"D:\\Program Files"),
        )
        mfs_truename(self, "UFS", names_to_create, tests)

    def test_mfs_truename_ufs_sfn(self):
        """MFS truename UFS SFN"""
        names_to_create = (
            ("DIR", "Sub"),
            ("DIR", "Sub/testname"),
            ("FILE", "shrtname.txt"),
        )
        tests = (  # Note: CurDrv == D:, CurDir == \SUB
            ("SFN", r"aux", r"D:/AUX"),

            ("SFN", r"fakename", r"D:\\SUB\\FAKENAME"),           # Non existent
            ("SFN", r"\\fakename", r"D:\\FAKENAME"),              # Non existent
            ("SFN", r"\\Sub\\fakename", r"D:\\SUB\\FAKENAME"),    # Non existent
            ("SFN", r"c:fakename", r"C:\\FAKENAME"),              # Non existent
            ("SFN", r"c:\\fakename", r"C:\\FAKENAME"),            # Non existent
            ("SFN", r"c:\\Sub\\fakename", r"C:\\SUB\\FAKENAME"),  # Non existent

            ("SFN", r"testname", r"D:\\SUB\\TESTNAME"),
            ("SFN", r"\\Sub\\testname", r"D:\\SUB\\TESTNAME"),
            ("SFN", r"d:testname", r"D:\\SUB\\TESTNAME"),
            ("SFN", r"d:\\Sub\\testname", r"D:\\SUB\\TESTNAME"),

            ("SFN", r"shrtname.txt", r"D:\\SUB\\SHRTNAME.TXT"),   # Non existent
            ("SFN", r"\\shrtname.txt", r"D:\\SHRTNAME.TXT"),
            ("SFN", r"d:shrtname.txt", r"D:\\SUB\\SHRTNAME.TXT"), # Non existent
            ("SFN", r"d:\\shrtname.txt", r"D:\\SHRTNAME.TXT"),
        )
        mfs_truename(self, "UFS", names_to_create, tests)

    def test_mfs_truename_vfat_linux_mounted_lfn(self):
        """MFS truename VFAT Linux mounted LFN"""
        names_to_create = (
            ("DIR", "Program Files"),
            ("FILE", "lfnInRoot.tXt"),
            ("FILE", "Sub/verylongfilename.txt"),
            ("FILE", "Sub/verylongfilename2.txt"),
            ("FILE", "Sub/space embedded filename.txt"),
            ("FILE", "Sub/MixedCaseFilename.ext"),
            ("DIR", "Sub/test/1234567890987654321"),
            ("DIR", "Sub/abcdefgfedcba/1234567890987654321"),
            ("FILE", "Sub/1234567890987654321/abcdefgfedcba.txt"),
            ("FILE", "Sub/1234567890987654321/abcdefclash.txt"),
        )
        tests = (
            # Since Truename 0x7160/0 does not interact with the filesystem,
            # the tests on UFS should give coverage, but just do a few for
            # belt and braces.
            ("LFN0", r"D:\\progra~1",                                    r"D:\\progra~1"),
            ("LFN0", r"D:\\PROGRA~1",                                    r"D:\\PROGRA~1"),
            ("LFN0", r"D:\\program files",                               r"D:\\program files"),
            ("LFN0", r"D:\\PROGRAM FILES",                               r"D:\\PROGRAM FILES"),
            ("LFN0", r"D:\\Program Files",                               r"D:\\Program Files"),
            ("LFN0", r"D:\\Program Files\\NewFile.txt",                  r"D:\\PROGRAM FILES\\NewFile.txt"),
            ("LFN0", r"D:\\Program Files\\NewFile.txt",                  r"D:\\PROGRAM FILES\\NewFile.txt"),
            ("LFN0", r"D:\\Program Files\\NonExist\\NewFile.txt",        r"D:\\PROGRAM FILES\\NONEXIST\\NewFile.txt"),

            ("LFN1", r"X:\\lfnNotInRoot.tXt",                            r"ERROR: invalid component"),  # Non existent
            ("LFN1", r"..\\lfnNotInRoot.tXt",                            r"ERROR: invalid component"),  # Non existent
            ("LFN1", r"X:\\lfnInRoot.tXt",                               r"X:\\LFNINR~1.TXT"),
            ("LFN1", r"..\\lfnInRoot.tXt",                               r"X:\\LFNINR~1.TXT"),
            ("LFN1", r"..\\rootc\\..\\lfnInRoot.tXt",                    r"X:\\LFNINR~1.TXT"),
            ("LFN1", r"X:\\sub\\verylongfilename.txt",                   r"X:\\SUB\\VERYLO~1.TXT"),
            ("LFN1", r"X:\\sub\\verylongfilename2.txt",                  r"X:\\SUB\\VERYLO~2.TXT"),
            ("LFN1", r"X:\\sub\\space embedded filename.txt",            r"X:\\SUB\\SPACEE~1.TXT"),
            ("LFN1", r"X:\\sub\\MixedCaseFilename.ext",                  r"X:\\SUB\\MIXEDC~1.EXT"),
            ("LFN1", r"X:\\sub\\test\\1234567890987654321",              r"X:\\SUB\\TEST\\123456~1"),
            ("LFN1", r"X:\\sub\\abcdefgfedcba\\1234567890987654321",     r"X:\\SUB\\ABCDEF~1\\123456~1"),
            ("LFN1", r"X:\\sub\\1234567890987654321\\abcdefgfedcba.txt", r"X:\\SUB\\123456~1\\ABCDEF~1.TXT"),
            ("LFN1", r"X:\\sub\\1234567890987654321\\abcdefclash.txt",   r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
            ("LFN1", r"1234567890987654321\\abcdefclash.txt",            r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
            ("LFN1", r".\\1234567890987654321\\abcdefclash.txt",         r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
            ("LFN1", r"..\\sub\\1234567890987654321\\abcdefclash.txt",   r"X:\\SUB\\123456~1\\ABCDEF~2.TXT"),
            ("LFN1", r"X:\\program files",                               r"X:\\PROGRA~1"),
            ("LFN1", r"X:\\PROGRAM FILES",                               r"X:\\PROGRA~1"),
            ("LFN1", r"X:\\Program Files",                               r"X:\\PROGRA~1"),

            ("LFN2", r"X:\\LFNNOT~1.TXT",                                r"ERROR: invalid component"),  # Non existent
            ("LFN2", r"X:\\LFNINR~1.TXT",                                r"X:\\lfnInRoot.tXt"),
            ("LFN2", r"X:\\sub\\VERYLO~1.TXT",                           r"X:\\Sub\\verylongfilename.txt"),
            ("LFN2", r"X:\\sub\\VERYLO~2.TXT",                           r"X:\\Sub\\verylongfilename2.txt"),
            ("LFN2", r"X:\\sub\\SPACEE~1.TXT",                           r"X:\\Sub\\space embedded filename.txt"),
            ("LFN2", r"X:\\sub\\MIXEDC~1.EXT",                           r"X:\\Sub\\MixedCaseFilename.ext"),
            ("LFN2", r"X:\\sub\\TEST\\123456~1",                         r"X:\\Sub\\test\\1234567890987654321"),
            ("LFN2", r"X:\\sub\\ABCDEF~1\\123456~1",                     r"X:\\Sub\\abcdefgfedcba\\1234567890987654321"),
            ("LFN2", r"X:\\sub\\123456~1\\ABCDEF~1.TXT",                 r"X:\\Sub\\1234567890987654321\\abcdefgfedcba.txt"),
            ("LFN2", r"X:\\sub\\123456~1\\ABCDEF~2.TXT",                 r"X:\\Sub\\1234567890987654321\\abcdefclash.txt"),
            ("LFN2", r"X:\\progra~1",                                    r"X:\\Program Files"),
            ("LFN2", r"X:\\PROGRA~1",                                    r"X:\\Program Files"),
            ("LFN2", r"X:\\PROGRA~1",                                    r"X:\\Program Files"),
        )
        mfs_truename(self, "VFAT", names_to_create, tests)

    def test_mfs_truename_vfat_linux_mounted_sfn(self):
        """MFS truename VFAT Linux mounted SFN"""
        names_to_create = (
            ("DIR", "testname"),
            ("FILE", "Sub/shrtname.txt"),
            ("FILE", "Sub/verylongfilename.txt"),
            ("FILE", "Sub/verylongfilename2.txt"),
            ("FILE", "Sub/space embedded filename.txt"),
            ("FILE", "Sub/MixedCaseFilename.ext"),
            ("DIR", "Sub/test/1234567890987654321"),
            ("DIR", "Sub/abcdefgfedcba/1234567890987654321"),
            ("FILE", "Sub/654321fedcba/abcdef123456.txt"),
            ("FILE", "Sub/654321fedcba/abcdefclash.txt"),
        )
        tests = (  # Note: CurDrv == X:, CurDir == \SUB
            ("SFN", r"X:\\testname",                r"X:\\TESTNAME"),
            ("SFN", r"..\\testname",                r"X:\\TESTNAME"),
            ("SFN", r"testname",                    r"X:\\SUB\\TESTNAME"),  # Non existent
            ("SFN", r"X:\\sub\\shrtname.txt",       r"X:\\SUB\\SHRTNAME.TXT"),
            ("SFN", r"X:\\sub\\verylo~1.txt",       r"X:\\SUB\\VERYLO~1.TXT"),
            ("SFN", r"X:\\sub\\verylo~2.txt",       r"X:\\SUB\\VERYLO~2.TXT"),
            ("SFN", r"X:\\sub\\spacee~1.txt",       r"X:\\SUB\\SPACEE~1.TXT"),
            ("SFN", r"X:\\sub\\mixedc~1.ext",       r"X:\\SUB\\MIXEDC~1.EXT"),
            ("SFN", r"X:\\sub\\test\\123456~1",     r"X:\\SUB\\TEST\\123456~1"),
            ("SFN", r"X:\\sub\\abcdef~1\\123456~1", r"X:\\SUB\\ABCDEF~1\\123456~1"),
            ("SFN", r"X:\\sub\\654321~1\\abcdef~1", r"X:\\SUB\\654321~1\\ABCDEF~1"),
            ("SFN", r"X:\\sub\\654321~1\\abcdef~2", r"X:\\SUB\\654321~1\\ABCDEF~2"),
        )
        mfs_truename(self, "VFAT", names_to_create, tests)

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

    def test_lfn_volume_info_mfs(self):
        """LFN volume info on MFS"""
        lfn_voln_info(self, "MFS")

    def test_lfn_volume_info_fat16(self):
        """LFN volume info on FAT16"""
        lfn_voln_info(self, "FAT16")

    def test_lfn_volume_info_fat32(self):
        """LFN volume info on FAT32"""
        lfn_voln_info(self, "FAT32")

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

        r1 = re.compile(r'total_bytes\((\d+)\)')
        self.assertRegex(results, r1)
        t = r1.search(results)
        dfs_total = int(t.group(1))

        r2 = re.compile(r'avail_bytes\((\d+)\)')
        self.assertRegex(results, r2)
        a = r2.search(results)
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

    def test_lfs_disk_info_fat32(self):
        """LFS disk info FAT32"""
        lfs_disk_info(self, "FAT32")

    def test_lfs_disk_info_mfs(self):
        """LFS disk info MFS"""
        lfs_disk_info(self, "MFS")

    def test_mfs_lfs_file_info_1MiB(self):
        """MFS LFS file info (1 MiB)"""
        lfs_file_info(self, "MFS", "1MiB")

    def test_mfs_lfs_file_info_6GiB(self):
        """MFS LFS file info (6 GiB)"""
        lfs_file_info(self, "MFS", "6GiB")

    def test_mfs_lfs_file_seek_tell_set(self):
        """MFS LFS file seek tell set"""
        lfs_file_seek_tell(self, "MFS", "SET")

    def test_mfs_lfs_file_seek_tell_cur(self):
        """MFS LFS file seek tell current"""
        lfs_file_seek_tell(self, "MFS", "CUR")

    def test_mfs_lfs_file_seek_tell_end(self):
        """MFS LFS file seek tell end"""
        lfs_file_seek_tell(self, "MFS", "END")

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

    def test_mfs_ds2_set_fattrs(self):
        """MFS DOSv2 set file attrs"""
        tests = ('RDONLY', 'HIDDEN', 'SYSTEM')
        for t in tests:
            with self.subTest(t=t):
                ds2_set_fattrs(self, "MFS", t)

    def test_fat_ds2_set_fattrs(self):
        """FAT DOSv2 set file attrs"""
        tests = ('RDONLY', 'HIDDEN', 'SYSTEM')
        for t in tests:
            with self.subTest(t=t):
                ds2_set_fattrs(self, "FAT", t)

    def test_fat_ds3_file_access_read(self):
        """FAT DOSv3 file access read"""
        ds3_file_access(self, "FAT", "READ")

    def test_mfs_ds3_file_access_read(self):
        """MFS DOSv3 file access read"""
        ds3_file_access(self, "MFS", "READ")

    def test_fat_ds3_file_access_write(self):
        """FAT DOSv3 file access write"""
        ds3_file_access(self, "FAT", "WRITE")

    def test_mfs_ds3_file_access_write(self):
        """MFS DOSv3 file access write"""
        ds3_file_access(self, "MFS", "WRITE")

    def test_fat_ds3_file_access_read_device_readonly(self):
        """FAT DOSv3 file access read device readonly"""
        ds3_file_access(self, "FATRO", "READ")

    def test_mfs_ds3_file_access_read_device_readonly(self):
        """MFS DOSv3 file access read device readonly"""
        ds3_file_access(self, "MFSRO", "READ")

    def test_fat_ds3_file_access_write_device_readonly(self):
        """FAT DOSv3 file access write device readonly"""
        ds3_file_access(self, "FATRO", "WRITE")

    def test_mfs_ds3_file_access_write_device_readonly(self):
        """MFS DOSv3 file access write device readonly"""
        ds3_file_access(self, "MFSRO", "WRITE")

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

    def test_mfs_ds3_lock_concurrent(self):
        """MFS DOSv3 lock file lock concurrent limit"""
        ds3_lock_concurrent(self, "MFS")

    def test_fat_ds3_lock_concurrent(self):
        """FAT DOSv3 lock file lock concurrent limit"""
        ds3_lock_concurrent(self, "FAT")

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

    def test_mfs_ds3_share_open_delete_one_process_ds2(self):
        """MFS DOSv3 share open delete one process DOSv2"""
        ds3_share_open_access(self, "ONE", "MFS", "DELPTH")

    def test_fat_ds3_share_open_delete_one_process_ds2(self):
        """FAT DOSv3 share open delete one process DOSv2"""
        ds3_share_open_access(self, "ONE", "FAT", "DELPTH")

    def test_mfs_ds3_share_open_delete_one_process_fcb(self):
        """MFS DOSv3 share open delete one process FCB"""
        ds3_share_open_access(self, "ONE", "MFS", "DELFCB")

    def test_fat_ds3_share_open_delete_one_process_fcb(self):
        """FAT DOSv3 share open delete one process FCB"""
        ds3_share_open_access(self, "ONE", "FAT", "DELFCB")

    def test_mfs_ds3_share_open_rename_one_process_ds2(self):
        """MFS DOSv3 share open rename one process DOSv2"""
        ds3_share_open_access(self, "ONE", "MFS", "RENPTH")

    def test_fat_ds3_share_open_rename_one_process_ds2(self):
        """FAT DOSv3 share open rename one process DOSv2"""
        ds3_share_open_access(self, "ONE", "FAT", "RENPTH")

    def test_mfs_ds3_share_open_rename_one_process_fcb(self):
        """MFS DOSv3 share open rename one process FCB"""
        ds3_share_open_access(self, "ONE", "MFS", "RENFCB")

    def test_fat_ds3_share_open_rename_one_process_fcb(self):
        """FAT DOSv3 share open rename one process FCB"""
        ds3_share_open_access(self, "ONE", "FAT", "RENFCB")

    def test_mfs_ds3_share_open_setfattrs_one_process(self):
        """MFS DOSv3 share open set file attrs one process DOSv2"""
        ds3_share_open_access(self, "ONE", "MFS", "SETATT")

    def test_fat_ds3_share_open_setfattrs_one_process(self):
        """FAT DOSv3 share open set file attrs one process DOSv2"""
        ds3_share_open_access(self, "ONE", "FAT", "SETATT")

    def test_mfs_ds3_share_open_delete_two_process_ds2(self):
        """MFS DOSv3 share open delete two process DOSv2"""
        ds3_share_open_access(self, "TWO", "MFS", "DELPTH")

    def test_fat_ds3_share_open_delete_two_process_ds2(self):
        """FAT DOSv3 share open delete two process DOSv2"""
        ds3_share_open_access(self, "TWO", "FAT", "DELPTH")

    def test_mfs_ds3_share_open_delete_two_process_fcb(self):
        """MFS DOSv3 share open delete two process FCB"""
        ds3_share_open_access(self, "TWO", "MFS", "DELFCB")

    def test_fat_ds3_share_open_delete_two_process_fcb(self):
        """FAT DOSv3 share open delete two process FCB"""
        ds3_share_open_access(self, "TWO", "FAT", "DELFCB")

    def test_mfs_ds3_share_open_rename_two_process_ds2(self):
        """MFS DOSv3 share open rename two process DOSv2"""
        ds3_share_open_access(self, "TWO", "MFS", "RENPTH")

    def test_fat_ds3_share_open_rename_two_process_ds2(self):
        """FAT DOSv3 share open rename two process DOSv2"""
        ds3_share_open_access(self, "TWO", "FAT", "RENPTH")

    def test_mfs_ds3_share_open_rename_two_process_fcb(self):
        """MFS DOSv3 share open rename two process FCB"""
        ds3_share_open_access(self, "TWO", "MFS", "RENFCB")

    def test_fat_ds3_share_open_rename_two_process_fcb(self):
        """FAT DOSv3 share open rename two process FCB"""
        ds3_share_open_access(self, "TWO", "FAT", "RENFCB")

    def test_mfs_ds3_share_open_setfattrs_two_process(self):
        """MFS DOSv3 share open set file attrs two process DOSv2"""
        ds3_share_open_access(self, "TWO", "MFS", "SETATT")

    def test_fat_ds3_share_open_setfattrs_two_process(self):
        """FAT DOSv3 share open set file attrs two process DOSv2"""
        ds3_share_open_access(self, "TWO", "FAT", "SETATT")

    def test_network_pktdriver_mtcp_builtin(self):
        """Network pktdriver mTCP built-in"""
        network_pktdriver_mtcp(self, 'builtin')
    test_network_pktdriver_mtcp_builtin.nettest = True

    def test_network_pktdriver_mtcp_ne2000(self):
        """Network pktdriver mTCP NE2000"""
        network_pktdriver_mtcp(self, 'ne2000')
    test_network_pktdriver_mtcp_ne2000.nettest = True

    def _test_cpu(self, cpu_vm, cpu_vm_dpmi, cpu_emu):
        if ('kvm' in cpu_vm or 'kvm' in cpu_vm_dpmi) and not access("/dev/kvm", W_OK|R_OK):
            self.skipTest("requires KVM")
        if cpu_vm == 'vm86' and uname()[4] != 'i686':
            self.skipTest("native vm86() only on 32bit x86")

        edir = self.topdir / "test" / "cpu"

        # Native reference file is now checked in to git and will
        # only need to be updated if the test source changes
        reffile = edir / "reffile.log"
        refoutput = []
        with reffile.open("r") as f:
            refoutput = f.readlines()

        # DOS test binary is built as part of normal build process
        copy(edir / "dosbin.exe", self.workdir / "dosbin.exe")
        dosfile = self.workdir / "dosfile.log"

        # output from DOS under test
        self.mkfile("testit.bat", """\
dosbin --common-tests > %s
rem end
""" % dosfile.name, newline="\r\n")

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
            with dosfile.open("r") as f:
                dosoutput = f.readlines()
        except FileNotFoundError:
            pass
        if not dosoutput:
            self.fail("DOS output file not found")

        # Compare DOS output to reference file
        if dosoutput != refoutput:
            diff = unified_diff(refoutput, dosoutput, fromfile=str(reffile), tofile=str(dosfile))
            self.fail('differences detected\n' + ''.join(list(diff)))

    def test_cpu_1_vm86native(self):
        """CPU native vm86 + native DPMI (i386 only)"""
        self._test_cpu("vm86", "native", 0)
    test_cpu_1_vm86native.cputest = True

    def test_cpu_2_jitnative(self):
        """CPU JIT vm86 + native DPMI"""
        self._test_cpu("emulated", "native", 0)
    test_cpu_2_jitnative.cputest = True

    def test_cpu_jitkvm(self):
        """CPU JIT vm86 + KVM DPMI"""
        self._test_cpu("emulated", "kvm", 0)
    test_cpu_jitkvm.cputest = True

    def test_cpu_simnative(self):
        """CPU simulated vm86 + native DPMI"""
        self._test_cpu("emulated", "native", 1)
    test_cpu_simnative.cputest = True

    def test_cpu_simkvm(self):
        """CPU simulated vm86 + KVM DPMI"""
        self._test_cpu("emulated", "kvm", 1)
    test_cpu_simkvm.cputest = True

    def test_cpu_kvmnative(self):
        """CPU KVM vm86 + native DPMI"""
        self._test_cpu("kvm", "native", 0)
    test_cpu_kvmnative.cputest = True

    def test_cpu_kvm(self):
        """CPU KVM vm86 + KVM DPMI"""
        self._test_cpu("kvm", "kvm", 0)
    test_cpu_kvm.cputest = True

    def test_cpu_kvmjit(self):
        """CPU KVM vm86 + JIT DPMI"""
        self._test_cpu("kvm", "emulated", 0)
    test_cpu_kvmjit.cputest = True

    def test_cpu_kvmsim(self):
        """CPU KVM vm86 + simulated DPMI"""
        self._test_cpu("kvm", "emulated", 1)
    test_cpu_kvmsim.cputest = True

    def test_cpu_jit(self):
        """CPU JIT vm86 + JIT DPMI"""
        self._test_cpu("emulated", "emulated", 0)
    test_cpu_jit.cputest = True

    def test_cpu_sim(self):
        """CPU simulated vm86 + simulated DPMI"""
        self._test_cpu("emulated", "emulated", 1)
    test_cpu_sim.cputest = True

    def test_cpu_trap_flag_emulated(self):
        """CPU Trap Flag emulated"""
        cpu_trap_flag(self, 'emulated')
    test_cpu_trap_flag_emulated.cputest = True

    def test_cpu_trap_flag_kvm(self):
        """CPU Trap Flag KVM"""
        cpu_trap_flag(self, 'kvm')
    test_cpu_trap_flag_kvm.cputest = True

    def test_pcmos_build(self):
        """PC-MOS build script"""
        if environ.get("SKIP_EXPENSIVE"):
            self.skipTest("expensive test")

        mosrepo = 'https://github.com/roelandjansen/pcmos386v501.git'
        mosroot = self.imagedir / 'pcmos.git'

        try:
            run(["git", "clone", "-q", "--depth=1", mosrepo, str(mosroot)], check=True)
        except CalledProcessError:
            self.skipTest("repository unavailable")

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

        self.assertNotIn('Timeout', results)
        self.assertNotIn('NonZeroReturn', results)

        missing = [str(x.relative_to(mosroot)) for x in outfiles if not x.exists()]
        if len(missing):
            msg = "Output file(s) missing %s\n" % str(missing)
            raise self.failureException(msg)

    def test_passing_environment_variable(self):
        """Passing Environment Variable to DOS"""

        tstring1 = "0123456789aBcDeF"

        self.mkfile("testit.bat", """\
@ECHO on
ECHO %TESTVAR1%
rem end
""", newline="\r\n")

        args = ["TESTVAR1=" + tstring1, "-q", "-E", "testit.bat"]
        results = self.runDosemuCmdline(args, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertNotIn('Timeout', results)
        self.assertNotIn('NonZeroReturn', results)
        self.assertIn("rem end", results, msg="Test incomplete:\n")
        self.assertIn(tstring1, results)

    def test_passing_dos_errorlevel_back(self):
        """Passing DOS Errorlevel back"""

        self.mkcom_with_ia16("justerro", r"""
int main(int argc, char *argv[])
{
  return 53;
}
""")

        results = self.runDosemuCmdline(["-q", "-E", "justerro.com"], config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertNotIn('Timeout', results)
        self.assertIn('NonZeroReturn:53', results)

    def test_pit_mode_2(self):
        """PIT Mode 2"""
        if environ.get("SKIP_EXPENSIVE"):
            self.skipTest("expensive test")
        if environ.get("SKIP_UNCERTAIN"):
            self.skipTest("uncertain test")

        pit_mode_2(self)


class DRDOS701TestCase(OurTestCase, unittest.TestCase):
    # OpenDOS 7.01

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
            r"test_fat_ds3_share_open_setfattrs_(one|two)_process": KNOWNFAIL,
            r"test_..._ds3_share_open_rename_one_process_fcb": KNOWNFAIL,
            r"test_..._fcb_rename_simple": KNOWNFAIL,
            r"test_..._fcb_rename_wild_\d": KNOWNFAIL,
            "test_mfs_truename_ufs_sfn": KNOWNFAIL,
            "test_mfs_truename_vfat_linux_mounted_sfn": KNOWNFAIL,
            "test_fat32_img_d_writable": UNSUPPORTED,
            "test_lfn_volume_info_fat32": UNSUPPORTED,
            "test_lfs_disk_info_fat32": UNSUPPORTED,
            "test_floppy_vfs": KNOWNFAIL,
            "test_memory_hma_alloc3": UNSUPPORTED,
            "test_memory_hma_chain": UNSUPPORTED,
            "test_pcmos_build": KNOWNFAIL,
            "test_passing_dos_errorlevel_back": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / self.autoexec).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        self.mkfile(self.autoexec, contents, newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands" / "dosemu")

        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / self.confsys).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")

    def setUpDosVersion(self):
        self.mkfile("version.bat", "ver\r\nrem end\r\n")


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
            "test_mfs_lfs_file_info_1MiB": KNOWNFAIL,
            "test_mfs_lfs_file_info_6GiB": KNOWNFAIL,
            "test_mfs_lfs_file_seek_tell_set": KNOWNFAIL,
            "test_mfs_lfs_file_seek_tell_cur": KNOWNFAIL,
            "test_mfs_lfs_file_seek_tell_end": KNOWNFAIL,
            "test_mfs_lredir_command": KNOWNFAIL,
            "test_mfs_lredir_command_no_perm": KNOWNFAIL,
            "test_fat_ds3_lock_writable": KNOWNFAIL,
            "test_fat_ds3_lock_readlckd": KNOWNFAIL,
            "test_fat_ds3_lock_two_handles": KNOWNFAIL,
            "test_mfs_truename_vfat_linux_mounted_lfn": KNOWNFAIL,
            "test_mfs_truename_vfat_linux_mounted_sfn": KNOWNFAIL,
            "test_mfs_findfile_vfat_linux_mounted_lfn": KNOWNFAIL,
            "test_mfs_findfile_vfat_linux_mounted_sfn": KNOWNFAIL,
            "test_fat_ds3_share_open_twice": KNOWNFAIL,
            r"test_fat_ds3_share_open_(delete|rename)_.*": KNOWNFAIL,
            r"test_mfs_ds3_share_open_rename_(one|two)_process_fcb": KNOWNFAIL,
            r"test_fat_ds3_share_open_setfattrs_(one|two)_process": KNOWNFAIL,
            "test_create_new_psp": KNOWNFAIL,
            "test_command_com_keyword_exist": KNOWNFAIL,
            "test_memory_emm286_borland": KNOWNFAIL,
            "test_memory_hma_alloc": KNOWNFAIL,
            "test_memory_hma_alloc3": UNSUPPORTED,
            "test_memory_hma_chain": UNSUPPORTED,
            "test_memory_uma_strategy": KNOWNFAIL,
            "test_pcmos_build": KNOWNFAIL,
            r"test_libi86_item_\d+": KNOWNFAIL,
            "test_passing_dos_errorlevel_back": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / self.autoexec).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        self.mkfile(self.autoexec, contents, newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands" / "dosemu")

        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / "c" / self.confsys).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")


class FRDOS130TestCase(OurTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(FRDOS130TestCase, cls).setUpClass()
        cls.version = "FreeDOS kernel 2043"
        cls.prettyname = "FR-DOS-1.30"
        cls.files = [
            ("kernel.sys", "2bdf90c8bc8c0406cfa01349265bf782507af016"),
            ("command.com", "15abab3d3ee4a50449517131a13b2c5164610582"),
            ("share.com", "cadc29d49115cb3a250f90921cca345e7c427464"),
        ]
        cls.systype = SYSTYPE_FRDOS_NEW
        cls.autoexec = "fdautoem.bat"
        cls.confsys = "fdconfig.sys"
        cls.bootblocks = [
            ("boot-306-4-17.blk", "0092a320500d7a8359d40bddc48f592686745aed"),
            ("boot-615-4-17.blk", "2b757178c7ba97f8a439c83dc627d61c2d6b3cf6"),
            ("boot-900-15-17.blk", "8cd7adeff4a0265e8a8e20f7942672c677cbc891"),
        ]
        cls.images = [
            ("boot-floppy.img", "7b68b4dc2de5891bb3700816d8e1a323e8d150bb"),
        ]
        cls.actions = {
            "test_command_com_keyword_exist": KNOWNFAIL,
            "test_create_new_psp": KNOWNFAIL,
            "test_fat_ds3_lock_readlckd": KNOWNFAIL,
            "test_fat_ds3_lock_two_handles": KNOWNFAIL,
            "test_fat_ds3_lock_writable": KNOWNFAIL,
            r"test_fat_ds3_share_open_(delete|rename)_.*": KNOWNFAIL,
            r"test_fat_ds3_share_open_setfattrs_(one|two)_process": KNOWNFAIL,
            "test_fat_ds3_share_open_twice": KNOWNFAIL,
            "test_fat_fcb_find_wild_1": KNOWNFAIL,
            "test_fat_fcb_find_wild_2": KNOWNFAIL,
            "test_fat_fcb_find_wild_3": KNOWNFAIL,
            "test_fat_fcb_rename_wild_1": KNOWNFAIL,
            "test_fat_fcb_rename_wild_2": KNOWNFAIL,
            "test_fat_fcb_rename_wild_3": KNOWNFAIL,
            "test_fat_fcb_rename_wild_4": KNOWNFAIL,
            "test_memory_emm286_borland": KNOWNFAIL,
            "test_memory_hma_alloc": KNOWNFAIL,
            "test_memory_hma_alloc3": UNSUPPORTED,
            "test_memory_hma_chain": UNSUPPORTED,
            "test_memory_uma_strategy": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_1": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_2": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_3": KNOWNFAIL,
            "test_mfs_fcb_rename_wild_4": KNOWNFAIL,
            "test_passing_dos_errorlevel_back": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / self.autoexec).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        self.mkfile(self.autoexec, contents, newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands" / "dosemu")

        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / "c" / self.confsys).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")


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
        cls.actions = {
            "test_fat32_img_d_writable": UNSUPPORTED,
            "test_lfn_volume_info_fat32": UNSUPPORTED,
            "test_lfs_disk_info_fat32": UNSUPPORTED,
            "test_memory_hma_alloc3": UNSUPPORTED,
            "test_memory_hma_chain": UNSUPPORTED,
            "test_passing_dos_errorlevel_back": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / self.autoexec).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        self.mkfile(self.autoexec, contents, newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands" / "dosemu")

        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / "c" / self.confsys).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")

    def setUpDosVersion(self):
        self.mkfile("version.bat", "ver\r\nrem end\r\n")


class MSDOS700TestCase(OurTestCase, unittest.TestCase):
    # badged Win95 RTM at winworldpc.com

    @classmethod
    def setUpClass(cls):
        super(MSDOS700TestCase, cls).setUpClass()
        cls.version = "Windows 95. [Version 4.00.950]"
        cls.prettyname = "MS-DOS-7.00"
        cls.files = [
            ("io.sys", "22924f93dd0f9ea6a4624ccdd1bbcdf5eb43a308"),
            ("msdos.sys", "f5d01c68d518f4b8b2482d3815af8bb88003831d"),
            ("command.com", "67696207c3963a0dc9afab8cf37dbdb966c1f663"),
        ]
        cls.systype = SYSTYPE_MSDOS_NEW
        cls.autoexec = "autoemu.bat"
        cls.bootblocks = [
            ("boot-306-4-17.blk", "8c016e339ca6b8126fd2026ed3a7eeeb6cbb8903"),
            ("boot-615-4-17.blk", "b6fdddbfb37442a2762d5897de1aa7d7a694286a"),
            ("boot-900-15-17.blk", "8c1243481112f320f2a5f557f30db11174fe7e3d"),
        ]
        cls.images = [
            ("boot-floppy.img", ""),
        ]
        cls.actions = {
            "test_fat32_img_d_writable": UNSUPPORTED,
            "test_lfn_volume_info_fat32": UNSUPPORTED,
            "test_lfs_disk_info_fat32": UNSUPPORTED,
            "test_lfs_disk_info_mfs": KNOWNFAIL,
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / self.autoexec).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        self.mkfile(self.autoexec, contents, newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands" / "dosemu")

        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / "c" / self.confsys).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")

    def setUpDosVersion(self):
        self.mkfile("version.bat", "ver\r\nrem end\r\n")

        # Disable the logo here or we get blank screen
        self.mkfile("msdos.sys", """
[Options]
BootGUI=0
Logo=0
""", newline="\r\n")


class MSDOS710TestCase(OurTestCase, unittest.TestCase):
    # badged CDU (Chinese DOS Union) at winworldpc.com

    @classmethod
    def setUpClass(cls):
        super(MSDOS710TestCase, cls).setUpClass()
        cls.version = "MS-DOS 7.1 [Version 7.10.1999]"
        cls.prettyname = "MS-DOS-7.10"
        cls.files = [
            ("io.sys", "8c586b1bf38fc2042f2383ca873283a466be2f44"),
            ("msdos.sys", "cd1e6103ce9cdebbc7a5611df13ff4fbd5e2159c"),
            ("command.com", "f6547d81e625a784633c059e536e90ee45532202"),
        ]
        cls.systype = SYSTYPE_MSDOS_NEW
        cls.autoexec = "autoemu.bat"
        cls.bootblocks = [
            ("boot-306-4-17.blk", "0f520de6e2a33ef8fd336b2844957689fc1060e9"),
            ("boot-615-4-17.blk", "5e49a8ee7747191d87a2214cc0281736262687b9"),
            ("boot-900-15-17.blk", "2c29d06909c7d5ca46a3ca26ddde9287a11ef315"),
        ]
        cls.images = [
            ("boot-floppy.img", ""),
        ]

        cls.actions = {
        }

        cls.setUpClassPost()

    def setUpDosAutoexec(self):
        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / self.autoexec).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        self.mkfile(self.autoexec, contents, newline="\r\n")

    def setUpDosConfig(self):
        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        p.symlink_to(self.topdir / "commands" / "dosemu")

        # Use the (almost) standard shipped config
        contents = (self.topdir / "src" / "bindist" / "c" / self.confsys).read_text()
        contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
        contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")

    def setUpDosVersion(self):
        self.mkfile("version.bat", "ver\r\nrem end\r\n")

        # Disable the logo here or we get blank screen
        self.mkfile("msdos.sys", """
[Options]
BootGUI=0
Logo=0
""", newline="\r\n")


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
        cls.autoexec = "fdppauto.bat"
        cls.confsys = "fdppconf.sys"

        cls.setUpClassPost()


if __name__ == '__main__':

    # Dynamically create libi86 tests
    libi86_create_items(OurTestCase)

    tests = [t[0] for t in
            inspect.getmembers(OurTestCase, predicate=inspect.isfunction)
            if t[0].startswith("test")]

    xtests = [t[0] for t in
            inspect.getmembers(OurTestCase, predicate=inspect.isfunction)
            if t[0].startswith("xtest")]

    cases = [c[0] for c in
            inspect.getmembers(modules[__name__], predicate=inspect.isclass)
            if issubclass(c[1], OurTestCase) and c[0] != "OurTestCase"]

    attrs = sorted(OurTestCase.attrs)

    def explode(n, attr=None):
        if n in tests:
            return [c + "." + n for c in cases]
        if n in xtests:
            return [c + "." + n for c in cases]
        if n in cases:
            if attr:
                return [n + "." + t[0] for t in
                    inspect.getmembers(OurTestCase, predicate=inspect.isfunction)
                    if hasattr(t[1], attr)]
            else:
                return [n,]
        p = n.split('.')
        if p and p[0] in cases and (p[1] in tests or p[1] in xtests):
            return [n,]
        return []

    if len(argv) > 1:
        if argv[1] == "--help":
            print(("Usage: %s [--help | --get-test-binaries | " +
                   "--list-attrs | --list-cases | --list-tests] | " +
                   "[--require-attr=STRING TestCase ...] | " +
                   "[TestCase[.testname] ...]") % argv[0])
            exit(0)
        elif argv[1] == "--get-test-binaries":
            get_test_binaries()
            exit(0)
        elif argv[1] == "--list-attrs":
            for a in attrs:
                print(str(a))
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
            x = re.match("^--require-attr=(\S+).*$", argv[1])
            if x:
                attr = x.groups()[0]
                del argv[1]
            else:
                attr = None

            a = []
            for b in [explode(x, attr=attr) for x in argv[1:]]:
                a.extend(b)

            if not len(a):
                print("No tests found, was your testcase or testname incorrect? See --help")
                exit(1)
            argv = [argv[0],] + a
            main(argv)

    main()
