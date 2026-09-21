#!/usr/bin/env python3

"""The SPICE remote display, driven by a real SPICE client, headless.

Everything here needs three things at once that no other test has: a real
SDL window (so Xvfb), a real render thread, and a client on the other end
of the wire.  The client is spice-client-glib through its introspection
bindings, which is the same library virt-viewer uses, so what it sees is
what a viewer would see.

Nothing here needs a DOS distribution: the guest side is a small program
assembled on the spot and handed to dosemu2 as its command interpreter.
"""

import ctypes
import signal
import socket
import struct
import subprocess
import unittest

from contextlib import contextmanager
from os import environ, getpgid, killpg
from pathlib import Path
from shutil import which, rmtree
from subprocess import Popen, DEVNULL, check_call, CalledProcessError
from tempfile import mkdtemp
from time import sleep, monotonic

try:
    import gi
    gi.require_version("SpiceClientGLib", "2.0")
    from gi.repository import SpiceClientGLib, GLib, GObject
except (ImportError, ValueError) as e:
    SpiceClientGLib = None
    SPICE_IMPORT_ERROR = str(e)

# Cycles 320x200x256, 80x25 text and 640x480x16 for as long as it is left
# alone, drawing into each so that there is something to send.  It must
# never exit, or DOS complains and shuts down.
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

# Appends every key it is given to a file on drive C, so a test can see
# what the guest received rather than what the wire carried.
KEY_LOGGER = r"""
bits 16
org 0x100

start:
        mov     ah, 0x3c                ; create the file, truncating it
        xor     cx, cx
        mov     dx, fname
        int     0x21
        jc      hang
        mov     bx, ax
        mov     ah, 0x3e                ; and close it again
        int     0x21

again:
        xor     ah, ah                  ; wait for a key
        int     0x16
        mov     [kbuf], al

        mov     ax, 0x3d01              ; open for writing
        mov     dx, fname
        int     0x21
        jc      again
        mov     bx, ax
        mov     ax, 0x4202              ; seek to the end
        xor     cx, cx
        xor     dx, dx
        int     0x21
        mov     ah, 0x40                ; append the character
        mov     cx, 1
        mov     dx, kbuf
        int     0x21
        mov     ah, 0x3e                ; close, so the host sees it at once
        int     0x21
        jmp     again

hang:   jmp     hang

fname:  db      'KEYS.TXT', 0
kbuf:   db      0
"""

# Holds a square wave on the PC speaker from the moment DOS starts it, so
# a client can attach before the tone or long after it.
BEEPER = r"""
bits 16
org 0x100

        mov     al, 0xb6                ; PIT channel 2, square wave
        out     0x43, al
        mov     ax, PIT_DIVISOR
        out     0x42, al
        mov     al, ah
        out     0x42, al
        in      al, 0x61
        or      al, 3                   ; gate the timer into the speaker
        out     0x61, al

hang:   mov     ah, 0                   ; keep asking the BIOS for the time,
        int     0x1a                    ; so this is not a bare spin
        jmp     hang
"""

# Reports the mouse driver's view of the world -- buttons and position as
# one hex line per change -- so a test can see what the guest was told
# rather than what the wire carried.
MOUSE_LOGGER = r"""
bits 16
org 0x100

start:
        cld
        mov     ah, 0x3c                ; create the file, truncating it
        xor     cx, cx
        mov     dx, fname
        int     0x21
        jc      hang
        mov     bx, ax
        mov     ah, 0x3e
        int     0x21

        xor     ax, ax                  ; reset the mouse driver
        int     0x33
        test    ax, ax
        jz      hang

again:
        mov     ax, 3                   ; buttons in bx, position in cx:dx
        int     0x33
        cmp     bx, [lastb]
        jne     report
        cmp     cx, [lastx]
        jne     report
        cmp     dx, [lasty]
        jne     report
        jmp     again

report:
        mov     [lastb], bx
        mov     [lastx], cx
        mov     [lasty], dx

        mov     di, line
        mov     ax, bx
        call    put1
        mov     ax, [lastx]
        call    put4
        mov     ax, [lasty]
        call    put4
        mov     al, 10
        stosb
        call    append
        jmp     again

hang:   jmp     hang

nib:    and     al, 0x0f
        add     al, '0'
        cmp     al, '9'
        jbe     .ok
        add     al, 7
.ok:    ret

put1:   call    nib
        stosb
        ret

put4:   push    ax
        mov     cl, 12
        shr     ax, cl
        call    nib
        stosb
        pop     ax
        push    ax
        mov     cl, 8
        shr     ax, cl
        call    nib
        stosb
        pop     ax
        push    ax
        mov     cl, 4
        shr     ax, cl
        call    nib
        stosb
        pop     ax
        call    nib
        stosb
        ret

append:
        mov     ax, di
        sub     ax, line
        mov     [linelen], ax
        mov     ax, 0x3d01              ; open for writing
        mov     dx, fname
        int     0x21
        jc      .out
        mov     bx, ax
        mov     ax, 0x4202              ; seek to the end
        xor     cx, cx
        xor     dx, dx
        int     0x21
        mov     ah, 0x40                ; append the line
        mov     cx, [linelen]
        mov     dx, line
        int     0x21
        mov     ah, 0x3e                ; close, so the host sees it at once
        int     0x21
.out:   ret

fname:  db      'MOUSE.TXT', 0
lastb:  dw      0xffff
lastx:  dw      0xffff
lasty:  dw      0xffff
linelen: dw     0
line:   times 16 db 0
"""

GUEST_PROGRAMS = {"modes": MODE_CYCLER, "keys": KEY_LOGGER, "beep": BEEPER,
                  "mouse": MOUSE_LOGGER}

# The PIT counts down from this at 1193182 Hz, giving a square wave of half
# that rate -- 1000 Hz, which is what the client must hear.
PIT_DIVISOR = 1193
PIT_HZ = 1193182
BEEP_HZ = PIT_HZ / PIT_DIVISOR
BEEP_TOLERANCE = 0.15
# Silence is zero and the tone measured about 8900 of 32767, so anything in
# between separates them.
BEEP_MIN_PEAK = 1000
BEEP_SECONDS = 3

SCANCODE_A = 0x1e
SCANCODE_LSHIFT = 0x2a

# Two points in the client's own coordinates, the second below and to the
# right of the first, so the guest's view of the move can be checked
# without knowing how the two coordinate spaces line up.
POINTER_NEAR = (100, 50)
POINTER_FAR = (600, 350)
BUTTON_LEFT = 1
MASK_LEFT = 1

CONNECT_CYCLES = 4
# 320x200, 720x400 and 640x480: one of each is what proves the client
# follows the guest through a mode change rather than only the first one.
WANT_SIZES = 3
CLIENT_TIMEOUT = 30

CRASH_MARKERS = (
    "Sync signal",            # dosemu2's own report of a fatal signal
    "Segmentation fault",
    "AddressSanitizer",       # when built with --enable-asan
    "Assertion `",            # glibc assert()
)


def freePort():
    """A port nothing is listening on, for dosemu2 to take."""
    s = socket.socket()
    try:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]
    finally:
        s.close()


class Client:
    """A SPICE client, pumped by hand so a test can wait on what it sees."""

    def __init__(self, port):
        self.sizes = []
        self.channels = set()
        self.inputs = None
        self.rate = 0
        self.pcm = bytearray()
        self.session = SpiceClientGLib.Session()
        self.session.set_property("host", "127.0.0.1")
        self.session.set_property("port", str(port))
        # Session.connect() is spice_session_connect(), so the GObject one
        # has to be reached through the class
        GObject.Object.connect(self.session, "channel-new", self.newChannel)

    def newChannel(self, session, channel):
        self.channels.add(type(channel).__name__)
        if isinstance(channel, SpiceClientGLib.DisplayChannel):
            GObject.Object.connect(channel, "display-primary-create",
                                   self.primaryCreate)
        elif isinstance(channel, SpiceClientGLib.InputsChannel):
            self.inputs = channel
        elif isinstance(channel, SpiceClientGLib.PlaybackChannel):
            GObject.Object.connect(channel, "playback-start", self.playbackStart)
            GObject.Object.connect(channel, "playback-data", self.playbackData)
        channel.connect()

    def primaryCreate(self, channel, fmt, w, h, stride, shmid, data):
        self.sizes.append((w, h))

    def playbackStart(self, channel, fmt, channels, rate):
        self.rate = rate

    def playbackData(self, channel, data, size):
        # the bindings hand the buffer over as a bare address
        self.pcm.extend(ctypes.string_at(data, size) if isinstance(data, int)
                        else bytes(data)[:size])

    def tone(self):
        """Peak amplitude and frequency of the left channel, as heard."""
        frames = len(self.pcm) // 4      # stereo s16
        if not frames or not self.rate:
            return 0, 0.0
        left = struct.unpack_from("<%dh" % (frames * 2), self.pcm, 0)[0::2]
        peak = max(abs(v) for v in left)
        crossings = sum(1 for a, b in zip(left, left[1:]) if (a >= 0) != (b >= 0))
        return peak, crossings / 2.0 / (frames / float(self.rate))

    def key(self, scancode):
        self.inputs.key_press_and_release(scancode)
        self.pump(0.4)

    def open(self):
        return self.session.connect()

    def close(self):
        self.session.disconnect()
        self.pump(0.5)

    def pump(self, seconds, until=None):
        """Run the client's event loop, stopping early once until() holds."""
        ctx = GLib.MainContext.default()
        deadline = monotonic() + seconds
        while True:
            while ctx.iteration(False):
                pass
            if until is not None and until():
                return True
            if monotonic() >= deadline:
                return until is None
            sleep(0.01)


@unittest.skipIf(SpiceClientGLib is None, "spice-client-glib bindings missing")
class SpiceTestCase(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        cls.topdir = Path('.').resolve()
        cls.dosemu = Path(environ.get("TEST_DOSEMU", cls.topdir / "bin" / "dosemu"))
        if not cls.dosemu.is_file():
            raise unittest.SkipTest("dosemu2 not built at %s" % cls.dosemu)
        for tool in ("Xvfb", "nasm"):
            if which(tool) is None:
                raise unittest.SkipTest("%s not installed" % tool)

        cls.workdir = Path(mkdtemp(prefix="spice."))
        # each guest program gets its own directory, because dosemu2 takes
        # the one named command.com in DOSEMU2_COMCOM_DIR
        for name, text in GUEST_PROGRAMS.items():
            d = cls.workdir / name
            d.mkdir()
            src = d / "guest.asm"
            src.write_text("%%define PIT_DIVISOR %d\n" % PIT_DIVISOR + text)
            try:
                check_call(["nasm", "-f", "bin", "-o",
                            str(d / "command.com"), str(src)])
            except CalledProcessError as e:
                raise unittest.SkipTest("nasm failed: %s" % e)

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

    def checkAlive(self, run, what):
        if run["proc"].poll() is not None:
            where = self.keepLog(self.readLogs(run))
            self.fail("dosemu2 died %s, log kept at %s" % (what, where))

    def readLogs(self, run):
        out = ""
        for f in (run["log"], run["out"]):
            if f.exists():
                out += f.read_text(errors="replace") + "\n"
        return out

    @contextmanager
    def dosemuRunning(self, program="modes", extra_conf=""):
        """dosemu2 running one guest program, with a client welcome."""
        port = freePort()
        progdir = self.workdir / program
        home = progdir / "home"
        # a home of its own, so the drive C the guest writes to is this
        # test's and nobody else's
        rmtree(str(home), ignore_errors=True)
        home.mkdir()
        conf = progdir / "dosemu.conf"
        conf.write_text('$_force_vga_fonts = (1)\n'
                        '$_spice = (on)\n'
                        '$_spice_addr = "127.0.0.1"\n'
                        '$_spice_port = (%d)\n' % port + extra_conf)
        logfile = progdir / "dosemu.log"
        outfile = progdir / "dosemu.out"
        for f in (logfile, outfile):
            if f.exists():
                f.unlink()

        xvfb, display = self.startXvfb()
        run = {"port": port, "log": logfile, "out": outfile,
               "dir": progdir, "home": home, "display": display}
        try:
            env = dict(environ)
            env.update({
                "DISPLAY": display,
                "SDL_VIDEODRIVER": "x11",
                "HOME": str(home),
                "DOSEMU2_COMCOM_DIR": str(progdir),
            })
            run["env"] = env
            with open(outfile, "wb") as out:
                # in its own session, because bin/dosemu is a wrapper script
                # that runs the real binary in a subshell: signalling the
                # script alone would leave dosemu2 behind
                run["proc"] = Popen([str(self.dosemu), "-S", "-D+v",
                                     "-f", str(conf), "-o", str(logfile)],
                                    env=env, stdout=out, stderr=out,
                                    stdin=DEVNULL, start_new_session=True)
                try:
                    if not self.waitFor(run, "spice: display worker attached"):
                        self.skipTest("this build has no SPICE support")
                    yield run
                finally:
                    self.stop(run["proc"])
        finally:
            self.stop(xvfb)

        text = self.readLogs(run)
        for marker in CRASH_MARKERS:
            if marker in text:
                where = self.keepLog(text)
                self.fail("dosemu2 died (%r), log kept at %s"
                          % (marker, where))

    def waitFor(self, run, what, timeout=30):
        """Wait for a line in dosemu2's log, giving up if it dies first."""
        deadline = monotonic() + timeout
        while monotonic() < deadline:
            if run["proc"].poll() is not None:
                return False
            if run["log"].exists() and \
                    what in run["log"].read_text(errors="replace"):
                return True
            sleep(0.2)
        return False

    def test_client_comes_and_goes(self):
        """clients connecting and disconnecting do not disturb the guest"""
        with self.dosemuRunning() as run:
            for i in range(CONNECT_CYCLES):
                c = Client(run["port"])
                self.assertTrue(c.open(), "client %d could not connect" % i)
                got = c.pump(CLIENT_TIMEOUT,
                             lambda: c.sizes and len(c.channels) >= 4)
                self.assertTrue(got, "client %d saw no screen, only %s"
                                % (i, sorted(c.channels)))
                c.close()
                self.checkAlive(run, "while client %d was leaving" % i)

    def test_mode_change_with_a_client(self):
        """the client follows the guest through its video mode changes"""
        with self.dosemuRunning() as run:
            c = Client(run["port"])
            self.assertTrue(c.open(), "the client could not connect")
            got = c.pump(CLIENT_TIMEOUT,
                         lambda: len(set(c.sizes)) >= WANT_SIZES)
            self.checkAlive(run, "while the mode was changing")
            self.assertTrue(got, "the client saw only %s" % sorted(set(c.sizes)))
            c.close()

    def test_exit_with_a_client_attached(self):
        """dosemu2 shuts down cleanly with a client still on the wire"""
        with self.dosemuRunning() as run:
            c = Client(run["port"])
            self.assertTrue(c.open(), "the client could not connect")
            self.assertTrue(c.pump(CLIENT_TIMEOUT, lambda: bool(c.sizes)),
                            "the client saw no screen")
            # no c.close(): the point is to leave it attached
            killpg(getpgid(run["proc"].pid), signal.SIGTERM)
            deadline = monotonic() + 20
            while monotonic() < deadline and run["proc"].poll() is None:
                c.pump(0.2)
            self.assertIsNotNone(run["proc"].poll(),
                                 "dosemu2 did not exit with a client attached")

    def test_shiftstate_stays_with_its_keyboard(self):
        """neither keyboard inherits the other's modifiers"""
        if which("xdotool") is None:
            self.skipTest("xdotool not installed")
        with self.dosemuRunning("keys") as run:
            c = Client(run["port"])
            self.assertTrue(c.open(), "the client could not connect")
            self.assertTrue(c.pump(CLIENT_TIMEOUT, lambda: bool(c.sizes)),
                            "the client saw no screen")
            keys = run["home"] / ".dosemu" / "drive_c" / "keys.txt"
            self.assertTrue(self.waitFor(run, "SPICE: client", timeout=10),
                            "dosemu2 did not notice the client")
            # there is no window manager, so the focus has to be given
            self.xdo(run, "windowfocus", self.window(run))

            c.key(SCANCODE_A)
            self.assertEqual(self.typed(c, keys, 1), "a",
                             "a plain remote key did not arrive as itself")

            self.xdo(run, "keydown", "shift")
            c.key(SCANCODE_A)
            self.xdo(run, "key", "a")
            self.xdo(run, "keyup", "shift")
            self.assertEqual(self.typed(c, keys, 3), "aaA",
                             "a shift held on the local keyboard reached the "
                             "remote one")

            c.inputs.key_press(SCANCODE_LSHIFT)
            c.pump(0.4)
            self.xdo(run, "key", "a")
            c.key(SCANCODE_A)
            c.inputs.key_release(SCANCODE_LSHIFT)
            self.assertEqual(self.typed(c, keys, 5), "aaAaA",
                             "a shift held on the remote keyboard reached the "
                             "local one")
            c.close()

    def test_sound_reaches_the_client(self):
        """what the client hears is the tone the guest asked the PIT for"""
        self.checkTone(self.listen())

    def test_sound_reaches_a_late_client(self):
        """a client attaching to a tone already playing hears it too"""
        # dosemu2 has been beeping into nothing for a while by then, which
        # is what a viewer started after the program does
        self.checkTone(self.listen(wait=BEEP_SECONDS))

    def listen(self, wait=0):
        """Record what a client attached to the beeping guest is sent."""
        with self.dosemuRunning("beep", '$_speaker = "sound"\n') as run:
            if wait:
                sleep(wait)
            c = Client(run["port"])
            self.assertTrue(c.open(), "the client could not connect")
            self.assertTrue(c.pump(CLIENT_TIMEOUT, lambda: bool(c.sizes)),
                            "the client saw no screen")
            c.pcm.clear()
            c.pump(BEEP_SECONDS)
            self.checkAlive(run, "while the speaker was on")
            c.close()
        return c

    def checkTone(self, c):
        peak, hz = c.tone()
        self.assertGreater(len(c.pcm), 0, "the client was sent no audio at all")
        self.assertGreater(peak, BEEP_MIN_PEAK,
                           "the client was sent silence, peak %d" % peak)
        self.assertAlmostEqual(hz, BEEP_HZ, delta=BEEP_HZ * BEEP_TOLERANCE,
                               msg="the client heard %.0f Hz where the PIT "
                                   "divisor asks for %.0f" % (hz, BEEP_HZ))

    def test_pointer_reaches_the_guest(self):
        """the client's pointer moves and clicks arrive at the DOS driver"""
        with self.dosemuRunning("mouse") as run:
            c = Client(run["port"])
            self.assertTrue(c.open(), "the client could not connect")
            self.assertTrue(c.pump(CLIENT_TIMEOUT, lambda: bool(c.sizes)),
                            "the client saw no screen")
            seen = run["home"] / ".dosemu" / "drive_c" / "mouse.txt"

            # the position is re-sent while we wait: the client tells the
            # server its logical size on its own schedule, and until it has,
            # there is no coordinate space to report a position in
            near = self.pointer(c, seen, lambda m: m[1] > 0 and m[2] > 0,
                                "the pointer to arrive at all",
                                self.mover(c, POINTER_NEAR))
            self.pointer(c, seen,
                         lambda m: m[1] > near[1] and m[2] > near[2],
                         "the pointer to move down and to the right",
                         self.mover(c, POINTER_FAR))

            # a press arrives through spice's wheel callback rather than its
            # buttons one, which is why this is worth a test of its own
            c.inputs.button_press(BUTTON_LEFT, MASK_LEFT)
            self.pointer(c, seen, lambda m: m[0] & MASK_LEFT,
                         "the button to go down")
            c.inputs.button_release(BUTTON_LEFT, 0)
            self.pointer(c, seen, lambda m: not m[0] & MASK_LEFT,
                         "the button to come back up")
            c.close()

    def mover(self, c, where):
        return lambda: c.inputs.position(where[0], where[1], 0, 0)

    def pointer(self, c, seen, ok, what, resend=None, timeout=20):
        """Wait for the guest's mouse driver to report what ok() asks for."""
        deadline = monotonic() + timeout
        last = None
        while monotonic() < deadline:
            if resend:
                resend()
            if seen.exists():
                lines = seen.read_text(errors="replace").split("\n")
                for line in lines:
                    if len(line) == 9:
                        last = (int(line[0], 16), int(line[1:5], 16),
                                int(line[5:9], 16))
                        if ok(last):
                            return last
            c.pump(0.2)
        self.fail("waited for %s, the guest last saw %s" % (what, last))

    def typed(self, c, keyfile, want, timeout=10):
        """What the guest has received so far, once it has received enough."""
        deadline = monotonic() + timeout
        got = ""
        while monotonic() < deadline:
            if keyfile.exists():
                got = keyfile.read_text(errors="replace")
                if len(got) >= want:
                    break
            c.pump(0.2)
        return got

    def window(self, run):
        p = subprocess.run(["xdotool", "search", "--class", "dosemu"],
                           env=run["env"], stdout=subprocess.PIPE,
                           stderr=DEVNULL)
        ids = p.stdout.split()
        self.assertTrue(ids, "dosemu2 has no window on %s" % run["display"])
        return ids[-1].decode()

    def xdo(self, run, *args):
        subprocess.run(["xdotool"] + list(args), env=run["env"], check=True,
                       stdout=DEVNULL, stderr=DEVNULL)
        sleep(0.4)

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
