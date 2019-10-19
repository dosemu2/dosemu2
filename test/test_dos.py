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
CC = "i586-pc-msdosdjgpp-gcc"
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
system -s DOSEMU_VERSION\r
system -e\r
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
                child.expect(['system -e[\r\n]*'], timeout=10)
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

### Tests using assembler

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

    def test_mfs_sfn_get_current_directory(self):
        """MFS SFN get current directory"""
        self._test_mfs_get_current_directory("SFN")

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

    def test_mfs_sfn_truename(self):
        """MFS SFN Truename"""
        self._test_mfs_truename("SFN", "testname", "C:\\TESTNAME")
        self._test_mfs_truename("SFN", "d:\\shrtname.txt", "D:\\SHRTNAME.TXT")
        self._test_mfs_truename("SFN", "d:\\testname", "D:\\TESTNAME")
        self._test_mfs_truename("SFN", "aux", "C:/AUX")
        self._test_mfs_truename("SFN", "d:testname", "D:\\TESTNAME")
# FAIL        self._test_mfs_truename("SFN", "d:\\fakedir\\testname", None)

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
                self.assertRegexpMatches(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()))

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

        mkfile("test_mfs.bat", """\
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
                self.assertRegexpMatches(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()), msg)

        def assertIsPresentDir(testdir, results, fstype, f, msg=None):
            if fstype == "MFS":
                self.assertTrue(exists(join(testdir, f)), msg)
            else:
                # 2019-06-27 11:29 <DIR>         DOSEMU
                # DOSEMU               <DIR>  06-27-19  5:33p
                self.assertRegexpMatches(results.upper(),
                    r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s<DIR>\s+%s"
                    r"|"
                    r"%s\s+<DIR>\s+\d{2}-\d{2}-\d{2}\s+\d+:\d+[AaPp]" % (f.upper(),f.upper()), msg)

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
                self.assertNotRegexpMatches(results.upper(), "%s( +|\.)%s" % (f.upper(), e.upper()))

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

### Tests using neiher compiler nor assembler

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

    def test_floppy_img(self):
        """Floppy image file"""

        if self.skipimage:
            self.skipTest("Booting from image not supported")
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

        if self.skipfloppy:
            self.skipTest("Booting from floppy not supported")

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

        self.assertIn(self.version, results) # Just to check we booted

    def _test_fat_img_d_writable(self, fat):
        mkfile("test_dfw.bat", """\
D:\r
mkdir test\r
echo hello > hello.txt\r
DIR\r
rem end\r
""")

        name = self.mkimage(fat, [("test_dfw.bat", "0")], bootblk=False)

        results = self.runDosemu("test_dfw.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
""" % name)

        # Std DOS format
        # TEST         <DIR>
        # HELLO    TXT 8
        #
        # ComCom32 format
        # 2019-06-28 22:29 <DIR>         TEST
        # 2019-06-28 22:29             8 HELLO.TXT
        self.assertRegexpMatches(results,
                r"TEST[\t ]+<DIR>"
                r"|"
                r"\d{4}-\d{2}-\d{2}\s\d{2}:\d{2}\s<DIR>\s+TEST")
        self.assertRegexpMatches(results,
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
        mkfile("test_mfs.bat", "lredir\r\nrem end\r\n")

        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

# C:\>lredir
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertRegexpMatches(results, r"C: = .*LINUX\\FS")

    def test_mfs_lredir_command(self):
        """MFS lredir command redirection"""
        mkfile("test_mfs.bat", """\
lredir X: LINUX\\FS\\bin\r
lredir\r
rem end\r
""")
        results = self.runDosemu("test_mfs.bat", config="""\
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

        self.assertRegexpMatches(results, r"X: = .*LINUX\\FS\\bin")

### Tests using the DJGPP DOS compiler

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

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config="""\
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

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config="""\
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

        testdata = mkstring(64) # need to be fairly short to pass as arg
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertNotIn("open failed", results);

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

        mkfile("test_mfs.bat", """\
c:\\lfnvinfo %s\r
rem end\r
""" % drive)

        # C exists as part of standard test
        makedirs("test-imagedir/dXXXXs/d")

        name = self.mkimage("FAT16", [("test_mfs.bat", "0")], bootblk=False)

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

        results = self.runDosemu("test_mfs.bat", config = """\
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

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertNotIn("Call failed", results)

        fsinfo = statvfs("test-imagedir/dXXXXs/c")
        lfs_total = fsinfo.f_blocks * fsinfo.f_bsize;
        lfs_avail = fsinfo.f_bavail * fsinfo.f_bsize;

        t = re.search(r'total_bytes\((\d+)\)', results)
        dfs_total = int(t.group(1))
        a = re.search(r'avail_bytes\((\d+)\)', results)
        dfs_avail = int(a.group(1))

# see if we are within 5% of the values obtained from Linux
        msg = "total dos %d, linux %d" % (dfs_total, lfs_total)
        self.assertLessEqual(dfs_total, lfs_total * 1.05, msg);
        self.assertGreaterEqual(dfs_total, lfs_total * 0.95, msg)

        msg = "avail dos %d, linux %d" % (dfs_avail, lfs_avail)
        self.assertLessEqual(dfs_avail, lfs_avail * 1.05, msg)
        self.assertGreaterEqual(dfs_avail, lfs_avail * 0.95, msg)


    def test_int21_disk_info(self):
        """INT21 disk info"""

        path = "C:\\"

        mkfile("test_mfs.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertNotIn("Call failed", results)

        fsinfo = statvfs("test-imagedir/dXXXXs/c")
        lfs_total = fsinfo.f_blocks * fsinfo.f_bsize;
        lfs_avail = fsinfo.f_bavail * fsinfo.f_bsize;

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
        self.assertLessEqual(dfs_total, lfs_total * 1.05, msg);
        self.assertGreaterEqual(dfs_total, lfs_total * 0.95, msg)

        msg = "avail dos %d, linux %d" % (dfs_avail, lfs_avail)
        self.assertLessEqual(dfs_avail, lfs_avail * 1.05, msg)
        self.assertGreaterEqual(dfs_avail, lfs_avail * 0.95, msg)


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
#        cls.autoexec = "fdppauto.bat"
        cls.confsys = "fdppconf.sys"

    def setUp(self):
        super(PPDOSGITTestCase, self).setUp()

        mkfile("version.bat", "ver /r\r\nrem end\r\n")
