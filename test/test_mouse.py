#!/usr/bin/env python3

"""What int 33h hands back after a long drag, with the mouse grabbed.

The mickey counters are dosemu2's to keep until the program asks for them,
and they go back in 16 bit registers.  A program that asks for them rarely
can build up more than fits, and truncating that turned a long move right
into a long move left: the cursor jumped into the top left corner just as
you pushed it into the bottom right one (issue #1726).

Relative motion only happens with the pointer grabbed, and grabbing needs a
real window with focus, so this test brings its own X server and drives the
pointer with xdotool; without either it skips.  The DOS side is a small
program assembled on the spot and handed to dosemu2 as its command
interpreter, so no DOS distribution is needed.
"""

import re
import signal
import unittest

from os import environ, getpgid, killpg
from shutil import which
from subprocess import Popen, DEVNULL, PIPE, run
from time import sleep, monotonic

from common_framework import (BaseTestCase, DOSEMU_CONF_DEFAULT,
                              main, main_setup, mark)

# Every step is one xdotool call, so keep the count low and the step large.
# Together they have to come to more than a 16 bit register can carry.
STEP_X = 300
STEP_Y = 200
STEPS = 150

# The framework's own drive, plus a key for the grab that no DOS program
# takes for itself.
CONF = DOSEMU_CONF_DEFAULT + ('$_force_vga_fonts = (1)\n'
                              '$_X_mgrab_key = "Home"\n')

PROBE = r"""
; Log the int 33h pointer position, and the mickey counters on demand.
	org	0x100
	bits	16
	cpu	386

start:
	mov	ax, 0x0013		; a graphics mode, like the games
	int	0x10

	xor	ax, ax			; reset the mouse driver
	int	0x33
	cmp	ax, 0xffff
	jne	hang

	mov	ah, 0x3c
	xor	cx, cx
	mov	dx, fname
	int	0x21
	jc	hang
	mov	[fh], ax

	mov	ax, 0x0001		; show cursor
	int	0x33

	mov	word [lastx], 0xffff
	mov	word [lasty], 0xffff
.loop:
	mov	ah, 0x01		; a keystroke means "read the mickeys"
	int	0x16
	jz	.pos
	xor	ah, ah
	int	0x16
	mov	ax, 0x000b
	int	0x33
	mov	[mkx], cx
	mov	[mky], dx
	mov	si, s_mk
	call	record_mk
.pos:
	mov	ax, 0x0003
	int	0x33
	cmp	cx, [lastx]
	jne	.rec
	cmp	dx, [lasty]
	je	.loop
.rec:
	mov	[lastx], cx
	mov	[lasty], dx
	call	record
	jmp	.loop

hang:					; DOS is unhappy if we ever return
	mov	ah, 0x00
	int	0x16
	jmp	hang

record:
	mov	di, line
	mov	si, s_pos
	call	puts
	mov	ax, [lastx]
	call	putdec
	mov	al, ' '
	stosb
	mov	ax, [lasty]
	call	putdec
	jmp	flush

record_mk:
	mov	di, line
	call	puts
	mov	ax, [mkx]
	call	putsigned
	mov	al, ' '
	stosb
	mov	ax, [mky]
	call	putsigned
	jmp	flush

flush:
	mov	ax, 0x0a0d
	stosw
	mov	cx, di
	sub	cx, line
	mov	bx, [fh]
	mov	dx, line
	mov	ah, 0x40
	int	0x21
	ret

puts:
	lodsb
	or	al, al
	jz	.e
	stosb
	jmp	puts
.e:
	ret

putsigned:
	test	ax, ax
	jns	putdec
	neg	ax
	push	ax
	mov	al, '-'
	stosb
	pop	ax
putdec:
	push	bx
	push	cx
	push	dx
	mov	bx, 10
	xor	cx, cx
.d:
	xor	dx, dx
	div	bx
	push	dx
	inc	cx
	or	ax, ax
	jnz	.d
.o:
	pop	ax
	add	al, '0'
	stosb
	loop	.o
	pop	dx
	pop	cx
	pop	bx
	ret

fname	db	'mouse.log', 0
s_pos	db	'pos ', 0
s_mk	db	'mickeys ', 0
fh	dw	0
lastx	dw	0
lasty	dw	0
mkx	dw	0
mky	dw	0
line	times 48 db 0
"""


class MouseGrabTestCase(BaseTestCase, unittest.TestCase):

    attrs = {'mouse'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "MouseGrab"
        # the probe is the command interpreter, so there is no DOS
        # distribution to unpack
        cls.tarfile = ""
        cls.report = None
        cls.bootlog = ""
        cls.out = ""

    test_0_basic_boot = None

    @mark('mouse')
    def test_a_long_drag_does_not_change_direction(self):
        """the mickeys of a drag right and down never come back negative"""
        reads, positions = self.drag()
        bad = ", ".join("read %d: %d %d" % (i, x, y)
                        for i, (x, y) in enumerate(reads) if x < 0 or y < 0)
        self.assertEqual("", bad,
                         "a drag %d right and %d down read back as %s"
                         % (STEPS * STEP_X, STEPS * STEP_Y, bad))

    @mark('mouse')
    def test_nothing_is_dropped_on_the_way(self):
        """what does not fit in one read is kept for the next one"""
        reads, positions = self.drag()
        got_x = sum(x for x, y in reads)
        got_y = sum(y for x, y in reads)
        # The X server drops motion when it cannot keep up, so this is a
        # floor, not an equality: what it must not be is a 16 bit register's
        # worth when far more than that went in.
        msg = ("only %d of %d mickeys right and %d of %d down came back "
               "over %d reads" % (got_x, STEPS * STEP_X, got_y,
                                  STEPS * STEP_Y, len(reads)))
        self.assertGreater(got_x, 32767, msg)
        self.assertGreater(got_y, 0, msg)

    def relog(self):
        """Hand this test the logs of the shared run.

        The framework looks at this test's own dosemu and output logs when
        it fails, so a test that took the reads from the cache has to be
        given them; without that a failure here has nothing to show.
        """
        self.logfiles['xpt'][1] = "output.log"
        self.logfiles['xpt'][0].write_text(self.__class__.out)
        self.logfiles['log'][0].write_text(self.__class__.bootlog)

    def drag(self):
        """Grab the pointer, drag it a long way, then ask for the mickeys.

        The drag is the same every time, so one run serves both tests;
        relog() keeps each test's logs in place.
        """
        if self.__class__.report is not None:
            self.relog()
            return self.__class__.report

        for tool in ("Xvfb", "xdotool", "nasm"):
            if which(tool) is None:
                self.skipTest("%s not installed" % tool)

        self.mkfile("dosemu.conf", CONF, dname=self.imagedir)
        # dosemu2 takes its command interpreter from DOSEMU2_COMCOM_DIR
        self.mkcom_with_nasm("command", PROBE)

        home = self.imagedir / "home"
        home.mkdir()
        logfile = self.topdir / self.logfiles['log'][0]
        outfile = self.imagedir / "dosemu.out"
        result = self.workdir / "mouse.log"

        args = [str(self.dosemu),
                "-S",
                "--Fimagedir", str(self.imagedir),
                "-f", str(self.imagedir / "dosemu.conf"),
                "-n",
                "-q",
                "-D+v",
                "-o", str(logfile)]
        if environ.get("NO_KVM", '0') == '1' or self.use_cpu == 'emu':
            args.extend(["-z", "0"])

        xvfb, display = self.startXvfb()
        try:
            env = dict(environ)
            env.update({
                "DISPLAY": display,
                "SDL_VIDEODRIVER": "x11",
                "HOME": str(home),
                "DOSEMU2_COMCOM_DIR": str(self.workdir),
            })
            with open(outfile, "wb") as out:
                # in its own session, because bin/dosemu is a wrapper script
                # that runs the real binary in a subshell: signalling the
                # script alone would leave dosemu2 behind
                dosemu = Popen(args, env=env, stdout=out, stderr=out,
                               stdin=DEVNULL, start_new_session=True)
                try:
                    self.waitFor(dosemu, result, "the DOS program to start")
                    win = self.findWindow(env, dosemu)
                    if win is None:
                        self.skipTest("dosemu2 never showed a window")
                    self.x(env, "windowfocus", win)
                    sleep(1)
                    # XTEST, not a synthetic event: SDL ignores those
                    self.x(env, "key", "--clearmodifiers", "ctrl+alt+Home")
                    sleep(1)
                    for _ in range(STEPS):
                        self.x(env, "mousemove_relative", "--",
                               str(STEP_X), str(STEP_Y))
                    sleep(1)
                    self.x(env, "key", "a")     # read the mickeys
                    sleep(1)
                    self.x(env, "key", "a")     # and again, for the rest
                    sleep(1)
                finally:
                    self.stop(dosemu)
        finally:
            self.stop(xvfb)

        self.__class__.bootlog = (logfile.read_text(errors="replace")
                                  if logfile.exists() else "")
        self.__class__.out = (outfile.read_text(errors="replace")
                              if outfile.exists() else "")
        self.logfiles['xpt'][1] = "output.log"
        self.logfiles['xpt'][0].write_text(self.__class__.out)

        log = self.__class__.bootlog + self.__class__.out
        if "initializing SDL plugin" not in log:
            self.skipTest("this build has no SDL plugin")
        if "mouse grab activated" not in log:
            self.skipTest("the pointer never got grabbed")
        self.assertTrue(result.exists(), "the probe wrote nothing")

        reads, positions = [], []
        for line in result.read_text(errors="replace").splitlines():
            m = re.match(r"mickeys (-?\d+) (-?\d+)$", line)
            if m:
                reads.append((int(m.group(1)), int(m.group(2))))
            m = re.match(r"pos (\d+) (\d+)$", line)
            if m:
                positions.append((int(m.group(1)), int(m.group(2))))
        self.assertTrue(reads, "the probe never read the mickeys")

        self.__class__.report = (reads, positions)
        return self.__class__.report

    def x(self, env, *args):
        run(["xdotool"] + list(args), env=env, stdout=DEVNULL, stderr=DEVNULL)

    def waitFor(self, proc, path, what, timeout=60):
        deadline = monotonic() + timeout
        while monotonic() < deadline:
            if proc.poll() is not None:
                self.fail("dosemu2 exited while waiting for %s" % what)
            if path.exists():
                sleep(1)
                return
            sleep(0.2)
        self.fail("timed out waiting for %s" % what)

    def findWindow(self, env, dosemu, timeout=30):
        """The id of dosemu2's window, once it has one."""
        deadline = monotonic() + timeout
        while monotonic() < deadline:
            if dosemu.poll() is not None:
                return None
            p = run(["xdotool", "search", "--class", "dosemu"],
                    env=env, stdout=PIPE, stderr=DEVNULL)
            ids = p.stdout.split()
            if ids:
                return ids[-1].decode()
            sleep(0.2)
        return None

    def startXvfb(self):
        """Bring up an X server on whatever display number is free.

        -displayfd both picks the number and says when the server is ready,
        which a stale socket left behind by a killed one would otherwise
        make us believe too early.
        """
        dispfile = self.imagedir / "xvfb.display"
        dispfile.write_text("")
        with open(dispfile, "w") as f:
            xvfb = Popen(["Xvfb", "-displayfd", str(f.fileno()),
                          "-screen", "0", "1280x1024x24", "-nolisten", "tcp"],
                         stdout=DEVNULL, stderr=DEVNULL, pass_fds=(f.fileno(),),
                         start_new_session=True)
        deadline = monotonic() + 15
        while monotonic() < deadline:
            if xvfb.poll() is not None:
                break
            got = dispfile.read_text()
            if got.endswith("\n"):
                return xvfb, ":" + got.strip()
            sleep(0.1)
        self.stop(xvfb)
        self.skipTest("Xvfb did not start")

    def stop(self, proc):
        """Take down the whole process group and make sure it is gone."""
        if proc.poll() is not None:
            return
        try:
            pgid = getpgid(proc.pid)
            killpg(pgid, signal.SIGTERM)
        except ProcessLookupError:
            return
        try:
            proc.wait(timeout=20)
        except Exception:
            pass
        try:
            killpg(pgid, signal.SIGKILL)
        except ProcessLookupError:
            pass
        try:
            proc.wait(timeout=5)
        except Exception:
            pass


if __name__ == "__main__":
    cases = [
        MouseGrabTestCase,
    ]
    main(main_setup(cases))
