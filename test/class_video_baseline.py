#!/usr/bin/env python3

"""The video that has always worked, as a floor under the other cases.

Nothing here is about a bug.  It is what every other video case stands on:
that a probe really is booted as the command interpreter and gets to write
its report, that the two modes that have always worked come up, and that a
pixel drawn in one of them lands where it belongs.  When the cases above it
start failing all at once, this one says whether the scaffolding or the
thing under test is what broke.

The scaffolding itself is VideoProbeTestCase, in common_framework.py.
"""

import unittest

from common_framework import VideoProbeTestCase, mark

# The colour the baseline probe draws with, and where, well inside a
# 320x200 screen.
COLOUR = 0x2a
PIXEL_X = 0x40
PIXEL_Y = 0x20

# Where the pixel lands in a mode whose pixels are one byte each, and the
# modes the probe sets: the plain colour text mode everything starts in,
# and the plain 256 colour graphics mode.
PIXEL_AT = PIXEL_Y * 320 + PIXEL_X
TEXT_MODE = 0x03
GRAPHICS_MODE = 0x13

# the modes the probe sets, in the order it sets them: text, graphics, and
# back to text so it leaves a screen behind that dosemu2 can shut down
MODES = (TEXT_MODE, GRAPHICS_MODE, TEXT_MODE)

BASELINE_REPORT = "vidbase.txt"

BASELINE_PROBE = r"""
; Set the two modes that have always worked, say what the BIOS and the BDA
; think is up in each, and put one pixel on the screen through the BIOS to
; read it back twice: through the BIOS again, and out of the aperture.
	org	0x100
	bits	16
	cpu	386

COLOUR	equ	%(colour)#x
PIXEL_X	equ	%(x)d
PIXEL_Y	equ	%(y)d
PIXEL_AT equ	%(at)d

start:
	mov	al, %(text)#x
	call	setmode

	mov	al, %(gfx)#x
	call	setmode

	mov	ah, 0x0c		; one pixel, through the BIOS
	mov	al, COLOUR
	xor	bx, bx
	mov	cx, PIXEL_X
	mov	dx, PIXEL_Y
	int	0x10

	mov	si, s_pix
	call	puts
	mov	ah, 0x0d		; and back, through the BIOS
	xor	bx, bx
	mov	cx, PIXEL_X
	mov	dx, PIXEL_Y
	int	0x10
	call	puthex

	mov	si, s_mem
	call	puts
	push	es
	mov	ax, 0xa000		; and out of the aperture itself
	mov	es, ax
	mov	al, [es:PIXEL_AT]
	pop	es
	call	puthex
	mov	si, s_nl
	call	puts

	mov	al, %(text)#x		; leave a text screen behind
	call	setmode

	mov	ax, 0x3c00		; the report, for the test to read
	xor	cx, cx
	mov	dx, fname
	int	0x21
	jc	done
	mov	bx, ax
	mov	ah, 0x40
	mov	cx, [bufp]
	sub	cx, buf
	mov	dx, buf
	int	0x21
	mov	ah, 0x3e
	int	0x21

; and stay put: dosemu2 starts the command interpreter again when it exits,
; and a second run of the probe would paint over the screen the first one
; reported on.  The test ends the run itself.
done:
	mov	dx, s_done
	mov	ah, 0x09
	int	0x21
.spin:
	hlt
	jmp	.spin

; al = the mode to set.  Reports it, then what the BDA and the BIOS say.
setmode:
	mov	ah, 0x00
	int	0x10
	mov	bl, al			; what we asked for
	mov	si, s_set
	call	puts
	mov	al, bl
	call	puthex
	mov	si, s_bda
	call	puts
	push	ds
	xor	cx, cx
	mov	ds, cx
	mov	al, [0x449]		; BDA: the mode DOS looks at
	pop	ds
	call	puthex
	mov	si, s_bios
	call	puts
	mov	ah, 0x0f		; and the mode the BIOS owns up to
	int	0x10
	call	puthex
	mov	si, s_nl
	call	puts
	ret

; si -> an asciiz string, appended to the report
puts:
	push	ax
	push	di
	mov	di, [bufp]
.ch:
	mov	al, [si]
	or	al, al
	jz	.end
	mov	[di], al
	inc	di
	inc	si
	jmp	.ch
.end:
	mov	[bufp], di
	pop	di
	pop	ax
	ret

; al -> two hex digits in the report
puthex:
	push	ax
	push	bx
	push	di
	mov	bh, al
	mov	di, [bufp]
	mov	al, bh
	shr	al, 4
	call	.nib
	mov	al, bh
	and	al, 0x0f
	call	.nib
	mov	[bufp], di
	pop	di
	pop	bx
	pop	ax
	ret
.nib:
	add	al, '0'
	cmp	al, '9'
	jbe	.store
	add	al, 'a' - '0' - 10
.store:
	mov	[di], al
	inc	di
	ret

s_set	db	'set=', 0
s_bda	db	' bda=', 0
s_bios	db	' bios=', 0
s_pix	db	'pix=', 0
s_mem	db	' mem=', 0
s_nl	db	13, 10, 0
s_done	db	'VIDEOBASE$'
fname	db	'C:\%(report)s', 0
bufp	dw	buf
buf:
"""


class VideoBaselineTestCase(VideoProbeTestCase, unittest.TestCase):
    """One run of the baseline probe, read by all three tests."""

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "VideoBaseline"

    @mark('video')
    def test_a_probe_runs_as_the_command_interpreter(self):
        """dosemu2 boots the probe instead of DOS, and it gets to report"""
        lines = self.probe()
        self.assertEqual(len(lines), len(MODES) + 1,
                         "the probe reported %r" % (lines,))

    @mark('video')
    def test_the_bios_owns_up_to_the_mode_it_set(self):
        """the BDA and int 10h ah=0f both name the mode that was asked for"""
        got_modes = [l for l in self.probe() if "set" in l]
        self.assertEqual(len(got_modes), len(MODES),
                         "the probe reported %d of %d mode changes"
                         % (len(got_modes), len(MODES)))
        for want, got in zip(MODES, got_modes):
            with self.subTest(mode="0x%02x" % want):
                self.assertEqual(int(got["set"], 16), want,
                                 "the modes come in the order they were set")
                self.assertEqual(int(got["bda"], 16), want,
                                 "the BDA says mode 0x%s is up" % got["bda"])
                self.assertEqual(int(got["bios"], 16), want,
                                 "int 10h ah=0f says mode 0x%s is up"
                                 % got["bios"])

    @mark('video')
    def test_a_pixel_reaches_the_aperture(self):
        """a pixel drawn through the BIOS is in video memory where it belongs"""
        pix = [l for l in self.probe() if "pix" in l]
        self.assertEqual(len(pix), 1, "the probe reported no pixel")
        pix = pix[0]
        self.assertEqual(int(pix["pix"], 16), COLOUR,
                         "int 10h ah=0d read back 0x%s where ah=0c wrote "
                         "0x%02x" % (pix["pix"], COLOUR))
        self.assertEqual(int(pix["mem"], 16), COLOUR,
                         "the aperture holds 0x%s at offset %d, where the "
                         "pixel belongs in mode 0x%02x"
                         % (pix["mem"], PIXEL_AT, GRAPHICS_MODE))

    def probe(self):
        """Run the probe once and hand back its report, a dict per line."""
        if self.__class__.report is not None:
            self.relog()
            return self.__class__.report

        result = self.workdir / BASELINE_REPORT

        # the probe paints on a screen nobody reads and says what it saw in
        # a file on the host, so that file is what says the run is done
        def reported(out):
            return result.is_file() and \
                len(self.fields(result)) > len(MODES)

        self.runProbe(BASELINE_PROBE % {
                          "colour": COLOUR,
                          "x": PIXEL_X,
                          "y": PIXEL_Y,
                          "at": PIXEL_AT,
                          "text": TEXT_MODE,
                          "gfx": GRAPHICS_MODE,
                          "report": BASELINE_REPORT.upper(),
                      }, until=reported)

        self.assertTrue(result.is_file(), "the probe wrote nothing")
        self.__class__.report = self.fields(result)
        return self.__class__.report
