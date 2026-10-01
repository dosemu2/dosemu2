#!/usr/bin/env python3

"""What the terminal backend actually puts on the terminal.

Everything dosemu2 draws in `-t` mode leaves as escape sequences, and until
now nothing checked them (issue #2933).  This test paints a known pattern
into the text screen from DOS, catches the bytes on the master side of a
pty, replays them through just enough of a terminal to know what ended up
in which cell, and compares that with what DOS wrote.

It needs no DOS distribution and no display: the program that paints is
assembled on the spot and handed to dosemu2 as its command interpreter.

A second case takes the other direction: it runs a host terminal program
from DOS through unix.com, so what the program draws crosses a pty, the DOS
screen and the backend before the same terminal reads it back.

The way in is the same backend turning the terminal's escape sequences back
into PC scancodes: a third case types at the pty and asks DOS through
int 16h what it got.
"""

import unittest
from shutil import which
from subprocess import check_output, CalledProcessError, DEVNULL
from time import monotonic, sleep

try:
    import pyte
except ImportError:
    pyte = None

from common_framework import (BaseTestCase, DOSEMU_CONF_DEFAULT,
                              main, main_setup, mark)

# 0.8.2 is where pyte learned blink and gave the bright colours names of
# their own, and the rendering cases read both off its cells.  Ubuntu 24.04
# packages 0.8.0, so say what is missing rather than fail obscurely.  The
# blink field is the test, not the version string: a distribution package
# can be importable with no metadata to ask.
PYTE_NEEDED = "0.8.2"
HAVE_PYTE = pyte is not None and "blink" in pyte.screens.Char._fields
PYTE_WHY = ("needs pyte %s or newer for blink and the bright colours "
            "(installed: %%s), pip install pyte==%s" % (PYTE_NEEDED,
                                                        PYTE_NEEDED))


def pyte_version():
    """The version pip knows about, for the skip message."""
    try:
        from importlib.metadata import version

        return version("pyte")
    except Exception:
        return "unknown" if pyte else "none installed"


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

# The colour the terminal backend is expected to pick for each of the
# sixteen DOS attribute values, named as pyte names it.  The low three bits
# are in the other order than ANSI wants them, which is what the rotation
# in terminal.c is for; the top bit is the bright half, SGR 90-97, which
# pyte reports with a "bright" prefix.
FG = {0: "black", 1: "blue", 2: "green", 3: "cyan",
      4: "red", 5: "magenta", 6: "brown", 7: "white",
      8: "brightblack", 9: "brightblue", 10: "brightgreen", 11: "brightcyan",
      12: "brightred", 13: "brightmagenta", 14: "brightbrown",
      15: "brightwhite"}
BG = {0: "black", 1: "blue", 2: "green", 3: "cyan",
      4: "red", 5: "magenta", 6: "brown", 7: "white"}

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
# pieces, comfortably inside the 250ms the backend waits, and how long to
# leave to make it give up on one.
SPLIT_GAP = 0.1
GIVE_UP = 1.2
# How long a key is given to come out of int 16h, and how long one that
# must not arrive is watched for.  The first is a ceiling only: the answer
# is waited for, not slept through.
ANSWER_WAIT = 1.5
SILENCE = 0.5
POLL = 0.02

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
    ("line",        [(b"\xe2\x94\x80", 0)],                 [0x00c4]),
    # a mouse report is the backend's own business, not a keystroke
    ("mouse",       [(b"\x1b[<35;10;5M", 0)],               []),
    ("split mouse", [(b"\x1b[<35;11", SPLIT_GAP), (b";6M", 0)], []),
    # a high byte that never becomes a character gets through as the meta
    # key a dumb ascii terminal would have meant by it, and the character
    # typed after it still has to arrive: mbrtowc() keeps what it could not
    # finish, and the next call must not read it as a continuation
    ("stray byte",  [(b"\xc3", GIVE_UP)],                   [0x2e00]),
    ("umlaut after", [(b"\xc3\xa4", 0)],                    [0x0084]),
    ("after stray", [(b"b", 0)],                            [0x3062]),
]


class Capture:
    """An `interact` that keeps the page as the program left it.

    What runDosemuRaw() returns is no good for the screen model: it
    includes dosemu2's teardown, which ends with a newline on the last
    row, and the model scrolls by one when it is fed that. READY moves
    from row 20 to 19, the painted rows move with it, and the case fails
    with "the probe never finished painting". How much of the teardown
    is in there depends on what the pty hands over before it closes,
    which is why the cases used to fail only now and then.

    `interact` runs once the marker has been seen and before the run is
    ended, so what is taken here is the page and nothing after it.
    `wait` is for a page whose last write is not the marker itself, such
    as the prompt DOS puts up after the marker was echoed.
    """

    def __init__(self, wait=None):
        self.out = b""
        self.wait = wait

    def __call__(self, conv):
        if self.wait is not None and self.wait not in conv.captured:
            conv.expect(self.wait)
        self.out = conv.captured


class Screen:
    """What ended up in which cell, read back off a terminal.

    The terminal is pyte's, so every sequence a real one knows is
    handled - tabs, insert and delete of characters, margins, the
    wrapping - rather than the subset a test would think to write down.
    Its cells carry the colour by name, the blink flag among them, which
    is what the rendering cases compare against.
    """

    def __init__(self, rows=ROWS, cols=COLS):
        self.rows, self.cols = rows, cols
        self.term = pyte.Screen(cols, rows)
        self.stream = pyte.ByteStream(self.term)

    def feed(self, data):
        self.stream.feed(data)

    @property
    def title(self):
        return self.term.title or None

    @property
    def y(self):
        return self.term.cursor.y

    @property
    def x(self):
        return self.term.cursor.x

    def cell(self, row, col):
        return self.term.buffer[row][col]

    def cells(self, pick):
        return [[pick(self.cell(y, x)) for x in range(self.cols)]
                for y in range(self.rows)]

    @property
    def ch(self):
        return self.cells(lambda c: c.data)

    @property
    def fg(self):
        return self.cells(lambda c: c.fg)

    @property
    def bg(self):
        return self.cells(lambda c: c.bg)

    @property
    def bl(self):
        return self.cells(lambda c: c.blink)

    def text(self, row):
        return self.term.display[row].rstrip()

    def dump(self):
        """The whole screen, for when an assertion wants to explain itself."""
        return "\n" + "\n".join("%2d |%s|" % (i, self.text(i))
                                 for i in range(self.rows))


class SharedRun(object):
    """One run read by every test of a case.

    The framework looks at each test's own dosemu and output logs when it
    fails, so a test that took the screen from the cache has to be given
    them; without that a failure has nothing to show.
    """

    def relog(self):
        self.logfiles['xpt'][1] = "output.log"
        self.logfiles['xpt'][0].write_bytes(self.__class__.raw)
        self.logfiles['log'][0].write_text(self.__class__.bootlog)


class TerminalRenderTestCase(SharedRun, BaseTestCase, unittest.TestCase):

    attrs = {'terminal'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "Terminal"
        if not HAVE_PYTE:
            raise unittest.SkipTest(PYTE_WHY % pyte_version())
        # There is no DOS distribution to unpack
        cls.tarfile = ""
        cls.screen = None
        cls.raw = None
        cls.bootlog = ""

    test_0_basic_boot = None

    @mark('terminal')
    def test_text_lands_where_dos_put_it(self):
        """a word written into the text screen comes out on the right row"""
        s = self.render()
        self.assertEqual(s.text(ROW_TEXT), "TERMPROBE", s.dump())

    @mark('terminal')
    def test_attributes_become_terminal_colours(self):
        """each of the fifteen visible attributes picks its own colour"""
        s = self.render()
        got = [s.fg[ROW_COLOUR][i] for i in range(15)]
        self.assertEqual(got, [FG[i + 1] for i in range(15)], s.dump())
        self.assertEqual(s.fg[ROW_GLYPH][0], FG[15], s.dump())
        self.assertEqual(s.bg[ROW_GLYPH][0], BG[1], s.dump())

    @mark('terminal')
    def test_a_character_set_glyph_is_translated(self):
        """a CP437 byte reaches the terminal as the character it means"""
        s = self.render()
        self.assertEqual(s.ch[ROW_GLYPH][1], "╔", s.dump())

    @mark('terminal')
    def test_a_bios_scroll_moves_only_its_own_window(self):
        """int 10h ah=06 scrolls the columns it was given and no others"""
        s = self.render()
        rows = [s.text(ROW_SCROLL + i) for i in range(3)]
        self.assertEqual(rows, ["BBBBBBBB", "CCCCCCCC", ""], s.dump())

    @mark('terminal')
    def test_the_cursor_ends_where_the_bios_put_it(self):
        """int 10h ah=02 moves the terminal's own cursor too"""
        s = self.render()
        self.assertEqual((s.y, s.x), (CURSOR_Y, CURSOR_X), s.dump())

    @mark('terminal')
    def test_the_window_title_names_dosemu(self):
        """the backend tells the terminal what it is running"""
        s = self.render()
        self.assertIsNotNone(s.title, s.dump())
        self.assertIn("dosemu2", s.title)

    def render(self):
        """Run the probe once under a pty and read the screen back.

        The probe paints the same screen every time, so one run serves the
        whole case; relog() keeps every test's logs in place.
        """
        if self.__class__.screen is not None:
            self.relog()
            return self.__class__.screen

        home = self.imagedir / "home"
        home.mkdir()

        # dosemu2 takes its command interpreter from DOSEMU2_COMCOM_DIR
        self.mkcom_with_nasm("command", PROBE)

        page = Capture()
        out = self.runDosemuRaw(
            ("-t", "-ks"), config=CONF, rows=ROWS, cols=COLS,
            until=b"READY", timeout=RUN_TIMEOUT, interact=page,
            env={
                "HOME": str(home),
                "DOSEMU2_COMCOM_DIR": str(self.workdir),
                "TERM": TERM,
                # the character set the backend translates CP437 into
                "LC_ALL": "C.UTF-8",
            })

        log = self.boot_log()
        self.__class__.bootlog = log
        if "VID: Video set to Video_term" not in log and b"\x1b[" not in out:
            self.skipTest("this build has no terminal plugin")

        screen = Screen()
        screen.feed(page.out)
        if screen.text(ROW_READY) != "READY":
            self.fail("the probe never finished painting; the bytes are in "
                      "the output log")

        self.__class__.raw = out
        self.__class__.screen = screen
        return screen


# The host program to run from DOS.  aainfo draws a page with its slang
# driver and leaves on its own, which is all this needs; it is not installed
# everywhere, so the case skips when it is missing.
HOST_PROG = "aainfo"
HOST_DONE = "HOSTDONE"

# comcom32 hands its children TERM=djgpp, the ncurses entry for the ANSI
# emulation DOS has, which is what the program should be drawing for.  It
# comes with ncurses-term, and without it the program says so and quits.
DOS_TERM = "djgpp"

# aalib picks its X11 driver whenever DISPLAY is set, and then it draws in
# a window of its own, which says nothing about the DOS screen.  Name the
# driver instead, so what the test measures does not depend on whether the
# host it runs on has a display.
AAOPTS = "-driver slang"


class TerminalHostProgramTestCase(SharedRun, BaseTestCase,
                                  unittest.TestCase):
    """A host terminal program run from DOS, drawn on the DOS screen.

    unix.com gives the program a pty, dos2linux pours what it writes into
    the DOS screen, and the backend puts that on the terminal.  A page that
    a curses-like library drew on the host therefore has to arrive as the
    same page here.
    """

    attrs = {'terminal'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "TermHost"
        if not HAVE_PYTE:
            raise unittest.SkipTest(PYTE_WHY % pyte_version())
        # There is no DOS distribution to unpack
        cls.tarfile = ""
        cls.autoexec = "fdppauto.bat"
        cls.confsys = "fdppconf.sys"
        cls.prog = which(HOST_PROG)
        cls.screen = None
        cls.raw = None
        cls.bootlog = ""

    test_0_basic_boot = None

    @mark('terminal')
    def test_what_the_host_program_drew_arrives(self):
        """the page a host program draws comes out on the terminal"""
        s = self.host()
        text = s.dump()
        self.assertRegex(text, r"AAlib version:\d", text)
        self.assertIn("Display:", text)
        self.assertIn("Keyboard:", text)

    @mark('terminal')
    def test_the_host_program_talks_to_a_terminal(self):
        """it finds a terminal driver, so TERM reached it intact"""
        s = self.host()
        text = s.dump()
        self.assertRegex(text, r"Current driver:Slang driver", text)
        # whichever of the two aalib has, as long as it is not the X11 one
        self.assertRegex(text, r"Current driver:(Slang|Curses) keyboard driver",
                         text)

    @mark('terminal')
    def test_the_host_program_sees_the_dos_screen_size(self):
        """the size DOS has is the size the program draws for"""
        s = self.host()
        text = s.dump()
        self.assertRegex(text, r"Width\s+:%i\b" % COLS, text)
        self.assertRegex(text, r"Height\s+:%i\b" % (ROWS - 1), text)

    @mark('terminal')
    def test_dos_is_still_there_afterwards(self):
        """the program leaves and DOS carries on writing"""
        s = self.host()
        text = s.dump()
        rows = [s.text(y) for y in range(ROWS)]
        self.assertIn(HOST_DONE, rows, text)
        self.assertIn("C:\\>", rows, text)

    def host(self):
        """Run the host program once under a pty and read the screen back."""
        if self.__class__.screen is not None:
            self.relog()
            return self.__class__.screen

        if self.prog is None:
            self.skipTest("%s is not installed" % HOST_PROG)

        # cls, so the boot messages are not part of the page being read
        self.mkfile(self.autoexec,
                    f"cls\nunix {HOST_PROG}\necho {HOST_DONE}\n",
                    mode="a", newline="\r\n")

        config = CONF + f'$_unix_exec = "{self.prog}"\n$_sound = (0)\n'

        # the prompt DOS puts up after the echo is part of the page
        page = Capture(wait=b"C:\\>")
        out = self.runDosemuRaw(
            ("-t", "-ks"), config=config, rows=ROWS, cols=COLS,
            until=HOST_DONE.encode(), timeout=RUN_TIMEOUT, interact=page,
            env={"TERM": TERM, "LC_ALL": "C.UTF-8", "AAOPTS": AAOPTS})

        log = self.boot_log()
        self.__class__.bootlog = log
        if "VID: Video set to Video_term" not in log and b"\x1b[" not in out:
            self.skipTest("this build has no terminal plugin")
        if b"Unknown terminal" in out:
            self.skipTest("no terminfo entry for %s; install ncurses-term"
                          % DOS_TERM)

        screen = Screen()
        screen.feed(page.out)
        self.__class__.raw = out
        self.__class__.screen = screen
        return screen


# The third case is about the other meaning of the attribute byte's top bit:
# int 10h ax=1003h lets the program choose between blinking text and a bright
# background (issue #1077).  The probe paints the same two rows under each
# setting, so this case runs twice and compares the two screens.
ROW_BRIGHT = 4          # attribute f0: black on bright white, or blinking
ROW_PLAIN = 6           # attribute 1f, the top bit clear
RUN = 8                 # cells per row, long enough not to occur by chance

BLINK_PROBE = r"""
; Paint two rows the test on the other side of the pty knows by heart,
; under the blink setting this build of the probe asks for, then sit still.
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

	mov	di, (%(ROW_BRIGHT)d * COLS) * 2
	mov	cx, %(RUN)d
	mov	ax, 0xf000 + '@'
	rep	stosw

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
	jz	.spin
	stosw
	jmp	.puts

.spin:
	mov	ah, 0x00		; DOS is unhappy if we ever return
	int	0x16
	jmp	.spin

ready	db	'READY', 0
"""

# A bright background only reaches the wire where the terminal has sixteen of
# them; blinking needs no such thing.
BG_BRIGHT_WHITE = "brightwhite"


def terminal_colours(term):
    """How many colours the terminfo entry offers, 0 if there is none.

    tput is asked in a process of its own: curses.setupterm() keeps the
    first entry it was given for the life of the process and would answer
    for that one instead.
    """
    try:
        return int(check_output(["tput", f"-T{term}", "colors"], stderr=DEVNULL))
    except (CalledProcessError, FileNotFoundError, ValueError):
        return 0


class TerminalBlinkTestCase(SharedRun, BaseTestCase, unittest.TestCase):

    attrs = {'terminal'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "TermBlink"
        if not HAVE_PYTE:
            raise unittest.SkipTest(PYTE_WHY % pyte_version())
        # There is no DOS distribution to unpack
        cls.tarfile = ""
        cls.screens = {}
        cls.raws = {}
        cls.bootlogs = {}
        cls.colors = terminal_colours(TERM)

    test_0_basic_boot = None

    @mark('terminal')
    def test_blinking_is_asked_for_when_the_program_wants_it(self):
        """with blinking chosen the top bit becomes blink, not a colour"""
        s = self.render(1)
        text = s.dump()
        self.assertTrue(s.bl[ROW_BRIGHT][0], text)
        self.assertEqual(s.bg[ROW_BRIGHT][0], BG[7], text)
        self.assertEqual(s.fg[ROW_BRIGHT][0], FG[0], text)

    @mark('terminal')
    def test_a_bright_background_is_used_when_the_program_wants_it(self):
        """with a bright background chosen the top bit becomes a colour"""
        if self.colors < 16:
            self.skipTest(f"{TERM} here has {self.colors} colours, too few for a bright background")
        s = self.render(0)
        text = s.dump()
        self.assertEqual(s.bg[ROW_BRIGHT][0], BG_BRIGHT_WHITE, text)
        self.assertEqual(s.fg[ROW_BRIGHT][0], FG[0], text)

    @mark('terminal')
    def test_a_bright_background_never_blinks(self):
        """choosing a bright background turns blinking off"""
        s = self.render(0)
        self.assertFalse(s.bl[ROW_BRIGHT][0], s.dump())

    @mark('terminal')
    def test_the_setting_changes_what_is_rendered(self):
        """the 1003h setting changes what is rendered: BL=0 colours, BL=1 blinks"""
        def cell(s):
            return (s.fg[ROW_BRIGHT][0], s.bg[ROW_BRIGHT][0],
                    s.bl[ROW_BRIGHT][0])
        self.assertNotEqual(cell(self.render(0)), cell(self.render(1)))

    @mark('terminal')
    def test_an_attribute_below_the_top_bit_is_left_alone(self):
        """an attribute below the top bit is left alone by both BL=0 and BL=1"""
        for bl in (0, 1):
            s = self.render(bl)
            text = s.dump()
            self.assertEqual(s.fg[ROW_PLAIN][0], FG[15], (bl, text))
            self.assertEqual(s.bg[ROW_PLAIN][0], BG[1], (bl, text))
            self.assertFalse(s.bl[ROW_PLAIN][0], (bl, text))

    def render(self, bl):
        """Run the probe once under one setting and read the screen back."""
        if bl in self.__class__.screens:
            self.recall(bl)
            return self.__class__.screens[bl]

        home = self.imagedir / f"home{bl}"
        home.mkdir()

        self.mkcom_with_nasm("command", BLINK_PROBE % dict(
            BL=bl, ROW_BRIGHT=ROW_BRIGHT, ROW_PLAIN=ROW_PLAIN,
            ROW_READY=ROW_READY, RUN=RUN))

        page = Capture()
        out = self.runDosemuRaw(
            ("-t", "-ks"), config=CONF, rows=ROWS, cols=COLS,
            until=b"READY", timeout=RUN_TIMEOUT, interact=page,
            env={
                "HOME": str(home),
                "DOSEMU2_COMCOM_DIR": str(self.workdir),
                "TERM": TERM,
                "LC_ALL": "C.UTF-8",
            })

        log = self.boot_log()
        if "VID: Video set to Video_term" not in log and b"\x1b[" not in out:
            self.skipTest("this build has no terminal plugin")

        screen = Screen()
        screen.feed(page.out)
        if screen.text(ROW_READY) != "READY":
            self.fail("the probe never finished painting; the bytes are in the output log")

        self.__class__.raws[bl] = out
        self.__class__.bootlogs[bl] = log
        self.__class__.screens[bl] = screen
        return screen

    def recall(self, bl):
        """Put the logs of the cached run back for this test to fail with."""
        self.__class__.raw = self.__class__.raws[bl]
        self.__class__.bootlog = self.__class__.bootlogs[bl]
        self.relog()


class TerminalKeysTestCase(BaseTestCase, unittest.TestCase):
    """What int 16h gives DOS for what was typed at the terminal."""

    attrs = {'terminal'}

    @classmethod
    def setUpClass(cls):
        super().setUpClass()
        cls.prettyname = "TermKeys"
        # There is no DOS distribution to unpack
        cls.tarfile = ""
        cls.report = None
        cls.keys = []
        cls.raw = b""
        cls.bootlog = ""

    test_0_basic_boot = None

    @mark('terminal')
    def test_every_sequence_becomes_the_right_key(self):
        """the terminal's escape sequences turn back into PC scancodes"""
        got, report = self.type()
        want = [k for name, pieces, keys in KEYS for k in keys]
        if got == want:
            return
        # The keys arrive in the order they were typed, so the stream is what
        # the run is judged on: a key the backend hands over a moment late
        # belongs to the case that typed it, not to the one being typed when
        # it lands. The per case split is only to say where it went wrong.
        lines = ["wanted %s" % self.hex(want), "got    %s" % self.hex(got)]
        lines += ["%s: wanted %s, saw %s"
                  % (name, self.hex(keys), self.hex(report[name]))
                  for name, pieces, keys in KEYS if report[name] != keys]
        self.fail("\n".join(lines))

    def hex(self, keys):
        return " ".join("%04x" % k for k in keys) or "nothing"

    def relog(self):
        """Hand this test the logs of the shared run."""
        self.logfiles['xpt'][1] = "output.log"
        self.logfiles['xpt'][0].write_bytes(self.__class__.raw)
        self.logfiles['log'][0].write_text(self.__class__.bootlog)

    def type(self):
        """Type everything in KEYS once and note what came back.

        Both the whole stream of keys, in the order they arrived, and the
        split of it per case.
        """
        if self.__class__.report is not None:
            self.relog()
            return self.__class__.keys, self.__class__.report

        # dosemu2 takes its command interpreter from DOSEMU2_COMCOM_DIR
        self.mkcom_with_nasm("command", KEYS_PROBE)

        result = self.workdir / "keys.txt"
        marks = []

        def answered(before):
            """A whole line has been added to the report since `before`."""
            data = result.read_bytes()
            return len(data) > before and data.endswith(b"\r\n")

        def settle(t, before, expect):
            """Wait for the key to be written down, or for nothing to be.

            A key that is owed is waited for rather than slept over, which
            is most of what this test used to spend its time on; one that
            must not arrive has to be watched for the whole window, since
            there is nothing to wait for.
            """
            deadline = monotonic() + (ANSWER_WAIT if expect else SILENCE)
            while monotonic() < deadline:
                t.read(POLL)
                if expect and answered(before):
                    return

        def typeall(t):
            if not t.ready:
                return
            for name, pieces, want in KEYS:
                before = len(result.read_bytes())
                for data, gap in pieces:
                    t.write(data)
                    if gap:
                        sleep(gap)
                settle(t, before, bool(want))
                marks.append((name, before))
            t.read()
            self.__class__.raw = result.read_bytes()

        # -kt is the slang keyboard, the one that knows escape sequences;
        # -ks would just pass bytes through
        self.runDosemuRaw(
            ("-t", "-kt"), config=CONF, rows=ROWS, cols=COLS,
            until=b"GO", timeout=RUN_TIMEOUT, interact=typeall,
            env={"DOSEMU2_COMCOM_DIR": str(self.workdir),
                 "TERM": TERM, "LC_ALL": "C.UTF-8"})

        self.__class__.bootlog = self.boot_log()
        if not marks:
            self.skipTest("the probe never ran; this build may have no "
                          "terminal plugin")

        data = self.__class__.raw

        def keys_in(text):
            return [int(l[4:8], 16) for l in text.split("\r\n")
                    if l.startswith("key ")]

        report = {}
        for i, (name, before) in enumerate(marks):
            end = marks[i + 1][1] if i + 1 < len(marks) else len(data)
            report[name] = keys_in(data[before:end].decode("ascii", "replace"))
        self.__class__.report = report
        self.__class__.keys = keys_in(data.decode("ascii", "replace"))
        return self.__class__.keys, report


# The terminal the mouse case asks for.  Its terminfo entry has to name a
# mouse report prefix, or the backend will not offer mouse support at all.
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


class DumbModeMouseTestCase(SharedRun, BaseTestCase, unittest.TestCase):
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
        # There is no DOS distribution to unpack
        cls.tarfile = ""
        cls.runs = {}
        cls.bootlogs = {}

    test_0_basic_boot = None

    def setUp(self):
        """Without a mouse prefix in terminfo the backend offers nothing."""
        super().setUp()
        try:
            kmous = check_output(["tput", "-T", MOUSE_TERM, "kmous"])
        except (OSError, CalledProcessError):
            kmous = b""
        if len(kmous) < 3 or not kmous.startswith(b"\033["):
            self.skipTest(f"terminfo entry for {MOUSE_TERM} names no mouse")
        self.mkcom_with_nasm("command", MOUSE_PROBE)

    @mark('terminal')
    def test_terminal_mode_still_tracks_the_mouse(self):
        """-t has a screen to point at, so tracking belongs there"""
        out = self.capture("-t", "-kt")
        missing = [s for s in MOUSE_ON if s not in out]
        self.assertEqual(missing, [], repr(out))

    @mark('terminal')
    def test_dumb_mode_leaves_the_mouse_alone(self):
        """-td has no screen, so nothing may switch tracking on"""
        out = self.capture("-td", "-kt")
        unwanted = [s for s in MOUSE_ON if s in out]
        self.assertEqual(unwanted, [], repr(out))

    @mark('terminal')
    def test_tracking_is_taken_back_off_at_the_end(self):
        """what -t switched on is switched off again on the way out"""
        out = self.capture("-t", "-kt")
        missing = [s for s in MOUSE_OFF if s not in out]
        self.assertEqual(missing, [], repr(out))

    def capture(self, *opts):
        """Run dosemu2 once under a pty and keep every byte it wrote."""
        if opts in self.__class__.runs:
            self.__class__.raw = self.__class__.runs[opts]
            self.__class__.bootlog = self.__class__.bootlogs[opts]
            self.relog()
            return self.__class__.runs[opts]

        out = self.runDosemuRaw(
            opts, config=CONF, rows=ROWS, cols=COLS,
            until=b"MOUSEPROBE", timeout=RUN_TIMEOUT,
            env={"DOSEMU2_COMCOM_DIR": str(self.workdir),
                 "TERM": MOUSE_TERM, "LC_ALL": "C.UTF-8"})

        self.__class__.runs[opts] = out
        self.__class__.bootlogs[opts] = self.boot_log()
        return out


if __name__ == "__main__":
    cases = [
        TerminalRenderTestCase,
        TerminalHostProgramTestCase,
        TerminalBlinkTestCase,
        TerminalKeysTestCase,
        DumbModeMouseTestCase,
    ]
    main(main_setup(cases))
