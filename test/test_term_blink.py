#!/usr/bin/env python3

"""Does the terminal backend do what int 10h ax=1003h asked?

The top bit of the DOS attribute byte means one of two things, and the
program chooses which: blinking text, or a bright background (issue #1077).
The terminal backend read the setting out of the attribute controller on
every update but nothing downstream looked at it, so the bit was always
rendered as a bright background and a program that asked for blinking never
got it.

The probe paints the same cells twice, once under each setting, and the
test reads the colours off the master side of a pty.

It needs no DOS distribution and no display: the program that paints is
assembled on the spot and handed to dosemu2 as its command interpreter.
"""

import fcntl
import os
import pty
import re
import signal
import struct
import termios
import unittest

from os import environ
from pathlib import Path
from select import select
from shutil import which, rmtree
from subprocess import (check_call, check_output, CalledProcessError,
                        DEVNULL)
from tempfile import mkdtemp
from time import sleep, monotonic

ROWS, COLS = 25, 80
RUN_TIMEOUT = 60
TERM = "xterm-256color"

ROW_BRIGHT = 4          # eight cells of attribute f0, black on bright white
ROW_PLAIN = 6           # eight cells of attribute 1f, the top bit clear
ROW_READY = 20
RUN = 8                 # cells per row, long enough not to occur by chance

PROBE = r"""
; Paint two rows the test on the other side of the pty knows by heart,
; under the blink setting it was built with, then sit still.
	org	0x100
	bits	16
	cpu	386

COLS	equ	80

start:
	mov	ax, 0x0003		; 80x25 colour text
	int	0x10

	mov	ax, 0x1003		; 0 = bright background, 1 = blinking
	mov	bx, %(BL)d
	int	0x10

	mov	ax, 0xb800
	mov	es, ax

	; the attribute from the report: black on bright white, or, with
	; blinking chosen, black on white and blinking
	mov	di, (%(ROW_BRIGHT)d * COLS) * 2
	mov	cx, %(RUN)d
	mov	ax, 0xf000 + '@'
	rep	stosw

	; the top bit clear, so the setting must not reach it
	mov	di, (%(ROW_PLAIN)d * COLS) * 2
	mov	cx, %(RUN)d
	mov	ax, 0x1f00 + '#'
	rep	stosw

	mov	di, (%(ROW_READY)d * COLS) * 2
	mov	si, ready
	mov	ah, 0x07
.puts:
	lodsb
	or	al, al
	jz	.done
	stosw
	jmp	.puts
.done:
	jmp	$

ready	db	'READY', 0
"""

CSI = re.compile(rb"\[([0-9;?]*)([ -/]*)([@-~])")

BLINK = 5
BG_WHITE = 47
BG_BRIGHT_WHITE = 107
FG_BLACK = 30
FG_BRIGHT_WHITE = 97
BG_BLUE = 44


def terminal_colours(term):
    """How many colours the terminfo entry offers, 0 if there is none.

    tput is asked in a process of its own: curses.setupterm() keeps the
    first entry it was given for the life of the process and would answer
    for that one instead.
    """
    try:
        return int(check_output(["tput", "-T" + term, "colors"],
                                stderr=DEVNULL))
    except (CalledProcessError, FileNotFoundError, ValueError):
        return 0


def sgr_at_runs(data, char, run):
    """The SGR parameters in force where a run of `char` was written.

    Returns a list, one entry per run found, each the set of parameters
    the stream had selected at the moment the run's first cell was
    written.
    """
    state = set()
    found = []
    pending = []                # (char, frozenset) for the cells just written
    i, n = 0, len(data)
    while i < n:
        b = data[i]
        if b == 0x1b:
            m = CSI.match(data, i + 1)
            if m:
                i = m.end()
                if m.group(3) == b"m" and not m.group(1).startswith(b"?"):
                    params = [int(p) for p in m.group(1).split(b";")
                              if p.isdigit()] or [0]
                    for v in params:
                        if v == 0:
                            state.clear()
                        elif 30 <= v <= 39 or 90 <= v <= 97:
                            state = {x for x in state
                                     if not (30 <= x <= 39 or 90 <= x <= 97)}
                            state.add(v)
                        elif 40 <= v <= 49 or 100 <= v <= 107:
                            state = {x for x in state
                                     if not (40 <= x <= 49 or
                                             100 <= x <= 107)}
                            state.add(v)
                        else:
                            state.add(v)
                continue
            i += 2                      # ESC ( B and the like
            pending = []
            continue
        i += 1
        if b < 0x20:
            pending = []
            continue
        pending.append((b, frozenset(state)))
        if len(pending) >= run and \
                all(c == char for c, _ in pending[-run:]):
            found.append(set(pending[-run][1]))
            pending = []
    return found


class TerminalBlinkTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU",
                                      cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        if which("nasm") is None:
            raise unittest.SkipTest("nasm not installed")
        cls.workdir = Path(mkdtemp(prefix="termblink."))
        cls.conf = cls.workdir / "dosemu.conf"
        cls.conf.write_text('$_term_color = (1)\n')
        cls.runs = {}
        # the bright background only reaches the wire on a terminal that
        # has sixteen of them; blinking needs no such thing
        cls.colors = terminal_colours(TERM)

    @classmethod
    def tearDownClass(cls):
        rmtree(str(cls.workdir), ignore_errors=True)

    def keepLog(self, text):
        """Leave the bytes where ci_test.sh collects them."""
        p = self.topdir / ("%s.%s.%s.log" % (Path(__file__).stem,
                                             type(self).__name__,
                                             self._testMethodName))
        p.write_text(text)
        return p

    def colours(self, bl):
        """Run the probe under one setting and read the two rows back."""
        if bl in self.__class__.runs:
            return self.__class__.runs[bl]

        src = self.workdir / "probe.asm"
        src.write_text(PROBE % {"BL": bl, "ROW_BRIGHT": ROW_BRIGHT,
                                "ROW_PLAIN": ROW_PLAIN,
                                "ROW_READY": ROW_READY, "RUN": RUN})
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(self.workdir / "command.com"), str(src)],
                       stdout=DEVNULL, stderr=DEVNULL)
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)

        logfile = self.workdir / ("dosemu%d.log" % bl)
        home = self.workdir / "home"
        rmtree(str(home), ignore_errors=True)
        home.mkdir()

        pid, fd = pty.fork()
        if pid == 0:
            env = dict(environ)
            env.update({
                "HOME": str(home),
                "DOSEMU2_COMCOM_DIR": str(self.workdir),
                # sixteen background colours, so the two settings are
                # told apart by the colour and not by the terminal
                "TERM": TERM,
                "LC_ALL": "C.UTF-8",
            })
            env.pop("LANG", None)
            env.pop("LC_CTYPE", None)
            try:
                os.execve("/bin/sh", ["sh", "-c", "%s -t -ks -q -f %s -o %s"
                                      % (self.dosemu, self.conf, logfile)],
                          env)
            finally:
                os._exit(127)

        fcntl.ioctl(fd, termios.TIOCSWINSZ,
                    struct.pack("HHHH", ROWS, COLS, 0, 0))

        out = b""
        deadline = monotonic() + RUN_TIMEOUT
        try:
            while monotonic() < deadline:
                r, _, _ = select([fd], [], [], 0.5)
                if r:
                    try:
                        d = os.read(fd, 65536)
                    except OSError:
                        break
                    if not d:
                        break
                    out += d
                if b"READY" in out:
                    sleep(1)            # let the last update arrive
                    try:
                        while select([fd], [], [], 0.2)[0]:
                            d = os.read(fd, 65536)
                            if not d:
                                break
                            out += d
                    except OSError:
                        pass
                    break
        finally:
            try:
                os.kill(pid, signal.SIGTERM)
                os.waitpid(pid, 0)
            except OSError:
                pass
            os.close(fd)

        log = logfile.read_text(errors="replace") if logfile.exists() else ""
        if "VID: Video set to Video_term" not in log and b"\x1b[" not in out:
            self.skipTest("this build has no terminal plugin")
        if b"READY" not in out:
            where = self.keepLog(out.decode("utf-8", "replace") + "\n" + log)
            self.fail("the probe never finished painting; bytes kept at %s"
                      % where)

        bright = sgr_at_runs(out, ord('@'), RUN)
        plain = sgr_at_runs(out, ord('#'), RUN)
        if not bright or not plain:
            where = self.keepLog(out.decode("utf-8", "replace") + "\n" + log)
            self.fail("the painted rows never reached the terminal; bytes "
                      "kept at %s" % where)
        self.__class__.runs[bl] = (bright[-1], plain[-1])
        return self.__class__.runs[bl]

    def test_blinking_is_asked_for_when_the_program_wants_it(self):
        """with blinking chosen the top bit becomes blink, not a colour"""
        bright, _ = self.colours(1)
        self.assertIn(BLINK, bright, bright)
        self.assertIn(BG_WHITE, bright, bright)
        self.assertNotIn(BG_BRIGHT_WHITE, bright, bright)
        self.assertIn(FG_BLACK, bright, bright)

    def test_a_bright_background_is_used_when_the_program_wants_it(self):
        """with a bright background chosen the top bit becomes a colour"""
        if self.colors < 16:
            self.skipTest("%s here has %d colours, too few for a bright "
                          "background" % (TERM, self.colors))
        bright, _ = self.colours(0)
        self.assertIn(BG_BRIGHT_WHITE, bright, bright)
        self.assertIn(FG_BLACK, bright, bright)

    def test_the_setting_changes_what_is_rendered(self):
        """the two settings do not render the same cell the same way"""
        self.assertNotEqual(self.colours(0)[0], self.colours(1)[0])

    def test_a_bright_background_never_blinks(self):
        """choosing a bright background turns blinking off"""
        bright, _ = self.colours(0)
        self.assertNotIn(BLINK, bright, bright)

    def test_an_attribute_below_the_top_bit_is_left_alone(self):
        """a cell the setting says nothing about renders the same either way"""
        for bl in (0, 1):
            plain = self.colours(bl)[1]
            self.assertIn(FG_BRIGHT_WHITE, plain, (bl, plain))
            self.assertIn(BG_BLUE, plain, (bl, plain))
            self.assertNotIn(BLINK, plain, (bl, plain))


if __name__ == "__main__":
    unittest.main(verbosity=2)
