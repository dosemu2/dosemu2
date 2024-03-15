
from shutil import copy
from os import uname, access, R_OK, W_OK

TESTS = [
    'test-i386-f2xm1',
    'test-i386-fbstp',
    'test-i386-fisttp',
    'test-i386-fldcst',
    'test-i386-fp-exceptions',
    'test-i386-fpatan',
    'test-i386-fscale',
    'test-i386-fxam',
    'test-i386-fxtract',
    'test-i386-fyl2x',
    'test-i386-fyl2xp1',
    'test-i386-pseudo-denormal',
    'test-i386-snan-convert',
]

#	test-i386-fprem

TYPES = [
#    (    "vm86",   "native", 0,  "VM86_NATIVE"), # CPU native vm86 + native DPMI (i386 only)
#    ("emulated",   "native", 0,   "JIT_NATIVE"), # CPU JIT vm86 + native DPMI
#    ("emulated",   "native", 1,   "SIM_NATIVE"), # CPU simulated vm86 + native DPMI
#    ("emulated",      "kvm", 0,   "JIT_KVM"), # CPU JIT vm86 + KVM DPMI
#    ("emulated",      "kvm", 1,   "SIM_KVM"), # CPU simulated vm86 + KVM DPMI
    ("emulated", "emulated", 0,   "JIT_JIT"), # CPU JIT vm86 + JIT DPMI
    ("emulated", "emulated", 1,   "SIM_SIM"), # CPU simulated vm86 + simulated DPMI
    (     "kvm",      "kvm", 0,   "KVM_KVM"), # CPU KVM vm86 + KVM DPMI
    (     "kvm",   "native", 0,   "KVM_NATIVE"),
#    (     "kvm", "emulated", 0,   "KVM_JIT"), # CPU KVM vm86 + JIT DPMI
#    (     "kvm", "emulated", 1,   "KVM_SIM"), # CPU KVM vm86 + simulated DPMI
]


def fpu_qemu_create_items(testcase):

    def create_test(test, dstr, x, y, z):
        def do_test_qemu(self):
            qemu_test_item(self, test, x, y, z)

        setattr(do_test_qemu, '__doc__', dstr)
        setattr(do_test_qemu, 'fputest', True)
        return do_test_qemu

    # Insert each test into the testcase
    for test in TESTS:
        sname = test.replace('test-i386-', '')
        for tipo in TYPES:
            dstr = """FPU (Qemu) %15s %11s""" % (sname, tipo[3])
            name = 'test_fpu_qemu_%s_%s' % (sname.replace('-', '_'), tipo[3])
            setattr(testcase, name, create_test(test, dstr, tipo[0], tipo[1], tipo[2]))

    testcase.attrs += ['fputest',]


def qemu_test_item(self, test, cpu_vm, cpu_vm_dpmi, cpu_emu):
    if ('kvm' in cpu_vm or 'kvm' in cpu_vm_dpmi) and not access("/dev/kvm", W_OK|R_OK):
        self.skipTest("requires KVM")
    if cpu_vm == 'vm86' and uname()[4] != 'i686':
        self.skipTest("native vm86() only on 32bit x86")

    efil = self.topdir / "test" / "fpu" / test

    # DOS test binary is built as part of normal build process
    copy(efil.with_suffix('.exe'), self.workdir / "fputest.exe")

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
