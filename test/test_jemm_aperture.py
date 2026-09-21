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
import signal
import unittest

from os import environ, getpgid, killpg
from pathlib import Path
from shutil import which, rmtree
from subprocess import Popen, DEVNULL, check_call, CalledProcessError
from tempfile import mkdtemp
from time import sleep, monotonic

# The window we write through, and the one we read the same page back
# through.  The first is on the video aperture, the second is well above it.
LOW_WINDOW = 1
HIGH_WINDOW = 18

RUN_TIMEOUT = 120
REPORT = re.compile(r"ctl=(\d) gfx=(\d)")

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
done:
	mov	dx, msg_done
	mov	ah, 9
	int	0x21
	mov	ax, 0x4c00
	int	0x21

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


class JemmApertureTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU",
                                      cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        if which("nasm") is None:
            raise unittest.SkipTest("nasm not installed")

        cls.workdir = Path(mkdtemp(prefix="jemmwin."))
        src = cls.workdir / "probe.asm"
        src.write_text(PROBE % {"low": LOW_WINDOW, "high": HIGH_WINDOW})
        # dosemu2 looks for command.com in DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.workdir / "command.com"), str(src)])
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)

        cls.conf = cls.workdir / "dosemu.conf"
        cls.conf.write_text('$_jemm = (on)\n$_ems = (8192)\n')
        cls.report = None

    @classmethod
    def tearDownClass(cls):
        rmtree(str(cls.workdir), ignore_errors=True)

    def keepLog(self, text):
        """Leave the log where ci_test.sh collects it."""
        p = self.topdir / ("%s.%s.%s.log" % (Path(__file__).stem,
                                             type(self).__name__,
                                             self._testMethodName))
        p.write_text(text)
        return p

    def stop(self, dosemu):
        if dosemu.poll() is not None:
            return
        try:
            killpg(getpgid(dosemu.pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError):
            pass
        try:
            dosemu.wait(timeout=10)
        except Exception:
            try:
                killpg(getpgid(dosemu.pid), signal.SIGKILL)
            except (ProcessLookupError, PermissionError):
                pass

    def probe(self):
        """Run the probe once and hand back (control, after a mode change)."""
        if self.__class__.report is not None:
            return self.__class__.report

        logfile = self.workdir / "dosemu.log"
        outfile = self.workdir / "dosemu.out"
        home = self.workdir / "home"
        rmtree(str(home), ignore_errors=True)
        home.mkdir()
        result = home / ".dosemu" / "drive_c" / "jemmwin.txt"

        env = dict(environ)
        env.update({
            "HOME": str(home),
            "SDL_VIDEODRIVER": "dummy",     # an aperture, but no display
            "DOSEMU2_COMCOM_DIR": str(self.workdir),
        })
        env.pop("DISPLAY", None)

        with open(outfile, "wb") as out:
            # in its own session, because bin/dosemu is a wrapper script that
            # runs the real binary in a subshell: signalling the script alone
            # would leave dosemu2 behind
            dosemu = Popen([str(self.dosemu), "-S", "-n", "-q",
                            "-f", str(self.conf), "-o", str(logfile)],
                           env=env, stdout=out, stderr=out, stdin=DEVNULL,
                           start_new_session=True)
            try:
                deadline = monotonic() + RUN_TIMEOUT
                while monotonic() < deadline:
                    if dosemu.poll() is not None:
                        break
                    if "PROBE DONE" in outfile.read_text(errors="replace"):
                        break
                    # under the dummy driver the probe's own "done" goes to a
                    # screen nobody reads, so watch for the report instead
                    if result.exists() and REPORT.search(
                            result.read_text(errors="replace")):
                        break
                    sleep(0.5)
            finally:
                self.stop(dosemu)

        log = logfile.read_text(errors="replace") if logfile.exists() else ""
        log += outfile.read_text(errors="replace")
        if "initializing SDL plugin" not in log:
            self.skipTest("this build has no SDL plugin")
        if "NO EMM" in log:
            self.skipTest("this build has no EMS")
        if not result.exists():
            where = self.keepLog(log)
            self.fail("the probe wrote nothing; log kept at %s" % where)

        m = REPORT.search(result.read_text(errors="replace"))
        if not m:
            where = self.keepLog(log + result.read_text(errors="replace"))
            self.fail("the probe wrote nothing we can read; kept at %s" % where)

        self.__class__.report = (int(m.group(1)), int(m.group(2)), log)
        return self.__class__.report

    def test_a_window_carries_what_is_written_to_it(self):
        """the probe's own check: no mode change, the bytes are simply there"""
        ctl, _, log = self.probe()
        if not ctl:
            where = self.keepLog(log)
            self.fail("a write through window %d never reached the page it "
                      "was mapped to, with nothing else going on; log kept "
                      "at %s" % (LOW_WINDOW, where))

    def test_a_graphics_mode_leaves_the_windows_alone(self):
        """the video bank does not take a window away from the client"""
        _, gfx, log = self.probe()
        if not gfx:
            where = self.keepLog(log)
            self.fail("after a round trip through mode 0x13, a write through "
                      "window %d went somewhere other than the page it is "
                      "mapped to: the bank was put over the window and never "
                      "taken back; log kept at %s" % (LOW_WINDOW, where))


if __name__ == "__main__":
    unittest.main(verbosity=2)
