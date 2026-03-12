import re
import os

from os import environ
from shutil import copy
from subprocess import check_call, check_output, CalledProcessError, DEVNULL, TimeoutExpired
from sys import stderr

from common_framework import DOSEMU_CONF_DEFAULT, maybeFailure

TESTSUITE = "/usr/ia16-elf/libexec/libi86/tests/testsuite"

WHITELIST = [104, 105, 106, 107, 108, 109, 110]


def libi86_create_items(testcase):
    if environ.get("SKIP_EXPENSIVE"):
        stderr.write('\n\nlibi86-testsuite-ia16-elf is expensive - skipping\n')
        stderr.flush()
        return

    # Enumerate the tests
    tests = []
    try:
        listing = check_output([TESTSUITE, '--list'])
    except FileNotFoundError:
        stderr.write('\n\nlibi86-testsuite-ia16-elf not installed - skipping\n')
        stderr.flush()
        return
    for l in listing.split(b'\n'):
        # b'  12: bios.h.at:83       _bios_equiplist'
        t = re.search(r"\s*(\d+): (.+):\d+\s+([^']+)", str(l))
        if t:
            tests += [t.groups(),]

    def create_test(num, oname):
        def do_test_libi86(self):
            libi86_test_item(self, num)
        docstring = f"""libi86 item {num: 3d} {oname}"""
        setattr(do_test_libi86, '__doc__', docstring)
        setattr(do_test_libi86, 'libi86test', True)
        if num in WHITELIST:
            return maybeFailure(do_test_libi86)
        else:
            return do_test_libi86

    # Insert each test into the testcase
    for test in tests:
        num = int(test[0])
        tname = f'test_libi86_item_{num:03d}'
        oname = test[2]
        setattr(testcase, tname, create_test(num, oname))

    testcase.attrs.add('libi86test')


def libi86_test_item(self, num):
    self.mkfile("dosemu.conf", DOSEMU_CONF_DEFAULT, dname=self.imagedir)

    os.umask(0)
    build = self.imagedir / "libi86-test"
    build.mkdir()

    options = '-f {0}/dosemu.conf -n --Fimagedir {0} -o {1}'.format(self.imagedir, self.logfiles['log'][0])
    if environ.get("NO_KVM", '0') == '1':
        options += " -z 0"

    args = [
        '--x-installcheck',
        '--x-test-underlying',
        '--x-with-dosemu=%s' % self.dosemu,
        '--x-with-dosemu-options=%s' % options,
    ]

    # Do just one
    try:
        starttime = self.utcnow()
        check_call([TESTSUITE, *args, str(num)], cwd=build, timeout=120, stdout=DEVNULL, stderr=DEVNULL)
        self.duration = self.utcnow() - starttime

    except (TimeoutExpired, CalledProcessError) as e:
        #  The libi86 test suite has its own log file called 'testsuite.log',
        #  so we will present it as our usual expect log
        logfile = build / "tests" / "testsuite.log"
        if logfile.is_file():
            copy(logfile, self.logfiles['xpt'][0])
            self.logfiles['xpt'][1] = "testsuite.log"

        #  It also has some test directory specific files
        for tup in (
                ("a.c", "c"),
                ("a.log", "out")):
            fil = build / "tests" / "testsuite.dir" / f"{num:03d}" / tup[0]
            ext = tup[1]
            if fil.is_file():
                self.logfiles[ext] = [self.topdir / f"{self.id()}.{ext}", f"{fil.parent.name}/{fil.name}"]
                copy(fil, self.logfiles[ext][0])

        if isinstance(e, TimeoutExpired):
            raise self.failureException("Test timed out, output files may be truncated") from None
        elif isinstance(e, CalledProcessError):
            raise self.failureException(f"Test failed with return code {e.returncode}") from None
