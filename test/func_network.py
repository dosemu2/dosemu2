from http.server import BaseHTTPRequestHandler, HTTPServer
from sys import stderr
import multiprocessing as mp
from random import randbytes

from common_framework import setup_tap_interface, teardown_tap_interface

# Note: this is the address assigned to libvirt's virbr0 bridge interface
HOST = '192.168.122.1'
PORT = 8080

NE2000_IOBASE = 0x300
NE2000_IRQ = 5


class MyServer(BaseHTTPRequestHandler):
    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-type", "application/octet-stream")
        self.send_header("Content-Length", str(len(self.content)))
        self.end_headers()
        self.wfile.write(self.content)
        raise KeyboardInterrupt     # Close afterwards

    def log_message(self, format, *args):
        pass                        # Quieten stderr


def little_webserver(content):
    with HTTPServer((HOST, PORT), MyServer) as ws:
        try:
            ws.RequestHandlerClass.content = content
            ws.serve_forever()
        except KeyboardInterrupt:
            pass


def network_mtcp(self, tipo, driver):
    content = randbytes(1024*1024)

    setup_tap_interface(self)
    self.addCleanup(teardown_tap_interface, self)

    ctx = mp.get_context('spawn')
    p = ctx.Process(target=little_webserver, args=(content,), daemon=True)
    p.start()

    self.unTarOrSkip("TEST_MTCP.tar", [
        ("dhcp.exe", "3658786197def91dce139f0d2aa1524ba409e426"),
        ("htget.exe", "26e72660d62a274577e874ba68bd6af03962fcce"),
        ("ping.exe", "6f8814e9ef4366b0a7597f005d1aad587eb6fc93"),
        ("pkttool.exe", "66a26d7fc18c0102ba6672c37fb6b04a027dc6ee"),
    ])

    if tipo in ['pkt',]:
        # Note: Only load the DOS NE2000 driver if you are going to use it
        #       as it interferes with the builtin packet driver's receipt
        #       of packets.
        if driver == 'ne2000':
            self.unTarOrSkip("TEST_CRYNWR.tar", [
                ("ne2000.com", "297cf2bc04aded016bb8051a9d2b061940c39569"),
            ])
            pktintr = '0x62'
            mtcpcfg = f'c:\\ne2000 {pktintr} {NE2000_IRQ} {NE2000_IOBASE:#x}'
        else:
            pktintr = '0x60'
            mtcpcfg = ''
    else:  # ndis
        self.unTarOrSkip("TEST_MSCLIENT.tar", [
            ("protman.dos", "e548281e0f477149c91bbd88c5b76decd289923c"),
            ("protman.exe", "d8f2f178cca189ba62c45851e5e3ec6c67385364"),
            ("netbind.com", "74a08230b1ac1e84a12db37031f5719165b3ff48"),
        ])
        self.unTarOrSkip("TEST_DISPKT.tar", [
            ("dis_pkt.dos", "634837f6789c929b19cc39fec3c9912278c1e59a"),
        ])
        pktintr = '0x62'
        mtcpcfg = 'c:\\netbind'
        #mtcpcfg += f'\npkttool stats {pktintr}'

        # Modify the config.sys
        contents = (self.workdir / self.confsys).read_text()
        contents += 'device=c:\\protman.dos /I:c:\\\n'

        if driver == 'ne2000':
            self.unTarOrSkip("TEST_DP83905.tar", [
                ("nic2000.dos", "56efe6eca4ca8973bb5af370b951bfdb07911da7"),
            ])
            contents += 'device=c:\\nic2000.dos\n'
            self.mkfile("protocol.ini", f"""\
[protman]
  DriverName = PROTMAN$

[NIC2000]
  DriverName = NIC2000$
  IOBASE = {NE2000_IOBASE:#x}
  INTERRUPT = {NE2000_IRQ}

[PKTDRV]
  DriverName = PKTDRV$
  Bindings = NIC2000
  INTVEC = {pktintr}
""", newline="\r\n")

        elif driver == 'builtin':

            contents += 'device=c:\\dosemu\\ndis.sys\n'
            self.mkfile("protocol.ini", f"""\
[PROTMAN]
  DriverName = PROTMAN$

[DE2NDIS]
  DriverName = DE2NDIS$

[PKTDRV]
  DriverName = PKTDRV$
  Bindings = DE2NDIS
  INTVEC = {pktintr}
""", newline="\r\n")


        elif driver == 'pktndis':
            self.unTarOrSkip("TEST_PKTNDIS.tar", [
                ("pktndis.dos", "7e91d874c8f2fba2f7e7bf1cb0836d178303b9a8"),
            ])
            contents += 'device=c:\\pktndis.dos\n'
            self.mkfile("protocol.ini", f"""\
[PROTMAN]
  DriverName = PROTMAN$

[PKTNDIS]
  DriverName = PKTNDIS$     ; Mandatory, must be PKTNDIS$
  interrupt = 0x60          ; Use the built in packet driver
  hide = 0                  ; Don't hide that Packet Driver interface

[PKTDRV]
  DriverName = PKTDRV$
  Bindings = PKTNDIS
  INTVEC = {pktintr}
""", newline="\r\n")

        else:
            assert ValueError(f'Invalid driver type {driver}')

        contents += 'device=c:\\dis_pkt.dos\n'
        self.mkfile(self.confsys, contents, newline="\r\n")

    self.mkfile("mtcp.cfg", f"""\
packetint {pktintr}
hostname dosemu
dhcp_lease_request_secs 3600
dhcp_lease_threshold 360
""", newline="\r\n")

    self.mkfile("testit.bat", f"""\
{mtcpcfg}
set MTCPCFG=c:\\mtcp.cfg
dhcp
htget -v -o test.fil http://{HOST}:{PORT}/test.fil
rem end
""", newline="\r\n")

    config = f"""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_vnet = "tap"
$_tapdev = "tap0"
"""

    if tipo == 'ndis' and driver == 'builtin':
        config += '$_pktdriver = (off)\n$_ndis = (on)\n'
    else:
        config += '$_pktdriver = (on)\n'

    if driver == 'ne2000':  # pkt or ndis driver
        config += f"""\
$_sound = (off)
$_ne2k = (on)
$_ne2k_iobase = ({NE2000_IOBASE:#x})
$_ne2k_irq = ({NE2000_IRQ})
"""

    results = self.runDosemu("testit.bat", config=config, timeout=60)

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

    self.assertEqual(len(content), len(tbytes))
    self.assertEqual(content, tbytes)
