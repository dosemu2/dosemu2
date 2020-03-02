import pexpect
import string
import random
import re
import unittest

from hashlib import sha1
from os import makedirs, mkdir, rename, unlink
from os.path import exists, join
from ptyprocess import PtyProcessError
from shutil import copytree, rmtree
from subprocess import Popen, check_call
from tarfile import open as topen

BINSDIR = "test-binaries"
WORKDIR = "test-imagedir/dXXXXs/c"
PASS = 0
SKIP = 1
EXPECTED = 2


def mkfile(fname, content, dname=WORKDIR, writemode="w"):
    with open(join(dname, fname), writemode) as f:
        f.write(content)


def mkexe(fname, content, dname=WORKDIR):
    basename = join(dname, fname)
    with open(basename + ".c", "w") as f:
        f.write(content)
    check_call(["i586-pc-msdosdjgpp-gcc",
                "-o", basename + ".exe", basename + ".c"])


def mkcom(fname, content, dname=WORKDIR):
    basename = join(dname, fname)
    with open(basename + ".S", "w") as f:
        f.write(content)
    check_call(["as", "-o", basename + ".o", basename + ".S"])
    check_call(["gcc",
                "-static",
                "-Wl,--section-start=.text=0x100,-e,_start16", "-nostdlib",
                "-o", basename + ".com.elf",
                basename + ".o"])
    check_call(["objcopy",
                "-j", ".text", "-O", "binary",
                basename + ".com.elf",
                basename + ".com"])
    check_call(["rm", basename + ".o", basename + ".com.elf"])


def mkstring(length):
    return ''.join(random.choice(string.hexdigits) for x in range(length))


class BaseTestCase(object):

    @classmethod
    def setUpClass(cls):
        imagedir = WORKDIR.split('/')[0]
        if not imagedir or imagedir == "." or imagedir[0] == "/":
            raise ValueError
        cls.imagedir = imagedir
        cls.version = "BaseTestCase default"
        cls.prettyname = "NoPrettyNameSet"
        cls.tarfile = None
        cls.files = [(None, None)]
        cls.actions = {}
        cls.systype = None
        cls.bootblocks = [(None, None)]
        cls.images = [(None, None)]
        cls.autoexec = "autoexec.bat"
        cls.confsys = "config.sys"

        if not exists("test-libdir"):
            mkdir("test-libdir")
            mkdir("test-libdir/dosemu2-cmds-0.2")

        cls.nologs = False

    @classmethod
    def setUpClassPost(cls):
        if cls.tarfile is None:
            cls.tarfile = cls.prettyname + ".tar"

        if cls.tarfile != "" and not exists(join(BINSDIR, cls.tarfile)):
            raise unittest.SkipTest(
                    "TestCase %s binary not available" % cls.prettyname)

    @classmethod
    def tearDownClass(cls):
        pass

    def setUp(self):
        if self.actions.get(self._testMethodName) == SKIP:
            self.skipTest("")

        rmtree(self.imagedir, ignore_errors=True)
        makedirs(WORKDIR)

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
        pass

    def shortDescription(self):
        doc = super(BaseTestCase, self).shortDescription()
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
                with open(join(WORKDIR, f[0]), "rb") as g:
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
        bootblock = [x for x in self.bootblocks if re.match(name, x[0])]
        if not len(bootblock) == 1:
            self.skipTest("Boot block signature not available")

        self.unTarOrSkip(self.tarfile, bootblock)

        if(mv):
            rename(join(WORKDIR, bootblock[0][0]), join(WORKDIR, "boot.blk"))

    def unTarImageOrSkip(self, name):
        image = [x for x in self.images if name == x[0]]
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
            blkarg = ["-b", "boot.blk"]
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
        result.wait()

        return name

    def runDosemu(self, cmd, opts="video{none}", outfile=None, config=None):
        # Note: if debugging is turned on then times increase 10x
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
        with open(self.xptname, "wb") as fout:
            child.logfile = fout
            child.setecho(False)
            try:
                child.expect(['(system|unix) -e[\r\n]*'], timeout=10)
                child.expect(['>[\r\n]*', pexpect.TIMEOUT], timeout=1)
                child.send(cmd + '\r\n')
                child.expect(['rem end'], timeout=20)
                if outfile is None:
                    ret = child.before.decode('ASCII')
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


class MyTestResult(unittest.TextTestResult):

    def getDescription(self, test):
        return '%-80s' % test.shortDescription()

    def startTest(self, test):
        super(MyTestResult, self).startTest(test)
        name = test.id().replace('__main__', test.pname)
        test.logname = name + ".log"
        test.xptname = name + ".xpt"

    def stopTest(self, test):
        super(MyTestResult, self).stopTest(test)
        if self.wasSuccessful():
            try:
                unlink(test.logname)
                unlink(test.xptname)
            except OSError:
                pass

    def addFailure(self, test, err):
        super(MyTestResult, self).addFailure(test, err)
        if not test.nologs:
            with open(test.logname) as f:
                self.stream.writeln("")
                self.stream.writeln(">>>>>>>>>>>>>>>>>>>>>>>>>>>> dosemu.log <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<")
                self.stream.writeln(f.read())
                self.stream.writeln("")
            with open(test.xptname) as f:
                self.stream.writeln("")
                self.stream.writeln(">>>>>>>>>>>>>>>>>>>>>>>>>>>> expect.log <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<")
                self.stream.writeln(f.read())
                self.stream.writeln("")


class MyTestRunner(unittest.TextTestRunner):
    resultclass = MyTestResult
