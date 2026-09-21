#!/usr/bin/env python3

"""What the terminal backend actually puts on the terminal.

Everything dosemu2 draws in `-t` mode leaves as escape sequences, and until
now nothing checked them (issue #2933).  This test paints a known pattern
into the text screen from DOS, catches the bytes on the master side of a
pty, replays them through just enough of a terminal to know what ended up
in which cell, and compares that with what DOS wrote.

It needs no DOS distribution and no display: the program that paints is
assembled on the spot and handed to dosemu2 as its command interpreter.

What it does not cover is the rendering of a host terminal program run from
DOS, which needs comcom64 and so a dj64 build.
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
from subprocess import check_call, CalledProcessError, DEVNULL
from tempfile import mkdtemp
from time import sleep, monotonic

ROWS, COLS = 25, 80
RUN_TIMEOUT = 60

# Where the probe paints, so the test and the DOS program agree.
ROW_TEXT = 1
ROW_COLOUR = 3
ROW_GLYPH = 5
ROW_SCROLL = 7          # three lines, the top one scrolled away
ROW_READY = 20
CURSOR_Y, CURSOR_X = 12, 40

PROBE = r"""
; Paint a pattern into the text screen that the test on the other side of
; the pty knows by heart, then sit still so it stays there.
	org	0x100
	bits	16
	cpu	386

COLS	equ	80

start:
	mov	ax, 0x0003		; 80x25 colour text
	int	0x10

	mov	ax, 0xb800
	mov	es, ax

	mov	di, (%(ROW_TEXT)d * COLS) * 2
	mov	si, word1
	mov	ah, 0x07
	call	puts

	; one cell per foreground colour, 1..15, on black.  Colour 0 is left
	; out: black on black says nothing about how it was rendered.
	mov	di, (%(ROW_COLOUR)d * COLS) * 2
	mov	cx, 15
	mov	bl, 1
.col:
	mov	al, bl
	add	al, 'a' - 1
	mov	ah, bl
	stosw
	inc	bl
	loop	.col

	; bright white on blue, then a character the terminal has to look up
	; in its character set rather than pass through
	mov	di, (%(ROW_GLYPH)d * COLS) * 2
	mov	ax, 0x1f00 + 'W'
	stosw
	mov	ax, 0x0700 + 0xc9	; CP437 box drawing, upper left corner
	stosw

	; three lines, of which the BIOS is asked to scroll the left eight
	; columns up by one
	mov	di, (%(ROW_SCROLL)d * COLS) * 2
	mov	si, line1
	mov	ah, 0x07
	call	puts
	mov	di, ((%(ROW_SCROLL)d + 1) * COLS) * 2
	mov	si, line2
	mov	ah, 0x07
	call	puts
	mov	di, ((%(ROW_SCROLL)d + 2) * COLS) * 2
	mov	si, line3
	mov	ah, 0x07
	call	puts

	mov	ax, 0x0601		; scroll up one line
	mov	bh, 0x07		; blank the exposed line with this
	mov	ch, %(ROW_SCROLL)d
	mov	cl, 0
	mov	dh, %(ROW_SCROLL)d + 2
	mov	dl, 7
	int	0x10

	mov	ah, 0x02		; put the cursor somewhere known
	xor	bh, bh
	mov	dh, %(CURSOR_Y)d
	mov	dl, %(CURSOR_X)d
	int	0x10

	; last of all, so the test knows the rest is already there
	mov	di, (%(ROW_READY)d * COLS) * 2
	mov	si, ready
	mov	ah, 0x07
	call	puts

.spin:
	mov	ah, 0x00		; DOS is unhappy if we ever return
	int	0x16
	jmp	.spin

puts:
	lodsb
	or	al, al
	jz	.out
	stosw
	jmp	puts
.out:
	ret

word1	db	'TERMPROBE', 0
line1	db	'AAAAAAAA', 0
line2	db	'BBBBBBBB', 0
line3	db	'CCCCCCCC', 0
ready	db	'READY', 0
""" % dict(ROW_TEXT=ROW_TEXT, ROW_COLOUR=ROW_COLOUR, ROW_GLYPH=ROW_GLYPH,
           ROW_SCROLL=ROW_SCROLL, ROW_READY=ROW_READY,
           CURSOR_Y=CURSOR_Y, CURSOR_X=CURSOR_X)

# The SGR colour the terminal backend is expected to pick for each of the
# sixteen DOS attribute values.  The low three bits are in the other order
# than ANSI wants them, which is what the rotation in terminal.c is for.
FG = {0: 30, 1: 34, 2: 32, 3: 36, 4: 31, 5: 35, 6: 33, 7: 37,
      8: 90, 9: 94, 10: 92, 11: 96, 12: 91, 13: 95, 14: 93, 15: 97}
BG = {0: 40, 1: 44, 2: 42, 3: 46, 4: 41, 5: 45, 6: 43, 7: 47}

CSI = re.compile(rb"\[([0-9;?]*)([ -/]*)([@-~])")
OSC = re.compile(rb"\](\d+);([^\x07\x1b]*)(?:\x07|\x1b\\)")


class Screen:
    """Just enough of a terminal to tell what ended up in which cell."""

    def __init__(self, rows=ROWS, cols=COLS):
        self.rows, self.cols = rows, cols
        self.title = None
        self.ch = [[" "] * cols for _ in range(rows)]
        self.fg = [[37] * cols for _ in range(rows)]
        self.bg = [[40] * cols for _ in range(rows)]
        self.y = self.x = 0
        self.cfg, self.cbg = 37, 40

    def text(self, row):
        return "".join(self.ch[row]).rstrip()

    def feed(self, data):
        i, n = 0, len(data)
        while i < n:
            b = data[i]
            if b == 0x1b:
                m = CSI.match(data, i + 1)
                if m:
                    i = m.end()
                    self.csi(m.group(1), m.group(3))
                    continue
                m = OSC.match(data, i + 1)
                if m:
                    i = m.end()
                    if m.group(1) == b"2":
                        self.title = m.group(2).decode("utf-8", "replace")
                    continue
                i += 2                  # ESC ( B and the like: no cell moves
                continue
            i += 1
            if b == 0x0d:
                self.x = 0
            elif b == 0x0a:
                self.newline()
            elif b == 0x08:
                self.x = max(0, self.x - 1)
            elif b >= 0x20:
                ln = 4 if b >= 0xf0 else 3 if b >= 0xe0 else 2 if b >= 0xc0 \
                    else 1
                self.put(data[i - 1:i - 1 + ln].decode("utf-8", "replace"))
                i += ln - 1

    def put(self, c):
        if self.x >= self.cols:
            self.x = 0
            self.newline()
        self.ch[self.y][self.x] = c
        self.fg[self.y][self.x] = self.cfg
        self.bg[self.y][self.x] = self.cbg
        self.x += 1

    def newline(self):
        if self.y + 1 < self.rows:
            self.y += 1
            return
        for a, blank in ((self.ch, " "), (self.fg, 37), (self.bg, 40)):
            a.pop(0)
            a.append([blank] * self.cols)

    def erase(self, cells):
        for y, x in cells:
            self.ch[y][x] = " "
            self.fg[y][x] = self.cfg
            self.bg[y][x] = self.cbg

    def csi(self, raw, final):
        if raw.startswith(b"?"):
            return                      # private modes move nothing
        params = [int(p) for p in raw.split(b";") if p.isdigit()]

        def p(k, d=1):
            return params[k] if len(params) > k and params[k] else d

        f = final.decode()
        if f in "Hf":
            self.y = min(self.rows - 1, p(0) - 1)
            self.x = min(self.cols - 1, p(1) - 1)
        elif f == "A":
            self.y = max(0, self.y - p(0))
        elif f == "B":
            self.y = min(self.rows - 1, self.y + p(0))
        elif f == "C":
            self.x = min(self.cols - 1, self.x + p(0))
        elif f == "D":
            self.x = max(0, self.x - p(0))
        elif f == "G":
            self.x = min(self.cols - 1, p(0) - 1)
        elif f == "d":
            self.y = min(self.rows - 1, p(0) - 1)
        elif f == "J":
            k = p(0, 0)
            if k == 0:
                self.erase([(self.y, x) for x in range(self.x, self.cols)] +
                           [(y, x) for y in range(self.y + 1, self.rows)
                            for x in range(self.cols)])
            elif k == 1:
                self.erase([(y, x) for y in range(self.y)
                            for x in range(self.cols)] +
                           [(self.y, x) for x in range(self.x + 1)])
            else:
                self.erase([(y, x) for y in range(self.rows)
                            for x in range(self.cols)])
        elif f == "K":
            k = p(0, 0)
            if k == 0:
                self.erase([(self.y, x) for x in range(self.x, self.cols)])
            elif k == 1:
                self.erase([(self.y, x) for x in range(self.x + 1)])
            else:
                self.erase([(self.y, x) for x in range(self.cols)])
        elif f == "m":
            for v in (params or [0]):
                if v == 0:
                    self.cfg, self.cbg = 37, 40
                elif 30 <= v <= 37 or 90 <= v <= 97:
                    self.cfg = v
                elif 40 <= v <= 47 or 100 <= v <= 107:
                    self.cbg = v
                elif v == 39:
                    self.cfg = 37
                elif v == 49:
                    self.cbg = 40


class TerminalRenderTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU",
                                      cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        if which("nasm") is None:
            raise unittest.SkipTest("nasm not installed")

        cls.workdir = Path(mkdtemp(prefix="termrender."))
        src = cls.workdir / "probe.asm"
        src.write_text(PROBE)
        # dosemu2 takes its command interpreter from DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.workdir / "command.com"), str(src)],
                       stdout=DEVNULL, stderr=DEVNULL)
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)
        cls.conf = cls.workdir / "dosemu.conf"
        cls.conf.write_text('$_term_color = (1)\n')
        cls.screen = None
        cls.raw = None

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

    def test_text_lands_where_dos_put_it(self):
        """a word written into the text screen comes out on the right row"""
        s = self.render()
        self.assertEqual(s.text(ROW_TEXT), "TERMPROBE", self.dump(s))

    def test_attributes_become_terminal_colours(self):
        """each of the fifteen visible attributes picks its own colour"""
        s = self.render()
        got = [s.fg[ROW_COLOUR][i] for i in range(15)]
        self.assertEqual(got, [FG[i + 1] for i in range(15)], self.dump(s))
        self.assertEqual(s.fg[ROW_GLYPH][0], FG[15], self.dump(s))
        self.assertEqual(s.bg[ROW_GLYPH][0], BG[1], self.dump(s))

    def test_a_character_set_glyph_is_translated(self):
        """a CP437 byte reaches the terminal as the character it means"""
        s = self.render()
        self.assertEqual(s.ch[ROW_GLYPH][1], "╔", self.dump(s))

    def test_a_bios_scroll_moves_only_its_own_window(self):
        """int 10h ah=06 scrolls the columns it was given and no others"""
        s = self.render()
        rows = [s.text(ROW_SCROLL + i) for i in range(3)]
        self.assertEqual(rows, ["BBBBBBBB", "CCCCCCCC", ""], self.dump(s))

    def test_the_cursor_ends_where_the_bios_put_it(self):
        """int 10h ah=02 moves the terminal's own cursor too"""
        s = self.render()
        self.assertEqual((s.y, s.x), (CURSOR_Y, CURSOR_X), self.dump(s))

    def test_the_window_title_names_dosemu(self):
        """the backend tells the terminal what it is running"""
        s = self.render()
        self.assertIsNotNone(s.title, self.dump(s))
        self.assertIn("dosemu2", s.title)

    def dump(self, s):
        """The whole screen, for when an assertion wants to explain itself."""
        return "\n" + "\n".join("%2d |%s|" % (i, s.text(i))
                                for i in range(s.rows))

    def render(self):
        """Run the probe once under a pty and read the screen back."""
        if self.__class__.screen is not None:
            return self.__class__.screen

        logfile = self.workdir / "dosemu.log"
        home = self.workdir / "home"
        rmtree(str(home), ignore_errors=True)
        home.mkdir()

        pid, fd = pty.fork()
        if pid == 0:
            env = dict(environ)
            env.update({
                "HOME": str(home),
                "DOSEMU2_COMCOM_DIR": str(self.workdir),
                "TERM": "xterm-256color",
                # the character set the backend translates CP437 into
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

        # DOS wants 25 lines, and a terminal that has fewer makes the
        # backend say so and render something else
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
        if "VID: Video set to Video_term" not in log and \
                b"\x1b[" not in out:
            self.skipTest("this build has no terminal plugin")

        screen = Screen()
        screen.feed(out)
        if screen.text(ROW_READY) != "READY":
            where = self.keepLog(out.decode("utf-8", "replace") + "\n" + log)
            self.fail("the probe never finished painting; bytes kept at %s"
                      % where)

        self.__class__.raw = out
        self.__class__.screen = screen
        return screen


if __name__ == "__main__":
    unittest.main(verbosity=2)
