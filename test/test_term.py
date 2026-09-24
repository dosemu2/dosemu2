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

import re
import unittest

from pathlib import Path
from shutil import rmtree
from subprocess import check_call, check_output, CalledProcessError, DEVNULL
from sys import argv
from tempfile import mkdtemp

from common_framework import (BaseTestCase, DOSEMU_CONF_DEFAULT,
                              main, main_setup, mark)

ROWS, COLS = 25, 80
RUN_TIMEOUT = 60

# The framework's own drive, plus the colour the rendering checks want.
CONF = DOSEMU_CONF_DEFAULT + '$_term_color = (1)\n'
TERM = "xterm-256color"

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

# The terminal the tests ask for.  Its terminfo entry has to name a mouse
# report prefix, or the backend will not offer mouse support at all.
MOUSE_TERM = "xterm"

# What xterm_mouse_init() puts on the terminal, and what
# xterm_mouse_close() is expected to take back off it.
MOUSE_ON = [b"\033[?9h", b"\033[?1000h", b"\033[?1002h", b"\033[?1003h"]
MOUSE_OFF = [b"\033[?9l", b"\033[?1000l", b"\033[?1002l", b"\033[?1003l"]

MOUSE_PROBE = r"""
; Say one word so the test knows DOS is up, then sit on a key that never
; comes.  Nothing is painted, so every escape sequence in the capture is
; one the backend put there of its own accord.
	org	0x100
	bits	16
	cpu	386

start:
	mov	ah, 0x09
	mov	dx, msg
	int	0x21
	mov	ah, 0x08		; wait for a key, quietly
	int	0x21
	mov	ax, 0x4c00
	int	0x21

msg	db	'MOUSEPROBE', 13, 10, '$'
"""

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


class TerminalRenderTestCase(BaseTestCase, unittest.TestCase):

    attrs = {'terminal'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "Terminal"
        # There is no DOS distribution to unpack: the probe assembled
        # below is handed to dosemu2 as its command interpreter, so
        # setUpClassPost() has nothing to do and is not called.
        cls.tarfile = ""
        # setUp() still lays out an imagedir, and the names it copies
        # from src/bindist are the ones a distribution class would have
        # set; the probe does not read either file.
        cls.autoexec = "dautoemu.bat"
        cls.confsys = "dconfig.sys"
        cls.probedir = Path(mkdtemp(prefix="termrender."))
        src = cls.probedir / "probe.asm"
        src.write_text(PROBE)
        # dosemu2 takes its command interpreter from DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.probedir / "command.com"), str(src)],
                       stdout=DEVNULL, stderr=DEVNULL)
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)
        cls.conf = cls.probedir / "dosemu.conf"
        cls.conf.write_text('$_term_color = (1)\n')
        cls.screen = None
        cls.raw = None

    @classmethod
    def tearDownClass(cls):
        rmtree(str(cls.probedir), ignore_errors=True)

    def test_0_basic_boot(self):
        """no DOS here: the probe is the command interpreter"""
        self.skipTest("this case installs no DOS distribution")

    @mark('terminal')
    def test_text_lands_where_dos_put_it(self):
        """a word written into the text screen comes out on the right row"""
        s = self.render()
        self.assertEqual(s.text(ROW_TEXT), "TERMPROBE", self.dump(s))

    @mark('terminal')
    def test_attributes_become_terminal_colours(self):
        """each of the fifteen visible attributes picks its own colour"""
        s = self.render()
        got = [s.fg[ROW_COLOUR][i] for i in range(15)]
        self.assertEqual(got, [FG[i + 1] for i in range(15)], self.dump(s))
        self.assertEqual(s.fg[ROW_GLYPH][0], FG[15], self.dump(s))
        self.assertEqual(s.bg[ROW_GLYPH][0], BG[1], self.dump(s))

    @mark('terminal')
    def test_a_character_set_glyph_is_translated(self):
        """a CP437 byte reaches the terminal as the character it means"""
        s = self.render()
        self.assertEqual(s.ch[ROW_GLYPH][1], "╔", self.dump(s))

    @mark('terminal')
    def test_a_bios_scroll_moves_only_its_own_window(self):
        """int 10h ah=06 scrolls the columns it was given and no others"""
        s = self.render()
        rows = [s.text(ROW_SCROLL + i) for i in range(3)]
        self.assertEqual(rows, ["BBBBBBBB", "CCCCCCCC", ""], self.dump(s))

    @mark('terminal')
    def test_the_cursor_ends_where_the_bios_put_it(self):
        """int 10h ah=02 moves the terminal's own cursor too"""
        s = self.render()
        self.assertEqual((s.y, s.x), (CURSOR_Y, CURSOR_X), self.dump(s))

    @mark('terminal')
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

        home = self.probedir / "home"
        rmtree(str(home), ignore_errors=True)
        home.mkdir()

        out = self.runDosemuRaw(
            ("-t", "-ks"), config=CONF, rows=ROWS, cols=COLS,
            until=b"READY", timeout=RUN_TIMEOUT,
            env={
                "HOME": str(home),
                "DOSEMU2_COMCOM_DIR": str(self.probedir),
                "TERM": TERM,
                # the character set the backend translates CP437 into
                "LC_ALL": "C.UTF-8",
            })

        log = self.boot_log()
        if "VID: Video set to Video_term" not in log and b"\x1b[" not in out:
            self.skipTest("this build has no terminal plugin")

        screen = Screen()
        screen.feed(out)
        if screen.text(ROW_READY) != "READY":
            self.fail("the probe never finished painting; the bytes are in "
                      "the output log")

        self.__class__.raw = out
        self.__class__.screen = screen
        return screen


class DumbModeMouseTestCase(BaseTestCase, unittest.TestCase):
    """Who gets the terminal's mouse switched on, and who does not.

    Dumb video mode draws no screen, so a mouse position in it means
    nothing, and gpm is already kept out of it.  The xterm mouse client is
    not: it rides in on the term plugin, which -kt loads for the keyboard
    alone, and puts the terminal into any-event tracking.  Every movement
    then comes back as a report that dumb mode has nobody to give to, and
    a terminal that does not speak SGR leaves the bytes on the screen
    (issue #2980).
    """

    attrs = {'terminal'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "TermMouse"
        cls.tarfile = ""
        cls.autoexec = "dautoemu.bat"
        cls.confsys = "dconfig.sys"
        cls.probedir = Path(mkdtemp(prefix="termmouse."))
        cls.runs = {}

    @classmethod
    def tearDownClass(cls):
        rmtree(str(cls.probedir), ignore_errors=True)

    def setUp(self):
        """Without a mouse prefix in terminfo the backend offers nothing."""
        super().setUp()
        try:
            kmous = check_output(["tput", "-T", MOUSE_TERM, "kmous"])
        except (OSError, CalledProcessError):
            kmous = b""
        if len(kmous) < 3 or not kmous.startswith(b"\033["):
            self.skipTest("terminfo entry for %s names no mouse"
                          % MOUSE_TERM)
        self.mkcom_with_nasm("command", MOUSE_PROBE, self.probedir)

    # no DOS here: the probe is the command interpreter, so the boot
    # test the base class brings has nothing to boot.  Unsetting it
    # keeps it out of the run rather than skipping it.
    test_0_basic_boot = None

    @mark('terminal')
    def test_terminal_mode_still_tracks_the_mouse(self):
        """-t has a screen to point at, so tracking belongs there"""
        out = self.capture("-t", "-kt")
        missing = [s for s in MOUSE_ON if s not in out]
        self.assertEqual(missing, [], self.dump(out))

    @mark('terminal')
    def test_dumb_mode_leaves_the_mouse_alone(self):
        """-td has no screen, so nothing may switch tracking on"""
        out = self.capture("-td", "-kt")
        unwanted = [s for s in MOUSE_ON if s in out]
        self.assertEqual(unwanted, [], self.dump(out))

    @mark('terminal')
    def test_tracking_is_taken_back_off_at_the_end(self):
        """what -t switched on is switched off again on the way out"""
        out = self.capture("-t", "-kt")
        missing = [s for s in MOUSE_OFF if s not in out]
        self.assertEqual(missing, [], self.dump(out))

    def dump(self, out):
        """The capture, for when an assertion wants to explain itself."""
        return "\n" + repr(out)

    def capture(self, *opts):
        """Run dosemu2 once under a pty and keep every byte it wrote."""
        if opts in self.__class__.runs:
            return self.__class__.runs[opts]

        out = self.runDosemuRaw(
            opts, config=CONF, rows=ROWS, cols=COLS,
            until=b"MOUSEPROBE", timeout=RUN_TIMEOUT,
            env={"DOSEMU2_COMCOM_DIR": str(self.probedir),
                 "TERM": MOUSE_TERM, "LC_ALL": "C.UTF-8"})
        self.__class__.runs[opts] = out
        return out


if __name__ == "__main__":
    cases = [
        TerminalRenderTestCase,
        DumbModeMouseTestCase,
    ]
    main(main_setup(cases))
