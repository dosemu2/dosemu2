from shutil import copy
from subprocess import check_call
import re

def mstcp_htget(self, sockets):
    """Exercises htget against a stand-in for the MS TCP/IP sockets TSR.

    The real interface belongs to MS Network Client, which cannot be
    shipped, so test/mstcp holds a TCPDRV$ device that speaks the same
    interface and answers from canned data. That covers how htget uses
    the interface - the request blocks, the process id, the entry point
    - and nothing about the Microsoft implementation of it.

    With sockets=False nothing implements TCPDRV$, which is what every
    DOS without the MS stack looks like; htget has to say so and exit.
    """
    if not getattr(self.__class__, 'mstcpmade', False):
        check_call(["make", "--quiet", "-C", "test/mstcp", "clean", "all"])
        self.__class__.mstcpmade = True

    edir = self.topdir / "test" / "mstcp"
    copy(edir / "htget.com", self.workdir / "htget.com")

    if sockets:
        copy(edir / "tsockets.sys", self.workdir / "tsockets.sys")
        contents = (self.workdir / self.confsys).read_text()
        contents = re.sub(r"(devicehigh=(c:\\)?dosemu\\cdrom.sys)",
                          r"\1\ndevice=\2tsockets.sys", contents)
        self.mkfile(self.confsys, contents, newline="\r\n")

    self.mkfile("testit.bat", """\
htget
htget 10.0.2.2 80 /index.html
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", timeout=30, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

    # with no arguments at all htget only prints its usage
    self.assertIn("usage: htget", results)

    if not sockets:
        self.assertIn("TCPDRV$ not found", results)
        self.assertNotIn("connected", results)
        return

    self.assertIn("connected", results)
    self.assertIn("HTTP/1.0 200 OK", results)
    self.assertIn("hello dosemu", results)
    self.assertIn("received 77 bytes", results)
