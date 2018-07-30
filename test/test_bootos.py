try:
    import unittest2 as unittest
except ImportError:
    import unittest

import pexpect
import string
import random
import re

from hashlib import sha1
from nose.plugins.attrib import attr
from os import makedirs, rename, unlink, statvfs
from os.path import isdir, join, exists
from ptyprocess import PtyProcessError
from shutil import copytree, rmtree
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
        if not imagedir or imagedir is "." or imagedir[0] is "/":
            raise ValueError
        cls.imagedir = imagedir
        cls.version = "BootTestCase default"
        cls.tarfile = None
        cls.files = [(None, None)]
        cls.skipfat16b = False
        cls.skipnativebootblk = False
        cls.systype = None
        cls.bootblocks  = [(None, None)]
        cls.images  = [(None, None)]

    @classmethod
    def tearDownClass(cls):
        pass

    def setUp(self):
        rmtree(self.imagedir, ignore_errors=True)
        makedirs(WORKDIR)

        self.logname = None
        self.xptname = None

        # Extract the boot files
        self.unTarOrSkip(self.tarfile, self.files)

        # Empty dosemu.conf for default values
        mkfile("dosemu.conf", """$_lpt1 = ""\n""", self.imagedir)

        # copy std dosemu commands
        copytree("commands", join(WORKDIR, "dosemu"))

        # create minimal startup files
        mkfile("config.sys", """\
lastdrive=Z\r
device=dosemu\emufs.sys /tmp\r
""")

        mkfile("autoexec.bat", """\
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
        return "Test %s %s" % (self.tarfile.split('.tar')[0], doc)

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

    def mkimage(self, fat, files, bootblk=True):
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

        # mkfatimage [-b bsectfile] [{-t tracks | -k Kbytes}]
        #            [-l volume-label] [-f outfile] [-p ] [file...]
        result = Popen(
            ["../../../bin/mkfatimage16",
                "-t", tnum,
                "-h", hnum,
                "-f", "../../" + name,
                "-p"
            ] + blkarg + xfiles,
            cwd=self.imagedir + "/dXXXXs/c/"
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

    def _test_fat_img(self, fat):
        mkfile("config.sys", """\
lastdrive=Z\r
device=emufs.sys /tmp\r
""")
        name = self.mkimage(fat, self.files +
               [("autoexec.bat", "0"),
                ("config.sys", "0"),
                ("version.bat", "0"),
                ("dosemu/emufs.sys", "0")
               ])

        results = self.runDosemu("version.bat",
            config="""$_hdimage = "%s"\n""" % name)

        self.assertIn(self.version, results)

    @attr('fat12_img')
    def test_fat12_img(self):
        """FAT12 image file"""
        if self.skipnativebootblk:
            self.skipTest("Not supported by Boot Block")

        self._test_fat_img("12")

    @attr('fat16_img')
    def test_fat16_img(self):
        """FAT16 image file"""
        if self.skipnativebootblk:
            self.skipTest("Not supported by Boot Block")

        self._test_fat_img("16")

    @attr('fat16b_img')
    def test_fat16b_img(self):
        """FAT16B image file"""
        if self.skipfat16b:
            self.skipTest("Not supported by OS")

        self._test_fat_img("16B")

    def _test_fat12_vfs(self):
        results = self.runDosemu("version.bat",
            config="""$_hdimage = "dXXXXs/c:hdtype1"\n""")

        self.assertIn(self.version, results)

    @attr('fat12_vfs')
    def test_fat12_vfs(self):
        """FAT12 vfs directory"""
        self._test_fat12_vfs()

    @attr('fat12_vfs_bootblk')
    def test_fat12_vfs_bootblk(self):
        """FAT12 vfs directory boot.blk"""
        if self.skipnativebootblk:
            self.skipTest("Not supported by Boot Block")

        self.unTarBootBlockOrSkip("boot-3..-4-17.blk", True)
        self._test_fat12_vfs()

    def _test_fat16_vfs(self):
        results = self.runDosemu("version.bat",
            config="""$_hdimage = "dXXXXs/c:hdtype2"\n""")

        self.assertIn(self.version, results)

    @attr('fat16_vfs')
    def test_fat16_vfs(self):
        """FAT16 vfs directory"""
        self._test_fat16_vfs()

    @attr('fat16_vfs_bootblk')
    def test_fat16_vfs_bootblk(self):
        """FAT16 vfs directory boot.blk"""
        if self.skipnativebootblk:
            self.skipTest("Not supported by Boot Block")

        self.unTarBootBlockOrSkip("boot-6..-4-17.blk", True)
        self._test_fat16_vfs()

    def _test_fat16b_vfs(self):
        """FAT16B vfs directory"""
        results = self.runDosemu("version.bat",
            config="""$_hdimage = "dXXXXs/c:hdtype9"\n""")

        self.assertIn(self.version, results)

    @attr('fat16b_vfs')
    def test_fat16b_vfs(self):
        """FAT16B vfs directory"""
        if self.skipfat16b:
            self.skipTest("Not supported by OS")

        self._test_fat16b_vfs()

    @attr('fat16b_vfs_bootblk')
    def test_fat16b_vfs_bootblk(self):
        """FAT16B vfs directory boot.blk"""
        if self.skipnativebootblk:
            self.skipTest("Not supported by Boot Block")

        if self.skipfat16b:
            self.skipTest("Not supported by OS")

        self.unTarBootBlockOrSkip("boot-[89]..-15-17.blk", True)
        self._test_fat16b_vfs()

    @attr('fatauto_vfs')
    def test_fatauto_vfs(self):
        """FAT AUTO vfs directory"""
        results = self.runDosemu("version.bat",
            config="""$_hdimage = "dXXXXs/c"\n""")

        self.assertIn(self.version, results)

    def _test_fat_img_d_writable(self, fat):
        mkfile("test_dfw.bat", """\
D:\r
mkdir test\r
echo hello > hello.txt\r
DIR\r
rem end\r
""")

        name = self.mkimage(fat, [("test_dfw.bat", "0")], bootblk=False)

        results = self.runDosemu("test_dfw.bat",
            config="""$_hdimage = "dXXXXs/c:hdtype1 %s"\n""" % name)

        self.assertRegexpMatches(results, "TEST[\t ]+<DIR>")
        self.assertRegexpMatches(results, "HELLO[\t ]+TXT[\t ]+8")

    @attr('fat12_img_d_writable')
    def test_fat12_img_d_writable(self):
        """FAT12 image file D writable"""
        self._test_fat_img_d_writable("12")

    @attr('fat16_img_d_writable')
    def test_fat16_img_d_writable(self):
        """FAT16 image file D writable"""
        self._test_fat_img_d_writable("16")

    @attr('floppy_img')
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

    @attr('floppy_vfs')
    def test_floppy_vfs(self):
        """Floppy vfs directory"""

        mkfile("autoexec.bat", """\
prompt $P$G\r
path a:\\bin;a:\\gnu;a:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        results = self.runDosemu("version.bat", config="""\
$_hdimage = ""
$_floppy_a = "dXXXXs/c:fiveinch_360"
$_bootdrive = "a"
""")

        self.assertIn(self.version, results)

    @attr('systype')
    def test_systype(self):
        """SysType"""
        self.runDosemu("version.bat",
            config="""$_hdimage = "dXXXXs/c:hdtype1"\n$_debug = "-D+d"\n""")

        # read the logfile
        systypeline = "Not found in logfile"
        with open(self.logname, "r") as f:
            for line in f:
                if "system type is" in line:
                    systypeline = line

        self.assertIn(self.systype, systypeline)

    @attr('mfs_auto_hd_vfs_directory')
    def test_mfs_auto_hd_vfs_directory(self):
        """MFS auto hd vfs directory redirection"""
        mkfile("autoexec.bat", """\
prompt $P$G\r
path a:\\bin;a:\\gnu;a:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")
        mkfile("test_mfs.bat", "lredir2\r\nrem end\r\n")

        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1"
$_floppy_a = "dXXXXs/c:fiveinch_360"
$_bootdrive = "a"
""")

# A:\>lredir2
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertRegexpMatches(results, r"C: = .*LINUX\\FS")

    @attr('mfs_lredir2_command')
    def test_mfs_lredir2_command(self):
        """MFS lredir2 command redirection"""
        mkfile("autoexec.bat", """\
prompt $P$G\r
path a:\\bin;a:\\gnu;a:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")
        mkfile("test_mfs.bat", """\
lredir2 E: LINUX\\FS\\bin\r
lredir2\r
rem end\r
""")
        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = ""
$_floppy_a = "dXXXXs/c:fiveinch_360"
$_bootdrive = "a"
""")

# A:\>lredir2
# Current Drive Redirections:
# C: = LINUX\FS\dosemu2.git\test-imagedir\dXXXXs\c\ attrib = READ/WRITE
# E: = LINUX\FS\bin\        attrib = READ/WRITE

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertRegexpMatches(results, r"E: = .*LINUX\\FS\\bin")

    def _test_mfs_file_find(self, nametype):
        if nametype is "LFN":
            testnames = [
                "verylongfilename.txt",
                "space embedded filename.txt",
                "shrtname.longextension"
            ]
            disablelfn = ""
        elif nametype is "SFN":
            testnames = [
                "SHRTNAME.TXT",
                "HELLO~1F.JAV",  # fake a mangled name
                "1.C"
            ]
            disablelfn = "set LFN=n"
        else:
            self.fail("Incorrect argument")

        testdir = "test-imagedir/dXXXXs/d"

        mkfile("autoexec.bat", """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

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
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        for name in testnames:
            self.assertIn(name, results)

    @attr('mfs_lfn_file_find')
    def test_mfs_lfn_file_find(self):
        """MFS LFN file find"""
        self._test_mfs_file_find("LFN")

    @attr('mfs_sfn_file_find')
    def test_mfs_sfn_file_find(self):
        """MFS SFN file find"""
        self._test_mfs_file_find("SFN")

    def _test_mfs_file_read(self, nametype):
        if nametype is "LFN":
            testname = "verylongname.txt"
            disablelfn = ""
        elif nametype is "SFN":
            testname = "shrtname.txt"
            disablelfn = "set LFN=n"
        else:
            self.fail("Incorrect argument")

        testdata = mkstring(128)
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("autoexec.bat", """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")
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
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        self.assertIn(testdata, results)

    @attr('mfs_lfn_file_read')
    def test_mfs_lfn_file_read(self):
        """MFS LFN file read"""
        self._test_mfs_file_read("LFN")

    @attr('mfs_sfn_file_read')
    def test_mfs_sfn_file_read(self):
        """MFS SFN file read"""
        self._test_mfs_file_read("SFN")

    def _test_mfs_file_write(self, nametype, operation):
        if nametype is "LFN":
            ename = "mfslfn"
            testname = "verylongname.txt"
            disablelfn = ""
        elif nametype is "SFN":
            ename = "mfssfn"
            testname = "shrtname.txt"
            disablelfn = "set LFN=n"
        else:
            self.fail("Incorrect argument")

        if operation is "create":
            ename += "wc"
            testprfx = ""
            openflags = "O_WRONLY | O_CREAT | O_TEXT"
            mode = ", 0222"
        elif operation is "createreadonly":
            ename += "wk"
            testprfx = ""
            openflags = "O_WRONLY | O_CREAT | O_TEXT"
            mode = ", 0444"
        elif operation is "truncate":
            ename += "wt"
            testprfx = "dummy data"
            openflags = "O_WRONLY | O_CREAT | O_TRUNC | O_TEXT"
            mode = ", 0222"
        elif operation is "append":
            ename += "wa"
            testprfx = "Original Data"
            openflags = "O_RDWR | O_APPEND | O_TEXT"
            mode = ""
        else:
            self.fail("Incorrect argument")

        testdata = mkstring(64) # need to be fairly short to pass as arg
        testdir = "test-imagedir/dXXXXs/d"

        mkfile("autoexec.bat", """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

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
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
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

    @attr('mfs_lfn_file_create')
    def test_mfs_lfn_file_create(self):
        """MFS LFN file create"""
        self._test_mfs_file_write("LFN", "create")

    @attr('mfs_sfn_file_create')
    def test_mfs_sfn_file_create(self):
        """MFS SFN file create"""
        self._test_mfs_file_write("SFN", "create")

    @attr('mfs_lfn_file_create_readonly')
    def test_mfs_lfn_file_create_readonly(self):
        """MFS LFN file create readonly"""
        self._test_mfs_file_write("LFN", "createreadonly")

    @attr('mfs_sfn_file_create_readonly')
    def test_mfs_sfn_file_create_readonly(self):
        """MFS SFN file create readonly"""
        self._test_mfs_file_write("SFN", "createreadonly")

    @attr('mfs_lfn_file_truncate')
    def test_mfs_lfn_file_truncate(self):
        """MFS LFN file truncate"""
        self._test_mfs_file_write("LFN", "truncate")

    @attr('mfs_sfn_file_truncate')
    def test_mfs_sfn_file_truncate(self):
        """MFS SFN file truncate"""
        self._test_mfs_file_write("SFN", "truncate")

    @attr('mfs_lfn_file_append')
    def test_mfs_lfn_file_append(self):
        """MFS LFN file append"""
        self._test_mfs_file_write("LFN", "append")

    @attr('mfs_sfn_file_append')
    def test_mfs_sfn_file_append(self):
        """MFS SFN file append"""
        self._test_mfs_file_write("SFN", "append")

    def _test_mfs_directory_common(self, nametype, operation):
        if nametype is "LFN":
            ename = "mfslfn"
            testname = "test very long directory"
        elif nametype is "SFN":
            ename = "mfssfn"
            testname = "testdir"
        else:
            self.fail("Incorrect argument")

        testdir = "test-imagedir/dXXXXs/d"

        cwdnum = "0x0"

        if operation == "Create":
            ename += "dc"
            if nametype is "SFN":
                intnum = "0x3900"  # create
            else:
                intnum = "0x7139"
            makedirs(testdir)
        elif operation in ["Delete", "DeleteNotEmpty"]:
            ename += "dd"
            if nametype is "SFN":
                intnum = "0x3a00"  # delete
            else:
                intnum = "0x713a"
            makedirs(join(testdir, testname))
            if operation == "DeleteNotEmpty":
                mkfile("DirNotEm.pty", """hello\r\n""", join(testdir, testname))
        elif operation == "Chdir":
            ename += "dh"
            if nametype is "SFN":
                intnum = "0x3b00"  # chdir
                cwdnum = "0x4700"  # getcwd
            else:
                intnum = "0x713b"
                cwdnum = "0x7147"
            makedirs(join(testdir, testname))
        else:
            self.fail("Incorrect argument")

        mkfile("autoexec.bat", """\
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
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
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
            if nametype is "SFN":
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
        if nametype is "LFN":
            ename = "mfslfngd"
            testname = PRGFIL_LFN
        elif nametype is "SFN":
            ename = "mfssfngd"
            testname = PRGFIL_SFN
        else:
            self.fail("Incorrect argument")

        testdir = "test-imagedir/dXXXXs/d"

        if nametype is "SFN":
            cwdnum = "0x4700"  # getcwd
        else:
            cwdnum = "0x7147"

        makedirs(join(testdir, PRGFIL_LFN))

        mkfile("autoexec.bat", """\
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
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

            if nametype is "SFN":
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
        if nametype is "LFN0":
            ename = "mfslfntn"
            intnum = "0x7160"
            qtype = "0"
        elif nametype is "LFN1":
            ename = "mfslfntn"
            intnum = "0x7160"
            qtype = "1"
        elif nametype is "LFN2":
            ename = "mfslfntn"
            intnum = "0x7160"
            qtype = "2"
        elif nametype is "SFN":
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

        mkfile("autoexec.bat", """\
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
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
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

    def _test_mfs_fcb_rename_common(self, testname):
        testdir = "test-imagedir/dXXXXs/d"

        makedirs(testdir)

        if testname is "simple":
            ename = "mfsfcbr1"
            fn1 = "testa"
            fe1 = "bat"
            fn2 = "testb"
            fe2 = "bal"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
        elif testname is "source_missing":
            ename = "mfsfcbr2"
            fn1 = "testa"
            fe1 = "bat"
            fn2 = "testb"
            fe2 = "bal"
        elif testname is "target_exists":
            ename = "mfsfcbr3"
            fn1 = "testa"
            fe1 = "bat"
            fn2 = "testb"
            fe2 = "bal"
            mkfile(fn1 + "." + fe1, """hello\r\n""", testdir)
            mkfile(fn2 + "." + fe2, """hello\r\n""", testdir)
        elif testname is "wild_one":
            ename = "mfsfcbr4"
            fn1 = "*"
            fe1 = "in"
            fn2 = "*"
            fe2 = "out"
            for f in ["one.in", "two.in", "three.in", "four.in", "five.in",
                      "none.ctl"]:
                mkfile(f, """hello\r\n""", testdir)
        elif testname is "wild_two":
            ename = "mfsfcbr5"
            fn1 = "a*"
            fe1 = "*"
            fn2 = "b*"
            fe2 = "out"
            for f in ["aone.in", "atwo.in", "athree.in", "afour.in",
                      "afive.in", "xnone.ctl"]:
                mkfile(f, """hello\r\n""", testdir)
        elif testname is "wild_three":
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
        elif testname is "wild_four":
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

        mkfile("autoexec.bat", """\
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

        results = self.runDosemu("test_mfs.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
""")

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        if testname is "simple":
            self.assertIn("Rename Operation Success", results);
            self.assertTrue(exists(join(testdir, fn2 + "." + fe2)), "File not renamed")
        elif testname is "source_missing":
            self.assertIn("Rename Operation Failed", results);
        elif testname is "target_exists":
            self.assertIn("Rename Operation Failed", results);
        elif testname is "wild_one":
            self.assertIn("Rename Operation Success", results);
            self.assertTrue(exists(join(testdir, "one.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "two.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "three.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "four.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "five.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "none.ctl")), "File incorrectly renamed")
        elif testname is "wild_two":
            self.assertIn("Rename Operation Success", results);
            self.assertTrue(exists(join(testdir, "bone.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "btwo.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "bthree.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "bfour.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "bfive.out")), "File not renamed")
            self.assertTrue(exists(join(testdir, "xnone.ctl")), "File incorrectly renamed")
        elif testname is "wild_three":
            self.assertIn("Rename Operation Success", results);
            self.assertTrue(exists(join(testdir, "abc601.txt")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc602.txt")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc603.txt")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc604.txt")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc605.txt")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc610.txt")), "File not renamed")
            self.assertTrue(exists(join(testdir, "xbc007.txt")), "File incorrectly renamed")
        elif testname is "wild_four":
            self.assertIn("Rename Operation Success", results);
            self.assertTrue(exists(join(testdir, "abc001.ht")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc002.ht")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc003.ht")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc004.ht")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc005.ht")), "File not renamed")
            self.assertTrue(exists(join(testdir, "abc010.ht")), "File not renamed")
            self.assertTrue(exists(join(testdir, "xbc007.htm")), "File incorrectly renamed")

    @attr('mfs_fcb_rename_simple')
    def test_mfs_fcb_rename_simple(self):
        """MFS FCB file rename simple"""
        self._test_mfs_fcb_rename_common("simple")

    @attr('mfs_fcb_rename_source_missing')
    def test_mfs_fcb_rename_source_missing(self):
        """MFS FCB file rename source missing"""
        self._test_mfs_fcb_rename_common("source_missing")

    @attr('mfs_fcb_rename_target_exists')
    def test_mfs_fcb_rename_target_exists(self):
        """MFS FCB file rename target exists"""
        self._test_mfs_fcb_rename_common("target_exists")

    @attr('mfs_fcb_rename_wild_1')
    def test_mfs_fcb_rename_wild_1(self):
        """MFS FCB file rename wildcard one"""
        self._test_mfs_fcb_rename_common("wild_one")

    @attr('mfs_fcb_rename_wild_2')
    def test_mfs_fcb_rename_wild_2(self):
        """MFS FCB file rename wildcard two"""
        self._test_mfs_fcb_rename_common("wild_two")

    @attr('mfs_fcb_rename_wild_3')
    def test_mfs_fcb_rename_wild_3(self):
        """MFS FCB file rename wildcard three"""
        self._test_mfs_fcb_rename_common("wild_three")

    @attr('mfs_fcb_rename_wild_4')
    def test_mfs_fcb_rename_wild_4(self):
        """MFS FCB file rename wildcard four"""
        self._test_mfs_fcb_rename_common("wild_four")

    @attr('three_drives_vfs')
    def test_three_drives_vfs(self):
        """Three vfs directories configured"""
        # C exists as part of standard test
        makedirs("test-imagedir/dXXXXs/d")
        makedirs("test-imagedir/dXXXXs/e")

        results = self.runDosemu("version.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 dXXXXs/e:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
""")

        self.assertIn(self.version, results) # Just to check we booted

    def _test_lfn_volume_info(self, fstype):
        if fstype is "MFS":
            drive = "C:\\"
        elif fstype is "FAT":
            drive = "D:\\"
        else:
            self.fail("Incorrect argument")

        mkfile("autoexec.bat", """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

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
    return 3;
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

        if fstype is "MFS":
            hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1"
        elif fstype is "FAT":
            hdimage = "dXXXXs/c:hdtype1 %s" % name

        results = self.runDosemu("test_mfs.bat", config = """\
$_hdimage = "%s"
$_floppy_a = ""
$_bootdrive = "c"
""" % hdimage)

        with open(self.xptname, "r") as f:
            xpt = f.read()
            if "EMUFS revectoring only" in xpt:
                self.skipTest("MFS unsupported")

        if fstype is "MFS":
            self.assertIn("FSTYPE(MFS)", results)
        elif fstype is "FAT":
            self.assertIn("FILESYS_UNKNOWN(", results)

    @attr('lfn_volume_info_mfs')
    def test_lfn_volume_info_mfs(self):
        """LFN volume info on MFS"""
        self._test_lfn_volume_info("MFS")

    @attr('lfn_volume_info_fat_img')
    def test_lfn_volume_info_fat_img(self):
        """LFN volume info on FAT(img)"""
        self._test_lfn_volume_info("FAT")


    @attr('fat32_disk_info')
    def test_fat32_disk_info(self):
        """FAT32 disk info"""

        path = "C:\\"

        mkfile("autoexec.bat", """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

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
$_hdimage = "dXXXXs/c:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
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


    @attr('int21_disk_info')
    def test_int21_disk_info(self):
        """INT21 disk info"""

        path = "C:\\"

        mkfile("autoexec.bat", """\
prompt $P$G\r
path c:\\bin;c:\\gnu;c:\\dosemu\r
unix -s DOSEMU_VERSION\r
unix -e\r
""")

        mkfile("test_mfs.bat", """\
c:\\int21dif %s\r
rem end\r
""" % path)

        # compile sources
        mkexe("int21dif", r"""\
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define MAXPATH 128

int main(int argc, char *argv[]) {
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
$_hdimage = "dXXXXs/c:hdtype1"
$_floppy_a = ""
$_bootdrive = "c"
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

# This function is clamped at 2GiB
        if lfs_total > 2147450880:
            lfs_total = 2147450880
        if lfs_avail > 2147450880:
            lfs_avail = 2147450880

# see if we are within 5% of the values obtained from Linux
        msg = "total dos %d, linux %d" % (dfs_total, lfs_total)
        self.assertLessEqual(dfs_total, lfs_total * 1.05, msg);
        self.assertGreaterEqual(dfs_total, lfs_total * 0.95, msg)

        msg = "avail dos %d, linux %d" % (dfs_avail, lfs_avail)
        self.assertLessEqual(dfs_avail, lfs_avail * 1.05, msg)
        self.assertGreaterEqual(dfs_avail, lfs_avail * 0.95, msg)


class MSDOS310TestCase(BootTestCase, unittest.TestCase):
    # Badged 'Olivetti'

    @classmethod
    def setUpClass(cls):
        super(MSDOS310TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 3.10"
        cls.tarfile = "MS-DOS-3.10.tar"
        cls.files = [
            ("io.sys", "b49e9c4ae92e65cf710178258a4419285c1d5855"),
            ("msdos.sys", "7f93eba7140f6266476585be203bfd3c7651e46c"),
            ("command.com", "9b1131bbbfc871920240e8db0159c1403fd7cec8"),
        ]
        cls.skipfat16b = True
        # OS doesn't support PARTITIONS - skip on hdisk with native boot blk
        cls.skipnativebootblk = True
        cls.systype = SYSTYPE_MSDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "38135ac40e51eec9f384725909fe67a81a23cdc3"),
            ("boot-615-4-17.blk", "526dc9e1d568e14a1569fa3776fddca3842e4e2d"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "5bfabd8c5d2b985470225f57a31d4900e72dbb98"),
        ]


class MSDOS320TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS320TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 3.20"
        cls.tarfile = "MS-DOS-3.20.tar"
        cls.files = [
            ("io.sys", "c843395b2b93220f3f9832b9bd30f3d114973df5"),
            ("msdos.sys", "5ad5f136f0de7b0b257d05c2e4c10c3d247664c4"),
            ("command.com", "4bbc8515cb9704c371de75990f4a88c8f400c0ec"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_MSDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "d0dab5b80c076ad5c440a900fd6a386463a6b9aa"),
            ("boot-615-4-17.blk", "3e85592b3f076381e1337626270c2089443cc0c2"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "ef770b818882799ef9d008b76e8a70a25f146b27"),
        ]


class MSDOS321TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS321TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 3.21"
        cls.tarfile = "MS-DOS-3.21.tar"
        cls.files = [
            ("io.sys", "b03fc267d4d7e3f207a46455344af1a2a0f46ecc"),
            ("msdos.sys", "a0d2e0ebd50e56c606495086dafced09b8f3479c"),
            ("command.com", "cf5f28bd7f062cc6e58ea982f7aedb17e171f68e"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_MSDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "d0dab5b80c076ad5c440a900fd6a386463a6b9aa"),
            ("boot-615-4-17.blk", "3e85592b3f076381e1337626270c2089443cc0c2"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "3cddbcadb501bc5ecbebf67a3251ba350e7abe77"),
        ]


class MSDOS330TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS330TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 3.30"
        cls.tarfile = "MS-DOS-3.30.tar"
        cls.files = [
            ("io.sys", "efcd4993279f94ccc9fd2adcf0b8a273e7978188"),
            ("msdos.sys", "e741511e9767cdc35cbdc656a31deaf6dfb99ee4"),
            ("command.com", "6ddc8d2facfcb0f846331c5dbc8275f1961db30a"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_MSDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "149ec13a96ec4b4c393cd2aba2803ed09d2bddc6"),
            ("boot-615-4-17.blk", "ab0dce151fd60da2edfca645885eb05438cd6b1a"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "4dbcd91b604d2b03f2b775489ebf296acb7109cd"),
        ]


class MSDOS330NecTestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS330NecTestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 3.30"
        cls.tarfile = "MS-DOS-3.30-Nec.tar"
        cls.files = [
            ("io.sys", "d99339764380a854372d9f2a09489662bfb13243"),
            ("msdos.sys", "86d863416115c9c9cbfc383d1f2aa3873c7486dd"),
            ("command.com", "6ddc8d2facfcb0f846331c5dbc8275f1961db30a"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_MSDOS_NEC
        cls.bootblocks = [
            ("boot-306-4-17.blk", "8f0d005dbd5fdd0f76b78d3d67c06b05d58936e7"),
            ("boot-615-4-17.blk", "8879fc8ff25320c81e94bdb2a2d640bd8d0b6000"),
            # OS doesn't support FAT16B, only a non-standard large filesystem
        ]
        cls.images = [
            ("boot-floppy.img", "a31ecb4e94dd5f6c46e26eaf9d4a0646c0a45169"),
        ]


class MSDOS331TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS331TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 3.31"
        cls.tarfile = "MS-DOS-3.31.tar"
        cls.files = [
            ("io.sys", "98e6d276b04a2f454e62245d7a8380c269e47263"),
            ("msdos.sys", "9793d1c5262238664f592169b39bd4156a6149f6"),
            ("command.com", "749fc519f640958af6e1204c8d39789b0035f8f8"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_MSDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "6c1d83f0996dd9dfc116867373eb52fcb73ea21e"),
            ("boot-615-4-17.blk", "eb7254f58f296dfc36ece3c8027092ac7b11e762"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "c29e6b8f0f3693aa46781ae8c61fa103d5ca0994"),
        ]


class MSDOS401TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS401TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 4.01"
        cls.tarfile = "MS-DOS-4.01.tar"
        cls.files = [
            ("io.sys", "b1aed88d20b8eade102f08f96dfb2e82a3f98051"),
            ("msdos.sys", "44fab6ab46fc2ab9af6e40cd770596591d1e2830"),
            ("command.com", "c13b29f52aeb6e4c73a43834a0899eea26d85253"),
        ]
        cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
        cls.bootblocks = [
            ("boot-306-4-17.blk", "de2354f11c8852d6bd41c8ca85add1c5e9d14410"),
            ("boot-615-4-17.blk", "5fc94eb2fcc665fcd43c4e668106a58386dd44aa"),
            ("boot-900-15-17.blk", "e445bb31c7c8e847121e67447ebe2c2cf8651db6"),
        ]
        cls.images = [
            ("boot-floppy.img", "1fe3ef855486ef18d157445084bf1e17b5dc2105"),
        ]


class MSDOS500TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS500TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 5.00"
        cls.tarfile = "MS-DOS-5.00.tar"
        cls.files = [
            ("io.sys", "132761e1b5191bf1c940c1f9a4d44c10f51829ec"),
            ("msdos.sys", "ad3e000ac6b21c5f734ef9981c3409e4b1ad88f8"),
            ("command.com", "a94b3f877bc933c21ea696ea032c53874df90807"),
        ]
        cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
        cls.bootblocks = [
                ("boot-306-4-17.blk", "7cb690c7976df85af7e47f4ca92ae519b2e2f72f"),
                ("boot-615-4-17.blk", "395689aa1257182a11d2e4bfc52789dd56607de7"),
                ("boot-900-15-17.blk", "57707d505b79229b8327726ad88d278cf0720b95"),
        ]
        cls.images = [
            ("boot-floppy.img", "18c89a34ea463be7c4fc29a1ce6857ad1dc81e5b"),
        ]


class MSDOS600TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS600TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 6.00"
        cls.tarfile = "MS-DOS-6.00.tar"
        cls.files = [
            ("io.sys", "1bc2f8b1366271450b039bd463a5da40de5be1b3"),
            ("msdos.sys", "6a234c5f57d593888180c34bdf715ffe1d9c250f"),
            ("command.com", "5cbf5e896b4d93da61a18b9805895e522ec3eec2"),
        ]
        cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
        cls.bootblocks = [
                ("boot-306-4-17.blk", "2ebe7e3f43fb1d8ef27c3c5cf4861c52b7bf74dc"),
                ("boot-615-4-17.blk", "dc85c146b26f8149f4e43bab213ee5d3181b62c1"),
                ("boot-900-15-17.blk", "9dfd22ca64179105eb85bb1e81fdcae6cac77cd0"),
        ]
        cls.images = [
            ("boot-floppy.img", "a6caaa85c1ea5f787891e526a219a69c43e5d131"),
        ]


class MSDOS620TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS620TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 6.20"
        cls.tarfile = "MS-DOS-6.20.tar"
        cls.files = [
            ("io.sys", "93f35190678c0dce0071067d9985d1697b3f6121"),
            ("msdos.sys", "2b1c0817d4b38f5013a2cc94d78ae075af7ff1f8"),
            ("command.com", "5b28d8db629940413d8c424b6d767df3c62fda61"),
        ]
        cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
        cls.bootblocks = [
            ("boot-306-4-17.blk", "a58fb7cc5fa306515cb87765c176dca03e86c304"),
            ("boot-615-4-17.blk", "323070d285bff0d9c37d8401d2c33d2f62e502d8"),
            ("boot-900-15-17.blk", "80a372183a55b793cb149dee814ef7cf3b6ebe00"),
        ]
        cls.images = [
            ("boot-floppy.img", "6e182c73a4311275d39dc447841165ef4f5a4fb1"),
        ]


class MSDOS621TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS621TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 6.21"
        cls.tarfile = "MS-DOS-6.21.tar"
        cls.files = [
            ("io.sys", "93f35190678c0dce0071067d9985d1697b3f6121"),
            ("msdos.sys", "2b1c0817d4b38f5013a2cc94d78ae075af7ff1f8"),
            ("command.com", "6fdec39c8fcdb0c3dc15c2fd5838ed8bc274e44e"),
        ]
        cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
        cls.bootblocks = [
            ("boot-306-4-17.blk", "db490e670de7247ace323b3a2ee38c52a2a078fb"),
            ("boot-615-4-17.blk", "4cadf8024d43330c58703800e3317ab4c77ad2a1"),
            ("boot-900-15-17.blk", "d04e1508ae8f9d22d1d80dc8213c70259e6db059"),
        ]
        cls.images = [
            ("boot-floppy.img", "35b74d5717b95ba8f0c1161fe0f21b1097c33591"),
        ]


class MSDOS622TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(MSDOS622TestCase, cls).setUpClass()
        cls.version = "MS-DOS Version 6.22"
        cls.tarfile = "MS-DOS-6.22.tar"
        cls.files = [
            ("io.sys", "d697961ca6edaf9a1aafe8b7eb949597506f7f95"),
            ("msdos.sys", "d6a5f54006e69c4407e56677cd77b82395acb60a"),
            ("command.com", "c2179d2abfa241edd388ab875cfabbac89fec44d"),
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


class MSDOS700TestCase(BootTestCase, unittest.TestCase):
    # badged Win95 RTM at winworldpc.com

    @classmethod
    def setUpClass(cls):
        super(MSDOS700TestCase, cls).setUpClass()
        cls.version = "Windows 95. [Version 4.00.950]"
        cls.tarfile = "MS-DOS-7.00.tar"
        cls.files = [
            ("io.sys", "22924f93dd0f9ea6a4624ccdd1bbcdf5eb43a308"),
            ("msdos.sys", "f5d01c68d518f4b8b2482d3815af8bb88003831d"),
            ("command.com", "67696207c3963a0dc9afab8cf37dbdb966c1f663"),
        ]
        cls.systype = SYSTYPE_MSDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "8c016e339ca6b8126fd2026ed3a7eeeb6cbb8903"),
            ("boot-615-4-17.blk", "b6fdddbfb37442a2762d5897de1aa7d7a694286a"),
            ("boot-900-15-17.blk", "8c1243481112f320f2a5f557f30db11174fe7e3d"),
        ]
        cls.images = [
            ("boot-floppy.img", ""),
        ]


class MSDOS710TestCase(BootTestCase, unittest.TestCase):
    # badged CDU (Chinese DOS Union) at winworldpc.com

    @classmethod
    def setUpClass(cls):
        super(MSDOS710TestCase, cls).setUpClass()
        cls.version = "MS-DOS 7.1 [Version 7.10.1999]"
        cls.tarfile = "MS-DOS-7.10.tar"
        cls.files = [
            ("io.sys", "8c586b1bf38fc2042f2383ca873283a466be2f44"),
            ("msdos.sys", "cd1e6103ce9cdebbc7a5611df13ff4fbd5e2159c"),
            ("command.com", "f6547d81e625a784633c059e536e90ee45532202"),
        ]
        cls.systype = SYSTYPE_MSDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "0f520de6e2a33ef8fd336b2844957689fc1060e9"),
            ("boot-615-4-17.blk", "5e49a8ee7747191d87a2214cc0281736262687b9"),
            ("boot-900-15-17.blk", "2c29d06909c7d5ca46a3ca26ddde9287a11ef315"),
        ]
        cls.images = [
            ("boot-floppy.img", ""),
        ]


class PCDOS300TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS300TestCase, cls).setUpClass()
        cls.version = "IBM Personal Computer DOS Version  3.00"
        cls.tarfile = "PC-DOS-3.00.tar"
        cls.files = [
            ("ibmbio.com", "0d89246fe915db4278778033c20a792b176cb7a1"),
            ("ibmdos.com", "cb4865a13b71a2a418085992574a7a709f102ba4"),
            ("command.com", "c1999a869d2bb7bdac9f63290d12e1bc33464f51"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "994bbbb88a9467bb7572ef82c562441f9196a125"),
            ("boot-615-4-17.blk", "482298b85db3482aae29fdfc0b01ac0474654112"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "94f23342e5b38149e52320fd693ce09cf13f7c69"),
        ]


class PCDOS300CompaqTestCase(BootTestCase, unittest.TestCase):
    # Badged 'Microsoft MS-DOS Compaq OEM'

    @classmethod
    def setUpClass(cls):
        super(PCDOS300CompaqTestCase, cls).setUpClass()
        cls.version = "COMPAQ Personal Computer MS-DOS Version  3.00"
        cls.tarfile = "PC-DOS-3.00-Compaq.tar"
        cls.files = [
            ("ibmbio.com", "04a388fee319990312e529c251d28307bb22154a"),
            ("ibmdos.com", "978415677aeea953f4dd463f0f8df35a7ee1365e"),
            ("command.com", "3c76c3ed861bf08b99b0295d0f885fce7ad2c6ac"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "530b041996caf3ccce0ff00cbced8f3f0f05f101"),
            ("boot-615-4-17.blk", "8787e3c97f398a1b6dc36af599c39ab5f1859e65"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "06fa044ec9cb50696f0eeae8fec4869ff8b0b7a4"),
        ]


class PCDOS3108503TestCase(BootTestCase, unittest.TestCase):
    # Badged at Winworld as 03-07-1985

    @classmethod
    def setUpClass(cls):
        super(PCDOS3108503TestCase, cls).setUpClass()
        cls.version = "IBM Personal Computer DOS Version  3.10"
        cls.tarfile = "PC-DOS-3.10-850307.tar"
        cls.files = [
            ("ibmbio.com", "986ae2e3cd1be266de692f2b20c45c34e0da27fe"),
            ("ibmdos.com", "dbfa511f19d218ae33bdae565aa3fa4d9cc3a8bf"),
            ("command.com", "d67991758da396f518fb1f645f89c665788353dd"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "8092bfae3daa78f201055c228e6a133a2cf9b39b"),
            ("boot-615-4-17.blk", "974e43338404de030441802fc6ec369a60e4f35b"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "8d22e57339e2a3a02ba72e7d8b4fe8504eb68043"),
        ]


class PCDOS3108504TestCase(BootTestCase, unittest.TestCase):
    # Badged at Winworld as 04-22-1985

    @classmethod
    def setUpClass(cls):
        super(PCDOS3108504TestCase, cls).setUpClass()
        cls.version = "IBM Personal Computer DOS Version  3.10"
        cls.tarfile = "PC-DOS-3.10-850422.tar"
        cls.files = [
            ("ibmbio.com", "986ae2e3cd1be266de692f2b20c45c34e0da27fe"),
            ("ibmdos.com", "d6f34ef243e70db89cbfd3b1228cbac38fb973f0"),
            ("command.com", "d67991758da396f518fb1f645f89c665788353dd"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "8092bfae3daa78f201055c228e6a133a2cf9b39b"),
            ("boot-615-4-17.blk", "974e43338404de030441802fc6ec369a60e4f35b"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "bc2c2a6aeb89b6ebf876ccb5cd52ac514da51c70"),
        ]


class PCDOS310CompaqTestCase(BootTestCase, unittest.TestCase):
    # Badged 'Microsoft MS-DOS Compaq OEM'

    @classmethod
    def setUpClass(cls):
        super(PCDOS310CompaqTestCase, cls).setUpClass()
        cls.version = "COMPAQ Personal Computer DOS Version  3.10"
        cls.tarfile = "PC-DOS-3.10-Compaq.tar"
        cls.files = [
            ("ibmbio.com", "23fb6d0ea43f0c35c1f05838a8dfd1b901fa1d29"),
            ("ibmdos.com", "dbfa511f19d218ae33bdae565aa3fa4d9cc3a8bf"),
            ("command.com", "8676e87264e669b763cabda5b2aa0a263e36251f"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "c869f605dbe29b254b36193c392206f4f67b1b64"),
            ("boot-615-4-17.blk", "1bf14c00edf3326d994dbecf154450940ad2b0b0"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "6da60da507e620b64bb8c69809d60fdb177a4b36"),
        ]


class PCDOS3208512TestCase(BootTestCase, unittest.TestCase):
    # badged 12-30-1985 at winworldpc

    @classmethod
    def setUpClass(cls):
        super(PCDOS3208512TestCase, cls).setUpClass()
        cls.version = "IBM Personal Computer DOS Version  3.20"
        cls.tarfile = "PC-DOS-3.20-851230.tar"
        cls.files = [
            ("ibmbio.com", "716d5ac33c2472aaf86adadeb9aa5701fe4f6dd9"),
            ("ibmdos.com", "3b9833823deb08cda1365ec3100ab6c738a78010"),
            ("command.com", "eedbe89ee2163d778e1d12053f3a511e1d235af6"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "b11bef67d6265592fae12e4052365f04cf7d0a6f"),
            ("boot-615-4-17.blk", "423291f6f0e2ffe96845f640d65a505692155132"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "822038b85eabba41ed01751321a59ee7200c24af"),
        ]


class PCDOS3208602TestCase(BootTestCase, unittest.TestCase):
    # badged 02-21-1986 at winworldpc

    @classmethod
    def setUpClass(cls):
        super(PCDOS3208602TestCase, cls).setUpClass()
        cls.version = "IBM Personal Computer DOS Version  3.20"
        cls.tarfile = "PC-DOS-3.20-860221.tar"
        cls.files = [
            ("ibmbio.com", "716d5ac33c2472aaf86adadeb9aa5701fe4f6dd9"),
            ("ibmdos.com", "3b9833823deb08cda1365ec3100ab6c738a78010"),
            ("command.com", "eedbe89ee2163d778e1d12053f3a511e1d235af6"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "b11bef67d6265592fae12e4052365f04cf7d0a6f"),
            ("boot-615-4-17.blk", "423291f6f0e2ffe96845f640d65a505692155132"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "653801fb05e4abe788b6962ac69dea3983583dfb"),
        ]


class PCDOS330TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS330TestCase, cls).setUpClass()
        cls.version = "IBM Personal Computer DOS Version  3.30"
        cls.tarfile = "PC-DOS-3.30.tar"
        cls.files = [
            ("ibmbio.com", "7e03fd80e52bba4d30ff0aa1878af34100be9319"),
            ("ibmdos.com", "0071cb27083f77f92942924dbd2b5d5c6bb3766e"),
            ("command.com", "ab762d2d383bd75f24a83799a043e94df9c63f16"),
        ]
        cls.skipfat16b = True
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "98299b599dc04f8ebe6eb612092ff8246fc80906"),
            ("boot-615-4-17.blk", "1e38ae6a70bfa4a5c4b2956abee90c0aaf77e9dd"),
            # OS doesn't support FAT16B
        ]
        cls.images = [
            ("boot-floppy.img", "7100f1430b432518af7c888b25fded8505d5fdf7"),
        ]


class PCDOS331CompaqTestCase(BootTestCase, unittest.TestCase):
    # Badged 'Microsoft MS-DOS Compaq OEM'

    @classmethod
    def setUpClass(cls):
        super(PCDOS331CompaqTestCase, cls).setUpClass()
        cls.version = "COMPAQ Personal Computer DOS Version  3.31"
        cls.tarfile = "PC-DOS-3.31-Compaq.tar"
        cls.files = [
            ("ibmbio.com", "4e100141fc5650672711abcd1aab8e53ba21f578"),
            ("ibmdos.com", "8eb3a7f96264e99727bb38044f17b216c0e74274"),
            ("command.com", "a546bf62745310013983d2004b5277c60c1c26a7"),
        ]
        cls.systype = SYSTYPE_PCDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "ef914044fa919941151fa0044ca4994bd84d54e7"),
            ("boot-615-4-17.blk", "ee7d5325df9389e6f3b5304b62ca615f3c189eea"),
            ("boot-900-15-17.blk", "32423c02c6ae1deb98676a8f92b5693eec8a086a"),
        ]
        cls.images = [
            ("boot-floppy.img", "53d2931824d9b6370f9b92530cd47e0a38958cdd"),
        ]


class PCDOS400TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS400TestCase, cls).setUpClass()
        cls.version = "IBM DOS Version 4.00"
        cls.tarfile = "PC-DOS-4.00.tar"
        cls.files = [
            ("ibmbio.com", "847801a0ea362f132ad1e59e435d136e16077dbc"),
            ("ibmdos.com", "ecb8e797b04f3e753c37eb3c95b132e0124acf97"),
            ("command.com", "371bf6071cbc3ad140103bfb83678f68efe9af0d"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "ee245add7aa885c72c47bccb2eb38a4b9f1e44ba"),
            ("boot-615-4-17.blk", "58d67bc01a67a43d2f7fd30911ec3bda2d5ba260"),
            ("boot-900-15-17.blk", "d0ed83f6ec9a4bbbb80dbae61af59c67fabcd5c7"),
        ]
        cls.images = [
            ("boot-floppy.img", "43626583bc0a74a3b3a47d5e68f131ee71a03c39"),
        ]


class PCDOS401TestCase(BootTestCase, unittest.TestCase):
    # Note: PC-DOS 4.01 is a bugfix release that identifies itself as 4.00

    @classmethod
    def setUpClass(cls):
        super(PCDOS401TestCase, cls).setUpClass()
        cls.version = "IBM DOS Version 4.00"
        cls.tarfile = "PC-DOS-4.01.tar"
        cls.files = [
            ("ibmbio.com", "925176cc22db355cf1df542c23be104f6ea50ca9"),
            ("ibmdos.com", "b32b877532d1ea4fe4c53ab18d69a94fb7d7164e"),
            ("command.com", "371bf6071cbc3ad140103bfb83678f68efe9af0d"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "cb96d6033f12ea4c6efb39ef9cc90e45cf7d3b6a"),
            ("boot-615-4-17.blk", "fc2c232c48aeb62bd93c14721dba05ddbe5bf480"),
            ("boot-900-15-17.blk", "4bed38ee41af469dc9dc323ea7fc765e4b3b8fbc"),
        ]
        cls.images = [
            ("boot-floppy.img", "b7a2d77ab7dcceb2e894919b38543ce0566a8a36"),
        ]


class PCDOS500TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS500TestCase, cls).setUpClass()
        cls.version = "IBM DOS Version 5.00"
        cls.tarfile = "PC-DOS-5.00.tar"
        cls.files = [
            ("ibmbio.com", "ee75bf1a4acf6563784344c093efe91cd7a2a0d4"),
            ("ibmdos.com", "41d757a644b57cfca111a90a13358aa1d0ac34ba"),
            ("command.com", "ae926764c0fe8a8c7c1013381625200324d55eb7"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "afb4d6e0f336fca22073ae2bf0ec195aadf8f187"),
            ("boot-615-4-17.blk", "0fcdb902a20d9e3d70c68400bed186603fce9776"),
            ("boot-900-15-17.blk", "758d4dd6cb70a553af4aab089eff35bd6421b7aa"),
        ]
        cls.images = [
            ("boot-floppy.img", "8eb43b2885fd8ce27d0284805e57cd29b5178be3"),
        ]


class PCDOS502TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS502TestCase, cls).setUpClass()
        cls.version = "IBM DOS Version 5.02"
        cls.tarfile = "PC-DOS-5.02.tar"
        cls.files = [
            ("ibmbio.com", "065c921763640658f1f3d462156e62d8483f4994"),
            ("ibmdos.com", "9901530af11e7e302021c006285f1d6ee6a7671c"),
            ("command.com", "2aed3e73c5deb31e598032cfa2907fa73bfc46ea"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "10486ec8573a88157e0cde6553043506558597a3"),
            ("boot-615-4-17.blk", "5b790ffa0a7d20316ef083abc46aae4a380dd2f8"),
            ("boot-900-15-17.blk", "21479ceb31af876943bd4bd8085457cdce126fcd"),
        ]
        cls.images = [
            ("boot-floppy.img", "d2cdd4416c614ab9a4c10b3801ee428113ade23f"),
        ]


class PCDOS610TestCase(BootTestCase, unittest.TestCase):
    # This version from windworldpc.

    @classmethod
    def setUpClass(cls):
        super(PCDOS610TestCase, cls).setUpClass()
        cls.version = "IBM DOS Version 6.1"
        cls.tarfile = "PC-DOS-6.10.tar"
        cls.files = [
            ("ibmbio.com", "841c1dc3d572ccdfc89439dcba625a14e7f2c435"),
            ("ibmdos.com", "9b80e4d8089403bf1b8f724c837ec0a4860de168"),
            ("command.com", "d117dc5ebf66d72135d30b27c1e3c82a089df6b5"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "bcaff34e315ea85cfdfa10eeedd66b642997ae55"),
            ("boot-615-4-17.blk", "58a8ade41f883f12f7e0678bf8d9c4a29afa5d34"),
            ("boot-900-15-17.blk", "b0a97aa09e9b4dbc8f7d3007e15335d937bb94f0"),
        ]
        cls.images = [
            ("boot-floppy.img", "2a732e5f72c4d48daf1123f28f7dc0752180e209"),
        ]


class PCDOS630TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS630TestCase, cls).setUpClass()
        cls.version = "PC DOS Version 6.3"
        cls.tarfile = "PC-DOS-6.30.tar"
        cls.files = [
            ("ibmbio.com", "420f3534105b59b32db195b74179239be9726da6"),
            ("ibmdos.com", "0219132518596ffb2d1e1f3bee4560f53039d2eb"),
            ("command.com", "2c346e1fa393674674b6026d024ef2dddb5d4b35"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "306e2c6c0e8570c8a4eadb5adbe68df2afa39c5c"),
            ("boot-615-4-17.blk", "096ed553d831705895223915b72d59e005b06a8f"),
            ("boot-900-15-17.blk", "d29d119c35aa635848cda6117e8dc939715804ad"),
        ]
        cls.images = [
            ("boot-floppy.img", "2622a317047696a53d42b9ccccf3828b0bb1c015"),
        ]


class PCDOS700TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS700TestCase, cls).setUpClass()
        cls.version = "PC DOS Version 7.0"
        cls.tarfile = "PC-DOS-7.00.tar"
        cls.files = [
            ("ibmbio.com", "1055d672738d1e6623a775023f117584961ec711"),
            ("ibmdos.com", "f6f63e93235e7f224bd7cd5f1c3b29da3dc6a8c4"),
            ("command.com", "9fe373c163186f5fcdbcdf3deb65e84ab9ab28cb"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "0aa71fa28c57929357be2dcead0943368b9367d4"),
            ("boot-615-4-17.blk", "d831c7228e21786a731b50428244ea61b5cc11d8"),
            ("boot-900-15-17.blk", "75a5b04a8e7452ad33c97d5092138756ad306613"),
        ]
        cls.images = [
            ("boot-floppy.img", "0dea8ae9651a94dc2d0cb4016a1957ae049298ef"),
        ]


class PCDOS72KTestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS72KTestCase, cls).setUpClass()
        cls.version = "PC DOS Version 7.0"
        cls.tarfile = "PC-DOS-7.2K.tar"
        cls.files = [
            ("ibmbio.com", "dcdb0b04f41b56783710cbfe497ae349449314e8"),
            ("ibmdos.com", "40c1e51276f4d434a3a1b9ecac85de500759540c"),
            ("command.com", "525dac736d511741b47afa730cd5182e920f57de"),
        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "0c2c76a9f5625ea2f260831b0576e16c3971f876"),
            ("boot-615-4-17.blk", "775da00c1a1479ddc5e6870a68fcd8a2b5e2cacc"),
            ("boot-900-15-17.blk", "11d2780bfebffb6c076ef75c1e393a4d60a3a1da"),
        ]
        cls.images = [
            ("boot-floppy.img", "c383ee09bff0c3596b9b284d69c51c2c65229faa"),
        ]


class PCDOS710TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(PCDOS710TestCase, cls).setUpClass()
        cls.version = "PC DOS Version 7.1"
        cls.tarfile = "PC-DOS-7.10.tar"
        cls.files = [
            ("ibmbio.com", "8da2a96dddcbd843b8cb15ba9bec098351019970"),
            ("ibmdos.com", "b2869241748a88b9db800ab4d5201615f6381f4d"),
            ("command.com", "aeaad5f4c32b9a33b636494e7b2022d08559f33c"),

        ]
        cls.systype = SYSTYPE_PCDOS_NEW
        cls.bootblocks = [
            ("boot-306-4-17.blk", "b014c96ae310113a67523ec144f28c1f779b0b61"),
            ("boot-615-4-17.blk", "ddd71ce8577d6212d7d2a7e707c3c3504641cf53"),
            ("boot-900-15-17.blk", "6a9864ef9b44671e54d3a7c9cc30faa0f8e843e8"),
        ]
        cls.images = [
            ("boot-floppy.img", "306794585fedec0f42cb88010f05902d898e7674"),
        ]


class DRDOS340TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(DRDOS340TestCase, cls).setUpClass()
        cls.version = "DR DOS Release 3.40"
        cls.tarfile = "DR-DOS-3.40.tar"
        cls.files = [
            ("drbios.sys", "9295cb3a984139700833a00a8898d2c8d3466aae"),
            ("drbdos.sys", "57bd8f7cc8f52b6b7ee086d1321091600c71bc3d"),
            ("command.com", "8b35def9457bca3d028f3d2d3ecac0444c2b9025"),
        ]
        cls.systype = SYSTYPE_DRDOS_OLD
        cls.bootblocks = [
            # bootblocks are made with slightly odd sizes
            ("boot-305-4-17.blk", "83a7c356b438fb85f1440b6149b335a4778ee1b6"),
            ("boot-614-4-17.blk", "a6eef0141622d913677f6621794642838e0937c0"),
            ("boot-899-15-17.blk", "8d1006d6134a8ddc0bd39aec17b765991d4e78e0"),
        ]
        cls.images = [
            ("boot-floppy.img", "175152fe606d6fa20a48a7a207c054b7a69e2f7d"),
        ]


class DRDOS341TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(DRDOS341TestCase, cls).setUpClass()
        cls.version = "DR DOS Release 3.41"
        cls.tarfile = "DR-DOS-3.41.tar"
        cls.files = [
            ("drbios.sys", "e32d7b23825cd0f54a0a6a72345839804d494345"),
            ("drbdos.sys", "f09a9dfc79036a1843d6ac4e1d4e8544688446fe"),
            ("command.com", "13b0572a433ce70eb1ede0843339a05a5f819125"),
        ]
        cls.systype = SYSTYPE_DRDOS_OLD
        cls.bootblocks = [
            ("boot-306-4-17.blk", "482fb7b984d217c68ca2ed8075fa24883a2ef9ea"),
            ("boot-615-4-17.blk", "6d129ca735edfa44c9e6e0d88d7fde0c9862b311"),
            ("boot-900-15-17.blk", "0aa255d3e88fbf8d84dd8567571ebec2f178812f"),

        ]
        cls.images = [
            ("boot-floppy.img", "2b4f3ec7415da8ec5313a575048bc2f3df988c27"),
        ]


class DRDOS5009006TestCase(BootTestCase, unittest.TestCase):
    # 06-15-1990 version on winworldpc.com

    @classmethod
    def setUpClass(cls):
        super(DRDOS5009006TestCase, cls).setUpClass()
        cls.version = "DR DOS Release 5.0"
        cls.tarfile = "DR-DOS-5.00-900615.tar"
        cls.files = [
            ("ibmbio.com", "082bb04981938a78339c42513de6613670a24ccd"),
            ("ibmdos.com", "82e6d096cc78591a9086aec841765886c3678c67"),
            ("command.com", "5110134706d885c8de79a8db33c86265e52a6ab5"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "8737fe87e09ae859434b9057e65017b9c2e16518"),
            ("boot-615-4-17.blk", "08f886c583ccb5a5a35df9ea3b7f409f436089e4"),
            ("boot-900-15-17.blk", "94c39add91d767a81e1a5c4e3c227a0089d83927"),
        ]
        cls.images = [
            ("boot-floppy.img", "5066558292f7da7535ac04ef97bc0c56200a25f4"),
        ]


class DRDOS5009008TestCase(BootTestCase, unittest.TestCase):
    # 08-14-1990 version on winworldpc.com

    @classmethod
    def setUpClass(cls):
        super(DRDOS5009008TestCase, cls).setUpClass()
        cls.version = "DR DOS Release 5.0"
        cls.tarfile = "DR-DOS-5.00-900814.tar"
        cls.files = [
            ("ibmbio.com", "6bca21a8efef7c8b48ff6541c9d578886a9a5bdb"),
            ("ibmdos.com", "b6709ebe1cf6dbea97340bd66154b4e7c6e59773"),
            ("command.com", "5110134706d885c8de79a8db33c86265e52a6ab5"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "8737fe87e09ae859434b9057e65017b9c2e16518"),
            ("boot-615-4-17.blk", "08f886c583ccb5a5a35df9ea3b7f409f436089e4"),
            ("boot-900-15-17.blk", "94c39add91d767a81e1a5c4e3c227a0089d83927"),
        ]
        cls.images = [
            ("boot-floppy.img", "4efe48a65705c237ad75ec7b78952807be64ac8d"),
        ]


class DRDOS600TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(DRDOS600TestCase, cls).setUpClass()
        cls.version = "DR DOS Release 6.0"
        cls.tarfile = "DR-DOS-6.00.tar"
        cls.files = [
            ("ibmbio.com", "90739d6655994d7898afb25f1c8daa929c9a8716"),
            ("ibmdos.com", "20d54c2e754cd1abd0bccb12e32b6e9ecc3fb56e"),
            ("command.com", "4f20db525f5d5cbf66a5364a52e37d8fa01b950f"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "11b6c9b93f988cb4a4699d1c78b7f644287eacfb"),
            ("boot-615-4-17.blk", "76beaf15dc1341f8ac7667d3a44fd7cd5d7f71c3"),
            ("boot-900-15-17.blk", "d601e8d0df80324a451bb717222305d690e0a4bf"),
        ]
        cls.images = [
            ("boot-floppy.img", "fc10d3f20ef7d9d18d14536df8fe781a09a0192d"),
        ]


class DRDOS6009303TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(DRDOS6009303TestCase, cls).setUpClass()
        cls.version = "DR DOS Release 6.0"
        cls.tarfile = "DR-DOS-6.00-930319.tar"
        cls.files = [
            ("ibmbio.com", "e6067cd294e7a365bd05e6bb5c0a298da4d3204e"),
            ("ibmdos.com", "ad0f6ca41c5b2f7a917c471f1d61cf3721d31f57"),
            ("command.com", "cee2755dbe4296b63b479a7adbbfc1b922dcb197"),
        ]

        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "aed8780bbbfbda2ccf4e5918912d80252eaafe14"),
            ("boot-615-4-17.blk", "aa93bd1b1dc787681487cca07fc3d5673c38dcb1"),
            ("boot-900-15-17.blk", "d601e8d0df80324a451bb717222305d690e0a4bf"),
        ]
        cls.images = [
            ("boot-floppy.img", "7b5f93221e6aaa844d0883fe07f0de18d0c527c5"),
        ]

class DRDOS700TestCase(BootTestCase, unittest.TestCase):
    # Badged Novell DOS 7.0

    @classmethod
    def setUpClass(cls):
        super(DRDOS700TestCase, cls).setUpClass()
        cls.version = "Novell DOS 7"
        cls.tarfile = "DR-DOS-7.00.tar"
        cls.files = [
            ("ibmbio.com", "29c31d730ed250049bccc571f94c1c5495d1bcea"),
            ("ibmdos.com", "ee20d956f21a8fa4e4a0fc1f1ef4de1ca6e3330b"),
            ("command.com", "051195a9f46571aef9f652c085f02093f3551293"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "37a330da5e224f629cc177ec2eea3ad5636e5f60"),
            ("boot-615-4-17.blk", "98a45096991b7fd04f5df4e54a64b8fa0ef42fb5"),
            ("boot-900-15-17.blk", "9b5c6823aa63cfb07653e6726892944bcfb20452"),
        ]
        cls.images = [
            ("boot-floppy.img", "a66eaaefe22c0c44cb865032e3c1ee60f4e98a9c"),
        ]


class DRDOS701TestCase(BootTestCase, unittest.TestCase):
    # OpenDOS 7.01

    @classmethod
    def setUpClass(cls):
        super(DRDOS701TestCase, cls).setUpClass()
        cls.version = "Caldera OpenDOS 7.01"
        cls.tarfile = "DR-DOS-7.01.tar"
        cls.files = [
            ("ibmbio.com", "61211eb63329a67fdd9d336271f06e1bfdab2b6f"),
            ("ibmdos.com", "52e71c8e9d74100f138071acaecdef4a79b67d3c"),
            ("command.com", "4bc38f973b622509aedad8c6af0eca59a2d90fca"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "1151ab9a3429163ac3ddf55b88d81359cb6975e5"),
            ("boot-615-4-17.blk", "a18ee96e63e384b766bafc4ff936e4087c31bf59"),
            ("boot-900-15-17.blk", "2ea4ea747f6e62a8ea46f14f6c9af1ad6fd0126b"),
        ]
        cls.images = [
            ("boot-floppy.img", "d38fb2dba30185ce510cf3366bd71a1cbc2635da"),
        ]


class DRDOS7029711TestCase(BootTestCase, unittest.TestCase):
    # November 1997 version

    @classmethod
    def setUpClass(cls):
        super(DRDOS7029711TestCase, cls).setUpClass()
        cls.version = "Caldera DR-OpenDOS 7.02"
        cls.tarfile = "DR-DOS-7.02-971119.tar"
        cls.files = [
            ("ibmbio.com", "902f8cae012445844f0e32b0effc190c8f52b33b"),
            ("ibmdos.com", "63bad7432ae39d33eb194e94b5a8bfe7f32da003"),
            ("command.com", "799123491276c13f35757cb2f2a9beb4a23237c5"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "75420dd9caf05614ed9a6da1e52c7c76cafcae2b"),
            ("boot-615-4-17.blk", "08e11d5e24b3c7abe26302f3ee8380f68d47702c"),
            ("boot-900-15-17.blk", "7fefbc3f4292da1055dc818feccf7d60415473cd"),
        ]
        cls.images = [
            ("boot-floppy.img", "d4f0b2475fef5200b813d5975321a852da74e491"),
        ]


class DRDOS7029801TestCase(BootTestCase, unittest.TestCase):
    # January 1998 version

    @classmethod
    def setUpClass(cls):
        super(DRDOS7029801TestCase, cls).setUpClass()
        cls.version = "Caldera DR-DOS 7.02"
        cls.tarfile = "DR-DOS-7.02-980123.tar"
        cls.files = [
            ("ibmbio.com", "af05e014d894cf4fda37d629446525e7018b7b17"),
            ("ibmdos.com", "39ed49c4b6c3641061087b0b799c42fbc41b129c"),
            ("command.com", "5757dc7604e12fb3bbfe96e53a37028060bcedf3"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "5bf990d2cb75a5065b1a5acc3413c016d00e0dfa"),
            ("boot-615-4-17.blk", "de059fc410e489579633f85fabf20921884f2037"),
            ("boot-900-15-17.blk", "947ce1c1b6d6ea4d8492f89c43a711ff7a7e7972"),
        ]
        cls.images = [
            ("boot-floppy.img", "7ada4fc082467d8a5de6457802e9107e31e3b4eb"),
        ]


class DRDOS703TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(DRDOS703TestCase, cls).setUpClass()
        cls.version = "Caldera DR-DOS 7.03"
        cls.tarfile = "DR-DOS-7.03.tar"
        cls.files = [
            ("ibmbio.com", "a2b588314f331a93ce2ddbe63e4e9ed19b5dc864"),
            ("ibmdos.com", "e89e9c1f956c8249864fdb42037f29061729b709"),
            ("command.com", "90eca5bdcc851fd0b977bf6dbbab040b4797cc2c"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "f6e39c1d4db33edec075351583465f0dd6bd9e0d"),
            ("boot-615-4-17.blk", "c054171ed51791cf187920831daaf7953ec70583"),
            ("boot-900-15-17.blk", "591c88b821896c9f0b266957b3a44d399785fdd6"),
        ]
        cls.images = [
            ("boot-floppy.img", "76db4ee538cd7cebf291ce49b68319ac8a56a8e5"),
        ]


class DRDOS800TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(DRDOS800TestCase, cls).setUpClass()
        cls.version = "DeviceLogics DR-DOS 8.0"
        cls.tarfile = "DR-DOS-8.00.tar"
        cls.files = [
            ("ibmbio.com", "72e1e2899160615a289b92992fa006bd6152e0c8"),
            ("ibmdos.com", "4a9fc6b578d8434f4d3966a01c37f00f06c1d8ae"),
            ("command.com", "0c0e21eeea27b848bce3bea218f03c93c10c4ac5"),
        ]
        cls.systype = SYSTYPE_DRDOS_ORIGINAL
        cls.bootblocks = [
            ("boot-306-4-17.blk", "4a85e2963d56117a2e18e984cf91cf789a580667"),
            ("boot-615-4-17.blk", "f4eb03f00d22a25b5ae4952f4030eafcdc2f3ec8"),
            ("boot-900-15-17.blk", "e6b59cb39a628c179489c17b5512ef94037fa4b5"),
        ]
        cls.images = [
            ("boot-floppy.img", "58eaa7df34fd3280f978b5b75817d610114022f9"),
        ]


class FRDOS100TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(FRDOS100TestCase, cls).setUpClass()
        cls.version = "FreeDOS kernel build 2036"
        cls.tarfile = "FR-DOS-1.00.tar"
        cls.files = [
            ("kernel.sys", "42568b259ad201bbbc20602aff4139171fef0d10"),
            ("command.com", "0733db7babadd73a1b98e8983c83b96eacef4e68"),
        ]
        cls.systype = SYSTYPE_FRDOS_NEW
        cls.bootblocks = [
            ("boot-302-4-17.blk", "64d2032e3771eef56d1d67c647665c8e1ead5fed"),
            ("boot-603-4-17.blk", "e40608bebd958a051d94b9b2ddffbdd7ed5de7ba"),
            ("boot-900-15-17.blk", "a984a82102cb3024dd953c328a4cd27a4f70fad9"),
        ]
        cls.images = [
            ("boot-floppy.img", "70ae40db80350c4c2eb032bf04017e8497ba60d0"),
        ]

    def setUp(self):
        super(FRDOS100TestCase, self).setUp()

        mkfile("version.bat", "ver /r\r\nrem end\r\n")


class FRDOS110TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(FRDOS110TestCase, cls).setUpClass()
        cls.version = "FreeDOS kernel 2041"
        cls.tarfile = "FR-DOS-1.10.tar"
        cls.files = [
            ("kernel.sys", "d6f78d17f75f1a775ce651c267fef4df2d7cdcc3"),
            ("command.com", "0733db7babadd73a1b98e8983c83b96eacef4e68"),
        ]
        cls.systype = SYSTYPE_FRDOS_NEW
        cls.bootblocks = [
            ("boot-302-4-17.blk", "dce5d45867dfe85d9a91217ff3e0e407ba5f0738"),
            ("boot-603-4-17.blk", "e400e632aaf0869bf00166d1c3c95cb94dd4e94d"),
            ("boot-900-15-17.blk", "bbe43a28824241828d6255fd54303227c4312350"),
        ]
        cls.images = [
            ("boot-floppy.img", "504bbfc707228179395fa6fb08983fb2125ddbc9"),
        ]

    def setUp(self):
        super(FRDOS110TestCase, self).setUp()

        mkfile("version.bat", "ver /r\r\nrem end\r\n")


class FRDOS120TestCase(BootTestCase, unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        super(FRDOS120TestCase, cls).setUpClass()
        cls.version = "FreeDOS kernel 2042"
        cls.tarfile = "FR-DOS-1.20.tar"
        cls.files = [
            ("kernel.sys", "0709f4e7146a8ad9b8acb33fe3fed0f6da9cc6e0"),
            ("command.com", "0733db7babadd73a1b98e8983c83b96eacef4e68"),
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

    def setUp(self):
        super(FRDOS120TestCase, self).setUp()

        mkfile("version.bat", "ver /r\r\nrem end\r\n")


if __name__ == '__main__':
    try:
        import nose
        nose.run(argv=[__file__, '-v'])
    except ImportError:
        unittest.main()
