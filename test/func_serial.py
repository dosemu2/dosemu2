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
