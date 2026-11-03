#!/usr/bin/env python3

"""CR11 bit 7 protects the CRTC timing registers, and software counts on it.

The VGA BIOS leaves the write protect bit of CR11 set, so CR0-CR7 are read
only until a program clears it.  The one exception is bit 4 of CR07, the
ninth bit of the line compare value: that bit stays writable, which is the
whole point of the arrangement.  A program that wants a split screen writes
CR07 to reach that bit and expects the vertical timing bits sitting in the
same register to be left alone.

vgaemu used to clear the protection on every mode set while loading a
register table that has it on, so such a write went through whole.  The
Catacomb Abyss (issue #910) writes CR07 = 0x01 to put its status bar on a
split screen, and that turned the display from 400 scan lines into 144: the
top of the view was stretched over the window and everything below it,
status bar included, was gone.

The probe needs a graphics backend to have CRTC ports at all, but not a
display, so it asks SDL for its dummy driver.  Nothing here needs a DOS
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
REPORT = re.compile(r"orig=([0-9a-f]{2}) prot=([0-9a-f]{2}) free=([0-9a-f]{2})")

PROBE = r"""
; Read CR07 as the BIOS leaves it, write it once with the protection on and
; once with the protection lifted, and write the three values to a file.
	org	0x100
	bits	16

CRTC	equ	0x3d4			; colour modes only

start:
	push	cs
	pop	ds

	mov	ax, 0x000d		; EGA 320x200, what the game uses
	int	0x10

	mov	al, 0x07
	call	crtc_read
	mov	[val_orig], al

; The line compare bit in CR07 is writable whatever the protection says,
; the timing bits in the same register are not.
	mov	al, 0x07
	mov	ah, 0x01
	call	crtc_write
	mov	al, 0x07
	call	crtc_read
	mov	[val_prot], al

; With the protection lifted the same write has to take effect whole.
	mov	al, 0x11
	call	crtc_read
	and	al, 0x7f
	mov	ah, al
	mov	al, 0x11
	call	crtc_write
	mov	al, 0x07
	mov	ah, 0x01
	call	crtc_write
	mov	al, 0x07
	call	crtc_read
	mov	[val_free], al

	mov	ax, 0x0003
	int	0x10

	mov	al, [val_orig]
	mov	di, out_orig
	call	hex8
	mov	al, [val_prot]
	mov	di, out_prot
	call	hex8
	mov	al, [val_free]
	mov	di, out_free
	call	hex8

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

; al = index, returns the value in al
crtc_read:
	mov	dx, CRTC
	out	dx, al
	inc	dx
	in	al, dx
	ret

; al = index, ah = value
crtc_write:
	mov	dx, CRTC
	out	dx, al
	inc	dx
	mov	al, ah
	out	dx, al
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

val_orig	db	0
val_prot	db	0
val_free	db	0

fname		db	'crtcprot.txt', 0
msg		db	'orig='
out_orig	db	'??'
		db	' prot='
out_prot	db	'??'
		db	' free='
out_free	db	'??'
		db	13, 10, '$'
msglen		equ	$ - msg
msg_done	db	'PROBE DONE', 13, 10, '$'
"""


class VgaCrtcProtectTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU",
                                      cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        if which("nasm") is None:
            raise unittest.SkipTest("nasm not installed")

        cls.workdir = Path(mkdtemp(prefix="crtcprot."))
        src = cls.workdir / "probe.asm"
        src.write_text(PROBE)
        # dosemu2 looks for command.com in DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.workdir / "command.com"), str(src)])
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
        """Run the probe once and hand back (orig, protected, unprotected)."""
        if self.__class__.report is not None:
            return self.__class__.report

        logfile = self.workdir / "dosemu.log"
        outfile = self.workdir / "dosemu.out"
        home = self.workdir / "home"
        rmtree(str(home), ignore_errors=True)
        home.mkdir()
        result = home / ".dosemu" / "drive_c" / "crtcprot.txt"

        env = dict(environ)
        env.update({
            "HOME": str(home),
            "SDL_VIDEODRIVER": "dummy",     # CRTC ports, but no display
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

        m = REPORT.search(result.read_text(errors="replace"))
        if not m:
            where = self.keepLog(log + result.read_text(errors="replace"))
            self.fail("the probe wrote nothing we can read; kept at %s" % where)

        self.__class__.report = (int(m.group(1), 16), int(m.group(2), 16),
                                 int(m.group(3), 16), log)
        return self.__class__.report

    def test_mode_set_leaves_the_timing_registers_protected(self):
        """writing CR07 changes the line compare bit and nothing else"""
        orig, prot, _, log = self.probe()
        if prot != (orig & 0xef):
            where = self.keepLog(log)
            self.fail("CR07 was 0x%02x after the mode set and 0x%02x after "
                      "writing 0x01 to it; the protection in CR11 should "
                      "have kept everything but bit 4, leaving 0x%02x. "
                      "Log kept at %s" % (orig, prot, orig & 0xef, where))

    def test_the_protection_can_be_lifted(self):
        """clearing CR11 bit 7 makes CR07 writable again"""
        _, _, free, log = self.probe()
        if free != 0x01:
            where = self.keepLog(log)
            self.fail("CR07 is 0x%02x after writing 0x01 to it with the "
                      "protection in CR11 cleared, and it should be 0x01: "
                      "the protection cannot be turned off. Log kept at %s"
                      % (free, where))


if __name__ == "__main__":
    unittest.main()
