#!/usr/bin/python3

from common_framework import (BaseTestCase, main, main_setup, mark, acceptFailure,
                              KNOWNFAIL, UNSUPPORTED)
from common_os import ppdosgit

from func_build_freecom import build_freecom
from func_build_freedos import build_freedos
from func_build_pcmos import build_pcmos
from func_cpu_trap_flag import cpu_trap_flag
from func_cpu_methods import cpu_create_items
from func_fpu_bart_exceptions import fpu_bart_exceptions_fpex, fpu_bart_exceptions_fpexes
from fpu.qemu import fpu_create_items


class OurTestCase(BaseTestCase):

    @mark('buildtest')
    def test_build_freecom(self):
        """Build FreeCOM"""
        build_freecom(self)

    @mark('buildtest')
    def test_build_freedos(self):
        """Build FreeDOS kernel"""
        build_freedos(self)

    @mark('buildtest')
    def test_build_pcmos(self):
        """Build PC-MOS"""
        build_pcmos(self)

    @mark('fputest')
    def test_fpu_bart_exceptions_fpex(self):
        """FPU Exceptions (Bart) (fpex)"""
        fpu_bart_exceptions_fpex(self)

    @mark('fputest')
    def test_fpu_bart_exceptions_fpexes(self):
        """FPU Exceptions (Bart) (fpexes)"""
        fpu_bart_exceptions_fpexes(self)


class KVMTestCase(ppdosgit(OurTestCase, {
        "test_fpu_f2xm1_kvm_sim": KNOWNFAIL,
        "test_fpu_fisttp_kvm_jit": UNSUPPORTED,  # Requires Pentium 4 (SSE3)
        "test_fpu_fisttp_kvm_sim": UNSUPPORTED,  # Requires Pentium 4 (SSE3)
        "test_fpu_fp_exceptions_kvm_sim": KNOWNFAIL,
        "test_fpu_fprem_kvm_sim": KNOWNFAIL,
        "test_fpu_fyl2x_kvm_sim": KNOWNFAIL,
        "test_fpu_fyl2xp1_kvm_sim": KNOWNFAIL,
    })):
    use_cpu = 'kvm'

    @mark('cputest')
    @acceptFailure
    def test_cpu_trap_flag(self):
        """CPU Trap Flag"""
        cpu_trap_flag(self)


class EMUTestCase(ppdosgit(OurTestCase, {
        "test_fpu_f2xm1_sim_sim": KNOWNFAIL,
        "test_fpu_fisttp_jit_jit": UNSUPPORTED,  # Requires Pentium 4 (SSE3)
        "test_fpu_fisttp_sim_sim": UNSUPPORTED,  # Requires Pentium 4 (SSE3)
        "test_fpu_fp_exceptions_sim_sim": KNOWNFAIL,
        "test_fpu_fprem_sim_sim": KNOWNFAIL,
        "test_fpu_fyl2x_sim_sim": KNOWNFAIL,
        "test_fpu_fyl2xp1_sim_sim": KNOWNFAIL,
    })):
    use_cpu = 'emu'

    @mark('cputest')
    def test_cpu_trap_flag(self):
        """CPU Trap Flag"""
        cpu_trap_flag(self)


if __name__ == '__main__':

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
