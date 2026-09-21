#!/usr/bin/env python3

"""Video mode changes and window resizes under the SDL plugin, headless.

This exists because of a crash that only the combination reaches: DOS
switches video mode, SDL_change_mode() frees the rendering surface, and the
render thread is left holding dirty rectangles that were cut from it.  A
terminal test cannot see it -- it needs the real plugin, a real window and
a real render thread -- so this one brings its own X server.

The window between queueing a rectangle and drawing it is narrow, so the
plugin takes DOSEMU_SDL_REND_DELAY_US to widen it on request.  Without that
the bug showed up about once in a dozen runs; with it, on the first mode
change.

Nothing here needs a DOS distribution: the video mode switching is done by
a small program assembled on the spot and handed to dosemu2 as its command
interpreter.
"""

import re
import signal
import unittest

from os import environ, getpgid, killpg
from pathlib import Path
from shutil import which, rmtree
from subprocess import Popen, DEVNULL, PIPE, check_call, run, CalledProcessError
from tempfile import mkdtemp
from time import sleep, monotonic

# The program dosemu2 runs instead of a command interpreter.  It cycles
# 320x200x256, 80x25 text and 640x480x16 for as long as it is left alone,
# drawing into each so that the render thread has rectangles in hand across
# every switch.  It must never exit, or DOS complains and shuts down.
MODE_CYCLER = r"""
bits 16
org 0x100

start:
        mov     ax, 0x0013              ; 320x200x256
        int     0x10
        push    0xa000
        pop     es
        xor     di, di
        mov     cx, 32000
        mov     ax, 0x2424
        rep     stosw
        call    tick

        mov     ax, 0x0003              ; 80x25 text
        int     0x10
        mov     si, msg
.putc:  lodsb
        test    al, al
        jz      .done
        mov     ah, 0x0e
        xor     bx, bx
        int     0x10
        jmp     .putc
.done:  call    tick

        mov     ax, 0x0012              ; 640x480x16
        int     0x10
        push    0xa000
        pop     es
        xor     di, di
        mov     cx, 8000
        mov     ax, 0xffff
        rep     stosw
        call    tick

        mov     ax, 0x0003
        int     0x10
        call    tick
        jmp     start

tick:                                   ; about half a second of BIOS ticks
        mov     ah, 0
        int     0x1a
        mov     bx, dx
        add     bx, 9
.spin:  mov     ah, 0
        int     0x1a
        cmp     dx, bx
        jb      .spin
        ret

msg:    db      'MODE 3 TEXT ', 0
"""

# How many mode changes to insist on before calling the run a pass.  The
# crash this guards against needed one of them with the delay in place.
WANT_MODE_CHANGES = 30
# The resize test runs the same cycler, so it wants mode changes too, but
# fewer of them -- what it is really after is resizes landing on a window
# that keeps changing underneath them.
WANT_RESIZE_CHANGES = 8
WANT_RESIZES = 12
RESIZE_SIZES = ((800, 600), (1024, 768), (640, 480), (900, 700))

RUN_TIMEOUT = 120
REND_DELAY_US = 20000

CRASH_MARKERS = (
    "Sync signal",            # dosemu2's own report of a fatal signal
    "Segmentation fault",
    "AddressSanitizer",       # when built with --enable-asan
    "Assertion `",            # glibc assert()
)


class SDL3VideoTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU", cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        for tool in ("Xvfb", "nasm"):
            if which(tool) is None:
                raise unittest.SkipTest("%s not installed" % tool)

        cls.workdir = Path(mkdtemp(prefix="sdl3video."))
        src = cls.workdir / "modes.asm"
        src.write_text(MODE_CYCLER)
        # dosemu2 looks for command.com in DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.workdir / "command.com"), str(src)])
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)

        cls.conf = cls.workdir / "dosemu.conf"
        cls.conf.write_text('$_force_vga_fonts = (1)\n')

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

    def test_video_mode_changes(self):
        """switching video mode repeatedly does not take the render thread down"""
        r = self.runCycler(WANT_MODE_CHANGES)
        self.assertGreaterEqual(
            r["changes"], WANT_MODE_CHANGES,
            "only %d video mode changes happened, the test proved nothing"
            % r["changes"])

    def test_window_resize(self):
        """resizing the window while the mode changes does not crash either"""
        if which("xdotool") is None:
            self.skipTest("xdotool not installed")
        r = self.runCycler(WANT_RESIZE_CHANGES, resize=True)
        self.assertGreaterEqual(
            r["resizes"], WANT_RESIZES,
            "only %d window resizes happened, the test proved nothing"
            % r["resizes"])
        self.assertGreaterEqual(
            r["changes"], WANT_RESIZE_CHANGES,
            "only %d video mode changes happened during the resizes"
            % r["changes"])

    def runCycler(self, want_changes, resize=False):
        """Run the mode cycler to completion and insist it did not crash."""
        logfile = self.workdir / "dosemu.log"
        outfile = self.workdir / "dosemu.out"
        for f in (logfile, outfile):
            if f.exists():
                f.unlink()

        xvfb, display = self.startXvfb()
        try:
            env = dict(environ)
            env.update({
                "DISPLAY": display,
                "SDL_VIDEODRIVER": "x11",
                "DOSEMU2_COMCOM_DIR": str(self.workdir),
                "DOSEMU_SDL_REND_DELAY_US": str(REND_DELAY_US),
            })

            with open(outfile, "wb") as out:
                # in its own session, because bin/dosemu is a wrapper script
                # that runs the real binary in a subshell: signalling the
                # script alone would leave dosemu2 behind
                dosemu = Popen([str(self.dosemu), "-S", "-D+v",
                                "-f", str(self.conf), "-o", str(logfile)],
                               env=env, stdout=out, stderr=out, stdin=DEVNULL,
                               start_new_session=True)
                try:
                    r = self.watch(dosemu, logfile, outfile, env,
                                   want_changes, resize)
                finally:
                    self.stop(dosemu)
        finally:
            self.stop(xvfb)

        log = logfile.read_text(errors="replace") if logfile.exists() else ""
        out = outfile.read_text(errors="replace") if outfile.exists() else ""

        if "initializing SDL plugin" not in log:
            self.skipTest("this build has no SDL plugin")

        for marker in CRASH_MARKERS:
            if marker in out:
                where = self.keepLog(log + "\n" + out)
                self.fail("dosemu2 died after %d video mode changes and %d "
                          "window resizes (%r), log kept at %s"
                          % (r["changes"], r["resizes"], marker, where))
        return r

    def watch(self, dosemu, logfile, outfile, env, want_changes, resize):
        """Watch the log until enough has gone by, resizing as we go.

        A crash does not always take the process with it -- dosemu2 catches
        the signal and reports it -- so watch for that too rather than
        sitting here until the timeout.
        """
        pattern = re.compile(r"^SDL: using mode ", re.M)
        deadline = monotonic() + RUN_TIMEOUT
        r = {"changes": 0, "resizes": 0}
        win = self.findWindow(env, dosemu) if resize else None
        while monotonic() < deadline:
            if dosemu.poll() is not None:
                break
            if outfile.exists():
                out = outfile.read_text(errors="replace")
                if any(m in out for m in CRASH_MARKERS):
                    break
            if logfile.exists():
                r["changes"] = len(pattern.findall(
                    logfile.read_text(errors="replace")))
            if win:
                w, h = RESIZE_SIZES[r["resizes"] % len(RESIZE_SIZES)]
                if run(["xdotool", "windowsize", win, str(w), str(h)],
                       env=env, stdout=DEVNULL, stderr=DEVNULL).returncode == 0:
                    r["resizes"] += 1
                else:
                    break       # the window is gone, no point carrying on
            if r["changes"] >= want_changes and \
                    (not resize or r["resizes"] >= WANT_RESIZES):
                break
            sleep(0.5)
        return r

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
        dispfile = self.workdir / "xvfb.display"
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
    unittest.main(verbosity=2)
