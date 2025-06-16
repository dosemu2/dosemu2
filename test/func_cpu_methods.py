from os import environ
from shutil import copy
from difflib import unified_diff


def _dotest(self, cpu_vm, cpu_vm_dpmi):

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

    if ('kvm' in cpu_vm or 'kvm' in cpu_vm_dpmi) and not self.have_kvm:
        self.skipTest("requires KVM")

    if cpu_vm == 'native' and not self.have_vm86:
        self.skipTest("native vm86() only on 32bit x86")

    if cpu_vm_dpmi == 'native' and environ.get("SKIP_NATIVE_DPMI"):
        self.skipTest("no native dpmi")

    edir = self.topdir / "test" / "cpu"

    # Native reference file is now checked in to git and will
    # only need to be updated if the test source changes. We open()
    # here without try/except as if it's missing we should 'ERROR'
    # not 'FAIL'
    reffile = edir / "reffile.log"
    refoutput = []
    with reffile.open("r") as f:
        refoutput = f.readlines()

    # DOS test binary is built as part of normal build process
    copy(edir / "dosbin.exe", self.workdir / "dosbin.exe")
    dosfile = self.workdir / "dosfile.log"

    # output from DOS under test
    self.mkfile("testit.bat", """\
dosbin --common-tests > %s
rem end
""" % dosfile.name, newline="\r\n")

    self.runDosemu("testit.bat", timeout=20, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "%s"
$_cpu_vm_dpmi = "%s"
$_cpuemu = (%i)
$_ignore_djgpp_null_derefs = (off)
"""%(cpu_vm, cpu_vm_dpmi, cpu_emu))

    try:
        with dosfile.open("r") as f:
            dosoutput = f.readlines()
    except Exception as e:   # Ensure we 'FAIL' not 'ERROR'
        raise self.failureException(e) from None

    # Compare DOS output to reference file
    if dosoutput != refoutput:
        diff = unified_diff(refoutput, dosoutput, fromfile=reffile.name, tofile=dosfile.name)
        self.fail('differences detected\n' + ''.join(list(diff)))

TESTS = (
#   cpu (native, kvm, jit, sim),  dpmi(native, kvm, remote)

    ('native', 'native'), #  CPU native vm86(i386 only) + native DPMI
    ('kvm',    'native'), #  CPU KVM vm86 + native DPMI
    ('jit',    'native'), #  CPU JIT vm86 + native DPMI
    ('sim',    'native'), #  CPU simulated vm86 + native DPMI

    ('kvm',    'kvm'),    #  CPU KVM vm86 + KVM DPMI
    ('jit',    'kvm'),    #  CPU JIT vm86 + KVM DPMI
    ('sim',    'kvm'),    #  CPU simulated vm86 + KVM DPMI

    ('kvm',    'jit'),    #  CPU KVM vm86 + JIT DPMI
    ('jit',    'jit'),    #  CPU JIT vm86 + JIT DPMI

    ('kvm',    'sim'),    #  CPU KVM vm86 + simulated DPMI
    ('sim',    'sim'),    #  CPU simulated vm86 + simulated DPMI
)


def create_test(test):
    def do_test(self):
        _dotest(self, *test)

    if test[0] == 'native':
        d1 = 'native vm86(i386 only)'
    elif test[0] == 'sim':
        d1 = 'simulated vm86'
    else:
        d1 = '%s vm86' % test[0].upper()

    if test[1] == 'native':
        d2 = 'native DPMI';
    elif test[1] == 'sim':
        d2 = 'simulated DPMI'
    else:
        d2 = '%s DPMI' % test[1].upper()
    setattr(do_test, '__doc__', 'CPU %s + %s' % (d1, d2))
    setattr(do_test, 'cputest', True)
    return do_test


def cpu_create_items(testcase):
    # Insert each test into the testcase
    for test in TESTS:
        name = 'test_cpu_method_%s_%s' % test
        setattr(testcase, name, create_test(test))

    if 'cputest' not in testcase.attrs:
        testcase.attrs += ['cputest',]
