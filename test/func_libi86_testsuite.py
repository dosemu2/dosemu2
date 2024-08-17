
import re

from shutil import copy
from subprocess import check_call, check_output, CalledProcessError, DEVNULL, TimeoutExpired
from os import environ


TESTSUITE = "/usr/ia16-elf/libexec/libi86/tests/testsuite"


def libi86_create_items(testcase):
    if environ.get("SKIP_EXPENSIVE"):
        print('\nlibi86-testsuite-ia16-elf is expensive - skipping')
        return

    # Enumerate the tests
    tests = []
    try:
        listing = check_output([TESTSUITE, '--list'])
    except FileNotFoundError:
        print('\nlibi86-testsuite-ia16-elf not installed - skipping')
        return
    for l in listing.split(b'\n'):
        # b'  12: bios.h.at:83       _bios_equiplist'
        t = re.search(r"\s*(\d+): (.+):\d+\s+([^']+)", str(l))
        if t:
            tests += [t.groups(),]

    def create_test(test):
        def do_test_libi86(self):
            libi86_test_item(self, test[0])
        docstring = """libi86 item % 3s %s""" % (test[0], test[2])
        setattr(do_test_libi86, '__doc__', docstring)
        setattr(do_test_libi86, 'libi86test', True)
        return do_test_libi86

    # Insert each test into the testcase
    for test in tests:
        name = 'test_libi86_item_%03d' % int(test[0])
        setattr(testcase, name, create_test(test))

    testcase.attrs += ['libi86test',]


def libi86_test_item(self, test):
    self.mkfile("dosemu.conf", """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""", dname=self.imagedir, mode="a")

    build = self.imagedir / "libi86-test"
    build.mkdir()

    dosemu = self.topdir / "bin" / "dosemu"
    options = '-f {0}/dosemu.conf -n --Fimagedir {0} -o {1}'.format(self.imagedir, self.logfiles['log'][0])
    args = [
        '--x-installcheck',
        '--x-test-underlying',
        '--x-with-dosemu=%s' % dosemu,
        '--x-with-dosemu-options=%s' % options,
    ]

    # Do just one
    try:
        starttime = self.utcnow()
        check_call([TESTSUITE, *args, test[0]], cwd=build, timeout=60, stdout=DEVNULL, stderr=DEVNULL)
        self.duration = self.utcnow() - starttime
    except CalledProcessError:
        raise self.failureException("Test error") from None
    except TimeoutExpired:
        raise self.failureException("Test timeout") from None
    finally:
        #  The libi86 test suite has its own log file called 'testsuite.log',
        #  so we will present it as our usual expect log
        logfile = build / "tests" / "testsuite.log"
        if logfile.is_file():
            copy(logfile, self.logfiles['xpt'][0])
            self.logfiles['xpt'][1] = "testsuite.log"
