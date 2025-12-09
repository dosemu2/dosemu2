
from shutil import copy
from os import environ

CTESTS = [
    'f2xm1',
    'fbstp',
    'fisttp',
    'fldcst',
    'fp-exceptions',
    'fpatan',
    'fprem',
    'fscale',
    'fxam',
    'fxtract',
    'fyl2x',
    'fyl2xp1',
    'pseudo-denormal',
    'snan-convert',
]

EMU_TESTS = (
    ('native', 'native'), #  CPU native vm86(i386 only) + native DPMI

    ('jit',    'native'), #  CPU JIT vm86 + native DPMI
    ('sim',    'native'), #  CPU simulated vm86 + native DPMI

    ('jit',    'jit'),    #  CPU JIT vm86 + JIT DPMI
    ('sim',    'sim'),    #  CPU simulated vm86 + simulated DPMI
)

KVM_TESTS = (
    ('kvm',    'native'), #  CPU KVM vm86 + native DPMI
    ('kvm',    'kvm'),    #  CPU KVM vm86 + KVM DPMI
    ('kvm',    'jit'),    #  CPU KVM vm86 + JIT DPMI
    ('kvm',    'sim'),    #  CPU KVM vm86 + simulated DPMI

    ('jit',    'kvm'),    #  CPU JIT vm86 + KVM DPMI
    ('sim',    'kvm'),    #  CPU simulated vm86 + KVM DPMI
)


def _dotest(self, test, cpu_vm, cpu_vm_dpmi):

    if (('jit' in cpu_vm and 'sim' in cpu_vm_dpmi) or
            ('sim' in cpu_vm and 'jit' in cpu_vm_dpmi)):
        raise ValueError("Invalid JIT/SIM combination")

    if ('sim' in cpu_vm) or ('sim' in cpu_vm_dpmi):
        cpu_emu = 1
    else:
        cpu_emu = 0

    if ('jit' in cpu_vm) or ('sim' in cpu_vm):
        cpu_vm = 'emulated'
    if ('jit' in cpu_vm_dpmi) or ('sim' in cpu_vm_dpmi):
        cpu_vm_dpmi = 'emulated'

    if 'native' in cpu_vm and not self.have_vm86:
        self.skipTest("requires 32bit kernel")

    if ('kvm' in cpu_vm or 'kvm' in cpu_vm_dpmi) and not self.have_kvm:
        self.skipTest("requires KVM")

    if cpu_vm_dpmi == 'native' and environ.get("SKIP_NATIVE_DPMI"):
        self.skipTest("no native dpmi")

    efil = self.topdir / "test" / "fpu" / ("test-i386-" + test + ".exe")

    # DOS test binary is built as part of normal build process
    copy(efil, self.workdir / "fputest.exe")

    self.mkfile("testit.bat", """\
c:\\fputest
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "%s"
$_cpu_vm_dpmi = "%s"
$_cpuemu = (%i)
$_ignore_djgpp_null_derefs = (off)
""" % (cpu_vm, cpu_vm_dpmi, cpu_emu))

    self.assertNotIn("FAIL:", results)
    self.assertIn("PASS:", results)


def create_test(test):
    def do_test(self):
        _dotest(self, *test)

    if test[1] == 'native':
        d1 = 'native vm86(i386 only)'
    elif test[1] == 'sim':
        d1 = 'simulated vm86'
    else:
        d1 = '%s vm86' % test[1].upper()

    if test[2] == 'native':
        d2 = 'native DPMI';
    elif test[2] == 'sim':
        d2 = 'simulated DPMI'
    else:
        d2 = '%s DPMI' % test[2].upper()
    setattr(do_test, '__doc__', 'FPU %s %s + %s' % (test[0], d1, d2))
    setattr(do_test, 'fputest', True)
    return do_test


def fpu_create_items(testcase):
    tests = KVM_TESTS if testcase.use_cpu == 'kvm' else EMU_TESTS

    # Insert each test into the testcase
    for ctest in CTESTS:
        for test in tests:
            t = create_test((ctest, *test))
            if t:
                name = 'test_fpu_%s_%s_%s' % (ctest.replace('-', '_'), *test)
                setattr(testcase, name, t)

    testcase.attrs.add('fputest')
