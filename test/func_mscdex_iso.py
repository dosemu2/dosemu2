import shutil
from subprocess import check_call, check_output

from common_framework import BINSDIR, DOSEMU_CONF_DEFAULT


def mscdex_nul_redirect_iso(self):
    # Redirecting to "nul:" from a CD drive served by SHSUCDX used to fail
    # with "Redirection to nul: failed".
    #
    # The disc contents are not used. mkisofs is also the name of the
    # genisoimage package's command.
    if not shutil.which("mkisofs"):
        self.skipTest("mkisofs not installed")

    pkg = self.topdir / BINSDIR / "shsucdx.zip"
    content = check_output(["unzip", "-p", "-C", str(pkg), "bin/shsucdx.com"])
    (self.workdir / "shsucdx.com").write_bytes(content)

    iso = self.imagedir / "nulcd.iso"
    cdroot = self.imagedir / "nulcd"
    cdroot.mkdir()
    (cdroot / "README.TXT").write_bytes(b"test\r\n")
    check_call(["mkisofs", "-quiet", "-o", str(iso), str(cdroot)])

    config = DOSEMU_CONF_DEFAULT + '$_cdrom = "%s"\n' % iso

    self.mkfile("testit.bat", """\
c:\\shsucdx /d:mscd0001 /l:g
g:
md c:\\nulmd >nul:
copy c:\\testit.bat c:\\nulcopy.bat >nul:
c:
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("Redirection to nul: failed", results)

    # the case of the names depends on the DOS
    names = {p.name.lower() for p in self.workdir.iterdir()}
    self.assertIn("nulmd", names, "md with >nul: did not run\n%s" % results)
    self.assertIn("nulcopy.bat", names,
                  "copy with >nul: did not run\n%s" % results)


def mscdex_iso_files(self):
    # Files on an ISO file given as $_cdrom must be readable through
    # SHSUCDX. They were not: landlock refused to open the ISO.
    if not shutil.which("mkisofs"):
        self.skipTest("mkisofs not installed")

    pkg = self.topdir / BINSDIR / "shsucdx.zip"
    content = check_output(["unzip", "-p", "-C", str(pkg), "bin/shsucdx.com"])
    (self.workdir / "shsucdx.com").write_bytes(content)

    iso = self.imagedir / "filescd.iso"
    cdroot = self.imagedir / "filescd"
    cdroot.mkdir()
    (cdroot / "HELLO.TXT").write_bytes(b"hello from the iso test\r\n")
    # ten sectors, so that more than one is read at a time
    want = bytes((i * 7 + (i >> 8) * 13 + 1) & 0xff for i in range(20000))
    (cdroot / "BIG.DAT").write_bytes(want)
    check_call(["mkisofs", "-quiet", "-o", str(iso), str(cdroot)])

    config = DOSEMU_CONF_DEFAULT + '$_cdrom = "%s"\n' % iso

    self.mkfile("testit.bat", """\
c:\\shsucdx /d:mscd0001 /l:g
g:
dir
type hello.txt
copy big.dat c:\\bigcopy.dat
c:
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("HELLO", results, "file not listed\n%s" % results)
    self.assertIn("hello from the iso test", results,
                  "file not read\n%s" % results)
    got = (self.workdir / "bigcopy.dat").read_bytes()
    self.assertEqual(got, want, "BIG.DAT differs\n%s" % results)
