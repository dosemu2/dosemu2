#!/usr/bin/python3

import pexpect
import re
import termios

from os import environ
from ptyprocess import PtyProcessError
from time import time

from common_framework import (BaseTestCase, main, main_setup, IPROMPT,
                              DOSEMU_CONF_DEFAULT, UNSUPPORTED)
from common_os import frdos130, ppdosgit

# How long the trace loop is given to end by itself.
TRACELOOP_LIMIT = 20

# Something for DOS to run while the debugger looks at it.
SIMPLE_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	mov dx, msg
	mov ah, 9
	int 21h
	mov ax, 4C00h
	int 21h
msg:	db "SIMPLE OK",13,10,'$'
"""

# Makes its first int 21h in the middle of its work, so that a breakpoint
# on the log line of that call stops with program code left to return to.
# 'after' is at 0108: three bytes of 'mov dx', three of 'mov ax', two of
# 'int 21h'.
LOGGER_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	mov dx, msg.one
	mov ax, 0900h
	int 21h
after:
	mov word [count], 1
	mov dx, msg.two
	mov ax, 0900h
	int 21h
	mov ax, 4C00h
	int 21h
count:	dw 0
msg:
.one:	db "LOGGER ONE",13,10,'$'
.two:	db "LOGGER TWO",13,10,'$'
"""

# Something to take the keys the debugger injects: it prints every key it
# gets as the word int 16h returned, and leaves on ESC.
KEYWAIT_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	mov dx, msg.ready
	mov ah, 9
	int 21h
.key:
	xor ah, ah
	int 16h			; wait for a key, ah=scancode al=ascii
	mov bx, ax		; puthex works in ax, so keep the key here
	mov dx, msg.key
	mov ah, 9
	int 21h
	mov al, bh
	call puthex
	mov al, bl
	call puthex
	mov dx, msg.crlf
	mov ah, 9
	int 21h
	cmp bl, 1Bh
	jne .key
	mov ax, 4C00h
	int 21h

puthex:				; al as two hex digits
	push ax
	shr al, 4
	call .digit
	pop ax
	and al, 0Fh
	call .digit
	ret
.digit:
	add al, '0'
	cmp al, '9'
	jbe .put
	add al, 7
.put:
	mov dl, al
	mov ah, 2
	int 21h
	ret

msg:
.ready:	db "KEYWAIT READY",13,10,'$'
.key:	db "KEY=$"
.crlf:	db 13,10,'$'
"""

# A minimal DPMI client. It does nothing but enter 16-bit protected mode and
# issue a few int 31h from there, which is all a debugger test needs: an int
# instruction executing in protected mode, at a known place, over and over.
PMINT_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	pop ax			; the word DOS pushes for .com files
	mov bx, sp
	shr bx, 4
	jnz .smallstack
	mov bx, 1000h		; it was a full 64 KiB stack
.smallstack:
	mov ah, 4Ah
	int 21h			; give the rest back, the DPMI host wants some
	xor ax, ax
	xchg ax, word [2Ch]
	mov es, ax
	mov ah, 49h
	int 21h			; free the environment if there is one

	mov ax, 1687h
	int 2Fh
	or ax, ax
	jnz nohost
	push es			; save the entry point
	push di
	or si, si		; does the host want memory of its own?
	jz .nomemneeded
	mov bx, si
	mov ah, 48h
	int 21h
	jc nomemory
	mov es, ax
.nomemneeded:
	mov bp, sp
	xor ax, ax		; a 16-bit client is enough
	call far [bp]
	jc initfailed

			; protected mode from here on
	int3			; tell a watching debugger we are here
	mov si, 16		; not cx: the call below returns the cpu
				; type in cl, so a loop on cx never ends
.again:
	mov ax, 0400h
	int 31h			; get DPMI version, has no side effects
	dec si
	jnz .again

	mov dx, msg.ok
	jmp short rmexit

nohost:
	mov dx, msg.nohost
	jmp short rmerror
nomemory:
	mov dx, msg.nomemory
	jmp short rmerror
initfailed:
	mov dx, msg.initfailed
rmerror:
	mov ah, 9
	int 21h
	mov ax, 4CFFh
	int 21h
rmexit:
	mov ah, 9
	int 21h
	mov ax, 4C00h
	int 21h

msg:
.ok:		db "PMINT OK",13,10,'$'
.nohost:	db "PMINT no DPMI host",13,10,'$'
.nomemory:	db "PMINT out of memory",13,10,'$'
.initfailed:	db "PMINT init failed",13,10,'$'
"""

# 'stopped' reports and the 'r' dump name the instruction pointer like this
# The watchpoint is a check in the CPU emulator's interpreter, so the client
# has to be run by it.
SIM_CONF = DOSEMU_CONF_DEFAULT + """\
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (1)
"""

# writes one byte at 0000:0500, in the scratch area nobody else is using
# while this runs
SELMOVE_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	pop ax			; the word DOS pushes for .com files
	mov bx, sp
	shr bx, 4
	jnz .smallstack
	mov bx, 1000h		; it was a full 64 KiB stack
.smallstack:
	mov ah, 4Ah
	int 21h			; give the rest back, the DPMI host wants some
	xor ax, ax
	xchg ax, word [2Ch]
	mov es, ax
	mov ah, 49h
	int 21h			; free the environment if there is one

	mov ax, 1687h
	int 2Fh
	or ax, ax
	jnz nohost
	push es			; save the entry point
	push di
	or si, si		; does the host want memory of its own?
	jz .nomemneeded
	mov bx, si
	mov ah, 48h
	int 21h
	jc nomemory
	mov es, ax
.nomemneeded:
	mov bp, sp
	xor ax, ax		; a 16-bit client is enough
	call far [bp]
	jc initfailed

			; protected mode from here on
	mov ax, 0006h
	mov bx, cs
	int 31h		; cx:dx = the base of our code
	mov si, cx
	mov di, dx
	xor ax, ax
	mov cx, 1
	int 31h		; a selector of our own
	jc initfailed
	mov bx, ax
	mov ax, 0008h
	xor cx, cx
	mov dx, 0FFFFh
	int 31h
	mov ax, 0007h
	mov cx, si
	mov dx, di
	int 31h		; it points where cs does
	int3			; the debugger puts a breakpoint behind it here
	mov ax, 0007h
	mov cx, si
	mov dx, di
	add dx, 16
	adc cx, 0
	int 31h		; and now it points elsewhere
	int3

	mov dx, msg.ok
	jmp short rmexit

never:			; nothing runs these, they only hold breakpoints
	nop
	nop

nohost:
	mov dx, msg.nohost
	jmp short rmerror
nomemory:
	mov dx, msg.nomemory
	jmp short rmerror
initfailed:
	mov dx, msg.initfailed
rmerror:
	mov ah, 9
	int 21h
	mov ax, 4CFFh
	int 21h
rmexit:
	mov ah, 9
	int 21h
	mov ax, 4C00h
	int 21h

msg:
.ok:		db "SELMOVE OK",13,10,'$'
.nohost:	db "SELMOVE no DPMI host",13,10,'$'
.nomemory:	db "SELMOVE out of memory",13,10,'$'
.initfailed:	db "SELMOVE init failed",13,10,'$'
"""

WRITER_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	xor ax, ax
	mov ds, ax
	mov byte [0500h], 5ah
	push cs
	pop ds
	mov dx, msg
	mov ah, 9
	int 21h
	mov ax, 4C00h
	int 21h
msg:	db "WRITER OK",13,10,'$'
"""

# calls one interrupt on purpose, for a breakpoint to stop on.  int 11h is
# the BIOS equipment list: it takes no arguments and changes nothing.
INTBP_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	int 11h			; where the breakpoint has to stop
	mov dx, msg
	mov ah, 9
	int 21h
	mov ax, 4C00h
	int 21h
msg:	db "INTBP OK",13,10,'$'
"""

RE_CSIP_RM = re.compile(r"CS:IP=([0-9a-f]{4}):([0-9a-f]{4})")
RE_CSIP_PM = re.compile(r"CS:EIP= ([0-9a-f]{4}):([0-9a-f]{8})")


class DbgLog:
    """The .dbg log, minus readline taking its prompt off the screen.

    When dosemu reports something nobody asked for, dosdebug has readline
    erase the prompt before printing it, and on a dumb terminal readline
    does that with a CR, a line of spaces and another CR.  On a screen it
    leaves no trace; in a file it is noise, so it is logged as the line
    break a reader would see there instead.
    """

    RE_ERASE = re.compile(rb"\r +\r")
    RE_TAIL = re.compile(rb"\r[ \r]*$")

    def __init__(self, f):
        self.f = f
        self.held = b''

    def write(self, data):
        data = self.held + data
        # an erase can be split across two reads, so keep a trailing CR
        # and whatever follows it until the next write shows what it was
        m = self.RE_TAIL.search(data)
        self.held = data[m.start():] if m else b''
        if m:
            data = data[:m.start()]
        self.f.write(self.RE_ERASE.sub(b"\n", data))

    def flush(self):
        self.f.flush()

    def close(self):
        self.f.write(self.held)
        self.held = b''
        self.f.flush()


class OurTestCase(BaseTestCase):

    # dosdebug plumbing

    def dbgStart(self):
        """Attach a dosdebug to the dosemu the test has running."""
        # A dumb terminal keeps readline from wrapping every prompt in the
        # bracketed paste escapes, and without ONLCR the pty does not add a
        # CR to every line, so the .dbg log reads as plain text.
        env = self.dbgenv.copy()
        env["TERM"] = "dumb"
        self.dbgchild = pexpect.spawn(str(self.dosdebug), env=env)
        fd = self.dbgchild.fileno()
        attrs = termios.tcgetattr(fd)
        attrs[1] &= ~termios.ONLCR
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        self.dbgchild.logfile_read = self.dbgfile
        self.dbgchild.expect([r"dosdebug> "], timeout=10)

    def dbgStop(self):
        try:
            self.dbgchild.close(force=True)
        except PtyProcessError:
            pass

    def dbgRead(self, settle=0.5, limit=10):
        """Collect output until dosdebug has been quiet for 'settle' seconds.

        dosemu writes a report a line at a time and each line arrives on its
        own, so there is no single marker to wait for; going by the pause at
        the end of the burst is what works on every build.
        """
        out = ''
        deadline = time() + limit
        quiet = time() + settle
        while time() < deadline and time() < quiet:
            try:
                out += self.dbgchild.read_nonblocking(size=8192,
                        timeout=0.1).decode('ASCII', 'replace')
                quiet = time() + settle
            except pexpect.TIMEOUT:
                pass
            except pexpect.EOF:
                break
        return out.replace('\r', '')

    def dbgCmd(self, cmd, settle=0.5, limit=10):
        """Send one command and return everything it produced.

        One command at a time: a build without the newline termination fix
        glues two commands together if they reach dosemu in one read().
        """
        self.dbgchild.sendline(cmd)
        return self.dbgRead(settle, limit)

    def dbgWaitStop(self, pm, limit=20):
        """Wait for the report dosemu prints when it stops, and return the
        instruction pointer it names."""
        pat = RE_CSIP_PM if pm else RE_CSIP_RM
        out = ''
        deadline = time() + limit
        while time() < deadline:
            out += self.dbgRead(0.5, limit)
            m = pat.search(out)
            if m:
                return int(m.group(1), 16), int(m.group(2), 16), out
        self.fail("no stop report in %ds, got:\n%s" % (limit, out))

    def dbgRegs(self, pm):
        """Ask for the registers and return the instruction pointer."""
        out = self.dbgCmd("r")
        m = (RE_CSIP_PM if pm else RE_CSIP_RM).search(out)
        if not m:
            self.fail("no registers in:\n%s" % out)
        return int(m.group(1), 16), int(m.group(2), 16), out

    def ldtLines(self, out):
        """How many descriptors an 'ldt' dump printed."""
        return len(re.findall(r"(?m)^[0-9a-f]{4}: [0-9a-f]{8} [0-9a-f]{8} ", out))

    def dbgInsnAt(self, seg, off, pm):
        """Disassemble the one instruction at seg:off."""
        out = self.dbgCmd("u %s%04x:%04x 1" % ('#' if pm else '', seg, off))
        # the echo of the command names the same address, so match on a line
        # that has an opcode byte after it, which only the dump has
        for l in out.split('\n'):
            if re.match(r"\s*#?[0-9a-f]{2,4}:0*%x [0-9A-F]{2}" % off, l):
                return l.strip()
        self.fail("no disassembly of %04x:%04x in:\n%s" % (seg, off, out))

    def runWithDosdebug(self, cmd, body, body_args=None,
                        config=DOSEMU_CONF_DEFAULT, timeout=30):
        """Boot DOS, attach dosdebug, run 'cmd', and let 'body' drive the
        debugger while it runs."""
        self.logfiles['dbg'] = [self.topdir / str(self.id() + ".dbg"),
                                'dosdebug.dbg']

        args = ["-f", str(self.imagedir / "dosemu.conf"),
                "-n",
                "-q",
                "-o", str(self.topdir / self.logfiles['log'][0]),
                "-td",
                "--Fimagedir", str(self.imagedir)]
        if environ.get("NO_KVM", '0') == '1' or self.use_cpu == 'emu':
            args.extend(["-z", "0"])

        self.mkfile("dosemu.conf", config, dname=self.imagedir)

        # The debugger fifos live in the XDG runtime directory, named after
        # the pid, and dosdebug refuses to start when it finds more than one
        # live pair there. Avoid a race by creating a run directory per test.
        run = self.imagedir / "run"
        run.mkdir()
        self.dbgenv = environ.copy()
        self.dbgenv["XDG_RUNTIME_DIR"] = str(run)

        child = pexpect.spawn(str(self.dosemu), args, env=self.dbgenv)

        # Tweak the pty to eliminate double CRs
        fd = child.fileno()
        attrs = termios.tcgetattr(fd)
        attrs[1] &= ~termios.ONLCR
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        ret = ''
        with open(self.logfiles['xpt'][0], "wb") as fout:
            child.logfile = fout
            child.setecho(False)
            try:
                prompt = r'(system -e|unix -e|' + IPROMPT + ')'
                child.expect([prompt + '[\r\n]*'], timeout=40)
                child.expect(['>[\r\n]*', pexpect.TIMEOUT], timeout=1)

                with open(self.logfiles['dbg'][0], "wb") as f:
                    self.dbgfile = DbgLog(f)
                    self.dbgStart()
                    try:
                        self.dbgCmd("stop")
                        child.send(cmd + '\n')
                        ret = body(body_args)
                    finally:
                        self.dbgCmd("g")
                        self.dbgStop()
                        self.dbgfile.close()

                child.expect(['rem end', pexpect.EOF], timeout=timeout)
            except pexpect.TIMEOUT:
                ret += ' Timeout'
            except pexpect.EOF:
                ret += ' EndOfFile'

        try:
            child.close(force=True)
        except PtyProcessError:
            pass

        return ret

    # the tests

    def test_dosdebug_devs(self):
        """Dosdebug devs"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            return self.dbgCmd("devs")

        results = self.runWithDosdebug("testit.bat", body)

        # These three drivers will always be there in some form.
        # f997:0048 Char 'NUL     '
        #   Attributes: 0x8004 (Char, NULDEV)
        #   Routines: Strategy(f997:0eb2), Interrupt(f997:0eb7)

        # f910:07e6 Char 'CLOCK$  '
        #   Attributes: 0x8008 (Char, CLOCK)
        #   Routines: Strategy(f910:0402), Interrupt(f910:04f9)

        # f910:07f8 Block (6 Units)
        #   Attributes: 0x08c2 (Block, Removable media calls, UNDEF7, Get/Set logical device calls, UNDEF1)
        #   Routines: Strategy(f910:0402), Interrupt(f910:0500)

        template = (
            r"(?s)[0-9A-Fa-f]{{4}}:[0-9A-Fa-f]{{4}} {variant}\n"
            r"\s*Attributes:.*?\n"
            r"\s*Routines:\s*Strategy\([0-9A-Fa-f]{{4}}:[0-9A-Fa-f]{{4}}\),\s*"
            r"Interrupt\([0-9A-Fa-f]{{4}}:[0-9A-Fa-f]{{4}}\)\n"
        )

        self.assertRegex(results, template.format(variant="Char 'NUL     '"))
        self.assertRegex(results, template.format(variant=r"Char 'CLOCK\$  '"))
        self.assertRegex(results, template.format(variant=r"Block \(\d+ Units\)"))

    def test_dosdebug_ivec(self):
        """Dosdebug ivec"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            return self.dbgCmd("ivec")

        results = self.runWithDosdebug("testit.bat", body)

        self.assertRegex(results, r"  33  [0-9A-F]{4}:[0-9A-F]{4}\(MOUSE_INT33_OFF\)")
        self.assertRegex(results, r"  61  [0-9A-F]{4}:[0-9A-F]{4}\(TCPDRV_OFF\)")

    def test_dosdebug_log(self):
        """Dosdebug log exercise"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        config = DOSEMU_CONF_DEFAULT + '$_debug = "-D-d"\n'

        # dosdebug> log
        # current Debug-log flags:
        # -#-A-B-C-D-E-F-I-J-L-M+N-P-Q-R-S-T-W-X-Z-a+c-d-e-f-g-h-i-j-k-m-n-p-q-r-s-u-v+w-x
        #
        # dosdebug> log info
        #
        # -#: default int           -A: ASPI                  -B: dosdebug trace
        # -C: CDROM                 -D: dos int 21h           -E: EMS
        # -F: MMIO trace            -I: IPC                   -J: dj64
        # -L: TCP                   -M: DPMI                  +N: NE2000 emulation
        # -P: Packet driver         -Q: Mapping driver        -R: disk READ
        # -S: SOUND                 -T: I/O trace             -W: disk WRITE
        # -X: X support             -Z: PCI                   -a: Set all levels
        # +c: configuration         -d: disk msgs             -e: cpu-emu
        # -f: fdpp                  -g: general messages      -h: hardware
        # -i: I/O instructions      -j: joystick              -k: keyboard
        # -m: mouse                 -n: IPX network           -p: printer
        # -q: DMA                   -r: PIC request           -s: serial
        # -u: Unicode translation   -v: video                 +w: warnings
        # -x: XMS

        def body(args):
            steps = []
            steps += (self.dbgCmd("log"),)
            steps += (self.dbgCmd("log info"),)
            steps += (self.dbgCmd("log +d"),)
            steps += (self.dbgCmd("log"),)
            steps += (self.dbgCmd("log info"),)
            steps += (self.dbgCmd("log +6d"),)
            steps += (self.dbgCmd("log"),)
            steps += (self.dbgCmd("log info"),)
            return '|'.join(steps)

        results = self.runWithDosdebug("testit.bat", body, config=config)

        self.assertNotIn('Timeout', results)

        steps = results.split('|')
        self.assertRegex(steps[0], r"(?m)^.*-d", steps[0])
        self.assertRegex(steps[1], r"(?m)^.*-d: disk msgs", steps[1])
        self.assertRegex(steps[2], r"(?m)^flags updated", steps[2])
        self.assertRegex(steps[3], r"(?m)^.*\+d", steps[3])
        self.assertRegex(steps[4], r"(?m)^.*\+d: disk msgs", steps[4])
        self.assertRegex(steps[5], r"(?m)^flags updated", steps[5])
        self.assertRegex(steps[6], r"(?m)^.*6d", steps[6])
        self.assertRegex(steps[7], r"(?m)^.*6d: disk msgs", steps[7])

    def test_dosdebug_injchar(self):
        """Dosdebug injchar"""

        self.mkbat_testit("keywait")
        self.mkcom_with_nasm("keywait", KEYWAIT_ASM)

        def body(args):
            # the injection is a coopth thread, so DOS has to be running
            steps = []
            steps += (self.dbgCmd("g"),)
            steps += (self.dbgCmd("injchar"),)
            steps += (self.dbgCmd("injchar q"),)
            steps += (self.dbgCmd("injchar 65536"),)
            steps += (self.dbgCmd("injchar 65"),)
            steps += (self.dbgCmd("injchar 0x1b"),)
            return '|'.join(steps)

        results = self.runWithDosdebug("testit.bat", body)
        screen = self.logfiles['xpt'][0].read_bytes().decode('ASCII', 'replace')

        self.assertNotIn('Timeout', results)

        steps = results.split('|')

        # an argument is required, and has to be a number dosdebug can read
        self.assertIn("missing argument", steps[1], steps[1])
        self.assertIn("Invalid character code 'q'", steps[2], steps[2])
        # int 16h AX=0500h takes a word, so anything larger is not a key
        self.assertIn("Invalid character code '65536'", steps[3], steps[3])

        # decimal and hex both go in, and the program gets what was sent
        self.assertIn("injecting 0x41", steps[4], steps[4])
        self.assertIn("injecting 0x1b", steps[5], steps[5])
        self.assertIn("KEYWAIT READY", screen)
        self.assertIn("KEY=0041", screen)
        self.assertIn("KEY=001B", screen)

    def test_dosdebug_step_rm(self):
        """Dosdebug single step in real mode"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            # bpload puts us on the first instruction of simple.com, whose
            # layout we know: mov dx (3), mov ah (2), int 21h (2),
            # mov ax (3)
            self.dbgCmd("bpload")
            self.dbgchild.sendline("g")
            self.dbgchild.expect(["bpload: INT3 caught at"], timeout=40)
            self.dbgRead(1.0)

            seg, off, _ = self.dbgRegs(pm=False)
            steps = ["entry %04x:%04x" % (seg, off)]
            for cmd in ("ti", "ti"):
                self.dbgchild.sendline(cmd)
                self.dbgWaitStop(pm=False, limit=20)
                seg, off, _ = self.dbgRegs(pm=False)
                steps.append("%s %04x:%04x" % (cmd, seg, off))
            steps.append("insn " + self.dbgInsnAt(seg, off, pm=False))
            # and 't' steps over the int rather than into it
            self.dbgchild.sendline("t")
            self.dbgWaitStop(pm=False, limit=20)
            seg, off, _ = self.dbgRegs(pm=False)
            steps.append("t %04x:%04x" % (seg, off))
            return " | ".join(steps)

        results = self.runWithDosdebug("testit.bat", body)

        self.assertNotIn('Timeout', results)
        self.assertRegex(results, r"entry [0-9a-f]{4}:0100", results)
        self.assertRegex(results, r"ti [0-9a-f]{4}:0103", results)
        self.assertRegex(results, r"ti [0-9a-f]{4}:0105", results)
        self.assertRegex(results, r"insn .*CD21", results)
        self.assertRegex(results, r"t [0-9a-f]{4}:0107", results)

    def test_dosdebug_bl_shows_bp_origin(self):
        """Dosdebug bl shows a breakpoint's address as it was typed"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            self.dbgCmd("bpload")
            self.dbgchild.sendline("g")
            self.dbgchild.expect(["bpload: INT3 caught at"], timeout=40)
            self.dbgRead(1.0)

            seg, off, _ = self.dbgRegs(pm=False)
            want = "%04x:0105" % seg
            self.dbgCmd("bp " + want)
            return "want=%s bl=%s" % (want, self.dbgCmd("bl"))

        results = self.runWithDosdebug("testit.bat", body)

        want = re.search(r"want=(\S+)", results).group(1)
        # "<n>: <linear address>, <address as typed>"; bpload has one
        # of its own, which was not typed and is listed without it
        self.assertRegex(results,
                         r"(?m)^\s*\d+: [0-9a-f]{8}, " + re.escape(want) + "$",
                         results)

    def test_dosdebug_bp_sel_moved(self):
        """Dosdebug drops a breakpoint whose selector moved"""

        self.mkbat_testit("selmove")
        self.mkcom_with_nasm("selmove", SELMOVE_ASM)

        def body(args):
            steps = []
            self.dbgCmd("bpint 3")
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=40)
            _, _, regs = self.dbgRegs(pm=True)
            sel = int(re.search(r"EBX: ([0-9a-f]{8})", regs).group(1), 16)
            # 'never' is the label after the jmp to rmexit, 2 bytes on
            # from the int3 we stopped behind: find it in the listing
            lst = self.dbgCmd("u * 40")
            m = re.search(r"(?m)^\s*#?[0-9a-f]{4}:0*([0-9a-f]+) 90\s", lst)
            never = int(m.group(1), 16)
            steps.append(self.dbgCmd("bp #%04x:%x" % (sel, never)))
            bl = self.dbgCmd("bl")
            lin = int(re.search(r"(?m)^\s*\d+: ([0-9a-f]{8}), #", bl).group(1), 16)
            steps.append(self.dbgCmd("bp %x" % (lin + 1)))
            steps.append(self.dbgCmd("bl"))
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=30)
            steps.append(self.dbgCmd("bl"))
            self.dbgCmd("bcint 3")
            self.dbgchild.sendline("g")
            return "sel=%04x lin=%08x|" % (sel, lin) + "|".join(steps)

        results = self.runWithDosdebug("testit.bat", body)

        m = re.search(r"sel=([0-9a-f]{4}) lin=([0-9a-f]{8})", results)
        self.assertIsNotNone(m, results)
        sel, lin = m.group(1), int(m.group(2), 16)
        before, after = results.split("|")[3:5]
        self.assertRegex(before, r"(?m)^\s*\d+: %08x, #%s:" % (lin, sel), before)
        self.assertRegex(before, r"(?m)^\s*\d+: %08x, " % (lin + 1), before)
        # the selector moved, so its breakpoint goes, with a word on why
        self.assertRegex(after, r"Warning: cleared breakpoint \d+ because "
                         r"#%s:\S+ no longer points to %08x" % (sel, lin), after)
        self.assertNotRegex(after, r"(?m)^\s*\d+: %08x" % lin, after)
        # and the one set by its linear address stays
        self.assertRegex(after, r"(?m)^\s*\d+: %08x, " % (lin + 1), after)

    def test_dosdebug_ldt(self):
        """Dosdebug ldt line count"""

        self.mkbat_testit("pmint")
        self.mkcom_with_nasm("pmint", PMINT_ASM)

        def body(args):
            # the client's descriptors only exist once it is in protected
            # mode, and the int3 it runs on the way in is what stops us there
            self.dbgCmd("bpint 3")
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=40)
            self.dbgCmd("bcint 3")

            steps = []
            steps += (self.dbgCmd("ldt"),)
            steps += (self.dbgCmd("ldt 0"),)
            steps += (self.dbgCmd("ldt 0 4"),)
            steps += (self.dbgCmd("ldt 0 0"),)
            steps += (self.dbgCmd("ldt 0 q"),)
            steps += (self.dbgCmd("ldt 0 3000000000"),)
            return '|'.join(steps)

        results = self.runWithDosdebug("testit.bat", body)

        self.assertNotIn('Timeout', results)

        steps = results.split('|')

        # empty descriptors are skipped rather than printed, so how many
        # lines a dump has depends on what the client allocated: a page is
        # sixteen at most, and this client has always had more than four
        page = self.ldtLines(steps[0])
        self.assertLessEqual(page, 16, steps[0])
        self.assertGreater(page, 4, steps[0])
        # a selector on its own dumps the one entry it names
        self.assertEqual(self.ldtLines(steps[1]), 1, steps[1])
        # and the count the manual documents is read now
        self.assertEqual(self.ldtLines(steps[2]), 4, steps[2])
        # it has to be a number, and no lines at all is not a dump
        self.assertIn("invalid line count '0'", steps[3], steps[3])
        self.assertIn("invalid line count 'q'", steps[4], steps[4])
        # a count that does not fit the int it is kept in used to come out
        # negative and print nothing at all; clamped to the table, it prints
        # at least what a page does
        self.assertGreaterEqual(self.ldtLines(steps[5]), page, steps[5])

    def test_dosdebug_step_over_int_pm(self):
        """Dosdebug step over an int in protected mode"""

        self.mkbat_testit("pmint")
        self.mkcom_with_nasm("pmint", PMINT_ASM)

        def body(args):
            # The int3 the client runs on entering protected mode is what
            # puts us inside it: an interrupt breakpoint on its own would
            # just as likely catch the command interpreter.
            self.dbgCmd("bpint 3")
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=40)
            self.dbgCmd("bcint 3")

            self.dbgCmd("bpintd 31")
            self.dbgchild.sendline("g")
            seg, off, _ = self.dbgWaitStop(pm=True, limit=30)
            insn = self.dbgInsnAt(seg, off, pm=True)
            # the breakpoint has to go before the step, or it fires again on
            # the very instruction we are stepping over
            self.dbgCmd("bcintd 31")
            self.dbgchild.sendline("t")
            seg2, off2, _ = self.dbgWaitStop(pm=True, limit=30)
            return "insn=%s at=%04x:%08x next=%04x:%08x bl=%s" % (
                    insn, seg, off, seg2, off2, self.dbgCmd("bl"))

        results = self.runWithDosdebug("testit.bat", body)

        # stepping the int with the trap flag leaves the client wedged, so
        # not finishing is a failure in its own right
        self.assertNotIn('Timeout', results,
                         "the client did not run on after the step: " + results)
        m = re.search(r"insn=(\S.*?) at=([0-9a-f]{4}):([0-9a-f]{8}) "
                      r"next=([0-9a-f]{4}):([0-9a-f]{8})", results)
        self.assertIsNotNone(m, results)
        self.assertRegex(m.group(1), r"CD31", results)
        self.assertEqual(m.group(2), m.group(4), "'t' left the segment: " + results)
        self.assertEqual(int(m.group(5), 16), int(m.group(3), 16) + 2,
                         "'t' did not land after the int: " + results)
        # the breakpoint 't' uses to step over the int is not the user's and
        # has to be gone again
        # 'bl' lists each one as "<n>: <linear address>"
        bl = results.split("bl=")[-1].split("Interrupts:")[0]
        self.assertNotRegex(bl, r"\n\s*\d+: [0-9a-f]+",
                            "a breakpoint was left behind: " + results)


    def test_dosdebug_tc_ends_on_int_bp(self):
        """Dosdebug an interrupt breakpoint ends a trace loop"""

        self.mkbat_testit("pmint")
        self.mkcom_with_nasm("pmint", PMINT_ASM)

        def body(args):
            # into protected mode first: the int3 the client runs on the way
            # in is a place we know we are inside it
            self.dbgCmd("bpint 3")
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=40)
            self.dbgCmd("bcint 3")

            # 'tc' traces on until something stops it, printing a report per
            # instruction, and the breakpoint on int 31h is such a something.
            # A loop that runs on instead leaves the debugger in its running
            # state, where it answers nothing else, so what the state is
            # afterwards is the whole question.
            self.dbgCmd("bpintd 31")
            self.dbgchild.sendline("tc")
            traced = self.dbgRead(settle=1.0, limit=TRACELOOP_LIMIT)
            regs = self.dbgCmd("r")
            # clear it whatever came of it, so that a debugger which did stop
            # is not left carrying the breakpoint into the next command
            self.dbgCmd("bcintd 31")

            state = ""
            for l in regs.split("\n"):
                if l.startswith("system state:"):
                    state = l.strip()
            insn = ""
            seg = off = 0
            m = RE_CSIP_PM.search(regs)
            if m:
                seg = int(m.group(1), 16)
                off = int(m.group(2), 16)
                insn = self.dbgInsnAt(seg, off, pm=True)
            return "[%s] insn=[%s] traced=%d at=%04x:%08x" % (
                    state, insn, len(traced), seg, off)

        results = self.runWithDosdebug("testit.bat", body,
                                       timeout=TRACELOOP_LIMIT + 30)

        self.assertNotIn('Timeout', results, results)
        m = re.search(r"\[system state: ([^\]]*)\] insn=\[(.*)\] "
                      r"traced=(\d+) at=([0-9a-f]{4}):([0-9a-f]{8})", results)
        self.assertIsNotNone(m, "the debugger answered nothing of what the "
                                "case asks for: " + results)
        self.assertIn("stopped", m.group(1),
                      "'tc' was still tracing: " + results)
        self.assertGreater(int(m.group(3)), 0,
                           "'tc' stopped without tracing anything: " + results)
        self.assertNotEqual(m.group(4, 5), ("0000", "00000000"),
                            "the debugger did not say where it stopped: "
                            + results)
        self.assertRegex(m.group(2), r"CD31",
                         "'tc' stopped somewhere other than the int: "
                         + results)


    def test_dosdebug_bp_pm(self):
        """Dosdebug breakpoint in protected mode"""

        self.mkbat_testit("pmint")
        self.mkcom_with_nasm("pmint", PMINT_ASM)

        def body(args):
            steps = []

            # The int3 the client runs on entering protected mode is what
            # puts us inside it: an interrupt breakpoint on its own would
            # just as likely catch the command interpreter.
            self.dbgCmd("bpint 3")
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=40)
            steps += (self.dbgCmd("bcint 3"),)

            # Let the client's loop come round twice before anything is
            # written into it, so that whatever the CPU emulator makes of
            # that code it has made already.
            self.dbgCmd("bpintd 31")
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=30)
            self.dbgchild.sendline("g")
            seg, off, _ = self.dbgWaitStop(pm=True, limit=30)
            steps += (self.dbgCmd("bcintd 31"),)
            steps += (self.dbgInsnAt(seg, off, pm=True),)

            # a breakpoint on the instruction after that int, which the
            # client has to come back to the moment the int returns
            want = off + 2
            steps += (self.dbgCmd("bp #%04x:%x" % (seg, want)),)
            steps += (self.dbgCmd("bl"),)
            self.dbgchild.sendline("g")
            seg2, off2, out = self.dbgWaitStop(pm=True, limit=30)
            steps += (out,)
            steps += ("hit=%04x:%08x want=%04x:%08x" % (seg2, off2, seg, want),)
            steps += (self.dbgCmd("bc 0"),)

            return '|'.join(steps)

        # the client runs its protected mode code under the CPU emulator
        # whatever the host can do, because that is where a breakpoint
        # written behind the emulator's back goes unnoticed
        results = self.runWithDosdebug("testit.bat", body,
                config=DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n')
        screen = self.logfiles['xpt'][0].read_bytes().decode('ASCII', 'replace')

        self.assertNotIn('Timeout', results,
                         "the client did not run to its end: " + results)

        steps = results.split('|')

        # the address the breakpoint went on is the int the loop runs
        self.assertRegex(steps[2], r"CD31", steps[2])
        # 'bl' lists each breakpoint as "<n>: <linear address>"
        self.assertRegex(steps[4], r"(?m)^\s*0: [0-9a-f]+", steps[4])

        m = re.search(r"hit=([0-9a-f]{4}):([0-9a-f]{8}) "
                      r"want=([0-9a-f]{4}):([0-9a-f]{8})", steps[6])
        self.assertIsNotNone(m, results)
        self.assertEqual(m.group(1, 2), m.group(3, 4),
                         "the breakpoint stopped somewhere else: " + steps[5])
        # and the client is itself again once the breakpoint is gone
        self.assertIn("PMINT OK", screen)

    def test_dosdebug_bpw_sim(self):
        """Dosdebug watchpoint under the interpreter"""

        self.mkbat_testit("writer")
        self.mkcom_with_nasm("writer", WRITER_ASM)

        def body(args):
            out = self.dbgCmd("bpw 0000:0500")
            out += self.dbgCmd("bpw")
            # the program runs, writes, and the watchpoint stops it
            out += self.dbgCmd("g", settle=2, limit=30)
            return out

        results = self.runWithDosdebug("testit.bat", body, config=SIM_CONF,
                                       timeout=60)

        self.assertIn("Watchpoint 0 set at 00000500, 1 byte", results)
        self.assertRegex(results, r"(?m)^0: 00000500 1$")
        # 104 is the mov in WRITER_ASM: org 100h, two 2-byte insns before it
        self.assertRegex(results, r"watchpoint 0: 00000500 written from "
                                  r"eip 104, 00000500 now holds 5a, "
                                  r"held [0-9a-f]{2}")
        # and the client is still alive after being let go
        screen = self.logfiles['xpt'][0].read_bytes().decode('ASCII', 'replace')
        self.assertIn("WRITER OK", screen)

    def test_dosdebug_bpint(self):
        """Dosdebug breakpoint on a real mode interrupt"""

        self.mkbat_testit("intbp")
        self.mkcom_with_nasm("intbp", INTBP_ASM)

        def body(args):
            out = ["set=" + self.dbgCmd("bpint 11")]
            out.append("list=" + self.dbgCmd("bl"))
            self.dbgchild.sendline("g")
            seg, off, stop = self.dbgWaitStop(pm=False, limit=40)
            out.append("stop=%04x:%04x " % (seg, off) + stop)
            out.append("insn=" + self.dbgInsnAt(seg, off, pm=False))
            out.append("clr=" + self.dbgCmd("bcint 11"))
            out.append("empty=" + self.dbgCmd("bl"))
            return " | ".join(out)

        results = self.runWithDosdebug("testit.bat", body, timeout=60)

        self.assertNotIn('Timeout', results, results)
        # 'bl' is where each kind of breakpoint is listed
        listed = results.split("list=")[-1].split(" | ")[0]
        self.assertRegex(listed, r"(?m)^Interrupts: 11 *$", results)
        # the stop is on the int itself: the debugger backs the instruction
        # pointer over the two bytes of it before reporting
        stop = results.split("stop=")[-1].split(" | ")[0]
        self.assertIn("INT 0x11", stop, results)
        insn = results.split("insn=")[-1].split(" | ")[0]
        self.assertRegex(insn, r"CD11", "stopped somewhere other than the "
                                        "int: " + results)
        self.assertRegex(stop, r"^[0-9a-f]{4}:0100 ",
                         "stopped at an int 11h other than the client's: "
                         + results)
        # and it has to be gone again
        empty = results.split("empty=")[-1]
        self.assertRegex(empty, r"(?m)^Interrupts: *$",
                         "the interrupt breakpoint was left behind: "
                         + results)
        screen = self.logfiles['xpt'][0].read_bytes().decode('ASCII', 'replace')
        self.assertIn("INTBP OK", screen)

    def test_dosdebug_bpintd(self):
        """Dosdebug breakpoint on a DPMI interrupt matching ax"""

        self.mkbat_testit("pmint")
        self.mkcom_with_nasm("pmint", PMINT_ASM)

        def body(args):
            # PMINT_ASM calls int 31h with ax=0400 sixteen times over, and
            # nothing else
            out = ["set=" + self.dbgCmd("bpintd 31 0400")]
            out.append("list=" + self.dbgCmd("bl"))
            # the client announces protected mode with an int3, and an int3
            # stops the debugger whether or not anything asked it to, so
            # that stop comes before the breakpoint's own
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=True, limit=40)
            self.dbgchild.sendline("g")
            seg, off, stop = self.dbgWaitStop(pm=True, limit=40)
            out.append("stop=" + stop)
            out.append("insn=" + self.dbgInsnAt(seg, off, pm=True))
            out.append("clr=" + self.dbgCmd("bcintd 31 0400"))
            # 0500 is get-free-memory-info, which this client never asks for,
            # so the ax it was given is the whole of what stopped it above
            out.append("other=" + self.dbgCmd("bpintd 31 0500"))
            self.dbgchild.sendline("g")
            out.append("ran=" + self.dbgRead(settle=2.0, limit=30))
            out.append("after=" + self.dbgCmd("r"))
            return " | ".join(out)

        results = self.runWithDosdebug("testit.bat", body, timeout=60)

        self.assertNotIn('Timeout', results, results)
        listed = results.split("list=")[-1].split(" | ")[0]
        self.assertRegex(listed, r"(?m)^DPMI Interrupts: 31\[400\]$", results)
        stop = results.split("stop=")[-1].split(" | ")[0]
        insn = results.split("insn=")[-1].split(" | ")[0]
        self.assertRegex(insn, r"CD31", "stopped somewhere other than the "
                                        "int: " + results)
        self.assertIn("EAX: 00000400", stop,
                      "stopped on an int 31h with another ax: " + results)
        # the ax the breakpoint does not name has to let the client run on
        after = results.split("after=")[-1]
        self.assertRegex(after, r"system state: .*running",
                         "the breakpoint stopped on an ax it was not set "
                         "for: " + results)
        screen = self.logfiles['xpt'][0].read_bytes().decode('ASCII', 'replace')
        self.assertIn("PMINT OK", screen)

    def test_dosdebug_bplog(self):
        """Dosdebug breakpoint on a log message"""

        self.mkbat_testit("logger")
        self.mkcom_with_nasm("logger", LOGGER_ASM)

        # +D logs every int 21h the client makes, as
        # 'INT21 at ff10:01e8: AX=0900, BX=...', naming the hlt that got
        # dosemu there rather than the client
        config = DOSEMU_CONF_DEFAULT + '$_debug = "-D+D"\n'

        def body(args):
            # the arguments of bplog are pasted together, so the regex it
            # gets is one word
            out = ["set=" + self.dbgCmd("bplog INT21.*AX=0900")]
            out.append("list=" + self.dbgCmd("bl"))
            self.dbgchild.sendline("g")
            # the first of the client's two prints is what it matches
            out.append("hit=" + self.dbgRead(settle=2.0, limit=30))
            regs = self.dbgCmd("r")
            out.append("at=" + regs)
            # The line is logged while dosemu serves the int, so the stop is
            # in the hlt that got it there, not in the client, and the
            # client's own address is nowhere the user could read it: the
            # frame on top of the stack is dosemu's.  An interrupt
            # breakpoint takes us into the client's code without knowing any
            # address, on the next call it makes there.
            out.append("set2=" + self.dbgCmd("bpint 21"))
            out.append("clr=" + self.dbgCmd("bclog 0"))
            self.dbgchild.sendline("g")
            seg2, off2, stop = self.dbgWaitStop(pm=False, limit=30)
            out.append("back=%04x:%04x " % (seg2, off2) + stop)
            out.append("insn=" + self.dbgInsnAt(seg2, off2, pm=False))
            out.append("done=" + self.dbgCmd("bcint 21"))
            return " | ".join(out)

        results = self.runWithDosdebug("testit.bat", body, config=config,
                                       timeout=60)

        self.assertNotIn('Timeout', results, results)
        listed = results.split("list=")[-1].split(" | ")[0]
        self.assertIn("log intercept on", listed, results)
        self.assertIn("log breakpoint 0: INT21.*AX=0900", listed, results)
        # the report quotes the line that matched
        self.assertRegex(results,
                         r"log break point 0 hit: >INT21 .*AX=0900[^<]*<",
                         results)
        # and the match is what stopped the machine
        at = results.split("at=")[-1].split(" | ")[0]
        self.assertRegex(at, r"system state: .*stopped",
                         "the log breakpoint did not stop anything: "
                         + results)
        # the segment the client is in is the one its ds points to, which
        # the stop report has
        ds = re.search(r"DS=([0-9a-f]{4})", at)
        self.assertIsNotNone(ds, "the stop report says no ds to tell what "
                                 "segment the client is in: " + results)
        # and the client's own code is where the interrupt breakpoint takes
        # us, on a call of its own
        back = results.split("back=")[-1].split(" | ")[0]
        m = re.match(r"([0-9a-f]{4}):[0-9a-f]{4} ", back)
        self.assertIsNotNone(m, "no stop of the interrupt breakpoint: "
                                + results)
        self.assertEqual(m.group(1), ds.group(1),
                         "stopped outside the client: " + results)
        self.assertIn("INT 0x21", back, results)
        insn = results.split("insn=")[-1].split(" | ")[0]
        self.assertRegex(insn, r"CD21", "not on the client's own int 21h: "
                                        + results)
        clr = results.split("clr=")[-1].split(" | ")[0]
        self.assertIn("log intercept off", clr, results)
        # and with the breakpoint gone the client runs to its end
        screen = self.logfiles['xpt'][0].read_bytes().decode('ASCII', 'replace')
        self.assertIn("LOGGER ONE", screen)
        self.assertIn("LOGGER TWO", screen)

    def test_dosdebug_bpload(self):
        """Dosdebug breakpoint on the next program loaded"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            out = ["set=" + self.dbgCmd("bpload")]
            out.append("list=" + self.dbgCmd("bl"))
            # it reports twice: once when it takes the EXEC over, and again
            # when the program it loaded is about to run, so wait for the
            # second rather than for a stop report
            self.dbgchild.sendline("g")
            load = ''
            deadline = time() + 40
            while time() < deadline and "INT3 caught at" not in load:
                load += self.dbgRead(0.5, 40)
            out.append("load=" + load)
            seg, off, regs = self.dbgRegs(pm=False)
            out.append("at=%04x:%04x " % (seg, off) + regs)
            out.append("insn=" + self.dbgInsnAt(seg, off, pm=False))
            return " | ".join(out)

        results = self.runWithDosdebug("testit.bat", body, timeout=60)

        self.assertNotIn('Timeout', results, results)
        listed = results.split("list=")[-1].split(" | ")[0]
        self.assertIn("bpload active", listed, results)
        load = results.split("load=")[-1].split(" | ")[0]
        self.assertIn("bpload: intercepting EXEC", load, results)
        self.assertRegex(load, r"bpload: INT3 caught at [0-9a-f]+:[0-9a-f]+",
                         results)
        # a .com file starts at 100h in its own segment, and the first
        # instruction of this one loads dx for the string it prints
        at = results.split("at=")[-1].split(" | ")[0]
        self.assertRegex(at, r"^[0-9a-f]{4}:0100 ",
                         "the stop is not at the entry point: " + results)
        self.assertRegex(at, r"system state: .*stopped", results)
        insn = results.split("insn=")[-1].split(" | ")[0]
        self.assertRegex(insn, r"BA[0-9A-F]{4}\s+mov\s+dx",
                         "the program the breakpoint stopped in is not the "
                         "one that was loaded: " + results)
        screen = self.logfiles['xpt'][0].read_bytes().decode('ASCII', 'replace')
        self.assertIn("SIMPLE OK", screen)

    def test_dosdebug_long_reply_unbroken(self):
        """Dosdebug a long reply has no blank lines in it"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            # dosemu writes a reply one line at a time, and 256 of them
            # are more than the pipe holds at once, so this one is read
            # in several chunks, which is where the blank lines came from
            return self.dbgCmd("u ff10:0100 256", settle=1, limit=25)

        results = self.runWithDosdebug("testit.bat", body)

        lines = results.split('\n')
        insn = [i for i, l in enumerate(lines)
                if re.match(r"[0-9a-f]{4}:[0-9a-f]{4} [0-9A-F]{2}", l)]
        self.assertGreater(len(insn), 200, results)
        blank = [l for l in lines[insn[0]:insn[-1] + 1] if l.strip() == '']
        self.assertEqual(blank, [], "blank lines inside the reply:\n" + results)

    def test_dosdebug_dump_ends_its_line(self):
        """Dosdebug a short dump row ends its line"""

        self.mkbat_testit("simple")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            # ten bytes is less than the sixteen a full row holds
            return self.dbgCmd("d 0:0 10", settle=1, limit=15)

        results = self.runWithDosdebug("testit.bat", body)

        m = re.search(r"(?m)^0000:0000( [0-9A-F]{2}){10}(.*)$", results)
        self.assertIsNotNone(m, results)
        self.assertEqual(m.group(2).strip(), '',
                         "the prompt is on the dump line: " + results)

# The DOS variants we want get included here
FRDOS130TestCase = frdos130(OurTestCase, {})

# bpload intercepts the DOS EXEC call, and comcom32 makes that call from
# protected mode, where the interception faults
PPDOSGITTestCase = ppdosgit(OurTestCase, {
})

if __name__ == '__main__':

    cases = [
        FRDOS130TestCase, PPDOSGITTestCase,
    ]

    argv = main_setup(cases)
    main(argv)
