#!/usr/bin/env python3

"""A VBE mode that has no reserved field reports position 0 for it.

VBE leaves the position of the reserved field undefined when its size is
zero, and cards report both as zero.  vgaemu used to compute the position
from the sum of the colour mask sizes and report it whatever the size came
out as, so every 16 bpp mode said "0 bits at 16" and every 24 bpp mode "0
bits at 24".

Allegro compares both numbers against the ones it wants for the colour
depth it was asked for, and for 16 and 24 bpp it wants 0 and 0.  So it
dropped every truecolour mode dosemu2 offered and fell back to 8 bpp,
which is how an Allegro game came to show a 24 bpp image as colour noise
(issue #1516).

The probe asks for the mode information of one 16 bpp and one 24 bpp mode
and writes both reserved fields out.  Nothing here needs a DOS
distribution: the probe is assembled on the spot and handed to dosemu2 as
its command interpreter.
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

RUN_TIMEOUT = 120
REPORT = re.compile(r"m16=([0-9a-f]{8}) m24=([0-9a-f]{8})")

MODE_16BPP = 0x111			# 640x480, 5:6:5
MODE_24BPP = 0x112			# 640x480, 8:8:8

PROBE = r"""
; Ask for the mode information of a 16 and a 24 bpp mode and write out the
; mode attributes together with the size and the position of the reserved
; field.
	org	0x100
	bits	16

start:
	push	cs
	pop	ds
	push	cs
	pop	es

	mov	cx, MODE16
	mov	bp, out16
	call	query
	mov	cx, MODE24
	mov	bp, out24
	call	query

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

; cx = mode, bp = where the eight characters go
query:
	push	cx
	mov	di, block		; a mode we cannot query must not
	mov	cx, 0x80		; leave the previous one's data behind
	xor	ax, ax
	cld
	rep	stosw
	pop	cx

	mov	di, block
	mov	ax, 0x4f01
	int	0x10
	cmp	ax, 0x004f
	je	.ok
	mov	word [block], 0		; not supported: no attributes at all
.ok:
	mov	ax, [block]		; ModeAttributes
	mov	di, bp
	call	hex16
	mov	al, [block + 0x25]	; RsvdMaskSize
	lea	di, [bp + 4]
	call	hex8
	mov	al, [block + 0x26]	; RsvdFieldPos
	lea	di, [bp + 6]
	call	hex8
	ret

; ax = word, ds:di = the four characters to write it to
hex16:
	push	ax
	mov	al, ah
	call	hex8
	pop	ax
	add	di, 2
	call	hex8
	ret

; al = byte, ds:di = the two characters to write it to
hex8:
	mov	ah, al
	shr	al, 4
	call	hexdig
	mov	[di], al
	mov	al, ah
	and	al, 0x0f
	call	hexdig
	mov	[di + 1], al
	ret

hexdig:
	add	al, '0'
	cmp	al, '9'
	jbe	.done
	add	al, 'a' - '0' - 10
.done:
	ret

fname		db	'vbemode.txt', 0
msg		db	'm16='
out16		db	'????????'
		db	' m24='
out24		db	'????????'
		db	13, 10, '$'
msglen		equ	$ - msg
msg_done	db	'PROBE DONE', 13, 10, '$'
block		times	0x100 db 0
"""


class VbeModeInfoTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU",
                                      cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        if which("nasm") is None:
            raise unittest.SkipTest("nasm not installed")

        cls.workdir = Path(mkdtemp(prefix="vbemode."))
        src = cls.workdir / "probe.asm"
        src.write_text(PROBE)
        # dosemu2 looks for command.com in DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin",
                        "-DMODE16=%#x" % MODE_16BPP,
                        "-DMODE24=%#x" % MODE_24BPP,
                        "-o", str(cls.workdir / "command.com"), str(src)])
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)

        cls.conf = cls.workdir / "dosemu.conf"
        cls.conf.write_text('$_cpu_vm = "emulated"\n')
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
        """Run the probe once and hand back both modes' numbers."""
        if self.__class__.report is not None:
            return self.__class__.report

        logfile = self.workdir / "dosemu.log"
        outfile = self.workdir / "dosemu.out"
        home = self.workdir / "home"
        rmtree(str(home), ignore_errors=True)
        home.mkdir()
        result = home / ".dosemu" / "drive_c" / "vbemode.txt"

        env = dict(environ)
        env.update({
            "HOME": str(home),
            "SDL_VIDEODRIVER": "dummy",     # vgaemu, but no display
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
        if not result.exists():
            where = self.keepLog(log)
            self.fail("the probe wrote nothing; log kept at %s" % where)

        text = result.read_text(errors="replace")
        m = REPORT.search(text)
        if not m:
            where = self.keepLog(log + text)
            self.fail("the probe wrote nothing we can read; kept at %s" % where)

        def split(s):
            return int(s[0:4], 16), int(s[4:6], 16), int(s[6:8], 16)

        self.__class__.report = (split(m.group(1)), split(m.group(2)), log)
        return self.__class__.report

    def check(self, mode, attr, size, pos, log):
        if not attr & 1:
            self.skipTest("this build does not offer mode %#x" % mode)
        if size:
            self.skipTest("mode %#x has a reserved field of %d bits"
                          % (mode, size))
        if pos:
            where = self.keepLog(log)
            self.fail("mode %#x reports a reserved field of 0 bits at "
                      "position %d, and a field of no bits has to sit at 0: "
                      "a program matching both numbers drops the mode. "
                      "Log kept at %s" % (mode, pos, where))

    def test_16bpp_mode_reserved_field(self):
        """a 5:6:5 mode reports no reserved field at position 0"""
        (attr, size, pos), _, log = self.probe()
        self.check(MODE_16BPP, attr, size, pos, log)

    def test_24bpp_mode_reserved_field(self):
        """an 8:8:8 mode reports no reserved field at position 0"""
        _, (attr, size, pos), log = self.probe()
        self.check(MODE_24BPP, attr, size, pos, log)


if __name__ == "__main__":
    unittest.main()
