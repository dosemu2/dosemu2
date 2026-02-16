from common_framework import IPROMPT


def floppy_img(self):
    # Note: image must have
    # dosemu directory
    # autoexec.bat
    # version.bat

    self.unTarImageOrSkip("boot-floppy.img")

    results = self.runDosemu("version.bat", config="""\
$_hdimage = ""
$_floppy_a = "boot-floppy.img"
$_bootdrive = "a"
""")

    self.assertIn(self.version, results)


def floppy_vfs(self):
    self.mkfile(self.confsys, """\
DOS=UMB,HIGH
lastdrive=Z
files=40
stacks=0,0
buffers=10
device=a:\\dosemu\\emufs.sys
device=a:\\dosemu\\umb.sys
devicehigh=a:\\dosemu\\ems.sys
devicehigh=a:\\dosemu\\cdrom.sys
install=a:\\dosemu\\emufs.com
shell=command.com /e:1024 /k %s
""" % self.autoexec, newline="\r\n")

    self.mkfile(self.autoexec, """\
prompt $P$G
path a:\\bin;a:\\gnu;a:\\dosemu
system -s DOSEMU_VERSION
@echo %s
""" % IPROMPT, newline="\r\n")

    results = self.runDosemu("version.bat", config="""\
$_hdimage = ""
$_floppy_a = "dXXXXs/c:fiveinch_360"
$_bootdrive = "a"
""")

    self.assertIn(self.version, results)
