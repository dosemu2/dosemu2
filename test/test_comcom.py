#!/usr/bin/python3

from common_framework import BaseTestCase, main, main_setup, mark
from common_os import ppdosgit

from func_build_freecom import build_freecom
from func_build_freedos import build_freedos
from func_build_pcmos import build_pcmos
from func_command_com_builtins import command_com_copy, command_com_keyword_exist
from func_command_com_cmdline_length import command_com_cmdline_length
from func_command_com_psp_checks import command_com_psp_fcbs

from func_comcom_internal import comcom_mem, comcom_r200fix


class OurTestCase(BaseTestCase):
    attrs = {'buildtest', 'inttest'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()

        if cls.__name__ == 'TestCase32':
            cls.commandcom_regex = r'(?m)^comcom.*dj32'
        elif cls.__name__ == 'TestCase64':
            cls.commandcom_regex = r'(?m)^comcom.*dj64'
        else:
            raise ValueError(f"TestCase suffix specifies which comcom version expected")

    def test_0_basic_boot(self):
        """Basic boot test"""
        # Since test names are processed alphabetically this test should
        # get to run first, and if we fail then even if failfast is disabled
        # we will still terminate the test run.
        self.shouldStop = True

        results = self.runDosemu("version.bat")

        self.assertNotIn('NonZeroReturn', results)
        self.assertIn(self.version, results)
        self.assertRegex(results, self.commandcom_regex)

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

    def test_command_cmdline_length_new_dos01(self):
        """Command.com cmdline length(127) (env var) 01"""
        command_com_cmdline_length(self, 'new_dos01')

    def test_command_cmdline_length_new_dos02(self):
        """Command.com cmdline length(150) (env var) 02"""
        command_com_cmdline_length(self, 'new_dos02')

    def test_command_cmdline_length_multiargs01(self):
        """Command.com cmdline length( 30) multiple args 01"""
        command_com_cmdline_length(self, 'multiarg01')

    def test_command_cmdline_length_singlearg01(self):
        """Command.com cmdline length( 60) single arg 01"""
        command_com_cmdline_length(self, 'singlearg01')

    def test_command_cmdline_length_singlearg02(self):
        """Command.com cmdline length(126) single arg 02"""
        command_com_cmdline_length(self, 'singlearg02')

    def test_command_copy(self):
        """Command.com command copy"""
        command_com_copy(self)

    def test_command_keyword_exist(self):
        """Command.com keyword exist"""
        command_com_keyword_exist(self)

    def test_command_psp_fcbs(self):
        """Command.com PSP FCB Values"""
        command_com_psp_fcbs(self)

    @mark('inttest')
    def test_comcom_mem(self):
        """Comcom mem report tool"""
        comcom_mem(self)

    @mark('inttest')
    def test_comcom_r200fix_real(self):
        """Comcom r200fix Real Mode"""
        comcom_r200fix(self, 'REAL')

    @mark('inttest')
    def test_comcom_r200fix_protected(self):
        """Comcom r200fix Protected Mode"""
        comcom_r200fix(self, 'PROTECTED')

class TestCase32(ppdosgit(OurTestCase, {})):
    pass

class TestCase64(ppdosgit(OurTestCase, {})):
    pass

if __name__ == '__main__':

    cases = [
        TestCase32, TestCase64,
    ]

    xargv = main_setup(cases)
    main(xargv)
