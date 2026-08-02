import socket
import threading
import time

from common_framework import DOSEMU_CONF_DEFAULT


def _vmodem_dial_data_exchange(self, family, host):
    # A DOS program dials a TCP peer through the vmodem (ATD) and data
    # flows both ways. The peer greets with a CP/M EOF-terminated line
    # (^Z ends the DOS `type`), then captures what DOS sends back.
    srv = socket.socket(family, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    try:
        srv.bind((host, 0))
    except OSError:
        srv.close()
        self.skipTest("cannot bind %s (address family unavailable)" % host)
    srv.listen(1)
    srv.settimeout(30)
    port = srv.getsockname()[1]
    received = []

    def peer():
        try:
            conn, _ = srv.accept()
        except (socket.timeout, OSError):
            return
        data = b''
        try:
            conn.sendall(b'HELLO_FROM_PEER\r\n\x1a')
            conn.settimeout(15)
            while b'DATA_FROM_DOS' not in data:
                chunk = conn.recv(128)
                if not chunk:
                    break
                data += chunk
        except OSError:
            pass
        finally:
            received.append(data)
            conn.close()

    t = threading.Thread(target=peer)
    t.start()

    config = DOSEMU_CONF_DEFAULT
    config += """\
$_com1 = "vmodem"
"""
    self.mkfile("testit.bat", f"""\
echo atd"{host} {port}" > com1
type com1
echo DATA_FROM_DOS > com1
rem end
""", newline="\r\n")

    try:
        results = self.runDosemu("testit.bat", config=config)
    finally:
        t.join(timeout=30)
        srv.close()

    self.assertIn('HELLO_FROM_PEER', results)
    self.assertTrue(received and b'DATA_FROM_DOS' in received[0],
                    f'peer did not receive DOS data: {received}')


def serial_vmodem_dial_data_exchange(self):
    _vmodem_dial_data_exchange(self, socket.AF_INET, '127.0.0.1')


def serial_vmodem_dial_data_exchange_ipv6(self):
    _vmodem_dial_data_exchange(self, socket.AF_INET6, '::1')


def serial_simple_read_echo(self):
    config = DOSEMU_CONF_DEFAULT
    config += """\
$_com1 = "exec '/usr/bin/echo -n -e hello_world\\\\n\\\\x1a'"
"""
    self.mkfile("testit.bat", """\
type com1
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn('hello_world', results)


def serial_simple_write_file(self):
    ofile = self.workdir / 'a.txt'

    config = DOSEMU_CONF_DEFAULT
    config += f"""\
$_com1 = "wrfile {ofile}"
"""
    self.mkfile("testit.bat", """\
echo hello > com1
rem end
""", newline="\r\n")

    self.runDosemu("testit.bat", config=config)
    text = ofile.read_text()
    self.assertIn('hello', text)


def lpt_simple_write_pipe(self):
    ofile = self.workdir / 'a.txt'

    config = DOSEMU_CONF_DEFAULT
    config += f"""\
$_lpt1 = "cat - > {ofile}"
"""
    self.mkfile("testit.bat", """\
echo hello > lpt1
rem end
""", newline="\r\n")

    self.runDosemu("testit.bat", config=config)
    try:
        text = ofile.read_text()
        self.assertIn('hello', text)
    except FileNotFoundError:
        self.fail(f"file {ofile} was not created")
