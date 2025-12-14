
import re
import unittest

from os import environ
from pathlib import Path
from shutil import copy

SYSTYPE_DRDOS_ENHANCED = "Enhanced DR-DOS"
SYSTYPE_DRDOS_ORIGINAL = "Original DR-DOS"
SYSTYPE_DRDOS_OLD = "Old DR-DOS"
SYSTYPE_PCDOS_NEW = "New PC-DOS"
SYSTYPE_PCDOS_OLD = "Old PC-DOS"
SYSTYPE_MSDOS_NEW = "New MS-DOS"
SYSTYPE_MSDOS_INTERMEDIATE = "Newer MS-DOS"
SYSTYPE_MSDOS_OLD = "Old MS-DOS"
SYSTYPE_MSDOS_NEC = "NEC MS-DOS"
SYSTYPE_FRDOS_OLD = "Old FreeDOS"
SYSTYPE_FRDOS_NEW = "FreeDOS"
SYSTYPE_FDPP = "FDPP"


def drdos701(baseclass, actions):

    class DRDOS701TestCase(baseclass, unittest.TestCase):
        # OpenDOS 7.01

        @classmethod
        def setUpClass(cls):
            super(DRDOS701TestCase, cls).setUpClass()
            cls.version = "Caldera OpenDOS 7.01"
            cls.prettyname = "DR-DOS-7.01"
            cls.files = [
                ("ibmbio.com", "61211eb63329a67fdd9d336271f06e1bfdab2b6f"),
                ("ibmdos.com", "52e71c8e9d74100f138071acaecdef4a79b67d3c"),
                ("command.com", "4bc38f973b622509aedad8c6af0eca59a2d90fca"),
                ("share.exe", "10f2c0e2cabe98617faa017806c80476b3b6c1e1"),
            ]
            cls.systype = SYSTYPE_DRDOS_ORIGINAL
            cls.autoexec = "dautoemu.bat"
            cls.confsys = "dconfig.sys"
            cls.bootblocks = [
                ("boot-306-4-17.blk", "1151ab9a3429163ac3ddf55b88d81359cb6975e5"),
                ("boot-615-4-17.blk", "a18ee96e63e384b766bafc4ff936e4087c31bf59"),
                ("boot-900-15-17.blk", "2ea4ea747f6e62a8ea46f14f6c9af1ad6fd0126b"),
            ]
            cls.images = [
                ("boot-floppy.img", "d38fb2dba30185ce510cf3366bd71a1cbc2635da"),
            ]
            cls.actions = actions

            cls.setUpClassPost()

        def setUpDosAutoexec(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.autoexec).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            self.mkfile(self.autoexec, contents, newline="\r\n")

        def setUpDosConfig(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.confsys).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

        def setUpDosVersion(self):
            self.mkfile("version.bat", "ver\r\nrem end\r\n")

    return DRDOS701TestCase


def frdos120(baseclass, actions):

    class FRDOS120TestCase(baseclass, unittest.TestCase):

        @classmethod
        def setUpClass(cls):
            super(FRDOS120TestCase, cls).setUpClass()
            cls.version = "FreeDOS kernel 2042"
            cls.prettyname = "FR-DOS-1.20"
            cls.files = [
                ("kernel.sys", "0709f4e7146a8ad9b8acb33fe3fed0f6da9cc6e0"),
                ("command.com", "0733db7babadd73a1b98e8983c83b96eacef4e68"),
                ("share.com", "cadc29d49115cb3a250f90921cca345e7c427464"),
            ]
            cls.systype = SYSTYPE_FRDOS_NEW
            cls.autoexec = "fdautoem.bat"
            cls.confsys = "fdconfig.sys"
            cls.bootblocks = [
                ("boot-302-4-17.blk", "8b5cfda502e59b067d1e34e993486440cad1d4f7"),
                ("boot-603-4-17.blk", "5c89a0c9c20ba9d581d8bf6969fda88df8ab2d45"),
                ("boot-900-15-17.blk", "523f699a79edde098fceee398b15711fac56a807"),
            ]
            cls.images = [
                ("boot-floppy.img", "c3faba3620c578b6e42a6ef26554cfc9d2ee3258"),
            ]
            cls.actions = actions

            cls.setUpClassPost()

        def setUpDosAutoexec(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.autoexec).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            self.mkfile(self.autoexec, contents, newline="\r\n")

        def setUpDosConfig(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / "c" / self.confsys).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

    return FRDOS120TestCase


def frdos130(baseclass, actions):

    class FRDOS130TestCase(baseclass, unittest.TestCase):

        @classmethod
        def setUpClass(cls):
            super(FRDOS130TestCase, cls).setUpClass()
            cls.version = "FreeDOS kernel 2043"
            cls.prettyname = "FR-DOS-1.30"
            cls.files = [
                ("kernel.sys", "2bdf90c8bc8c0406cfa01349265bf782507af016"),
                ("command.com", "15abab3d3ee4a50449517131a13b2c5164610582"),
                ("share.com", "cadc29d49115cb3a250f90921cca345e7c427464"),
            ]
            cls.systype = SYSTYPE_FRDOS_NEW
            cls.autoexec = "fdautoem.bat"
            cls.confsys = "fdconfig.sys"
            cls.bootblocks = [
                ("boot-306-4-17.blk", "0092a320500d7a8359d40bddc48f592686745aed"),
                ("boot-615-4-17.blk", "2b757178c7ba97f8a439c83dc627d61c2d6b3cf6"),
                ("boot-900-15-17.blk", "8cd7adeff4a0265e8a8e20f7942672c677cbc891"),
            ]
            cls.images = [
                ("boot-floppy.img", "7b68b4dc2de5891bb3700816d8e1a323e8d150bb"),
            ]
            cls.actions = actions

            cls.setUpClassPost()

        def setUpDosAutoexec(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.autoexec).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            self.mkfile(self.autoexec, contents, newline="\r\n")

        def setUpDosConfig(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / "c" / self.confsys).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

    return FRDOS130TestCase


def frdosgit(baseclass, actions):

    class FRDOSGITTestCase(baseclass, unittest.TestCase):

        @classmethod
        def setUpClass(cls):
            super(FRDOSGITTestCase, cls).setUpClass()
            cls.version = "DOS version 7.10"
            cls.prettyname = "FR-DOS-GIT"
            cls.tarfile = ""
            cls.systype = SYSTYPE_FRDOS_NEW
            cls.autoexec = "fdautoem.bat"
            cls.confsys = "fdconfig.sys"
            cls.bootblocks = [
            ]
            cls.images = [
            ]
            cls.actions = actions

            cls.setUpClassPost()

            # Check files under test exist, or skip the whole thing
            cls.files_to_copy = [
                Path(environ.get("FDOS_KERNEL_SYS", "../fdos/kernel.git/bin/kernel.sys")),
                Path(environ.get("FDOS_COMMAND_COM", "../fdos/freecom.git/command.com")),
                Path(environ.get("FDOS_SHARE_COM", "../fdos/share.git/src/share.com")),
            ]
            for f in cls.files_to_copy:
                if not f.is_file():
                    raise unittest.SkipTest("File '%s' not found" % f.name)

        def setUp(self):
            super(FRDOSGITTestCase, self).setUp()
            for f in self.files_to_copy:
                copy(f, self.workdir)

        def setUpDosAutoexec(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.autoexec).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            self.mkfile(self.autoexec, contents, newline="\r\n")

        def setUpDosConfig(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / "c" / self.confsys).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

    return FRDOSGITTestCase


def msdos622(baseclass, actions):

    class MSDOS622TestCase(baseclass, unittest.TestCase):

        @classmethod
        def setUpClass(cls):
            super(MSDOS622TestCase, cls).setUpClass()
            cls.version = "MS-DOS Version 6.22"
            cls.prettyname = "MS-DOS-6.22"
            cls.files = [
                ("io.sys", "d697961ca6edaf9a1aafe8b7eb949597506f7f95"),
                ("msdos.sys", "d6a5f54006e69c4407e56677cd77b82395acb60a"),
                ("command.com", "c2179d2abfa241edd388ab875cfabbac89fec44d"),
                ("share.exe", "9e7385cfa91a012638520e89b9884e4ce616d131"),
                ("dos/himem.sys", "fb41fbc1c4bdd8652d445055508bc8265bc64aea"),
            ]
            cls.systype = SYSTYPE_MSDOS_INTERMEDIATE
            cls.autoexec = "autoemu.bat"
            cls.bootblocks = [
                ("boot-306-4-17.blk", "d40c24ef5f5f9fd6ef28c29240786c70477a0b06"),
                ("boot-615-4-17.blk", "7fc96777727072471dbaab6f817c8d13262260d2"),
                ("boot-900-15-17.blk", "2a0ca1b87b82013fd417542a5ac28e965fb13e7a"),
            ]
            cls.images = [
                ("boot-floppy.img", "14b8310910bf19d6e375298f3b06da7ffdec9932"),
            ]
            cls.actions = actions

            cls.setUpClassPost()

        def setUpDosAutoexec(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.autoexec).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            self.mkfile(self.autoexec, contents, newline="\r\n")

        def setUpDosConfig(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / "c" / self.confsys).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

        def setUpDosVersion(self):
            self.mkfile("version.bat", "ver\r\nrem end\r\n")

    return MSDOS622TestCase


def msdos700(baseclass, actions):

    class MSDOS700TestCase(baseclass, unittest.TestCase):
        # badged Win95 RTM at winworldpc.com

        @classmethod
        def setUpClass(cls):
            super(MSDOS700TestCase, cls).setUpClass()
            cls.version = "Windows 95. [Version 4.00.950]"
            cls.prettyname = "MS-DOS-7.00"
            cls.files = [
                ("io.sys", "22924f93dd0f9ea6a4624ccdd1bbcdf5eb43a308"),
                ("msdos.sys", "f5d01c68d518f4b8b2482d3815af8bb88003831d"),
                ("command.com", "67696207c3963a0dc9afab8cf37dbdb966c1f663"),
                ("share.exe", "67d6524f7d700147013365e1656e93c842b9af07"),
                ("dos/himem.sys", "837993a3ec3edec102de9ab78a39ae049007d991"),
            ]
            cls.systype = SYSTYPE_MSDOS_NEW
            cls.autoexec = "autoemu.bat"
            cls.bootblocks = [
                ("boot-306-4-17.blk", "8c016e339ca6b8126fd2026ed3a7eeeb6cbb8903"),
                ("boot-615-4-17.blk", "b6fdddbfb37442a2762d5897de1aa7d7a694286a"),
                ("boot-900-15-17.blk", "8c1243481112f320f2a5f557f30db11174fe7e3d"),
            ]
            cls.images = [
                ("boot-floppy.img", ""),
            ]
            cls.actions = actions

            cls.setUpClassPost()

        def setUpDosAutoexec(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.autoexec).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            self.mkfile(self.autoexec, contents, newline="\r\n")

        def setUpDosConfig(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / "c" / self.confsys).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

        def setUpDosVersion(self):
            self.mkfile("version.bat", "ver\r\nrem end\r\n")

            # Disable the logo here or we get blank screen
            self.mkfile("msdos.sys", """
[Options]
BootGUI=0
Logo=0
""", newline="\r\n")

    return MSDOS700TestCase


def msdos710(baseclass, actions):

    class MSDOS710TestCase(baseclass, unittest.TestCase):
        # badged CDU (Chinese DOS Union) at winworldpc.com

        @classmethod
        def setUpClass(cls):
            super(MSDOS710TestCase, cls).setUpClass()
            cls.version = "MS-DOS 7.1 [Version 7.10.1999]"
            cls.prettyname = "MS-DOS-7.10"
            cls.files = [
                ("io.sys", "8c586b1bf38fc2042f2383ca873283a466be2f44"),
                ("msdos.sys", "cd1e6103ce9cdebbc7a5611df13ff4fbd5e2159c"),
                ("command.com", "f6547d81e625a784633c059e536e90ee45532202"),
            ]
            cls.systype = SYSTYPE_MSDOS_NEW
            cls.autoexec = "autoemu.bat"
            cls.bootblocks = [
                ("boot-306-4-17.blk", "0f520de6e2a33ef8fd336b2844957689fc1060e9"),
                ("boot-615-4-17.blk", "5e49a8ee7747191d87a2214cc0281736262687b9"),
                ("boot-900-15-17.blk", "2c29d06909c7d5ca46a3ca26ddde9287a11ef315"),
            ]
            cls.images = [
                ("boot-floppy.img", ""),
            ]

            cls.actions = actions

            cls.setUpClassPost()

        def setUpDosAutoexec(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / self.autoexec).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            self.mkfile(self.autoexec, contents, newline="\r\n")

        def setUpDosConfig(self):
            # Use the (almost) standard shipped config
            contents = (self.cmddir / "c" / self.confsys).read_text()
            contents = re.sub(r"[Dd]:\\", r"c:\\", contents)
            contents = re.sub(r"rem SWITCHES=/F", r"SWITCHES=/F", contents)
            self.mkfile(self.confsys, contents, newline="\r\n")

        def setUpDosVersion(self):
            self.mkfile("version.bat", "ver\r\nrem end\r\n")

            # Disable the logo here or we get blank screen
            self.mkfile("msdos.sys", """
[Options]
BootGUI=0
Logo=0
""", newline="\r\n")

    return MSDOS710TestCase


def ppdosgit(baseclass, actions):

    class PPDOSGITTestCase(baseclass, unittest.TestCase):

        @classmethod
        def setUpClass(cls):
            super(PPDOSGITTestCase, cls).setUpClass()
            cls.version = "FDPP kernel"
            cls.prettyname = "PP-DOS-GIT"
            cls.actions = actions

            # Use the default files that FDPP installed
            cls.tarfile = ""

            cls.systype = SYSTYPE_FDPP
            cls.autoexec = "fdppauto.bat"
            cls.confsys = "fdppconf.sys"

            cls.setUpClassPost()

    return PPDOSGITTestCase
