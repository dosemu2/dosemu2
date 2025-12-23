import inspect
import pexpect
import string
import random
import re
import traceback
import unittest

from datetime import datetime, timezone
from functools import wraps
from hashlib import sha1
from os import environ, rename
from os.path import exists, join
from pathlib import Path
from platform import system, machine, release
from ptyprocess import PtyProcessError
from shutil import copy, rmtree
from subprocess import (Popen, call, check_call, check_output,
                        DEVNULL, STDOUT, TimeoutExpired, CalledProcessError)
from sys import argv, exit, stdout, stderr, version_info
from tarfile import open as topen
from time import sleep
from unittest.util import strclass

__common_framework = True

BINSDIR = "test-binaries"
IMAGEDIR = "test-imagedir"
PASS = 0
SKIP = 1
KNOWNFAIL = 2
UNSUPPORTED = 3

IPROMPT = "Interactive Prompt!"

# hardcoded in mount helper
VFAT_FIMAGE = "/img/dosemu.img"
VFAT_HELPER = "/bin/dosemu_fat_mount.sh"
VFAT_MNTPNT = "/mnt/dosemu"

TAP_HELPER = "/bin/dosemu_tap_interface.sh"

# Get any test binaries we need
TEST_BINARY_HOST = "http://www.spheresystems.co.uk/test-binaries"
TEST_BINARIES = (
    'DR-DOS-7.01.tar',
    'FR-DOS-1.20.tar',
    'FR-DOS-1.30.tar',
    'MS-DOS-6.22.tar',
    'MS-DOS-7.00.tar',
    'MS-DOS-7.10.tar',
    'VARIOUS.tar',
    'TEST_CRYNWR.tar',
    'TEST_DOSLFN.tar',
    'TEST_EMM286.tar',
    'TEST_JAPHETH.tar',
    'TEST_MTCP.tar',
    'TEST_R200.tar',
)

DOSEMU_CONF_DEFAULT = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""

RED = "\x1b[31m"
GREEN = "\x1b[32m"
YELLOW = "\x1b[33m"
LIGHT_BLUE = "\x1b[94m"
RESET = "\x1b[0m"


def mkstring(length):
    return ''.join(random.choice(string.hexdigits) for x in range(length))


def setup_vfat_mounted_image(self):
    if not exists(VFAT_HELPER):
        self.skipTest("mount helper not installed")

    call(["sudo", VFAT_HELPER, "umount"], stderr=DEVNULL)

    call(["sudo", VFAT_HELPER, "setup"], stderr=STDOUT)

    with open(VFAT_FIMAGE, "wb") as f:
        f.write(b'\x00' * 1474560)

    check_call(["mkfs", "-t", "vfat", VFAT_FIMAGE], stdout=DEVNULL, stderr=DEVNULL)

    try:
        check_call(["sudo", VFAT_HELPER, "mount"], stderr=STDOUT)
    except (CalledProcessError, TimeoutExpired):
        self.skipTest("mount helper ineffective")


def teardown_vfat_mounted_image(self):
    if not exists(VFAT_HELPER):
        self.skipTest("mount helper not installed")

    check_call(["sudo", VFAT_HELPER, "umount"], stderr=STDOUT)


def setup_tap_interface(self):
    if not exists(TAP_HELPER):
        self.skipTest("tap interface helper not installed")

    check_call(["sudo", TAP_HELPER, "setup"], stderr=STDOUT)


def teardown_tap_interface(self):
    if not exists(TAP_HELPER):
        self.skipTest("tap interface helper not installed")

    sleep(5)    # wait for any process to die before attempting teardown
    check_call(["sudo", TAP_HELPER, "teardown"], stderr=STDOUT)


def get_test_binaries():
    tbindir = Path('.').resolve() / BINSDIR

    if tbindir.is_symlink():
        tbindir = tbindir.resolve()
    if not tbindir.exists():
        tbindir.mkdir()

    for tfile in TEST_BINARIES:
        if not Path(tbindir / tfile).exists():
            check_call([
                "wget",
                "--no-verbose",
                TEST_BINARY_HOST + '/' + tfile,
            ], stderr=STDOUT, cwd=tbindir)


def mark(attr_names):
    """
    Usage:
      @mark('attr')            -> sets attr = True
      @mark(['a', 'b'])       -> sets a = True and b = True
    """
    if isinstance(attr_names, str):
        names = (attr_names,)
    else:
        names = tuple(attr_names)

    def decorator(func):
        @wraps(func)
        def wrapper(*args, **kwargs):
            return func(*args, **kwargs)
        for n in names:
            setattr(wrapper, n, True)
        return wrapper
    return decorator


class BaseTestCase(object):

    attrs = set()
    use_cpu = None

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()

        idir = cls.topdir / IMAGEDIR
        if idir.is_symlink():
            target = idir.resolve()
            if target.name != idir.name:
                raise ValueError("Imagedir link target name '%s' != '%s'" % (target.name, idir.name))
            cls.imagedir = target
        else:
            cls.imagedir = idir
        if not cls.imagedir.exists():
            cls.imagedir.mkdir()
        if not cls.imagedir.is_dir():
            raise ValueError("Imagedir must be non-existent, a directory or a link to a directory '%s'" % str(cls.imagedir))

        cls.cmddir = Path(environ.get("TEST_CMDDIR", cls.topdir / "src" / "bindist"))
        cls.dosemu = Path(environ.get("TEST_DOSEMU", cls.topdir / "bin" / "dosemu"))

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
        cls.have_kvm = False
        cls.have_vm86 = False

    @classmethod
    def setUpClassPost(cls):
        if cls.tarfile is None:
            cls.tarfile = cls.prettyname + ".tar"

        if cls.tarfile != "":
            if cls.tarfile not in TEST_BINARIES:
                exit("\nUpdate tuple TEST_BINARIES for '%s'\n" % cls.prettyname)
            if not exists(join(BINSDIR, cls.tarfile)):
                exit("\nMissing test binary file, please run again with argument --get-test-binaries\n")

        if system() == 'Linux':
            if machine() == 'i686':
                cls.have_vm86 = True

            if cls.use_cpu == 'emu':
                print(" \nUsing Emulation", file=stderr, flush=True)
                return

            reason = ""
            try:
                with open("/dev/kvm", "r+b"):
                    major, minor = release().split('.')[0:2]
                    if not (major == '6' and minor in ['11', '12', '13']):
                        if environ.get("NO_KVM", '0') == '1':
                            reason = "NO_KVM=1"
                        else:
                            cls.have_kvm = True
                    else:
                        reason = "Blacklisted kernel [6.11 - 6.13]"

            except FileNotFoundError:
                pass
            except PermissionError:
                op = check_output(["getfacl", "/dev/kvm"])
                reason = "Permissions wrong on /dev/kvm\n%s" % op.decode('ASCII')

            if cls.use_cpu == 'kvm':
                if not cls.have_kvm:
                    print(" \nUsing KVM ", end='', file=stderr, flush=True)
                    raise unittest.SkipTest("KVM not available: %s" % reason)
                print(" \nUsing KVM", file=stderr, flush=True)
                return

            # No use_cpu specified, use whatever
            if cls.have_kvm:
                print(" \nUsable KVM found", file=stderr, flush=True)
            else:
                print(" \nUsable KVM not found, falling back to emulation: %s" % reason , file=stderr, flush=True)

    @classmethod
    def tearDownClass(cls):
        pass

    @classmethod
    def getTests(cls):
        funcs = inspect.getmembers(cls, predicate=inspect.isfunction)
        return [t for t in funcs if t[0].startswith('test')]

    @classmethod
    def getTestsDefinedIn(cls):
        parent = []
        us = []

        funcs = cls.getTests()
        for f in funcs:
            if f[0] in cls.__dict__:
                us += [f,]
            else:
                parent += [f,]

        return (parent, us)

    @classmethod
    def getTestnames(cls):
        funcs = cls.getTests()
        return [cls.__name__ + '.' + t[0] for t in funcs]

    @classmethod
    def getTestnames_with_attr(cls, attr):
        funcs = cls.getTests()
        return [cls.__name__ + '.' + t[0] for t in funcs if hasattr(t[1], attr)]

    def setUp(self):
        # Process and skip actions
        if not environ.get("NO_ACTIONS", '0') == '1':
            for key, value in self.actions.items():
                if re.match(key, self._testMethodName):
                    d = {
                        SKIP: "",
                        KNOWNFAIL: "known failure",
                        UNSUPPORTED: "unsupported",
                    }
                    self.skipTest(d.get(value, "unknown key"))

        for p in self.imagedir.iterdir():
            if p.is_dir():
                rmtree(str(p), ignore_errors=True)
            else:
                p.unlink()

        self.workdir = self.imagedir / "dXXXXs" / "c"
        self.workdir.mkdir(parents=True)

        # Extract the boot files
        if self.tarfile != "":
            self.unTarOrSkip(self.tarfile, self.files)

        # Link back to std dosemu commands and scripts
        p = self.workdir / "dosemu"
        if environ.get("TEST_CMDDIR"):
            p.symlink_to(self.cmddir / "dosemu")
        else:
            p.symlink_to(self.topdir / "commands" / "dosemu")

        # Create startup files
        self.setUpDosAutoexec()
        self.setUpDosConfig()
        self.setUpDosVersion()

        # Tag the end of autoexec.bat for runDosemu()
        self.mkfile(self.autoexec, "\r\n@echo " + IPROMPT + "\r\n", mode="a")

    def setUpDosAutoexec(self):
        # Use the standard shipped autoexec
        copy(self.cmddir / self.autoexec, self.workdir)

    def setUpDosConfig(self):
        # Use the standard shipped config
        copy(self.cmddir / self.confsys, self.workdir)

    def setUpDosVersion(self):
        # FreeCom / Comcom32 compatible
        self.mkfile("version.bat", "ver /r\r\nrem end\r\n")

    def tearDown(self):
        pass

    def shortDescription(self):
        doc = super(BaseTestCase, self).shortDescription()
        return "Test %-11s %s" % (self.prettyname, doc)

    def setMessage(self, msg):
        self.msg = msg

    def id(self):
        import __main__ as main
        pname = Path(main.__file__).stem
        myid = super(BaseTestCase, self).id().split('.')[-2:]
        return '%s.%s.%s' % (pname, *myid)

# helpers

    def utcnow(self):
        return datetime.now(timezone.utc)

    def mkcom_with_ia16(self, fname, content, dname=None, extraargs=None):
        if dname is None:
            p = self.workdir
        else:
            p = Path(dname).resolve()
        basename = str(p / fname)

        with open(basename + ".c", "w") as f:
            f.write(content)
        args = ["ia16-elf-gcc", "-mcmodel=tiny",
                "-o", basename + ".com", basename + ".c", "-li86"]
        if extraargs:
            args += extraargs
        check_call(args)

    def mkexe_with_djgpp(self, fname, content, dname=None, extraargs=None):
        if dname is None:
            p = self.workdir
        else:
            p = Path(dname).resolve()
        basename = str(p / fname)

        with open(basename + ".c", "w") as f:
            f.write(content)
        args = ["i586-pc-msdosdjgpp-gcc",
                "-o", basename + ".exe", basename + ".c"]
        if extraargs:
            args += extraargs
        check_call(args)

    def mkcom_with_nasm(self, fname, content, dname=None):
        if dname is None:
            p = self.workdir
        else:
            p = Path(dname).resolve()
        basename = p / fname

        sfile = basename.with_suffix('.asm')
        sfile.write_text(content)
        ofile = basename.with_suffix('.com')
        check_call(["nasm", "-f", "bin", "-o", str(ofile), str(sfile)])

    def mkfile(self, fname, content, dname=None, mode="w", newline=None):
        if dname is None:
            p = self.workdir / fname
        else:
            p = Path(dname).resolve() / fname

        with p.open(mode=mode, newline=newline) as f:
            f.write(content)

    def mkworkdir(self, name, dname=None):
        if dname is None:
            testdir = self.workdir.with_name(name)
        else:
            testdir = Path(dname).resolve() / name
        if testdir.is_dir():
            rmtree(testdir)
        elif testdir.exists():
            testdir.unlink()
        testdir.mkdir()
        return testdir

    def unTarOrSkip(self, tname, files):
        tfile = self.topdir / BINSDIR / tname

        try:
            with topen(tfile) as tar:
                for f in files:
                    try:
                        if version_info >= (3, 12):
                            tar.extract(f[0], path=self.workdir, filter='data')
                        else:
                            tar.extract(f[0], path=self.workdir)
                        with open(self.workdir / f[0], "rb") as g:
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

        if mv:
            rename(self.workdir / bootblock[0][0], self.workdir / "boot.blk")

    def unTarImageOrSkip(self, name):
        image = [x for x in self.images if name == x[0]]
        if not len(image) == 1:
            self.skipTest("Image signature not available")

        self.unTarOrSkip(self.tarfile, image)
        rename(self.workdir / name, self.imagedir / name)

    def mkimage(self, fat, files=None, bootblk=False, cwd=None):
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

        if cwd is None:
            cwd = self.workdir

        if files is None:
            xfiles = [x.name for x in cwd.iterdir()]
        else:
            xfiles = [x[0] for x in files]

        name = "fat%s.img" % fat

        # mkfatimage [-b bsectfile] [{-t tracks | -k Kbytes}]
        #            [-l volume-label] [-f outfile] [-p ] [file...]
        result = Popen(
            [str(self.dosemu.parent / "mkfatimage16"),
                "-t", tnum,
                "-h", hnum,
                "-f", str(self.imagedir / name),
                "-p"
            ] + blkarg + xfiles,
            cwd=cwd
        )
        result.wait()

        return name

    def mkimage_vbr(self, fat, lfn=False, cwd=None):
        if fat == "12":
            bcount = 306 * 4 * 17   # type 1
        elif fat == "16":
            bcount = 615 * 4 * 17   # type 2
        elif fat == "16b":
            bcount = 900 * 15 * 17  # type 9
        elif fat == "32":
            bcount = 1048576        # 1 GiB
        else:
            raise ValueError("Invalid arg '%s'" % fat)

        name = "fat%s.img" % fat

        # mkfs.fat [OPTIONS] DEVICE [BLOCK-COUNT]
        check_call(
            ["mkfs",
                "-t", ("fat", "vfat")[lfn],
                "-C",
                "-F", fat[0:2],
                str(self.imagedir / name),
                str(bcount)],
            stdout=DEVNULL, stderr=DEVNULL)

        if cwd is None:
            cwd = self.workdir

        # mcopy -i ../fat32.img -s -v * ::/
        srcs = [str(f) for f in cwd.glob('*')]
        if srcs:   # copy files
            args = ["mcopy",
                    "-i", str(self.imagedir / name),
                    "-s"]
            args += srcs
            args += ["::/",]
            check_call(args, cwd=cwd, stdout=DEVNULL, stderr=DEVNULL)

        return name

    def patch(self, fname, changes, cwd=None):
        if cwd is None:
            cwd = self.workdir

        with open(cwd / fname, "r+b") as f:
            for c in changes:
                if len(c[1]) != len(c[2]):
                    raise ValueError("Old and new lengths differ")

                f.seek(c[0])
                old = f.read(len(c[1]))
                if old != c[1]:
                    raise ValueError("Old sequence not found")

                f.seek(c[0])
                f.write(c[2])

    def runDosemu(self, cmd, opts=None, outfile=None, config=DOSEMU_CONF_DEFAULT, timeout=15,
                    eofisok=False, interactions=[]):
        # Note: if debugging is turned on then times increase 10x
        dbin = str(self.dosemu)
        args = ["-f", str(self.imagedir / "dosemu.conf"),
                "-n",
                "-q",
                "-o", str(self.topdir / self.logfiles['log'][0]),
                "-td",
                #    "-Da",
                "--Fimagedir", str(self.imagedir)]
        if environ.get("NO_KVM", '0') == '1' or self.use_cpu == 'emu':
            args.extend(["-z", "0"])

        if opts is not None:
            args.extend(["-I", opts])

        self.mkfile("dosemu.conf", config, dname=self.imagedir)

        child = pexpect.spawn(dbin, args)
        ret = ''
        with open(self.logfiles['xpt'][0], "wb") as fout:
            child.logfile = fout
            child.setecho(False)
            try:
                prompt = r'(system -e|unix -e|' + IPROMPT + ')'
                child.expect([prompt + '[\r\n]*'], timeout=40)
                child.expect(['>[\r\n]*', pexpect.TIMEOUT], timeout=1)
                child.send(cmd + '\r\n')
                for resp in interactions:
                    child.expect(resp[0])
                    child.send(resp[1])
                    if outfile is None:
                        ret += child.before.decode('ASCII', 'replace')
                trms = ['rem end',]
                if eofisok:
                    trms += [pexpect.EOF,]
                child.expect(trms, timeout=timeout)
                if outfile is None:
                    ret += child.before.decode('ASCII', 'replace')
                else:
                    ret = outfile.read_text()
            except pexpect.TIMEOUT:
                ret = 'Timeout'
                tlog = self.logfiles['log'][0].read_text()
                if '(gdb) Attaching to program' in tlog:
                    sleep(60)
                    self.shouldStop = True
            except pexpect.EOF:
                ret = 'EndOfFile'

        try:
            child.close(force=True)
        except PtyProcessError:
            pass

        return ret

    def runDosemuCmdline(self, xargs, cwd=None, config=DOSEMU_CONF_DEFAULT, timeout=30):
        args = [str(self.dosemu),
                "--Fimagedir", str(self.imagedir),
                "-f", str(self.imagedir / "dosemu.conf"),
                "-n",
                "-q",
                "-o", str(self.topdir / self.logfiles['log'][0]),
                "-td",
                "-ks"]
        if environ.get("NO_KVM", '0') == '1' or self.use_cpu == 'emu':
            args.extend(["-z", "0"])
        args.extend(xargs)

        self.mkfile("dosemu.conf", config, dname=self.imagedir)

        self.logfiles['xpt'][1] = "output.log"
        ret = 'No output'
        try:
            ret = check_output(args, cwd=cwd, timeout=timeout, stderr=STDOUT).decode(
                'ASCII', errors='backslashreplace')
        except CalledProcessError as e:
            if e.output is not None:
                ret = e.output.decode('ASCII', errors='backslashreplace')
            ret += '\nNonZeroReturn:%d\n' % e.returncode
        except TimeoutExpired as e:
            if e.output is not None:
                ret = e.output.decode('ASCII', errors='backslashreplace')
            ret += '\nTimeout:%d seconds\n' % timeout
            # Now wait for any further logging from dosemu as hopefully it's
            # dying.
            sleep(5)
        finally:
            self.logfiles['xpt'][0].write_text(ret)

        return ret

    def test_0_basic_boot(self):
        """Basic boot test"""
        # Since test names are processed alphabetically this test should
        # get to run first, and if we fail then even if failfast is disabled
        # we will still terminate the test run.
        self.shouldStop = True

        results = self.runDosemu("version.bat")

        self.assertNotIn('Timeout', results)
        self.assertNotIn('NonZeroReturn', results)
        self.assertIn(self.version, results)


class MyTestResult(unittest.TextTestResult):

    with_color_terminal = False

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        if (self.stream.isatty() or environ.get("CI")) and not environ.get("NO_COLOR"):
            self.with_color_terminal = True

    def getDescription(self, test):
        if 'SubTest' in strclass(test.__class__):
            return str(test)
        return '%-80s' % test.shortDescription()

    def startTest(self, test):
        super().startTest(test)
        self.starttime = test.utcnow()

        name = test.id()
        test.logfiles = {
            'log': [test.topdir / str(name + ".log"), "dosemu.log"],
            'xpt': [test.topdir / str(name + ".xpt"), "expect.log"],
        }
        test.firstsub = True
        test.msg = None

    def _is_relevant_tb_level(self, tb):
        return ('__unittest' in tb.tb_frame.f_globals
            or '__common_framework' in tb.tb_frame.f_globals)

    def _count_relevant_tb_levels(self, tb):
        length = 0
        while tb and not self._is_relevant_tb_level(tb):
            length += 1
            tb = tb.tb_next
        return length

    def gather_info_for_failure(self, err, test):
        """Gather traceback, stdout, stderr, dosemu and expect logs"""

        TITLE_NAME_FMT = '{:^16}'
        TITLE_BANNER_FMT = '{:-^70}\n'
        STDOUT_LINE = '\nStdout:\n%s'
        STDERR_LINE = '\nStderr:\n%s'

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

        # Decode(maybe wrap) and add colour if the line is long
        maxLineLen = 0
        for l in msgLines:
            if len(l) > maxLineLen:
                maxLineLen = len(l)
        if maxLineLen > 120:
            n = list()
            for l in msgLines:
                lf = l.encode('utf-8').decode('unicode_escape')
                if self.with_color_terminal:
                    lf = lf.replace("FAIL:", f"{RED}FAIL{RESET}:")
                    lf = lf.replace("PASS:", f"{GREEN}PASS{RESET}:")
                    lf = lf.replace("OKAY:", f"{YELLOW}OKAY{RESET}:")
                    lf = lf.replace("INFO:", f"{LIGHT_BLUE}INFO{RESET}:")
                n += [ lf, ]
            msgLines = n

        # Stdout, Stderr
        if self.buffer:
            output = stdout.getvalue()
            error = stderr.getvalue()
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
            if not environ.get("CI"):
                msgLines.append("Further info in file '%s'\n" % l[0])
                continue

            msgLines.append("::group::%s\n" % l[0])

            name = TITLE_NAME_FMT.format(l[1])
            msgLines.append(TITLE_BANNER_FMT.format(name))
            try:
                cnt = l[0].read_text()
                if not cnt.endswith('\n'):
                    cnt += '\n'
                msgLines.append(cnt)
            except FileNotFoundError:
                msgLines.append("File not present\n")

            msgLines.append("::endgroup::\n")

        return ''.join(msgLines)

    def addExpectedFailure(self, test, err):
        super().addExpectedFailure(test, err)
        for _, l in test.logfiles.items():
            l[0].unlink(missing_ok=True)

    def addFailure(self, test, err):
        if self.showAll:
            if self.with_color_terminal:
                self.stream.writeln(f"{RED}FAIL{RESET}")
            else:
                self.stream.writeln("FAIL")
        elif self.dots:
            self.stream.write('F')
            self.stream.flush()
        self.failures.append((test, self.gather_info_for_failure(err, test)))
        self._mirrorOutput = True
        for _, l in test.logfiles.items():
            if not l[0].is_file():
                l[0].write_text("Failed before any output was received\n")
        if getattr(test, 'shouldStop', None) is not None:
            self.shouldStop = test.shouldStop
        if getattr(self, 'failfast', False):
            self.stop()

    def addSuccess(self, test):
        super(unittest.TextTestResult, self).addSuccess(test)
        if self.showAll:
            if self.starttime is not None:
                duration = test.utcnow() - self.starttime
                msg = (" " + test.msg) if test.msg else ""
                self.stream.writeln("ok ({:>6.2f}s){}".format(duration.total_seconds(), msg))
            else:
                self.stream.writeln("ok")
        elif self.dots:
            self.stream.write('.')
            self.stream.flush()

        for _, l in test.logfiles.items():
            l[0].unlink(missing_ok=True)

    def addSubTest(self, test, subtest, err):
        super(MyTestResult, self).addSubTest(test, subtest, err)
        if err is not None:
            if test.firstsub:
                test.firstsub = False
                self.stream.writeln("FAIL (one or more subtests)")
            self.stream.writeln("    %-76s ... FAIL" % subtest._subDescription())


class MyTestRunner(unittest.TextTestRunner):
    resultclass = MyTestResult

    def run(self, test):
        self.stream.flush()
        result = super().run(test)
        self.stream.flush()
        return result


def main(argv=None):
    if version_info < (3, 2):
        exit("Python 3.2 or later is required.")
    failfast = not (environ.get("NO_FAILFAST"))
    unittest.main(testRunner=MyTestRunner, argv=argv, verbosity=2, failfast=failfast)


def main_setup(cases):
    if len(argv) > 1:
        if argv[1] == "--help":
            print(("Usage: %s [--help | --get-test-binaries | " +
                   "--list-attrs | --list-cases | --list-tests] | " +
                   "[--require-attr=STRING [TestCase1 .. TestCaseN]] | " +
                   "[TestCase[.testname] ...]") % argv[0])
            exit(0)

        if argv[1] == "--get-test-binaries":
            get_test_binaries()
            exit(0)

        if argv[1] == "--list-attrs":
            for c in cases:
                print(c.__name__)
                print("    %s" % str(c.attrs))
            exit(0)

        if argv[1] == "--list-cases":
            for c in cases:
                print(c.__name__)
            exit(0)

        if argv[1] == "--list-tests":
            common = set()
            children = {}
            for c in cases:
                parent, child = c.getTestsDefinedIn()
                common.update(parent)
                children[c.__name__] = child

            print("Common:")
            for tname in sorted([t[0] for t in common]):
                print("    %s" % tname)

            for cname, tests in children.items():
                print("%s:" % cname)
                for tname in sorted([t[0] for t in tests]):
                    print("    %s" % tname)
            exit(0)

        x = re.match(r"^--require-attr=(\S+).*$", argv[1])
        if x:
            attr = x.groups()[0]
            tests = []
            # Check if we have valid cases following on the command line
            xcases = [c for c in cases if c.__name__ in argv[2:]]
            if len(xcases):
                cases = xcases
            for c in cases:
                tests += [n for n in c.getTestnames_with_attr(attr)]
            if not len(tests):
                exit("No tests found with attr, was it correct? See --help")
            return [argv[0],] + tests

        # Need to expand multiple TestCases on command line
        cnames = [ c.__name__ for c in cases]
        cvalid = []
        tvalid = []
        for a in argv[1:]:
            if a in cnames:  # Test case
                cvalid += [a,]

            elif '.' in a:  # Potentially it's a fully qualified test name
                cname, tname = a.split('.')
                for c in cases:
                    if c.__name__ == cname and hasattr(c, tname):
                        tvalid += [a,]

            else:  # Maybe we are a test name, and qualify if so
                for c in cases:
                    if hasattr(c, a):
                        tvalid += [c.__name__ + '.' + a,]

        if len(cvalid) == 0 and len(tvalid) == 0:
            exit("No valid testcases or testnames on command line")

        if len(cvalid) and len(tvalid):
            exit("Invalid to have both testcases and testnames on command line")

        return [argv[0],] + sorted(cvalid + tvalid)

    else:  # No args, so expand all tests from all cases
        tests = []
        for c in cases:
            tests += [c.__name__ + '.' + t[0] for t in c.getTests()]

        return [argv[0],] + tests

