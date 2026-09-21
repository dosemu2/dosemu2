#!/usr/bin/env python3

"""The int 10h calls that draw into graphics memory, in every kind of
graphics mode.

Four of them -- 15, 16, 24 and 32 bits per pixel -- used to do nothing at
all and say so in the log (issue #830), because the VGA BIOS only knew how
to reach planar, CGA and 8 bit memory.  Nothing but a graphics video
backend can exercise them: under -td there are no direct colour modes to
set, so this test asks SDL for its dummy driver, which needs no display.

Nothing here needs a DOS distribution either: the program that does the
drawing is assembled on the spot and handed to dosemu2 as its command
interpreter.  It writes down what it saw in video memory after each call,
and this file reads that back.
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

# The colour everything is drawn in.  It has to have bit 0 set, or in the
# planar modes the glyph lands in planes the 0xa0000 window does not show
# and the program would conclude that nothing had been drawn.
COLOUR = 0x0f

PROBE = r"""
; Exercise the int 10h calls that draw into graphics memory, in every kind
; of graphics mode the BIOS knows, and write down what came back.
	org	0x100
	bits	16
	cpu	386

WIN	equ	0xa000
NCHK	equ	0xc000			; how much of the window we look at
COLOUR	equ	0x0f			; the colour everything is drawn in
PX	equ	10			; the pixel we write and read back
PY	equ	5

start:
	mov	ax, 0x0003
	int	0x10

	mov	ah, 0x3c		; create the report
	xor	cx, cx
	mov	dx, fname
	int	0x21
	jc	done
	mov	[fh], ax

	mov	word [idx], 0
.loop:
	mov	bx, [idx]
	shl	bx, 1
	mov	al, [modelist + bx]
	cmp	al, 0xff
	je	.fin
	mov	[kind], al
	mov	al, [modelist + bx + 1]
	mov	[value], al
	inc	word [idx]
	call	do_one
	jmp	.loop
.fin:
	mov	ax, 0x0003
	int	0x10
	mov	bx, [fh]
	mov	ah, 0x3e
	int	0x21
done:
	mov	dx, msg_done
	mov	ah, 0x09
	int	0x21
.spin:
	mov	ah, 0x00		; DOS is unhappy if we ever return
	int	0x16
	jmp	.spin

; ------------------------------------------------------------- one mode
do_one:
	mov	word [mode], 0
	mov	word [xres], 0
	mov	word [yres], 0
	mov	word [stride], 0
	mov	byte [bpp], 0
	cmp	byte [kind], 0
	jne	.vbe

	xor	ah, ah			; plain int 10h mode set
	mov	al, [value]
	int	0x10
	xor	ah, ah
	mov	al, [value]
	mov	[mode], ax
	jmp	.ready

.vbe:
	call	find_mode
	or	cx, cx
	jnz	.have
	jmp	report
.have:
	mov	[mode], cx
	mov	bx, cx
	mov	ax, 0x4f02
	int	0x10
	cmp	ax, 0x004f
	je	.ready
	mov	word [mode], 0
	jmp	report

.ready:
	call	clear_win
	call	home
	mov	ax, 0x0941		; AH=09 'A' with attribute
	xor	bh, bh
	mov	bl, COLOUR
	mov	cx, 1
	int	0x10
	call	count_nz
	mov	[n_attr], ax

	call	clear_win
	call	home
	mov	ax, 0x0a42		; AH=0A 'B', attribute untouched
	xor	bh, bh
	mov	bl, COLOUR
	mov	cx, 1
	int	0x10
	call	count_nz
	mov	[n_only], ax

	call	clear_win
	call	home
	mov	ax, 0x0e43		; AH=0E teletype 'C'
	xor	bh, bh
	mov	bl, COLOUR
	int	0x10
	call	count_nz
	mov	[n_tty], ax

	call	clear_win
	mov	ah, 0x0c		; AH=0C write one pixel
	mov	al, COLOUR
	xor	bh, bh
	mov	cx, PX
	mov	dx, PY
	int	0x10
	call	count_nz
	mov	[n_pix], ax
	mov	ah, 0x0d		; AH=0D read it back
	xor	bh, bh
	mov	cx, PX
	mov	dx, PY
	int	0x10
	xor	ah, ah
	mov	[v_pix], ax
	call	pixel_bytes

	call	fill_win
	call	count_nz
	mov	[n_fill], ax
	mov	ax, 0x0601		; AH=06 scroll a window up one line
	xor	bh, bh
	xor	cx, cx
	mov	dx, 0x0a0a
	int	0x10
	call	count_nz
	mov	[n_scrl], ax

	mov	ax, 0x0003
	int	0x10
	jmp	report

; --------------------------------------------------- find a VBE mode
; wanted bpp in [value]; out: cx = mode number, or 0
find_mode:
	mov	cx, 0x100
.l:
	push	cx
	mov	ax, 0x4f01
	push	ds
	pop	es
	mov	di, mib
	int	0x10
	pop	cx
	cmp	ax, 0x004f
	jne	.n
	test	byte [mib], 1		; mode supported?
	jz	.n
	cmp	byte [mib + 0x1b], 6	; direct colour memory model
	jne	.n
	mov	al, [mib + 0x19]	; BitsPerPixel
	cmp	al, [value]
	jne	.n
	mov	[bpp], al
	mov	ax, [mib + 0x10]	; BytesPerScanLine
	mov	[stride], ax
	mov	ax, [mib + 0x12]	; XResolution
	mov	[xres], ax
	mov	ax, [mib + 0x14]	; YResolution
	mov	[yres], ax
	ret
.n:
	inc	cx
	cmp	cx, 0x200
	jb	.l
	xor	cx, cx
	ret

; Count the non-zero bytes of the pixel the BIOS just wrote, and of the one
; before it.  Only the mode's own scan line length and pixel size put the
; pixel where we look, so this says the BIOS worked on the right mode and
; not on some other one with the same low byte of mode number.
pixel_bytes:
	mov	word [n_at], 0
	mov	word [n_bef], 0
	cmp	byte [bpp], 0
	je	.out
	mov	al, [bpp]
	add	al, 7
	shr	al, 3			; bytes per pixel
	xor	ah, ah
	mov	[pxsz], ax
	mov	ax, PY
	mul	word [stride]
	or	dx, dx			; further than the window reaches
	jnz	.out
	mov	bx, ax
	mov	ax, PX
	mul	word [pxsz]
	add	bx, ax
	jc	.out
	mov	ax, bx
	add	ax, [pxsz]
	cmp	ax, NCHK
	ja	.out
	push	es
	mov	ax, WIN
	mov	es, ax
	mov	di, bx
	mov	cx, [pxsz]
	call	.nzrun
	mov	[n_at], dx
	sub	bx, [pxsz]
	jc	.pop
	mov	di, bx
	mov	cx, [pxsz]
	call	.nzrun
	mov	[n_bef], dx
.pop:
	pop	es
.out:
	ret
.nzrun:
	xor	dx, dx
.r:
	mov	al, [es:di]
	or	al, al
	jz	.rz
	inc	dx
.rz:
	inc	di
	dec	cx
	jnz	.r
	ret

home:
	mov	ah, 0x02
	xor	bx, bx
	xor	dx, dx
	int	0x10
	ret

clear_win:
	push	es
	mov	ax, WIN
	mov	es, ax
	xor	di, di
	mov	cx, NCHK / 2
	xor	ax, ax
	rep	stosw
	pop	es
	ret

fill_win:
	push	es
	mov	ax, WIN
	mov	es, ax
	xor	di, di
	mov	cx, NCHK / 2
	mov	ax, 0x5a5a
	rep	stosw
	pop	es
	ret

; out: ax = number of non-zero bytes in the first NCHK of the window
count_nz:
	push	es
	mov	ax, WIN
	mov	es, ax
	xor	di, di
	mov	cx, NCHK
	xor	dx, dx
.l:
	mov	al, [es:di]
	or	al, al
	jz	.z
	inc	dx
.z:
	inc	di
	dec	cx
	jnz	.l
	mov	ax, dx
	pop	es
	ret

; ---------------------------------------------------------------- report
report:
	mov	di, line
	mov	si, s_mode
	call	puts
	mov	ax, [mode]
	call	puthex
	mov	si, s_bpp
	call	puts
	xor	ah, ah
	mov	al, [bpp]
	call	putdec
	mov	ax, [mode]
	or	ax, ax
	jz	.eol
	mov	si, s_res
	call	puts
	mov	ax, [xres]
	call	putdec
	mov	al, 'x'
	stosb
	mov	ax, [yres]
	call	putdec
	mov	si, s_attr
	call	puts
	mov	ax, [n_attr]
	call	putdec
	mov	si, s_only
	call	puts
	mov	ax, [n_only]
	call	putdec
	mov	si, s_tty
	call	puts
	mov	ax, [n_tty]
	call	putdec
	mov	si, s_pix
	call	puts
	mov	ax, [n_pix]
	call	putdec
	mov	si, s_rd
	call	puts
	mov	ax, [v_pix]
	call	puthex
	mov	si, s_at
	call	puts
	mov	ax, [n_at]
	call	putdec
	mov	si, s_bef
	call	puts
	mov	ax, [n_bef]
	call	putdec
	mov	si, s_fill
	call	puts
	mov	ax, [n_fill]
	call	putdec
	mov	si, s_scrl
	call	puts
	mov	ax, [n_scrl]
	call	putdec
.eol:
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

puthex:
	push	cx
	mov	cx, 4
.d:
	rol	ax, 4
	push	ax
	and	al, 0x0f
	cmp	al, 10
	jb	.dig
	add	al, 'a' - '0' - 10
.dig:
	add	al, '0'
	stosb
	pop	ax
	loop	.d
	pop	cx
	ret

; ------------------------------------------------------------------ data
; kind 0: an int 10h mode number.  kind 1: the first VBE direct colour mode
; with that many bits per pixel.
modelist:
	db	0, 0x0d			; 320x200, 4 planes
	db	0, 0x12			; 640x480, 4 planes
	db	0, 0x13			; 320x200, 256 colours
	db	1, 15
	db	1, 16
	db	1, 24
	db	1, 32
	db	0xff, 0

fname		db	'vgaprobe.txt', 0
msg_done	db	'PROBE DONE', 13, 10, '$'
s_mode		db	'mode=', 0
s_bpp		db	' bpp=', 0
s_res		db	' res=', 0
s_attr		db	' attr=', 0
s_only		db	' only=', 0
s_tty		db	' tty=', 0
s_pix		db	' pix=', 0
s_rd		db	' rd=', 0
s_at		db	' at=', 0
s_bef		db	' bef=', 0
s_fill		db	' fill=', 0
s_scrl		db	' scroll=', 0

fh		dw	0
idx		dw	0
kind		db	0
value		db	0
mode		dw	0
bpp		db	0
pxsz		dw	0
stride		dw	0
xres		dw	0
yres		dw	0
n_attr		dw	0
n_only		dw	0
n_tty		dw	0
n_pix		dw	0
v_pix		dw	0
n_at		dw	0
n_bef		dw	0
n_fill		dw	0
n_scrl		dw	0
line		times 160 db 0
mib		times 256 db 0
"""

# What the probe walks, in the order it walks it.  The three plain VGA
# modes are here because the fix touches how the BIOS decides which mode is
# up, and those are the modes that already worked.
VGA_MODES = (0x0d, 0x12, 0x13)
DIRECT_MODES = (15, 16, 24, 32)

RUN_TIMEOUT = 120
FIELDS = re.compile(r"(\w+)=([0-9a-fx]+)")


class VgaBiosTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU", cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        if which("nasm") is None:
            raise unittest.SkipTest("nasm not installed")

        cls.workdir = Path(mkdtemp(prefix="vgabios."))
        src = cls.workdir / "probe.asm"
        src.write_text(PROBE)
        # dosemu2 looks for command.com in DOSEMU2_COMCOM_DIR
        try:
            check_call(["nasm", "-f", "bin", "-o",
                        str(cls.workdir / "command.com"), str(src)])
        except CalledProcessError as e:
            raise unittest.SkipTest("nasm failed: %s" % e)

        cls.conf = cls.workdir / "dosemu.conf"
        cls.conf.write_text('$_force_vga_fonts = (1)\n')
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

    def test_no_mode_is_unimplemented(self):
        """every graphics mode the BIOS offers can be drawn into"""
        lines, log = self.probe()
        bad = [l for l in log.splitlines() if "vgabios: unimplemented" in l]
        if bad:
            where = self.keepLog(log)
            self.fail("the VGA BIOS gave up on %d int 10h calls, first was "
                      "%r; log kept at %s"
                      % (len(bad), bad[0].strip(), where))
        self.assertNotIn("Scroll in graphics mode", log)

    def test_every_mode_draws(self):
        """characters, pixels and scrolling all reach video memory"""
        lines, log = self.probe()
        for want in VGA_MODES:
            self.checkMode(lines["%04x" % want], "mode 0x%02x" % want)
        for bits in DIRECT_MODES:
            m = self.directMode(lines, bits)
            self.checkMode(m, "the %d bpp mode 0x%s" % (bits, m["mode"]))

    def test_direct_colour_uses_its_own_pixel_size(self):
        """a direct colour mode is not mistaken for some other mode

        The BDA holds seven bits of mode number, which cannot name a VESA
        mode, so asking it which mode is up answers with an unrelated one --
        a 32bpp mode read back as 16bpp, say.  The pixel the BIOS wrote then
        lands somewhere other than where the mode's own scan line length and
        pixel size put it.
        """
        lines, log = self.probe()
        for bits in DIRECT_MODES:
            m = self.directMode(lines, bits)
            with self.subTest(bpp=bits):
                self.assertEqual(int(m["bpp"]), bits)
                self.assertGreater(
                    int(m["at"]), 0,
                    "nothing was written where a %d bpp pixel belongs" % bits)
                self.assertEqual(
                    int(m["bef"]), 0,
                    "the %d bpp pixel landed short of where it belongs" % bits)

    def checkMode(self, m, what):
        with self.subTest(mode=what):
            self.assertGreater(int(m["attr"]), 0,
                               "int 10h ah=09 drew nothing in %s" % what)
            self.assertGreater(int(m["only"]), 0,
                               "int 10h ah=0a drew nothing in %s" % what)
            self.assertGreater(int(m["tty"]), 0,
                               "int 10h ah=0e drew nothing in %s" % what)
            self.assertGreater(int(m["pix"]), 0,
                               "int 10h ah=0c drew nothing in %s" % what)
            self.assertEqual(int(m["rd"], 16), COLOUR,
                             "int 10h ah=0d did not read back what ah=0c "
                             "wrote in %s" % what)
            self.assertNotEqual(int(m["scroll"]), int(m["fill"]),
                                "int 10h ah=06 moved nothing in %s" % what)

    def directMode(self, lines, bits):
        """The line the probe wrote for the mode it found at `bits` bpp."""
        for m in lines.values():
            if int(m["bpp"]) == bits:
                return m
        self.fail("the BIOS offers no %d bpp direct colour mode" % bits)

    def probe(self):
        """Run the probe once and hand back what it wrote, keyed by mode."""
        if self.__class__.report is not None:
            return self.__class__.report

        logfile = self.workdir / "dosemu.log"
        outfile = self.workdir / "dosemu.out"
        home = self.workdir / "home"
        rmtree(str(home), ignore_errors=True)
        home.mkdir()
        result = home / ".dosemu" / "drive_c" / "vgaprobe.txt"

        env = dict(environ)
        env.update({
            "HOME": str(home),
            "SDL_VIDEODRIVER": "dummy",     # a real display is not needed
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
                    if result.exists() and \
                            len(result.read_text(errors="replace").splitlines()) \
                            >= len(VGA_MODES) + len(DIRECT_MODES):
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

        lines = {}
        for line in result.read_text(errors="replace").splitlines():
            m = dict(FIELDS.findall(line))
            if not m.get("mode") or m["mode"] == "0000":
                continue
            lines[m["mode"]] = m
        if len(lines) < len(VGA_MODES) + len(DIRECT_MODES):
            where = self.keepLog(log + "\n" + result.read_text())
            self.fail("the probe only reported %d of %d modes; log kept at %s"
                      % (len(lines), len(VGA_MODES) + len(DIRECT_MODES), where))

        self.__class__.report = (lines, log)
        return self.__class__.report

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
