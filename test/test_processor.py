#!/usr/bin/python3

from common_framework import BaseTestCase, main, main_setup
from common_os import ppdosgit

from func_build_freecom import build_freecom
from func_cpu_trap_flag import cpu_trap_flag
from func_cpu_methods import cpu_create_items


class OurTestCase(BaseTestCase):

    def test_build_freecom(self):
        """Build FreeCOM"""
        build_freecom(self)

    def test_cpu_trap_flag(self):
        """CPU Trap Flag"""
        cpu_trap_flag(self)

    test_cpu_trap_flag.cputests=True


class KVMTestCase(ppdosgit(OurTestCase, { })):
    use_cpu = 'kvm'


class EMUTestCase(ppdosgit(OurTestCase, { })):
    use_cpu = 'emu'


if __name__ == '__main__':

    cases = [
        EMUTestCase,
        KVMTestCase,
    ]

    # Dynamically create tests
    for tc in cases:
        cpu_create_items(tc)

    xargv = main_setup(cases)
    main(xargv)
