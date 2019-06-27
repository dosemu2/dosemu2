try:
    import unittest2 as unittest
except ImportError:
    import unittest

import pexpect
import string
import random
import re
import pkgconfig

from hashlib import sha1
from nose.plugins.attrib import attr
from os import mkdir, makedirs, rename, unlink, statvfs, environ, listdir
from os.path import isdir, join, exists
from ptyprocess import PtyProcessError
from shutil import copy, copytree, rmtree
from subprocess import Popen, check_call
from tarfile import open as topen
from tempfile import TemporaryFile
from time import sleep
from threading import Thread

BINSDIR = "test-binaries"
WORKDIR = "test-imagedir/dXXXXs/c"
CC = "/opt/djgpp/bin/i586-pc-msdosdjgpp-gcc"
AS = "as"
LD = "gcc"
OBJCOPY = "objcopy"

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


def mkfile(fname, content, dname=WORKDIR, writemode="w"):
    with open(join(dname, fname), writemode) as f:
        f.write(content)


def mkexe(fname, content, dname=WORKDIR):
    basename = join(dname, fname)
    with open(basename + ".c", "w") as f:
        f.write(content)
    check_call([CC, "-o", basename + ".exe", basename + ".c"])


def mkcom(fname, content, dname=WORKDIR):
    basename = join(dname, fname)
    with open(basename + ".S", "w") as f:
        f.write(content)
    check_call([AS, "-o", basename + ".o", basename + ".S"])
    check_call([LD,
        "-static",
        "-Wl,--section-start=.text=0x100,-e,_start16", "-nostdlib",
        "-o", basename + ".com.elf",
        basename + ".o"])
    check_call([OBJCOPY,
        "-j", ".text", "-O", "binary",
        basename + ".com.elf",
        basename + ".com"])


def mkstring(length):
    return ''.join(random.choice(string.hexdigits) for x in range(length))


class BootTestCase(object):

    @classmethod
    def setUpClass(cls):
        imagedir = WORKDIR.split('/')[0]
        if not imagedir or imagedir == "." or imagedir[0] == "/":
            raise ValueError
        cls.imagedir = imagedir
        cls.version = "BootTestCase default"
        cls.prettyname = "NoPrettyNameSet"
        cls.tarfile = None
        cls.files = [(None, None)]
        cls.skipfat16b = False
        cls.skipimage = False
        cls.skipfloppy = False
        cls.skipnativebootblk = False
        cls.systype = None
        cls.bootblocks  = [(None, None)]
        cls.images  = [(None, None)]
        cls.autoexec = "autoexec.bat"
        cls.confsys = "config.sys"

        if not exists("test-libdir"):
            mkdir("test-libdir");
            mkdir("test-libdir/dosemu2-cmds-0.2");

    @classmethod
    def tearDownClass(cls):
        pass

    def setUp(self):
        if self.tarfile is None:
            self.tarfile = self.prettyname + ".tar"

        rmtree(self.imagedir, ignore_errors=True)
        makedirs(WORKDIR)

        self.logname = None
        self.xptname = None

        # Extract the boot files
        if self.tarfile != "":
            self.unTarOrSkip(self.tarfile, self.files)

        # Empty dosemu.conf for default values
        mkfile("dosemu.conf", """$_lpt1 = ""\n""", self.imagedir)

        # copy std dosemu commands
        copytree("commands", join(WORKDIR, "dosemu"), symlinks=True)

        # create minimal startup files
        mkfile(self.confsys, """\
lastdrive=Z\r
device=dosemu\emufs.sys\r
""")

        mkfile(self.autoexec, """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("version.bat", "ver\r\nrem end\r\n")

    def tearDown(self):
        if hasattr(self, '_resultForDoCleanups'):
            wasSuccessful = True
            for i in self._resultForDoCleanups.failures:
                if self.id() == i[0].id():
                    wasSuccessful = False
                    break;
            if self.logname and wasSuccessful:
                unlink(self.logname)
            if self.xptname and wasSuccessful:
                unlink(self.xptname)

    def shortDescription(self):
        doc = super(BootTestCase, self).shortDescription()
        return "Test %s %s" % (self.prettyname, doc)

# helpers

    def unTarOrSkip(self, tname, files):
        tfile = join(BINSDIR, tname)

        try:
            tar = topen(tfile)
        except IOError:
            self.skipTest("Archive not found or unreadable(%s)" % tfile)

        for f in files:
            try:
                tar.extract(f[0], path=WORKDIR)
                with open(join(WORKDIR, f[0])) as g:
                    s1 = sha1(g.read()).hexdigest()
                    self.assertEqual(
                        f[1],
                        s1, "Sha1sum mismatch file (%s), %s != %s" %
                        (f[0], f[1], s1)
                    )
            except KeyError:
                self.skipTest("File (%s) not found in archive" % f[0])

        tar.close()

    def unTarBootBlockOrSkip(self, name, mv=False):
        bootblock = [ x for x in self.bootblocks if re.match(name, x[0]) ]
        if not len(bootblock) == 1:
            self.skipTest("Boot block signature not available")

        self.unTarOrSkip(self.tarfile, bootblock)

        if(mv):
            rename(join(WORKDIR, bootblock[0][0]), join(WORKDIR, "boot.blk"))

    def unTarImageOrSkip(self, name):
        image = [ x for x in self.images if name == x[0] ]
        if not len(image) == 1:
            self.skipTest("Image signature not available")

        self.unTarOrSkip(self.tarfile, image)
        rename(join(WORKDIR, name), join(self.imagedir, name))

    def mkimage(self, fat, files, bootblk=True, cwd=None):
        if fat == "12":
            tnum = "306"
            hnum = "4"
            regx = "3.."
        elif fat == "16":
            tnum = "615"
            hnum = "4"
            regx = "6.."
        else:       # 16B
            tnum = "900"
            hnum = "15"
            regx = "[89].."

        if bootblk:
            blkname = "boot-%s-%s-17.blk" % (regx, hnum)
            self.unTarBootBlockOrSkip(blkname, True)
            blkarg = [ "-b", "boot.blk" ]
        else:
            blkarg = []

        xfiles = [x[0] for x in files]

        name = "fat%s.img" % fat

        if cwd is None:
            cwd = self.imagedir + "/dXXXXs/c/"

        # mkfatimage [-b bsectfile] [{-t tracks | -k Kbytes}]
        #            [-l volume-label] [-f outfile] [-p ] [file...]
        result = Popen(
            ["../../../bin/mkfatimage16",
                "-t", tnum,
                "-h", hnum,
                "-f", "../../" + name,
                "-p"
            ] + blkarg + xfiles,
            cwd=cwd
        )

        return name

    def runDosemu(self, cmd, opts="video{none}", outfile=None, config=None):
        # Note: if debugging is turned on then times increase 10x
        self.logname = "%s.log" % self.id()
        self.xptname = "%s.xpt" % self.id()

        dbin = "bin/dosemu.bin"
        args = ["-n",
                "-f", join(self.imagedir, "dosemu.conf"),
                #    "-Da",
                "--Fimagedir", self.imagedir,
                "--Flibdir", "test-libdir",
                "-o", self.logname,
                "-I", opts]

        if config is not None:
            mkfile("dosemu.conf", config, dname=self.imagedir, writemode="a")

        child = pexpect.spawn(dbin, args)
        with open(self.xptname, "w") as fout:
            child.logfile = fout
            child.setecho(False)
            try:
                child.expect(['unix -e[\r\n]*'], timeout=10)
                child.expect(['>[\r\n]*', pexpect.TIMEOUT], timeout=1)
                child.send(cmd + '\r\n')
                child.expect(['rem end'], timeout=5)
                if outfile is None:
                   ret = child.before
                else:
                   with open(join(WORKDIR, outfile), "r") as f:
                      ret = f.read()
            except pexpect.TIMEOUT:
                ret = 'Timeout'
            except pexpect.EOF:
                ret = 'EndOfFile'

        try:
            child.close(force=True)
        except PtyProcessError:
            pass

        return ret

# tests

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

        self.assertIn(self.systype, systypeline)

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

        mkfile(self.autoexec, """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        # test to see if the directory intnum made it through to linux
        if operation == "Create":
            self.assertIn("Directory Operation Success", results);
            self.assertTrue(isdir(join(testdir, testname)), "Directory not created")
        elif operation == "Delete":
            self.assertIn("Directory Operation Success", results);
            self.assertFalse(isdir(join(testdir, testname)), "Directory not deleted")
        elif operation == "DeleteNotEmpty":
            self.assertIn("Directory Operation Failed", results);
            self.assertTrue(isdir(join(testdir, testname)), "Directory incorrectly deleted")
        elif operation == "Chdir":
            self.assertIn("Directory Operation Success", results);
            if nametype == "SFN":
                self.assertIn("(" + testname.upper() + ")", results);
            else:
                self.assertIn("(" + testname + ")", results);

    @attr('mfs_sfn_directory_create')
    def test_mfs_sfn_directory_create(self):
        """MFS SFN directory create"""
        self._test_mfs_directory_common("SFN", "Create")

    @attr('mfs_sfn_directory_delete')
    def test_mfs_sfn_directory_delete(self):
        """MFS SFN directory delete"""
        self._test_mfs_directory_common("SFN", "Delete")

    @attr('mfs_sfn_directory_delete_not_empty')
    def test_mfs_sfn_directory_delete_not_empty(self):
        """MFS SFN directory delete not empty"""
        self._test_mfs_directory_common("SFN", "DeleteNotEmpty")

    @attr('mfs_sfn_directory_chdir')
    def test_mfs_sfn_directory_chdir(self):
        """MFS SFN directory change current"""
        self._test_mfs_directory_common("SFN", "Chdir")

    @attr('mfs_lfn_directory_create')
    def test_mfs_lfn_directory_create(self):
        """MFS LFN directory create"""
        self._test_mfs_directory_common("LFN", "Create")

    @attr('mfs_lfn_directory_delete')
    def test_mfs_lfn_directory_delete(self):
        """MFS LFN directory delete"""
        self._test_mfs_directory_common("LFN", "Delete")

    @attr('mfs_lfn_directory_delete_not_empty')
    def test_mfs_lfn_directory_delete_not_empty(self):
        """MFS LFN directory delete not empty"""
        self._test_mfs_directory_common("LFN", "DeleteNotEmpty")

    @attr('mfs_lfn_directory_chdir')
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

        mkfile(self.autoexec, """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

            if nametype == "SFN":
                self.assertIn("(" + testname.upper() + ")", results);
            else:
                self.assertIn("(" + testname + ")", results);

    @attr('mfs_sfn_get_current_directory')
    def test_mfs_sfn_get_current_directory(self):
        """MFS SFN get current directory"""
        self._test_mfs_get_current_directory("SFN")

    @attr('mfs_lfn_get_current_directory')
    def test_mfs_lfn_get_current_directory(self):
        """MFS LFN get current directory"""
        self._test_mfs_get_current_directory("LFN")

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

        mkfile(self.autoexec, """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        if expected is None:
            self.assertIn("Directory Operation Failed", results);
        else:
            self.assertIn("Directory Operation Success", results);
            self.assertIn("(" + expected + ")", results);

    @attr('mfs_sfn_truename')
    def test_mfs_sfn_truename(self):
        """MFS SFN Truename"""
        self._test_mfs_truename("SFN", "testname", "C:\\TESTNAME")
        self._test_mfs_truename("SFN", "d:\\shrtname.txt", "D:\\SHRTNAME.TXT")
        self._test_mfs_truename("SFN", "d:\\testname", "D:\\TESTNAME")
        self._test_mfs_truename("SFN", "aux", "C:/AUX")
        self._test_mfs_truename("SFN", "d:testname", "D:\\TESTNAME")
# FAIL        self._test_mfs_truename("SFN", "d:\\fakedir\\testname", None)

    @attr('mfs_lfn_truename')
    def test_mfs_lfn_truename(self):
        """MFS LFN Truename"""
        # NOTE: not sure that the output should be UPCASED.
        self._test_mfs_truename("LFN0", "very long testname", "C:\\VERY LONG TESTNAME")
        self._test_mfs_truename("LFN0", "d:\\verylongtestname.txt", "D:\\VERYLONGTESTNAME.TXT")
        self._test_mfs_truename("LFN0", "d:\\very long testname", "D:\\VERY LONG TESTNAME")
        self._test_mfs_truename("LFN0", "aux", "C:/AUX")
        self._test_mfs_truename("LFN0", "d:very long testname", "D:\\VERY LONG TESTNAME")
# FAIL        self._test_mfs_truename("LFN", "d:\\fakedir\\very long testname", None)

        self._test_mfs_truename("LFN0", "D:\\" + PRGFIL_SFN, "D:\\" + PRGFIL_SFN)
        self._test_mfs_truename("LFN1", "D:\\" + PRGFIL_SFN, "D:\\" + PRGFIL_SFN)
        self._test_mfs_truename("LFN2", "D:\\" + PRGFIL_SFN, "D:\\" + PRGFIL_LFN)

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

        mkfile(self.autoexec, """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("test_mfs.bat", """\
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
                self.assertIn("\n{:<8} {:<3}".format(f,e).upper(), results.upper())

        if fstype == "MFS":
            results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else: # FAT
            files = [(x,0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

        if testname == "simple":
            self.assertIn("Rename Operation Success", results);
            assertIsPresent(testdir, results, fstype, fn2, fe2, "File not renamed")

        elif testname == "source_missing":
            self.assertIn("Rename Operation Failed", results);

        elif testname == "target_exists":
            self.assertIn("Rename Operation Failed", results);

        elif testname == "wild_one":
            self.assertIn("Rename Operation Success", results);
            assertIsPresent(testdir, results, fstype, "one", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "two", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "three", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "four", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "five", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "none", "ctl", "File incorrectly renamed")

        elif testname == "wild_two":
            self.assertIn("Rename Operation Success", results);
            assertIsPresent(testdir, results, fstype, "bone", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "btwo", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "bthree", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "bfour", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "bfive", "out", "File not renamed")
            assertIsPresent(testdir, results, fstype, "xnone", "ctl", "File incorrectly renamed")

        elif testname == "wild_three":
            self.assertIn("Rename Operation Success", results);
            assertIsPresent(testdir, results, fstype, "abc601", "txt", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc602", "txt", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc603", "txt", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc604", "txt", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc605", "txt", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc610", "txt", "File not renamed")
            assertIsPresent(testdir, results, fstype, "xbc007", "txt", "File incorrectly renamed")

        elif testname == "wild_four":
            self.assertIn("Rename Operation Success", results);
            assertIsPresent(testdir, results, fstype, "abc001", "ht", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc002", "ht", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc003", "ht", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc004", "ht", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc005", "ht", "File not renamed")
            assertIsPresent(testdir, results, fstype, "abc010", "ht", "File not renamed")
            assertIsPresent(testdir, results, fstype, "xbc007", "htm", "File incorrectly renamed")

    @attr('fcb_rename_simple')
    def test_fat_fcb_rename_simple(self):
        """FAT FCB file rename simple"""
        self._test_fcb_rename_common("FAT", "simple")

    @attr('fcb_rename_simple')
    def test_mfs_fcb_rename_simple(self):
        """MFS FCB file rename simple"""
        self._test_fcb_rename_common("MFS", "simple")

    @attr('fcb_rename_source_missing')
    def test_fat_fcb_rename_source_missing(self):
        """FAT FCB file rename source missing"""
        self._test_fcb_rename_common("FAT", "source_missing")

    @attr('fcb_rename_source_missing')
    def test_mfs_fcb_rename_source_missing(self):
        """MFS FCB file rename source missing"""
        self._test_fcb_rename_common("MFS", "source_missing")

    @attr('fcb_rename_target_exists')
    def test_fat_fcb_rename_target_exists(self):
        """FAT FCB file rename target exists"""
        self._test_fcb_rename_common("FAT", "target_exists")

    @attr('fcb_rename_target_exists')
    def test_mfs_fcb_rename_target_exists(self):
        """MFS FCB file rename target exists"""
        self._test_fcb_rename_common("MFS", "target_exists")

    @attr('fcb_rename_wild_1')
    def test_fat_fcb_rename_wild_1(self):
        """FAT FCB file rename wildcard one"""
        self._test_fcb_rename_common("FAT", "wild_one")

    @attr('fcb_rename_wild_1')
    def test_mfs_fcb_rename_wild_1(self):
        """MFS FCB file rename wildcard one"""
        self._test_fcb_rename_common("MFS", "wild_one")

    @attr('fcb_rename_wild_2')
    def test_fat_fcb_rename_wild_2(self):
        """FAT FCB file rename wildcard two"""
        self._test_fcb_rename_common("FAT", "wild_two")

    @attr('fcb_rename_wild_2')
    def test_mfs_fcb_rename_wild_2(self):
        """MFS FCB file rename wildcard two"""
        self._test_fcb_rename_common("MFS", "wild_two")

    @attr('fcb_rename_wild_3')
    def test_fat_fcb_rename_wild_3(self):
        """FAT FCB file rename wildcard three"""
        self._test_fcb_rename_common("FAT", "wild_three")

    @attr('fcb_rename_wild_3')
    def test_mfs_fcb_rename_wild_3(self):
        """MFS FCB file rename wildcard three"""
        self._test_fcb_rename_common("MFS", "wild_three")

    @attr('fcb_rename_wild_4')
    def test_fat_fcb_rename_wild_4(self):
        """FAT FCB file rename wildcard four"""
        self._test_fcb_rename_common("FAT", "wild_four")

    @attr('fcb_rename_wild_4')
    def test_mfs_fcb_rename_wild_4(self):
        """MFS FCB file rename wildcard four"""
        self._test_fcb_rename_common("MFS", "wild_four")

    def _test_ds2_rename_common(self, fstype, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

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
            makedirs(testdir + "/" + fn1)
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
            makedirs(testdir + "/" + fn1)
            makedirs(testdir + "/" + fn2)

        mkfile(self.autoexec, """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("test_mfs.bat", """\
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
                self.assertIn("\n{:<8} {:<3}".format(f,e).upper(), results.upper())

        def assertIsPresentDir(testdir, results, fstype, f, msg=None):
            if fstype == "MFS":
                self.assertTrue(exists(join(testdir, f)), msg)
            else:
                self.assertIn("\n{:<8}".format(f).upper(), results.upper())

        if fstype == "MFS":
            results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else: # FAT
            files = [(x,0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

        if testname == "file":
            self.assertIn("Rename Operation Success", results);
            assertIsPresent(testdir, results, fstype, fn2, fe2, "File not renamed")

        elif testname == "file_src_missing":
            self.assertIn("Rename Operation Failed", results);

        elif testname == "file_tgt_exists":
            self.assertIn("Rename Operation Failed", results);

        elif testname == "dir":
            self.assertIn("Rename Operation Success", results);
            assertIsPresentDir(testdir, results, fstype, fn2, "Directory not renamed")

        elif testname == "dir_src_missing":
            self.assertIn("Rename Operation Failed", results);

        elif testname == "dir_tgt_exists":
            self.assertIn("Rename Operation Failed", results);

    @attr('ds2_rename')
    def test_fat_ds2_rename_file(self):
        """FAT DOSv2 rename file"""
        self._test_ds2_rename_common("FAT", "file")

    @attr('ds2_rename')
    def test_mfs_ds2_rename_file(self):
        """MFS DOSv2 rename file"""
        self._test_ds2_rename_common("MFS", "file")

    @attr('ds2_rename')
    def test_fat_ds2_rename_file_src_missing(self):
        """FAT DOSv2 rename file src missing"""
        self._test_ds2_rename_common("FAT", "file_src_missing")

    @attr('ds2_rename')
    def test_mfs_ds2_rename_file_src_missing(self):
        """MFS DOSv2 rename file src missing"""
        self._test_ds2_rename_common("MFS", "file_src_missing")

    @attr('ds2_rename')
    def test_fat_ds2_rename_file_tgt_exists(self):
        """FAT DOSv2 rename file tgt exists"""
        self._test_ds2_rename_common("FAT", "file_tgt_exists")

    @attr('ds2_rename')
    def test_mfs_ds2_rename_file_tgt_exists(self):
        """MFS DOSv2 rename file tgt exists"""
        self._test_ds2_rename_common("MFS", "file_tgt_exists")

    @attr('ds2_rename')
    def test_fat_ds2_rename_dir(self):
        """FAT DOSv2 rename dir"""
        self._test_ds2_rename_common("FAT", "dir")

    @attr('ds2_rename')
    def test_mfs_ds2_rename_dir(self):
        """MFS DOSv2 rename dir"""
        self._test_ds2_rename_common("MFS", "dir")

    @attr('ds2_rename')
    def test_fat_ds2_rename_dir_src_missing(self):
        """FAT DOSv2 rename dir src missing"""
        self._test_ds2_rename_common("FAT", "dir_src_missing")

    @attr('ds2_rename')
    def test_mfs_ds2_rename_dir_src_missing(self):
        """MFS DOSv2 rename dir src missing"""
        self._test_ds2_rename_common("MFS", "dir_src_missing")

    @attr('ds2_rename')
    def test_fat_ds2_rename_dir_tgt_exists(self):
        """FAT DOSv2 rename dir tgt exists"""
        self._test_ds2_rename_common("FAT", "dir_tgt_exists")

    @attr('ds2_rename')
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

        mkfile(self.autoexec, """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("test_mfs.bat", """\
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
                self.assertNotIn("\n{:<8} {:<3}".format(f,e).upper(), results.upper())

        if fstype == "MFS":
            results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")
            with open(self.xptname, "r") as f:
                xpt = f.read()
                if "EMUFS revectoring only" in xpt:
                    self.skipTest("MFS unsupported")
        else: # FAT
            files = [(x,0) for x in listdir(testdir)]

            name = self.mkimage("12", files, bootblk=False, cwd=testdir)
            results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % name)

        if testname == "file":
            self.assertIn("Delete Operation Success", results);
            assertIsNotPresent(testdir, results, fstype, fn1, fe1, "File not deleted")

        elif testname == "file_missing":
            self.assertIn("Delete Operation Failed", results);

    @attr('ds2_delete')
    def test_fat_ds2_delete_file(self):
        """FAT DOSv2 delete file"""
        self._test_ds2_delete_common("FAT", "file")

    @attr('ds2_delete')
    def test_mfs_ds2_delete_file(self):
        """MFS DOSv2 delete file"""
        self._test_ds2_delete_common("MFS", "file")

    @attr('ds2_delete')
    def test_fat_ds2_delete_file_missing(self):
        """FAT DOSv2 delete file missing"""
        self._test_ds2_delete_common("FAT", "file_missing")

    @attr('ds2_delete')
    def test_mfs_ds2_delete_file_missing(self):
        """MFS DOSv2 delete file missing"""
        self._test_ds2_delete_common("MFS", "file_missing")

    @attr('three_drives_vfs')
    def test_three_drives_vfs(self):
        """Three vfs directories configured"""
        # C exists as part of standard test
        makedirs("test-imagedir/dXXXXs/d")
        makedirs("test-imagedir/dXXXXs/e")

        results = self.runDosemu("version.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 dXXXXs/e:hdtype1 +1"
$_floppy_a = ""
""")

        self.assertIn(self.version, results) # Just to check we booted


class PPDOSGITTestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PPDOSGITTestCase, cls).setUpClass()
        cls.version = "FDPP kernel"
        cls.prettyname = "PP-DOS-GIT"

        # Use the default files that FDPP installed
        cls.tarfile = ""

        cls.systype = SYSTYPE_FDPP
        cls.skipimage = True
        cls.skipfloppy = True
        cls.skipnativebootblk = True
#        cls.autoexec = "autofdpp.bat"
        cls.confsys = "fdppconf.sys"

    def setUp(self):
        super(PPDOSGITTestCase, self).setUp()

        mkfile("version.bat", "ver /r\r\nrem end\r\n")
