#!/usr/bin/python3

import pexpect
import re
import termios

from os import environ
from ptyprocess import PtyProcessError
from shutil import rmtree
from tempfile import mkdtemp
from time import time

from common_framework import (BaseTestCase, main, main_setup, IPROMPT,
                              DOSEMU_CONF_DEFAULT, UNSUPPORTED)

# A watchpoint is the CPU emulator's jit taking a fault, so the test has to
# ask for the emulator rather than take whatever the machine offers.
CPUEMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm = "emulated"\n'
from common_os import frdos130, ppdosgit

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

# The debugger stops this one on its own int3, then watches the write that
# follows.  0x500 is the scratch byte right above the BIOS data area, so
# nothing else in the machine has an opinion about what is in it.
#
# 0x501 is written first and must not stop anything: the protection is by
# page while the watch is by byte, so the neighbour faults too and has to be
# let through.  Letting it through takes the protection off the page, and it
# goes back on only at the next poll, so the client waits for a BIOS tick
# before the write the test is really about.
WATCH_ASM = r"""
	cpu 386
	org 100h
	bits 16
start:
	int3			; tell a watching debugger we are here
	xor ax, ax
	mov es, ax
	mov byte [es:501h], 0A5h ; next door to the watch, must not stop
	mov ebx, [es:46Ch]	; the BIOS tick count
.wait:	cmp ebx, [es:46Ch]	; one tick is more than enough for the poll
	je .wait		; that puts the protection back
	mov byte [es:500h], 5Ah	; the write the watchpoint is for
	mov dx, msg
	mov ah, 9
	int 21h
	mov ax, 4C00h
	int 21h
msg:	db "WATCH OK",13,10,'$'
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
RE_CSIP_RM = re.compile(r"CS:IP=([0-9a-f]{4}):([0-9a-f]{4})")
RE_CSIP_PM = re.compile(r"CS:EIP= ([0-9a-f]{4}):([0-9a-f]{8})")


class OurTestCase(BaseTestCase):

    # dosdebug plumbing

    def dbgStart(self):
        """Attach a dosdebug to the dosemu the test has running."""
        self.dbgchild = pexpect.spawn(str(self.dosdebug), env=self.dbgenv)
        self.dbgchild.logfile = self.dbgfile
        self.dbgchild.setecho(False)
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

        # The debugger fifos live in the XDG runtime directory and there is
        # no fallback, so make one if the environment has none.
        self.dbgenv = environ.copy()
        tmprundir = None
        if not self.dbgenv.get("XDG_RUNTIME_DIR"):
            tmprundir = mkdtemp(prefix="dosemu2-dbg-")
            self.dbgenv["XDG_RUNTIME_DIR"] = tmprundir

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
                    self.dbgfile = f
                    self.dbgStart()
                    try:
                        self.dbgCmd("stop")
                        child.send(cmd + '\r\n')
                        ret = body(body_args)
                    finally:
                        self.dbgCmd("g")
                        self.dbgStop()

                child.expect(['rem end', pexpect.EOF], timeout=timeout)
            except pexpect.TIMEOUT:
                ret += ' Timeout'
            except pexpect.EOF:
                ret += ' EndOfFile'

        try:
            child.close(force=True)
        except PtyProcessError:
            pass

        if tmprundir:
            rmtree(tmprundir, ignore_errors=True)

        return ret

    # the tests

    def test_dosdebug_ivec(self):
        """Dosdebug ivec"""

        self.mkfile("testit.bat", "c:\\simple\nrem end\n", newline="\r\n")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            return self.dbgCmd("ivec")

        results = self.runWithDosdebug("testit.bat", body)

        self.assertRegex(results, r"  33  [0-9A-F]{4}:[0-9A-F]{4}\(MOUSE_INT33_OFF\)")
        self.assertRegex(results, r"  61  [0-9A-F]{4}:[0-9A-F]{4}\(TCPDRV_OFF\)")

    def test_dosdebug_step_rm(self):
        """Dosdebug single step in real mode"""

        self.mkfile("testit.bat", "c:\\simple\nrem end\n", newline="\r\n")
        self.mkcom_with_nasm("simple", SIMPLE_ASM)

        def body(args):
            # bpload puts us on the first instruction of simple.com, whose
            # layout we know: mov dx (3), mov ah (2), int 21h (2)
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
            return " | ".join(steps)

        results = self.runWithDosdebug("testit.bat", body)

        self.assertNotIn('Timeout', results)
        self.assertRegex(results, r"entry [0-9a-f]{4}:0100", results)
        self.assertRegex(results, r"ti [0-9a-f]{4}:0103", results)
        self.assertRegex(results, r"ti [0-9a-f]{4}:0105", results)
        self.assertRegex(results, r"insn .*CD21", results)

    def test_dosdebug_step_over_int_pm(self):
        """Dosdebug step over an int in protected mode"""

        self.mkfile("testit.bat", "c:\\pmint\nrem end\n", newline="\r\n")
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


    def test_dosdebug_watchpoint(self):
        """Dosdebug watchpoint on a client write"""

        self.mkfile("testit.bat", "c:\\watch\nrem end\n", newline="\r\n")
        self.mkcom_with_nasm("watch", WATCH_ASM)

        def body(args):
            # the client's own int3 is what gets us inside it
            self.dbgCmd("bpint 3")
            self.dbgchild.sendline("g")
            self.dbgWaitStop(pm=False, limit=40)
            self.dbgCmd("bcint 3")

            # a watch takes an address the way every other command does
            out = ["set=" + self.dbgCmd("bpw 0:500 1")]
            out.append("list=" + self.dbgCmd("bpw"))
            self.dbgchild.sendline("g")
            _, _, stop = self.dbgWaitStop(pm=False, limit=40)
            out.append("stop=" + stop)
            out.append("mem=" + self.dbgCmd("d 0:500 2"))
            out.append("clr=" + self.dbgCmd("bcw"))
            out.append("empty=" + self.dbgCmd("bpw"))
            return " | ".join(out)

        results = self.runWithDosdebug("testit.bat", body, config=CPUEMU_CONF)

        # the client has to run on afterwards, not be left wedged on the
        # instruction the watchpoint stopped it at
        self.assertNotIn('Timeout', results)
        self.assertRegex(results, r"Watchpoint 0 set at 00000500", results)
        self.assertRegex(results,
                         r"watchpoint 0: 00000500 about to be written from"
                         r" [0-9a-f]{8}, it holds 00", results)
        # The dump at the stop settles both halves of it: the watched byte
        # is still 00, so the stop is before the write, and the neighbour
        # already holds a5, so its own write was let through silently.
        mem = results.split("mem=")[-1].split(" | ")[0]
        self.assertRegex(mem, r"0500\s+00 A5", mem)
        # clearing has to leave the list empty
        empty = results.split("empty=")[-1]
        self.assertNotRegex(empty, r"\n\s*\d+: [0-9a-f]+",
                            "a watchpoint was left behind: " + results)



# The DOS variants we want get included here
FRDOS130TestCase = frdos130(OurTestCase, {})

# bpload intercepts the DOS EXEC call, and comcom32 makes that call from
# protected mode, where the interception faults
PPDOSGITTestCase = ppdosgit(OurTestCase, {
    "test_dosdebug_step_rm": UNSUPPORTED,
})

if __name__ == '__main__':

    cases = [
        FRDOS130TestCase, PPDOSGITTestCase,
    ]

    argv = main_setup(cases)
    main(argv)
