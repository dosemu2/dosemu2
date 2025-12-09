#!/usr/bin/python3

from subprocess import check_call

from common_framework import BaseTestCase, main, main_setup, mark, KNOWNFAIL
from common_os import ppdosgit

from func_build_freecom import build_freecom
from func_cpu_trap_flag import cpu_trap_flag
from func_cpu_methods import cpu_create_items
from fpu.qemu import fpu_create_items


class OurTestCase(BaseTestCase):

    def test_build_freecom(self):
        """Build FreeCOM"""
        build_freecom(self)

    @mark('cputest')
    def test_cpu_trap_flag(self):
        """CPU Trap Flag"""
        cpu_trap_flag(self)


class KVMTestCase(ppdosgit(OurTestCase, {
        "test_fpu_f2xm1_kvm_sim": KNOWNFAIL,
        "test_fpu_fisttp_kvm_jit": KNOWNFAIL,
        "test_fpu_fisttp_kvm_sim": KNOWNFAIL,
        "test_fpu_fp_exceptions_kvm_jit": KNOWNFAIL,
        "test_fpu_fp_exceptions_kvm_sim": KNOWNFAIL,
        "test_fpu_fyl2x_kvm_sim": KNOWNFAIL,
        "test_fpu_fyl2xp1_kvm_sim": KNOWNFAIL,
        r"test_fpu_fprem_.*":KNOWNFAIL,
    })):
    use_cpu = 'kvm'


class EMUTestCase(ppdosgit(OurTestCase, {
        "test_fpu_f2xm1_sim_sim": KNOWNFAIL,
        "test_fpu_fisttp_jit_jit": KNOWNFAIL,
        "test_fpu_fisttp_sim_sim": KNOWNFAIL,
        "test_fpu_fp_exceptions_jit_jit": KNOWNFAIL,
        "test_fpu_fp_exceptions_sim_sim": KNOWNFAIL,
        "test_fpu_fyl2x_sim_sim": KNOWNFAIL,
        "test_fpu_fyl2xp1_sim_sim": KNOWNFAIL,
        r"test_fpu_fprem_.*":KNOWNFAIL,
    })):
    use_cpu = 'emu'


if __name__ == '__main__':

    # Make tests here so that we see any failures
    check_call(["make", "--quiet", "-C", "test/cpu", "clean", "all"])
    check_call(["make", "--quiet", "-C", "test/fpu", "clean", "all"])

    cases = [
        EMUTestCase,
        KVMTestCase,
    ]

    # Dynamically create tests
    for tc in cases:
        cpu_create_items(tc)
        fpu_create_items(tc)

    xargv = main_setup(cases)
    main(xargv)
