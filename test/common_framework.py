import pexpect
import string
import random
import re
import traceback
import unittest

from datetime import datetime
from hashlib import sha1
from os import environ, getcwd, makedirs, rename, unlink
from os.path import exists, join
from ptyprocess import PtyProcessError
from shutil import copy, rmtree
from subprocess import Popen, check_call, check_output, STDOUT, TimeoutExpired
from sys import exit, version_info
from tarfile import open as topen
from unittest.util import strclass

BINSDIR = "test-binaries"
WORKDIR = "test-imagedir/dXXXXs/c"
PASS = 0
SKIP = 1
KNOWNFAIL = 2
UNSUPPORTED = 3

IPROMPT = "Interactive Prompt!"


def mkfile(fname, content, dname=WORKDIR, writemode="w", newline=None):
    with open(join(dname, fname), writemode, newline=newline) as f:
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

        cls.logfiles = {}
        cls.msg = None

    @classmethod
    def setUpClassPost(cls):
        try:
            skip_class_threshold = environ.get("SKIP_CLASS_THRESHOLD")
            if cls.priority > int(skip_class_threshold):
               raise unittest.SkipTest(
                        "TestCase %s skipped having priority(%d)" % (
                        cls.prettyname, cls.priority))
        except (TypeError, ValueError):
            pass

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
        elif self.actions.get(self._testMethodName) == KNOWNFAIL:
            self.skipTest("known failure")
        elif self.actions.get(self._testMethodName) == UNSUPPORTED:
            self.skipTest("unsupported")

        rmtree(self.imagedir, ignore_errors=True)
        makedirs(WORKDIR)

        # Extract the boot files
        if self.tarfile != "":
            self.unTarOrSkip(self.tarfile, self.files)

        # Empty dosemu.conf for default values
        mkfile("dosemu.conf", """$_force_fs_redirect = (off)\n""", self.imagedir)

        # Create startup files
        self.setUpDosAutoexec()
        self.setUpDosConfig()
        self.setUpDosVersion()

        # Tag the end of autoexec.bat for runDosemu()
        mkfile(self.autoexec, "\r\n@echo " + IPROMPT + "\r\n", writemode="a")

    def setUpDosAutoexec(self):
        # Use the standard shipped autoexec
        copy(join("src/bindist", self.autoexec), WORKDIR)

    def setUpDosConfig(self):
        # Use the standard shipped config
        copy(join("src/bindist", self.confsys), WORKDIR)

    def setUpDosVersion(self):
        # FreeCom / Comcom32 compatible
        mkfile("version.bat", "ver /r\r\nrem end\r\n")

    def tearDown(self):
        pass

    def shortDescription(self):
        doc = super(BaseTestCase, self).shortDescription()
        return "Test %s %s" % (self.prettyname, doc)

    def setMessage(self, msg):
        self.msg = msg

# helpers

    def unTarOrSkip(self, tname, files):
        tfile = join(BINSDIR, tname)

        try:
            with topen(tfile) as tar:
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
        except IOError:
            self.skipTest("Archive not found or unreadable(%s)" % tfile)

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

    def runDosemu(self, cmd, opts=None, outfile=None, config=None, timeout=5,
                    interactions=[]):
        # Note: if debugging is turned on then times increase 10x
        dbin = "bin/dosemu"
        args = ["-f", join(self.imagedir, "dosemu.conf"),
                "-n",
                "-o", self.logfiles['log'][0],
                "-td",
                #    "-Da",
                "--Fimagedir", self.imagedir]
        if opts is not None:
            args.extend(["-I", opts])

        if config is not None:
            mkfile("dosemu.conf", config, dname=self.imagedir, writemode="a")

        child = pexpect.spawn(dbin, args)
        ret = ''
        with open(self.logfiles['xpt'][0], "wb") as fout:
            child.logfile = fout
            child.setecho(False)
            try:
                prompt = r'(system -e|unix -e|' + IPROMPT + ')'
                child.expect([prompt + '[\r\n]*'], timeout=10)
                child.expect(['>[\r\n]*', pexpect.TIMEOUT], timeout=1)
                child.send(cmd + '\r\n')
                for resp in interactions:
                    child.expect(resp[0])
                    child.send(resp[1])
                    if outfile is None:
                        ret += child.before.decode('ASCII', 'replace')
                child.expect(['rem end'], timeout=timeout)
                if outfile is None:
                    ret += child.before.decode('ASCII', 'replace')
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

    def runDosemuCmdline(self, xargs, cwd=None, config=None, timeout=30):
        testroot = getcwd()

        args = [join(testroot, "bin", "dosemu"),
                "--Fimagedir", join(testroot, self.imagedir),
                "-f", join(testroot, self.imagedir, "dosemu.conf"),
                "-n",
                "-o", join(testroot, self.logfiles['log'][0]),
                "-td",
                "-ks"]
        args.extend(xargs)

        if config is not None:
            mkfile("dosemu.conf", config, dname=self.imagedir, writemode="a")

        try:
            ret = check_output(args, cwd=cwd, timeout=timeout, stderr=STDOUT)
            with open(self.logfiles['xpt'][0], "w") as f:
                f.write(ret.decode('ASCII'))
        except TimeoutExpired as e:
            ret = 'Timeout'
            with open(self.logfiles['xpt'][0], "w") as f:
                f.write(e.output.decode('ASCII'))

        return ret


class MyTestResult(unittest.TextTestResult):

    def getDescription(self, test):
        if 'SubTest' in strclass(test.__class__):
            return str(test)
        return '%-80s' % test.shortDescription()

    def startTest(self, test):
        super(MyTestResult, self).startTest(test)
        self.starttime = datetime.utcnow()

        name = test.id().replace('__main__', test.pname)
        test.logfiles = {
            'log': (name + ".log", "dosemu.log"),
            'xpt': (name + ".xpt", "expect.log"),
        }
        test.firstsub = True
        test.msg = None

    def addFailure(self, test, err):
        if self.showAll:
            self.stream.writeln("FAIL")
        elif self.dots:
            self.stream.write('F')
            self.stream.flush()
        self.failures.append((test, self.gather_info_for_failure(err, test)))
        self._mirrorOutput = True

    def gather_info_for_failure(self, err, test):
        """Gather traceback, stdout, stderr, dosemu and expect logs"""

        TITLE_NAME_FMT = '{:^16}'
        TITLE_BANNER_FMT = '{:-^70}\n'

        # Traceback
        exctype, value, tb = err
        while tb and self._is_relevant_tb_level(tb):
            # Skip test runner traceback levels
            tb = tb.tb_next
        if exctype is test.failureException:
            # Skip assert*() traceback levels
            length = self._count_relevant_tb_levels(tb)
        else:
            length = None
        tb_e = traceback.TracebackException(
            exctype, value, tb, limit=length, capture_locals=self.tb_locals)
        msgLines = list(tb_e.format())

        # Stdout, Stderr
        if self.buffer:
            output = sys.stdout.getvalue()
            error = sys.stderr.getvalue()
            if output:
                name = TITLE_NAME_FMT.format('stdout')
                msgLines.append(TITLE_BANNER_FMT.format(name))
                if not output.endswith('\n'):
                    output += '\n'
                msgLines.append(STDOUT_LINE % output)
            if error:
                name = TITLE_NAME_FMT.format('stderr')
                msgLines.append(TITLE_BANNER_FMT.format(name))
                if not error.endswith('\n'):
                    error += '\n'
                msgLines.append(STDERR_LINE % error)

        # Our logs
        for _, l in test.logfiles.items():
            name = TITLE_NAME_FMT.format(l[1])
            msgLines.append(TITLE_BANNER_FMT.format(name))
            try:
                with open(l[0]) as f:
                    cnt = f.read()
                    if not cnt.endswith('\n'):
                        cnt += '\n'
                    msgLines.append(cnt)
            except FileNotFoundError:
                msgLines.append("File not present\n")

        return ''.join(msgLines)

    def addSuccess(self, test):
        super(unittest.TextTestResult, self).addSuccess(test)
        if self.showAll:
            if self.starttime is not None:
                duration = datetime.utcnow() - self.starttime
                msg = (" " + test.msg) if test.msg else ""
                self.stream.writeln("ok ({:>6.2f}s){}".format(duration.total_seconds(), msg))
            else:
                self.stream.writeln("ok")
        elif self.dots:
            self.stream.write('.')
            self.stream.flush()

        for _, l in test.logfiles.items():
            try:
                unlink(l[0])
            except OSError:
                pass

    def addSubTest(self, test, subtest, err):
        super(MyTestResult, self).addSubTest(test, subtest, err)
        if err is not None:
            if test.firstsub:
                test.firstsub = False
                self.stream.writeln("FAIL (one or more subtests)")
            self.stream.writeln("    %-76s ... FAIL" % subtest._subDescription())


class MyTestRunner(unittest.TextTestRunner):
    resultclass = MyTestResult


def main(argv=None):
    if version_info < (3, 0):
        exit("Python 3.0 or later is required.")
    unittest.main(testRunner=MyTestRunner, argv=argv, verbosity=2)
