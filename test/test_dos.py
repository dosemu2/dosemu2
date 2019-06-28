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
