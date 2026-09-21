#!/usr/bin/env python3

"""A JEMM client's EMS windows sit on the video aperture, and they have to
survive a video mode change.

The window array of a JEMM client covers 0xa0000 upwards and the client uses
all of it as plain memory, so the two never take turns: the client owns the
windows for as long as it is loaded.  Setting a graphics mode used to put
the video bank over them anyway, and nothing ever took that mapping back --
no mode change unmaps the bank, and vgaemu_unmap() is not even compiled in.
From then on the client's writes to those windows went to the screen and it
read zeroes back, until it happened to map the same window again.

The probe below is deliberately blunt: it writes through a window that sits
on the aperture without mapping it again first, because mapping is what used
to hide the fault.  It needs a graphics backend to have an aperture at all,
but not a display, so it asks SDL for its dummy driver.  Nothing here needs
a DOS distribution: the probe is assembled on the spot and handed to dosemu2
as its command interpreter.
"""

import re
import unittest

from common_framework import (BaseTestCase, DOSEMU_CONF_DEFAULT,
                              main, main_setup, mark)

# The window we write through, and the one we read the same page back
# through.  The first is on the video aperture, the second is well above it.
LOW_WINDOW = 1
HIGH_WINDOW = 18

RUN_TIMEOUT = 30
REPORT = re.compile(r"ctl=(\d) gfx=(\d)")

# $_jemm brings the VCPI page pool up too, and that has to fit below 16M
# together with the first megabyte and $_ext_mem
CONF = DOSEMU_CONF_DEFAULT + """\
$_ems = (8192)
$_jemm = (on)
$_ext_mem = (6144)
"""

PROBE = r"""
; Write through an EMS window that sits on the video aperture and check the
; bytes reached the page, reading the same logical page back through a
; window above the aperture.  Twice: once with nothing in between, once
; with a round trip through a graphics mode.
	org	0x100
	bits	16

LOW	equ	%(low)d
HIGH	equ	%(high)d
WORDS	equ	0x2000			; 16K, a whole window

start:
	mov	ah, 0x40		; is there an EMM at all
	int	0x67
	or	ah, ah
	jnz	no_emm

	mov	ah, 0x41		; the page frame
	int	0x67
	or	ah, ah
	jnz	no_emm
	mov	[frame], bx

	mov	ah, 0x43		; one page is all we need
	mov	bx, 1
	int	0x67
	or	ah, ah
	jnz	no_emm
	mov	[handle], dx

; with nothing in between, which is what says the probe itself works
	mov	al, LOW
	call	map_page
	mov	ax, 0x1111
	call	fill_low
	mov	al, HIGH
	call	map_page
	mov	ax, 0x1111
	call	check_high
	mov	[res_ctl], al

; and now with a graphics mode in between, and no mapping after it
	mov	al, LOW
	call	map_page
	mov	ax, 0x0013
	int	0x10
	mov	ax, 0x0003
	int	0x10
	mov	ax, 0x2222
	call	fill_low
	mov	al, HIGH
	call	map_page
	mov	ax, 0x2222
	call	check_high
	mov	[res_gfx], al

report:
	mov	al, [res_ctl]
	add	al, '0'
	mov	[out_ctl], al
	mov	al, [res_gfx]
	add	al, '0'
	mov	[out_gfx], al

	mov	ah, 0x3c
	xor	cx, cx
	mov	dx, fname
	int	0x21
	jc	done
	mov	bx, ax
	mov	dx, msg
	mov	cx, msglen
	mov	ah, 0x40
	int	0x21
	mov	ah, 0x3e
	int	0x21
; and stay put: dosemu2 starts the command interpreter again when it exits,
; and a second run of the probe would measure a screen the first one left
; behind.  The test ends the run itself.
done:
	mov	dx, msg_done
	mov	ah, 9
	int	0x21
.spin:
	hlt
	jmp	.spin

no_emm:
	mov	dx, msg_noemm
	mov	ah, 9
	int	0x21
	mov	ax, 0x4c01
	int	0x21

; map our one logical page into the physical page in al
map_page:
	mov	ah, 0x44
	xor	bx, bx
	mov	dx, [handle]
	int	0x67
	ret

; fill the low window with the word in ax
fill_low:
	push	ax
	mov	ax, [frame]
	add	ax, 0x400 * LOW
	mov	es, ax
	pop	ax
	xor	di, di
	mov	cx, WORDS
	cld
	rep	stosw
	ret

; 1 if the high window holds the word in ax all through, 0 if it does not
check_high:
	push	ax
	mov	ax, [frame]
	add	ax, 0x400 * HIGH
	mov	es, ax
	pop	ax
	xor	di, di
	mov	cx, WORDS
	cld
	repe	scasw
	jne	.bad
	mov	al, 1
	ret
.bad:
	xor	al, al
	ret

frame		dw	0
handle		dw	0
res_ctl		db	0
res_gfx		db	0

fname		db	'jemmwin.txt', 0
msg		db	'ctl='
out_ctl		db	'?'
		db	' gfx='
out_gfx		db	'?'
		db	13, 10, '$'
msglen		equ	$ - msg
msg_done	db	'PROBE DONE', 13, 10, '$'
msg_noemm	db	'NO EMM', 13, 10, '$'
"""


class JemmApertureTestCase(BaseTestCase, unittest.TestCase):
    """Both tests read one run, the way the terminal cases do."""

    attrs = {'video'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "JEMM aperture"
        # There is no DOS distribution to unpack
        cls.tarfile = ""
        cls.report = None
        cls.raw = None
        cls.bootlog = ""

    test_0_basic_boot = None

    def relog(self):
        """Give a test that took the cached run the logs of that run.

        The framework shows each test its own dosemu and output logs when
        it fails, so without this a second failure has nothing to show.
        """
        self.logfiles['xpt'][1] = "output.log"
        self.logfiles['xpt'][0].write_bytes(self.__class__.raw)
        self.logfiles['log'][0].write_text(self.__class__.bootlog)

    def probe(self):
        """Run the probe once and hand back (control, after a mode change)."""
        if self.__class__.report is not None:
            self.relog()
            return self.__class__.report

        # dosemu2 takes its command interpreter from DOSEMU2_COMCOM_DIR
        self.mkcom_with_nasm("command",
                             PROBE % {"low": LOW_WINDOW, "high": HIGH_WINDOW})
        home = self.imagedir / "home"
        home.mkdir()
        result = self.workdir / "jemmwin.txt"

        # -S for a backend with a video aperture, the dummy driver so that
        # it needs no display; the probe exits on its own and takes dosemu2
        # with it, so there is no marker to wait for
        raw = self.runDosemuRaw(("-S",), config=CONF, timeout=RUN_TIMEOUT,
                                env={
                                    "HOME": str(home),
                                    "DOSEMU2_COMCOM_DIR": str(self.workdir),
                                    "SDL_VIDEODRIVER": "dummy",
                                    "DISPLAY": "",
                                })

        log = self.boot_log()
        self.__class__.raw = raw
        self.__class__.bootlog = log
        if "initializing SDL plugin" not in log:
            self.skipTest("this build has no SDL plugin")
        if "NO EMM" in log or b"NO EMM" in raw:
            self.skipTest("this build has no EMS")
        self.assertTrue(result.is_file(), "the probe wrote nothing")

        m = REPORT.search(result.read_text(errors="replace"))
        self.assertIsNotNone(m, "the probe wrote nothing we can read")
        self.__class__.report = (int(m.group(1)), int(m.group(2)))
        return self.__class__.report

    @mark('video')
    def test_a_window_carries_what_is_written_to_it(self):
        """the probe's own check: no mode change, the bytes are simply there"""
        ctl, _ = self.probe()
        self.assertTrue(ctl, "a write through window %d never reached the "
                             "page it was mapped to, with nothing else "
                             "going on" % LOW_WINDOW)

    @mark('video')
    def test_a_graphics_mode_leaves_the_windows_alone(self):
        """the video bank does not take a window away from the client"""
        _, gfx = self.probe()
        self.assertTrue(gfx, "after a round trip through mode 0x13, a write "
                             "through window %d went somewhere other than "
                             "the page it is mapped to: the bank was put "
                             "over the window and never taken back"
                             % LOW_WINDOW)


if __name__ == "__main__":
    cases = [
        JemmApertureTestCase,
    ]
    main(main_setup(cases))
