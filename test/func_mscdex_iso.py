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
