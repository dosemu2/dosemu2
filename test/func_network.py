from http.server import BaseHTTPRequestHandler, HTTPServer
from sys import stderr
import multiprocessing as mp

from common_framework import setup_tap_interface, teardown_tap_interface

# Note: this is the address assigned to libvirt's virbr0 bridge interface
HOST = '192.168.122.1'
PORT = 8080

CONTENT = b"""\
This is very short string. Ideally we'd use the new random byte generator
in python 3.9, but not all test platforms have that yet(mine included)"""


class MyServer(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "application/octet-stream")
        self.end_headers()
        self.wfile.write(CONTENT)
        raise KeyboardInterrupt     # Close afterwards

    def log_message(self, format, *args):
        pass                        # Quieten stderr


def little_webserver():
    with HTTPServer((HOST, PORT), MyServer) as ws:
        try:
            ws.serve_forever()
        except KeyboardInterrupt:
            pass


def network_pktdriver_mtcp(self, driver):
    setup_tap_interface(self)
    self.addCleanup(teardown_tap_interface, self)

    ctx = mp.get_context('spawn')
    p = ctx.Process(target=little_webserver, daemon=True)
    p.start()

    self.unTarOrSkip("TEST_CRYNWR.tar", [
        ("ne2000.com", "297cf2bc04aded016bb8051a9d2b061940c39569"),
    ])

    self.unTarOrSkip("TEST_MTCP.tar", [
        ("dhcp.exe", "3658786197def91dce139f0d2aa1524ba409e426"),
        ("htget.exe", "26e72660d62a274577e874ba68bd6af03962fcce"),
        ("ping.exe", "6f8814e9ef4366b0a7597f005d1aad587eb6fc93"),
        ("pkttool.exe", "66a26d7fc18c0102ba6672c37fb6b04a027dc6ee"),
    ])

    # Note: Only load the DOS NE2000 driver if you are going to use it
    #       as it interferes with the builtin packet driver's receipt
    #       of packets.
    if driver == 'ne2000':
        pktintr = '0x61'
        mtcpcfg = 'c:\\ne2000 %s 10 0x310' % pktintr
    else:
        pktintr = '0x60'
        mtcpcfg = ''

    self.mkfile("mtcp.cfg", """\
packetint %s
hostname dosemu
dhcp_lease_request_secs 3600
dhcp_lease_threshold 360
""" % pktintr, newline="\r\n")

    self.mkfile("testit.bat", """\
%s
set MTCPCFG=c:\\mtcp.cfg
dhcp
htget -o test.fil http://%s:%d/test.fil
rem end
""" % (mtcpcfg, HOST, PORT), newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_pktdriver = (on)
$_vnet = "tap"
$_tapdev = "tap0"
""", timeout=30)

    p.join(timeout=45)
    if p.is_alive():
        stderr.write("Timeout on join() for little_webserver - killing ")
        stderr.flush()
        p.kill()
        p.join(timeout=5)
        if p.is_alive():
            stderr.write("Timeout on join() after kill() - aborting ")
            stderr.flush()
            raise mp.ProcessError
    p.close()

    try:
        tbytes = (self.workdir / 'test.fil').read_bytes()
    except FileNotFoundError:
        tbytes = b'File not found'

    self.assertEqual(CONTENT, tbytes)
