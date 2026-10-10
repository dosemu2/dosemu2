#!/usr/bin/env python3

"""Regression test for the int 10h teletype and scroll in packed-pixel modes.

A 256 colour VESA mode is a linear frame buffer that is much larger than the
64K window at 0xa000 - 640x400 already needs 250K - so the BIOS has to carry
the offset in more than sixteen bits.  It did not, which broke every scroll
in such a mode and hung the whole emulator on a full screen clear.

It needs no DOS distribution and no display: the probe is assembled on the
spot and handed to dosemu2 as its command interpreter, and the video
emulation runs against SDL's dummy driver.  The probe prints more lines
than the screen holds, dumps the frame buffer back over the VESA window
and then clears the screen.
"""

import unittest

from common_framework import (BaseTestCase, DOSEMU_CONF_DEFAULT,
                              main, main_setup, mark)

# mode 0x100 is 640x400x256: 80x25 cells of 8x16, four 64K banks
WIDTH = 640
CELL_W = 8
CELL_H = 16
COLS = 80
ROWS = 25
NLINES = 32
RUN_TIMEOUT = 45

# Mode 0x100 needs 250K of video memory, so say how much there is rather
# than leave it to whatever the default happens to be: the mode is then
# always there, and its absence is a failure rather than something to skip.
CONF = DOSEMU_CONF_DEFAULT + '$_vmemsize = (4096)\n'

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

class VesaTestCase(BaseTestCase, unittest.TestCase):

    attrs = {'video'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "Vesa"
        # the probe is the command interpreter, so there is no DOS
        # distribution to unpack
        cls.tarfile = ""

    test_0_basic_boot = None

    def runProbe(self):
        """Run the probe once and leave its two files on drive C."""
        home = self.imagedir / "home"
        home.mkdir()

        # dosemu2 takes its command interpreter from DOSEMU2_COMCOM_DIR
        self.mkcom_with_nasm("command", PROBE)

        self.runDosemuRaw(("-S",), config=CONF, timeout=RUN_TIMEOUT,
                          env={
                              "HOME": str(home),
                              "DOSEMU2_COMCOM_DIR": str(self.workdir),
                              # the video emulation has to be a real one,
                              # the display does not
                              "SDL_VIDEODRIVER": "dummy",
                          })

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

    @mark('video')
    def test_vesa_teletype_scroll(self):
        """the teletype and the scroll reach the right lines of a VESA mode"""
        self.runProbe()

        # a build without SDL has no packed-pixel video at all, which is the
        # one thing worth skipping over
        if "VID: initializing SDL plugin" not in self.boot_log():
            self.skipTest("this build has no SDL plugin")

        fbname = self.workdir / "vesafb.bin"
        self.assertTrue(fbname.exists(),
                        "no frame buffer came back: the probe found no VESA "
                        "mode 0x100, or never ran")
        fb = fbname.read_bytes()
        self.assertGreaterEqual(len(fb), WIDTH * CELL_H * ROWS,
                                "the frame buffer came back short: %d bytes "
                                "of %d" % (len(fb), WIDTH * CELL_H * ROWS))

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
        markname = self.workdir / "vesamark.txt"
        self.assertTrue(markname.exists(), "no marker file")
        self.assertEqual("CLEARED", markname.read_text().strip(),
                         "clearing the screen never returned")


if __name__ == "__main__":
    cases = [
        VesaTestCase,
    ]
    main(main_setup(cases))
