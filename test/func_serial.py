from common_framework import DOSEMU_CONF_DEFAULT


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


def serial_nullmm_loopback(self):
    # Two COM ports wired to each other: what DOS writes on one it
    # reads back on the other. The payload (with CR LF ^Z, 31 bytes)
    # is under RX_BUFFER_SIZE (128) so add_buf never drops, and over
    # TX_QUEUE_THRESHOLD (14) so the copy crosses the flow-control
    # regime: DOS is single-tasking, so nothing drains the peer until
    # the copy has completed. Both markers asserted separately, so a
    # truncated transfer fails on the tail marker instead of as an
    # opaque expect timeout.
    head = "NULLMM_BEGIN"
    tail = "NULLMM_END"
    payload = head + "_data_" + tail

    config = DOSEMU_CONF_DEFAULT
    config += """\
$_com1 = "nullmodem 2"
$_com2 = "nullmodem 1"
"""
    # ^Z terminates the `type`, as DOS has no other end-of-input here.
    self.mkfile("payload.txt", payload + "\r\n\x1a", newline="")
    self.mkfile("testit.bat", """\
copy /b payload.txt com1
type com2
rem end
""", newline="\r\n")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn(head, results)
    self.assertIn(tail, results)
