from shutil import copy
from subprocess import check_call
import re

def ndis_mac_driver(self, path):
    """Exercises the built-in NDIS MAC driver.

    test/ndis holds a stand-in for PROTMAN.DOS and one for an NDIS
    protocol, so that the driver can be tested without any of the
    proprietary NDIS network clients. The protocol binds to the MAC,
    sends an ARP request and waits for slirp to answer it, which makes
    the reply come back as an indication.
    """
    if not getattr(self.__class__, 'ndismade', False):
        check_call(["make", "--quiet", "-C", "test/ndis", "clean", "all"])
        self.__class__.ndismade = True

    edir = self.topdir / "test" / "ndis"
    copy(edir / "tprotman.sys", self.workdir / "tprotman.sys")
    copy(edir / "tndisapp.com", self.workdir / "tndisapp.com")

    # the MAC driver has to be loaded after the protocol manager
    # Modify the config.sys
    contents = (self.workdir / self.confsys).read_text()
    contents = re.sub(r"(devicehigh=(c:\\)?dosemu\\cdrom.sys)", r"\1\ndevice=\2tprotman.sys\ndevice=\2dosemu\\ndis.sys", contents)
    self.mkfile(self.confsys, contents, newline="\r\n")

    self.mkfile("testit.bat", """\
tndisapp %s
rem end
""" % ("nochain" if path == "lookahead" else ""), newline="\r\n")

    results = self.runDosemu("testit.bat", timeout=30, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_ndis = (on)
$_pktdriver = (off)
$_vnet = "slirp"
""")

    if "unable to open the network link" in self.boot_log():
        self.skipTest("no networking available")

    self.assertIn("bound to the MAC", results)
    self.assertIn("ARP request sent", results)
    # 'C' is ReceiveChain, 'L' is ReceiveLookahead + TransferData
    self.assertIn("got a frame via %s" % ("L" if path == "lookahead" else "C"),
                  results)
    self.assertIn("PASS", results)
    self.assertNotIn("FAIL", results)
