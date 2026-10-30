import tempfile

from common_framework import DOSEMU_CONF_DEFAULT


def mscdex_seek(self):
    # The seek request keeps its starting sector in a 4 byte field at
    # offset 20 of a 24 byte request block. The four bytes after the block
    # belong to the caller, and are all ones here. The helper answers AL=1
    # when the seek fails. The disc is a file in /tmp, which the sandbox
    # lets dosemu2 read.
    with tempfile.NamedTemporaryFile(suffix=".img") as img:
        config = DOSEMU_CONF_DEFAULT + '$_cdrom = "%s"\n' % img.name

        self.mkfile("testit.bat", """\
c:\\cdseek
rem end
""", newline="\r\n")

        self.mkcom_with_nasm("cdseek", r"""

bits 16
org 100h

    mov     di, req
    mov     ax, 0340h       ; AH=03h seek, AL=40h helper
    int     0e6h
    mov     dx, ok
    cmp     al, 1
    jne     show
    mov     dx, failed
show:
    mov     ah, 9
    int     21h
    ret

ok      db      'SEEK=ok$'
failed  db      'SEEK=failed$'
req     times 24 db 0
        dd      0ffffffffh
""")

        results = self.runDosemu("testit.bat", config=config)

    self.assertIn("SEEK=ok", results, "seek failed\n%s" % results)
