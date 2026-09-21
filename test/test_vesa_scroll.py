#!/usr/bin/python3

"""Regression test for the int 10h teletype and scroll in packed-pixel modes.

A 256 colour VESA mode is a linear frame buffer that is much larger than the
64K window at 0xa000 - 640x400 already needs 250K - so the BIOS has to carry
the offset in more than sixteen bits.  It did not, which broke every scroll
in such a mode and hung the whole emulator on a full screen clear.

The probe runs as command.com, so the test needs no DOS distribution; it
prints more lines than the screen holds, dumps the frame buffer back over
the VESA window and then clears the screen.
"""

import unittest
from os import environ, makedirs
from os.path import dirname, exists, join, realpath
from shutil import rmtree, which
from subprocess import DEVNULL, call, check_call
from tempfile import mkdtemp

TOPDIR = dirname(dirname(realpath(__file__)))
DOSEMU = environ.get("TEST_DOSEMU", join(TOPDIR, "bin", "dosemu"))

# mode 0x100 is 640x400x256: 80x25 cells of 8x16, four 64K banks
WIDTH = 640
CELL_W = 8
CELL_H = 16
COLS = 80
ROWS = 25
NLINES = 32

PROBE = r"""
org 0x100
bits 16

NLINES  equ %(nlines)d
NBANKS  equ 4

    mov ax, 0x4f02
    mov bx, 0x100
    int 0x10
    cmp ax, 0x004f
    jne .quit                   ; no such mode, leave both files absent
    xor si, si
.line:
    mov cx, si
    inc cx                      ; line n is n+1 hashes wide
.hash:
    push cx
    mov al, '#'
    call tty
    pop cx
    loop .hash
    mov al, 13
    call tty
    mov al, 10
    call tty
    inc si
    cmp si, NLINES
    jb .line

    mov dx, ffb
    call create
    jc .quit
    xor si, si
.bank:
    mov ax, 0x4f05
    xor bx, bx
    mov dx, si
    int 0x10
    cmp ax, 0x004f
    jne .dumped
    xor di, di
    mov cx, 256
.chunk:
    push cx
    push ds
    mov ax, 0xa000
    mov ds, ax
    mov dx, di
    mov cx, 256
    call wr
    pop ds
    add di, 256
    pop cx
    loop .chunk
    inc si
    cmp si, NBANKS
    jb .bank
.dumped:
    call close

    ; clearing the whole window is a scroll of no lines; it used to spin
    ; forever because the fill loop counted scan lines in a byte
    mov dx, fmark
    call create
    jc .quit
    mov ax, 0x0600
    mov bh, 0x07
    xor cx, cx
    mov dx, 0x184f              ; lower right corner, row 24 column 79
    int 0x10
    mov dx, mdone
    mov cx, mdone_len
    call wr
    call close
.quit:
    mov ax, 0x0003
    int 0x10
    mov ax, 0xffff              ; DOS_HELPER_REALLY_EXIT
    int 0xe6
    mov ax, 0x4c00
    int 0x21

create:
    mov ah, 0x3c
    xor cx, cx
    int 0x21
    jc .out
    mov [cs:fh], ax
    clc
.out:
    ret

wr:
    mov bx, [cs:fh]
    mov ah, 0x40
    int 0x21
    ret

close:
    mov bx, [cs:fh]
    mov ah, 0x3e
    int 0x21
    ret

tty:
    push si
    mov ah, 0x0e
    mov bx, 0x000f
    int 0x10
    pop si
    ret

fh:     dw 0
ffb:    db 'VESAFB.BIN', 0
fmark:  db 'VESAMARK.TXT', 0
mdone:  db 'CLEARED', 10
mdone_len equ $ - mdone
""" % {"nlines": NLINES}


class VesaScrollTestCase(unittest.TestCase):

    def setUp(self):
        if which("nasm") is None:
            self.skipTest("nasm not installed")
        if not exists(DOSEMU):
            self.skipTest("dosemu not built")
        self.workdir = mkdtemp(prefix="vesascroll")
        self.shelldir = join(self.workdir, "shell")
        self.homedir = join(self.workdir, "home")
        makedirs(self.shelldir)
        makedirs(self.homedir)
        self.drive_c = join(self.homedir, ".dosemu", "drive_c")

    def tearDown(self):
        rmtree(self.workdir, ignore_errors=True)

    def mkprobe(self):
        src = join(self.workdir, "probe.asm")
        with open(src, "w") as f:
            f.write(PROBE)
        # the probe stands in for the command interpreter
        check_call(["nasm", "-f", "bin", "-o",
                    join(self.shelldir, "command.com"), src])

    def rundosemu(self):
        env = dict(environ)
        # a real video emulation is needed, but not a real display
        env["SDL_VIDEODRIVER"] = "dummy"
        env["DOSEMU2_COMCOM_DIR"] = self.shelldir
        env["HOME"] = self.homedir
        args = [DOSEMU, "-S", "-o", join(self.workdir, "dosemu.log")]
        timeout = int(environ.get("DEFAULT_TIMEOUT", "15")) + 45
        try:
            return call(args, env=env, stdout=DEVNULL, stderr=DEVNULL,
                        timeout=timeout)
        except Exception:
            return None

    def cells(self, fb, row):
        """Number of character cells of the row that have any ink."""
        n = 0
        for col in range(COLS):
            for y in range(row * CELL_H, (row + 1) * CELL_H):
                off = y * WIDTH + col * CELL_W
                if any(fb[off:off + CELL_W]):
                    n += 1
                    break
        return n

    def test_vesa_teletype_scroll(self):
        self.mkprobe()
        self.rundosemu()

        fbname = join(self.drive_c, "vesafb.bin")
        if not exists(fbname):
            self.skipTest("VESA mode 0x100 unavailable in this build")
        with open(fbname, "rb") as f:
            fb = f.read()
        if len(fb) < WIDTH * CELL_H * ROWS:
            self.skipTest("VESA mode 0x100 unavailable in this build")

        # NLINES lines were printed on a screen of ROWS rows, so the screen
        # holds the last ROWS - 1 of them plus the empty line the cursor
        # sits on.  Line n is n + 1 cells wide.
        for row in range(ROWS - 1):
            line = NLINES - (ROWS - 1) + row
            self.assertEqual(line + 1, self.cells(fb, row),
                             "row %d holds the wrong line" % row)
        self.assertEqual(0, self.cells(fb, ROWS - 1),
                         "the cursor line is not empty")

        # and the full screen clear has to come back at all
        markname = join(self.drive_c, "vesamark.txt")
        self.assertTrue(exists(markname), "no marker file")
        with open(markname) as f:
            mark = f.read().strip()
        self.assertEqual("CLEARED", mark,
                         "clearing the screen never returned")


if __name__ == '__main__':
    unittest.main()
