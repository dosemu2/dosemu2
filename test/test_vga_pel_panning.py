#!/usr/bin/env python3

"""Horizontal pel panning moves the picture by single pixels.

Register 0x13 of the attribute controller is the fine half of the display
start: it drops up to seven pixels from the left of the first byte the CRTC
fetches, so the picture slides sideways a pixel at a time while the start
address moves it eight at a time.  Commander Keen and the rest of id's EGA
engines scroll that way -- they write the register on every frame -- and
vgaemu used to store the value and never look at it again, which is why
scrolling moved in jumps of eight pixels (issue #501).

The probe draws two one-pixel vertical lines 32 pixels apart on a screen
that is wider than the display, then flips the panning between 0 and 4 for
as long as it runs.  The test watches the framebuffer of an Xvfb that has
nothing else in it and looks at where the lines are: the distance between
the two lines gives the scale the window is drawn at, and the lines have to
take exactly two positions, four source pixels apart.
"""

import signal
import struct
import unittest

from os import environ, getpgid, killpg
from pathlib import Path
from shutil import which, rmtree
from subprocess import Popen, DEVNULL, check_call, CalledProcessError
from tempfile import mkdtemp
from time import sleep, monotonic

# what the probe alternates between, in source pixels
PAN = 4
# the two lines it draws, in source pixels
LINE_A = 16
LINE_B = 48

SHOTS = 14
SHOT_INTERVAL = 1.0

PROBE = r"""
; Draw two one-pixel vertical lines %(a)d pixels apart on a screen that is
; wider than the display, then step the horizontal pel panning register.
	org	0x100
	bits	16

start:
	mov	ax, 0x000d		; EGA 320x200, 16 colours, planar
	int	0x10

	mov	dx, 0x3d4		; a logical line of 80 bytes = 640 px,
	mov	al, 0x13		; so there is something to pan in from
	out	dx, al			; the right
	inc	dx
	mov	al, 0x28
	out	dx, al

	mov	dx, 0x3c4		; write to all four planes at once
	mov	al, 0x02
	out	dx, al
	inc	dx
	mov	al, 0x0f
	out	dx, al

	mov	ax, 0xa000
	mov	es, ax
	xor	di, di
	mov	cx, 0x4000		; clear what we are going to show
	xor	ax, ax
	cld
	rep	stosw

	xor	di, di
	mov	cx, 200
.row:
	mov	byte [es:di + %(ba)d], 0x80
	mov	byte [es:di + %(bb)d], 0x80
	add	di, 80
	loop	.row

loop_pan:
	xor	al, al
	call	set_pan
	call	wait_a_while
	mov	al, %(pan)d
	call	set_pan
	call	wait_a_while
	jmp	loop_pan

; al = panning value
set_pan:
	push	ax
	mov	dx, 0x3da		; the flipflop starts at the index
	in	al, dx
	mov	dx, 0x3c0
	mov	al, 0x13 | 0x20		; index, and keep the video on
	out	dx, al
	pop	ax
	out	dx, al
	ret

wait_a_while:
	push	cx
	mov	cx, 55			; about three seconds
.t:
	push	cx
	mov	ah, 0x86
	mov	cx, 0
	mov	dx, 55000
	int	0x15
	pop	cx
	loop	.t
	pop	cx
	ret
"""


def xwd_row(path, y):
    """One row of an Xvfb -fbdir dump, as a list of (r + g + b)."""
    data = path.read_bytes()
    if len(data) < 100:
        return None, 0, 0
    f = struct.unpack(">25I", data[:100])
    hdr, width, height, ncolors = f[0], f[4], f[5], f[19]
    bpp, bpl = f[11], f[12]
    if bpp not in (24, 32) or y >= height:
        return None, width, height
    off = hdr + ncolors * 12 + y * bpl
    step = bpp // 8
    line = data[off:off + width * step]
    if len(line) < width * step:
        return None, width, height
    return ([sum(line[x * step:x * step + 3]) for x in range(width)],
            width, height)


def bright_runs(row, threshold=300):
    """The first column of each run of bright pixels in the row."""
    runs = []
    prev = False
    for x, v in enumerate(row):
        cur = v > threshold
        if cur and not prev:
            runs.append(x)
        prev = cur
    return runs


class VgaPelPanningTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU",
                                      cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        for tool in ("Xvfb", "nasm"):
            if which(tool) is None:
                raise unittest.SkipTest("%s not installed" % tool)

        cls.workdir = Path(mkdtemp(prefix="pelpan."))
        src = cls.workdir / "probe.asm"
        src.write_text(PROBE % {"a": LINE_B - LINE_A, "pan": PAN,
                                "ba": LINE_A // 8, "bb": LINE_B // 8})
        # dosemu2 looks for command.com in DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.workdir / "command.com"), str(src)])
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)

        cls.conf = cls.workdir / "dosemu.conf"
        cls.conf.write_text('$_cpu_vm = "emulated"\n')
        cls.seen = None

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

    def stop(self, proc):
        if proc is None or proc.poll() is not None:
            return
        try:
            killpg(getpgid(proc.pid), signal.SIGTERM)
        except (ProcessLookupError, PermissionError):
            pass
        try:
            proc.wait(timeout=10)
        except Exception:
            try:
                killpg(getpgid(proc.pid), signal.SIGKILL)
            except (ProcessLookupError, PermissionError):
                pass

    def startXvfb(self, fbdir):
        for display in range(70, 100):
            xvfb = Popen(["Xvfb", ":%d" % display, "-screen", "0",
                          "800x600x24", "-fbdir", str(fbdir)],
                         stdout=DEVNULL, stderr=DEVNULL, stdin=DEVNULL,
                         start_new_session=True)
            deadline = monotonic() + 10
            while monotonic() < deadline:
                if xvfb.poll() is not None:
                    break
                if (fbdir / "Xvfb_screen0").exists():
                    return xvfb, display
                sleep(0.2)
            self.stop(xvfb)
        self.skipTest("Xvfb did not start")

    def positions(self):
        """Where the two lines are in each snapshot, as (first, second)."""
        if self.__class__.seen is not None:
            return self.__class__.seen

        logfile = self.workdir / "dosemu.log"
        outfile = self.workdir / "dosemu.out"
        fbdir = self.workdir / "fb"
        home = self.workdir / "home"
        for d in (fbdir, home):
            rmtree(str(d), ignore_errors=True)
            d.mkdir()

        xvfb, display = self.startXvfb(fbdir)
        fb = fbdir / "Xvfb_screen0"
        env = dict(environ)
        env.update({
            "HOME": str(home),
            "DISPLAY": ":%d" % display,
            "SDL_VIDEODRIVER": "x11",
            "DOSEMU2_COMCOM_DIR": str(self.workdir),
        })

        seen = []
        dosemu = None
        try:
            with open(outfile, "wb") as out:
                # in its own session, because bin/dosemu is a wrapper script
                # that runs the real binary in a subshell: signalling the
                # script alone would leave dosemu2 behind
                dosemu = Popen([str(self.dosemu), "-S", "-n", "-q",
                                "-f", str(self.conf), "-o", str(logfile)],
                               env=env, stdout=out, stderr=out, stdin=DEVNULL,
                               start_new_session=True)
                sleep(8)
                for _ in range(SHOTS):
                    row = None
                    for y in range(40, 560, 20):
                        r, _w, _h = xwd_row(fb, y)
                        if r is None:
                            continue
                        if len(bright_runs(r)) == 2:
                            row = r
                            break
                    if row is not None:
                        seen.append(tuple(bright_runs(row)))
                    sleep(SHOT_INTERVAL)
        finally:
            self.stop(dosemu)
            self.stop(xvfb)

        log = logfile.read_text(errors="replace") if logfile.exists() else ""
        log += outfile.read_text(errors="replace")
        if "initializing SDL plugin" not in log:
            self.skipTest("this build has no SDL plugin")
        if not seen:
            where = self.keepLog(log)
            self.fail("the two lines the probe draws were never on the "
                      "screen; log kept at %s" % where)

        self.__class__.seen = (seen, log)
        return self.__class__.seen

    def test_the_picture_moves_by_single_pixels(self):
        """panning by 4 moves the picture by 4 source pixels, not by 8"""
        seen, log = self.positions()
        places = sorted(set(a for a, _b in seen))
        scale = set((b - a) for a, b in seen)
        if len(scale) != 1:
            where = self.keepLog(log)
            self.fail("the two lines are %s apart in different snapshots and "
                      "should always be the same; log kept at %s"
                      % (sorted(scale), where))
        scale = scale.pop() / (LINE_B - LINE_A)
        if len(places) == 1:
            where = self.keepLog(log)
            self.fail("the picture never moved: the probe alternates the "
                      "panning between 0 and %d and the lines stayed at "
                      "%d. Log kept at %s" % (PAN, places[0], where))
        if len(places) != 2:
            where = self.keepLog(log)
            self.fail("the lines took %d positions (%s) and the probe only "
                      "asks for two; log kept at %s"
                      % (len(places), places, where))
        moved = (places[1] - places[0]) / scale
        if moved != PAN:
            where = self.keepLog(log)
            self.fail("panning by %d moved the picture by %g source pixels; "
                      "the lines were at %s with the window drawn at %g "
                      "times the size. Log kept at %s"
                      % (PAN, moved, places, scale, where))


if __name__ == "__main__":
    unittest.main()
