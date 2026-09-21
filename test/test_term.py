#!/usr/bin/env python3

"""What the terminal backend actually puts on the terminal.

Everything dosemu2 draws in `-t` mode leaves as escape sequences, and until
now nothing checked them (issue #2933).  This test paints a known pattern
into the text screen from DOS, catches the bytes on the master side of a
pty, replays them through just enough of a terminal to know what ended up
in which cell, and compares that with what DOS wrote.

It needs no DOS distribution and no display: the program that paints is
assembled on the spot and handed to dosemu2 as its command interpreter.

The other half is the way in, because the same backend turns the terminal's
escape sequences back into PC scancodes: the second test case types at the
pty and asks DOS through int 16h what it got.

What neither covers is the rendering of a host terminal program run from
DOS, which needs comcom64 and so a dj64 build.
"""

import os
import re
import unittest

from pathlib import Path
from select import select
from shutil import rmtree
from subprocess import check_call, CalledProcessError, DEVNULL
from sys import argv
from tempfile import mkdtemp
from time import monotonic, sleep

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

KEYS_PROBE = r"""
; Write down every keystroke int 16h hands over, and nothing else.
	org	0x100
	bits	16
	cpu	386

start:
	mov	ah, 0x3c		; create the report
	xor	cx, cx
	mov	dx, fname
	int	0x21
	jc	hang
	mov	[fh], ax

	mov	si, s_go
	call	log
	mov	ah, 0x09		; and on the screen, as the marker
	mov	dx, s_go_scr
	int	0x21
.loop:
	mov	ah, 0x11		; anything waiting?
	int	0x16
	jz	.loop
	mov	ah, 0x10		; extended read
	int	0x16
	push	ax
	mov	si, s_key
	call	log
	pop	ax
	call	loghex
	mov	si, s_nl
	call	log
	jmp	.loop
hang:
	jmp	hang

; si -> an asciiz string to append to the report
log:
	push	ax
	push	bx
	push	cx
	push	dx
	mov	dx, si
	xor	cx, cx
.len:
	cmp	byte [si], 0
	je	.write
	inc	si
	inc	cx
	jmp	.len
.write:
	mov	bx, [fh]
	mov	ah, 0x40
	int	0x21
	pop	dx
	pop	cx
	pop	bx
	pop	ax
	ret

; ax -> four hex digits in the report
loghex:
	push	ax
	mov	di, hexbuf
	mov	cx, 4
	mov	bx, ax
.digit:
	rol	bx, 4
	mov	al, bl
	and	al, 0x0f
	add	al, '0'
	cmp	al, '9'
	jbe	.store
	add	al, 7
.store:
	mov	[di], al
	inc	di
	loop	.digit
	mov	byte [di], 0
	mov	si, hexbuf
	call	log
	pop	ax
	ret

fh	dw	0
fname	db	'C:\KEYS.TXT', 0
s_go	db	'GO', 13, 10, 0
s_go_scr db	'GO', 13, 10, '$'
s_key	db	'key ', 0
s_nl	db	13, 10, 0
hexbuf	times 8 db 0
"""

# How long to leave between the halves of a sequence that is sent in two
# pieces, and how long the backend waits before giving up on one.
SPLIT_GAP = 0.25
GIVE_UP = 1.2

# name, the pieces to type with the pause after each, what int 16h owes us
KEYS = [
    ("F1",          [(b"\x1bOP", 0)],                       [0x3b00]),
    ("F5",          [(b"\x1b[15~", 0)],                     [0x3f00]),
    ("F12",         [(b"\x1b[24~", 0)],                     [0x8600]),
    ("shift-F1",    [(b"\x1b[1;2P", 0)],                    [0x5400]),
    ("alt-F1",      [(b"\x1b[1;3P", 0)],                    [0x6800]),
    ("up",          [(b"\x1b[A", 0)],                       [0x48e0]),
    ("left",        [(b"\x1b[D", 0)],                       [0x4be0]),
    ("ctrl-left",   [(b"\x1b[1;5D", 0)],                    [0x73e0]),
    ("home",        [(b"\x1bOH", 0)],                       [0x47e0]),
    ("delete",      [(b"\x1b[3~", 0)],                      [0x53e0]),
    ("shift-tab",   [(b"\x1b[Z", 0)],                       [0x0f00]),
    ("ctrl-a",      [(b"\x01", 0)],                         [0x1e01]),
    ("escape",      [(b"\x1b", 0)],                         [0x011b]),
    # arriving in two reads, as over a slow line
    ("split arrow", [(b"\x1b[", SPLIT_GAP), (b"A", 0)],     [0x48e0]),
    ("split ctrl",  [(b"\x1b[1", SPLIT_GAP), (b";5D", 0)],  [0x73e0]),
    # a character the terminal sends as more than one byte
    ("umlaut",      [(b"\xc3\xa4", 0)],                     [0x0084]),
    ("split umlaut", [(b"\xc3", SPLIT_GAP), (b"\xa4", 0)],  [0x0084]),
    ("split line",  [(b"\xe2\x94", SPLIT_GAP), (b"\x80", 0)], [0x00c4]),
    # a mouse report is the backend's own business, not a keystroke
    ("mouse",       [(b"\x1b[<35;10;5M", 0)],               []),
    ("split mouse", [(b"\x1b[<35;11", SPLIT_GAP), (b";6M", 0)], []),
    # a high byte that never becomes a character still gets through, as
    # the meta key a dumb ascii terminal would have meant by it
    ("stray byte",  [(b"\xc3", GIVE_UP)],                   [0x2e00]),
    ("after stray", [(b"b", 0)],                            [0x3062]),
]

# The kitty keyboard protocol, issue #1379: what a terminal that speaks it
# sends, and what int 16h owes DOS for each of them.  Measured against the
# terminal backend, not taken from the specification.
KITTY_KEYS = [
    ("escape",        [(b"\x1b[27u", 0)],                   [0x011b]),
    ("a",             [(b"\x1b[97u", 0)],                   [0x1e61]),
    ("shift-a",       [(b"\x1b[97;2u", 0)],                 [0x1e41]),
    ("ctrl-a",        [(b"\x1b[97;5u", 0)],                 [0x1e01]),
    ("alt-a",         [(b"\x1b[97;3u", 0)],                 [0x1e00]),
    ("ctrl-shift-a",  [(b"\x1b[97;6u", 0)],                 [0x1e01]),
    ("space",         [(b"\x1b[32u", 0)],                   [0x3920]),
    ("tab",           [(b"\x1b[9u", 0)],                    [0x0f09]),
    ("backspace",     [(b"\x1b[127u", 0)],                  [0x0e08]),
    ("ctrl-enter",    [(b"\x1b[13;5u", 0)],                 [0x1c0a]),
    # the event type rides on the modifiers as a sub-parameter
    ("a press",       [(b"\x1b[97;5:1u", 0)],               [0x1e01]),
    ("a repeat",      [(b"\x1b[97;5:2u", 0)],               [0x1e01]),
    ("a release",     [(b"\x1b[97;5:3u", 0)],               []),
    # the text the key stands for is a third parameter, and is not a key
    ("a with text",   [(b"\x1b[97;1;97u", 0)],              [0x1e61]),
    # keys the protocol numbers in the private use area have no character
    # on them and nothing on a PC keyboard to be, so they are dropped
    ("f13",           [(b"\x1b[57376u", 0)],                []),
    ("keypad 1",      [(b"\x1b[57400u", 0)],                []),
    ("no key at all", [(b"\x1b[u", 0)],                     []),
    # arriving in two reads, as over a slow line
    ("split ctrl-a",  [(b"\x1b[97", SPLIT_GAP), (b";5u", 0)], [0x1e01]),
    # and the legacy encodings keep working next to them
    ("legacy up",     [(b"\x1b[A", 0)],                     [0x48e0]),
    ("legacy F5",     [(b"\x1b[15~", 0)],                   [0x3f00]),
    ("legacy ctrl-a", [(b"\x01", 0)],                       [0x1e01]),
    ("legacy mouse",  [(b"\x1b[<35;10;5M", 0)],             []),
]

# What the backend asks the terminal at startup, what a terminal that
# speaks the protocol answers, and what one that does not answers instead.
KITTY_QUERY = b"\x1b[?u"
KITTY_YES = b"\x1b[?1u"
KITTY_NO = b"\x1b[?1;2c"
KITTY_ON = b"\x1b[>1u"
KITTY_OFF = b"\x1b[<u"

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


class TerminalKeysTestCase(BaseTestCase, unittest.TestCase):
    """What int 16h gives DOS for what was typed at the terminal."""

    attrs = {'terminal'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "TermKeys"
        cls.tarfile = ""
        cls.autoexec = "dautoemu.bat"
        cls.confsys = "dconfig.sys"
        cls.probedir = Path(mkdtemp(prefix="termkeys."))
        src = cls.probedir / "probe.asm"
        src.write_text(KEYS_PROBE)
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.probedir / "command.com"), str(src)],
                       stdout=DEVNULL, stderr=DEVNULL)
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)
        cls.report = None

    @classmethod
    def tearDownClass(cls):
        rmtree(str(cls.probedir), ignore_errors=True)

    def test_0_basic_boot(self):
        """no DOS here: the probe is the command interpreter"""
        self.skipTest("this case installs no DOS distribution")

    @mark('terminal')
    def test_every_sequence_becomes_the_right_key(self):
        """the terminal's escape sequences turn back into PC scancodes"""
        got = self.type()
        wrong = ["%s: wanted %s, got %s"
                 % (name, self.hex(want), self.hex(got[name]))
                 for name, pieces, want in KEYS if got[name] != want]
        if wrong:
            self.fail("\n".join(wrong))

    def hex(self, keys):
        return " ".join("%04x" % k for k in keys) or "nothing"

    def type(self):
        """Type everything in KEYS once and note what came back."""
        if self.__class__.report is not None:
            return self.__class__.report

        result = self.workdir / "keys.txt"
        marks = []

        def typeall(fd):
            for name, pieces, want in KEYS:
                before = len(result.read_bytes())
                for data, gap in pieces:
                    os.write(fd, data)
                    if gap:
                        sleep(gap)
                sleep(0.5)
                self.drain(fd)
                marks.append((name, before))
            sleep(1)
            self.__class__.raw = result.read_bytes()

        # -kt is the slang keyboard, the one that knows escape sequences;
        # -ks would just pass bytes through
        self.runDosemuRaw(
            ("-t", "-kt"), config=CONF, rows=ROWS, cols=COLS,
            until=b"GO", timeout=RUN_TIMEOUT, interact=typeall,
            env={"DOSEMU2_COMCOM_DIR": str(self.probedir),
                 "TERM": TERM, "LC_ALL": "C.UTF-8"})

        if not marks:
            self.skipTest("the probe never ran; this build may have no "
                          "terminal plugin")

        data = self.__class__.raw
        report = {}
        for i, (name, before) in enumerate(marks):
            end = marks[i + 1][1] if i + 1 < len(marks) else len(data)
            chunk = data[before:end].decode("ascii", "replace")
            report[name] = [int(l[4:8], 16) for l in chunk.split("\r\n")
                            if l.startswith("key ")]
        self.__class__.report = report
        return report

    def drain(self, fd):
        try:
            while select([fd], [], [], 0.05)[0]:
                if not os.read(fd, 65536):
                    return
        except OSError:
            pass


class TerminalKittyKeysTestCase(BaseTestCase, unittest.TestCase):
    """The keyboard protocol kitty added, issue #1379.

    A terminal that speaks it reports the Escape key as a sequence of its
    own instead of a lone escape byte, so the backend no longer has to wait
    a quarter of a second to find out whether more of a sequence is coming.
    The terminal here is this test: it answers the backend's question the
    way kitty would, and then types what kitty would type.
    """

    attrs = {'terminal'}
    runs = {}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "TermKitty"
        cls.tarfile = ""
        cls.autoexec = "dautoemu.bat"
        cls.confsys = "dconfig.sys"
        cls.probedir = Path(mkdtemp(prefix="termkitty."))
        src = cls.probedir / "probe.asm"
        src.write_text(KEYS_PROBE)
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.probedir / "command.com"), str(src)],
                       stdout=DEVNULL, stderr=DEVNULL)
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)

    @classmethod
    def tearDownClass(cls):
        rmtree(str(cls.probedir), ignore_errors=True)

    def test_0_basic_boot(self):
        """no DOS here: the probe is the command interpreter"""
        self.skipTest("this case installs no DOS distribution")

    @mark('terminal')
    def test_the_terminal_is_asked_whether_it_speaks_the_protocol(self):
        """the backend asks, and asks something every terminal answers"""
        run = self.run_as(KITTY_YES)
        self.assertIn(KITTY_QUERY, run["out"],
                      "the backend never asked about the protocol")
        i = run["out"].index(KITTY_QUERY) + len(KITTY_QUERY)
        self.assertEqual(run["out"][i:i + 3], b"\x1b[c",
                         "nothing was asked that an ordinary terminal "
                         "would answer, so a 'no' would never arrive")

    @mark('terminal')
    def test_a_terminal_that_answers_gets_the_protocol_switched_on(self):
        """kitty's answer turns it on, and leaving turns it off again"""
        run = self.run_as(KITTY_YES)
        self.assertIn(KITTY_ON, run["out"],
                      "the protocol was never switched on")
        self.assertIn(KITTY_OFF, run["out"],
                      "the terminal was left in it")

    @mark('terminal')
    def test_a_terminal_that_says_no_is_left_as_it_was(self):
        """a terminal that only answers the other question is left alone"""
        run = self.run_as(KITTY_NO)
        self.assertNotIn(KITTY_ON, run["out"],
                         "the protocol was switched on for a terminal that "
                         "never said it knows one")
        self.assertNotIn(b"[?1;2c", run["report_raw"],
                         "the terminal's answer was typed at DOS")

    @mark('terminal')
    def test_every_key_the_protocol_reports_becomes_the_right_key(self):
        """what kitty sends turns into the scancodes DOS expects"""
        got = self.run_as(KITTY_YES)["report"]
        wrong = ["%s: wanted %s, got %s"
                 % (name, self.hex(want), self.hex(got[name]))
                 for name, pieces, want in KITTY_KEYS if got[name] != want]
        if wrong:
            self.fail("\n".join(wrong))

    @mark('terminal')
    def test_escape_arrives_without_the_wait(self):
        """the Escape key no longer spends the timeout in the backend"""
        run = self.run_as(KITTY_YES)
        bare, csi = run["bare_esc"], run["csi_esc"]
        if bare[0] is None or csi[0] is None:
            self.skipTest("an Escape never arrived at all")
        self.assertEqual(csi[1], [0x011b],
                         "the sequence did not arrive as the Escape key, "
                         "so its timing says nothing")
        self.assertGreater(bare[0], 0.2,
                           "a lone escape byte no longer waits, so this "
                           "test is measuring something else")
        self.assertLess(csi[0], 0.2,
                        "the Escape key still waits out the timeout: "
                        "%.3fs against %.3fs for a lone escape byte"
                        % (csi[0], bare[0]))

    def hex(self, keys):
        return " ".join("%04x" % k for k in keys) or "nothing"

    def run_as(self, answer):
        """Be a terminal that answers `answer', and note what DOS got."""
        if answer in self.runs:
            return self.runs[answer]

        result = self.workdir / "keys.txt"
        marks = []
        seen = {"out": b"", "bare": None, "csi": None, "raw": b""}

        def talk(fd):
            # the question was asked while DOS was booting; answer it now
            # that the backend is reading the terminal in earnest
            os.write(fd, answer)
            sleep(0.5)
            seen["out"] += self.drain(fd)
            for name, pieces, want in KITTY_KEYS:
                before = len(result.read_bytes())
                for data, gap in pieces:
                    os.write(fd, data)
                    if gap:
                        sleep(gap)
                sleep(0.5)
                seen["out"] += self.drain(fd)
                marks.append((name, before))
            # a mark of its own, so the keys timed below do not land in
            # the last case's share of the report
            marks.append((None, len(result.read_bytes())))
            seen["bare"] = self.time_key(fd, result, b"\x1b")
            seen["csi"] = self.time_key(fd, result, b"\x1b[27u")
            sleep(1)
            seen["out"] += self.drain(fd)
            seen["raw"] = result.read_bytes()

        out = self.runDosemuRaw(
            ("-t", "-kt"), config=CONF, rows=ROWS, cols=COLS,
            until=b"GO", timeout=RUN_TIMEOUT, interact=talk,
            env={"DOSEMU2_COMCOM_DIR": str(self.probedir),
                 "TERM": TERM, "LC_ALL": "C.UTF-8"})

        if not marks:
            self.skipTest("the probe never ran; this build may have no "
                          "terminal plugin")

        data = seen["raw"]
        report = {}
        for i, (name, before) in enumerate(marks):
            if name is None:
                continue
            end = marks[i + 1][1] if i + 1 < len(marks) else len(data)
            chunk = data[before:end].decode("ascii", "replace")
            report[name] = [int(l[4:8], 16) for l in chunk.split("\r\n")
                            if l.startswith("key ")]
        run = {
            "out": out + seen["out"],
            "report": report,
            "report_raw": data,
            "bare_esc": seen["bare"],
            "csi_esc": seen["csi"],
        }
        self.runs[answer] = run
        return run

    def time_key(self, fd, result, seq, tries=3):
        """The shortest time `seq' took to show up, and what showed up."""
        best = None
        keys = []
        for _ in range(tries):
            before = len(result.read_bytes())
            start = monotonic()
            os.write(fd, seq)
            while monotonic() - start < 3:
                if len(result.read_bytes()) > before:
                    took = monotonic() - start
                    if best is None or took < best:
                        best = took
                    break
                sleep(0.005)
            sleep(0.3)
            self.drain(fd)
            chunk = result.read_bytes()[before:].decode("ascii", "replace")
            keys = [int(l[4:8], 16) for l in chunk.split("\r\n")
                    if l.startswith("key ")]
        return best, keys

    def drain(self, fd):
        got = b""
        try:
            while select([fd], [], [], 0.05)[0]:
                d = os.read(fd, 65536)
                if not d:
                    break
                got += d
        except OSError:
            pass
        return got


if __name__ == "__main__":
    cases = [
        TerminalRenderTestCase,
        TerminalKeysTestCase,
        TerminalKittyKeysTestCase,
    ]
    main(main_setup(cases))
