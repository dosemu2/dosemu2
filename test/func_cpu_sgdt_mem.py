import re


def cpu_sgdt_mem(self):
    """sgdt and sidt are all a DOS program ever learns about the two
    tables, and one that believes what it is told goes and reads them.
    A base of zero sends it to the interrupt vector table, which in real
    mode is right there and writable, so it reads descriptors out of
    interrupt vectors or writes over them. The answer has to name an
    address it cannot reach, and a limit a real machine has."""

    if self.use_cpu == "emu":
        cpu_vm = 'emulated'
    elif self.use_cpu == "kvm":
        if not self.have_kvm:
            self.skipTest("requires KVM")
        cpu_vm = 'kvm'
    elif self.use_cpu == "vm86":
        if not self.have_vm86:
            self.skipTest("requires vm86() only on 32bit x86")
        cpu_vm = 'vm86'
    else:
        raise ValueError('invalid self.use_cpu')

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "%s"
""" % cpu_vm

    self.mkfile("testit.bat", """\
c:\\sgdtmem
rem end
""", newline="\r\n")

    self.mkcom_with_nasm("sgdtmem", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     bx, gdtbuf
    sgdt    [bx]
    mov     bx, idtbuf
    sidt    [bx]

    mov     ax, [gdtbuf]
    mov     di, glim
    call    hex16
    mov     ax, [gdtbuf + 4]
    mov     di, gbase
    call    hex16
    mov     ax, [gdtbuf + 2]
    mov     di, gbase + 4
    call    hex16

    mov     ax, [idtbuf]
    mov     di, ilim
    call    hex16
    mov     ax, [idtbuf + 4]
    mov     di, ibase
    call    hex16
    mov     ax, [idtbuf + 2]
    mov     di, ibase + 4
    call    hex16

    mov     ah, 9
    mov     dx, result
    int     21h

    mov     ax, 4c00h
    int     21h

; ax = value to print, di -> 4 chars of output
hex16:
    mov     cx, 4
.loop:
    rol     ax, 4
    push    ax
    and     al, 0fh
    add     al, '0'
    cmp     al, '9'
    jbe     .store
    add     al, 7
.store:
    mov     [di], al
    inc     di
    pop     ax
    loop    .loop
    ret

section .data

; poisoned, so that an instruction skipped instead of emulated shows up
; as such rather than as a plausible looking answer
gdtbuf: dw 0xdead
        dd 0xdeadbeef
idtbuf: dw 0xdead
        dd 0xdeadbeef

result:
    db "GDT("
glim:
    db "XXXX"
    db ":"
gbase:
    db "XXXXXXXX"
    db ") IDT("
ilim:
    db "XXXX"
    db ":"
ibase:
    db "XXXXXXXX"
    db ")",13,10,'$'
""")

    results = self.runDosemu("testit.bat", config=config)

    r = re.compile(r'GDT\(([0-9A-F]{4}):([0-9A-F]{8})\) '
                   r'IDT\(([0-9A-F]{4}):([0-9A-F]{8})\)')
    self.assertRegex(results, r)
    t = r.search(results)
    glim = int(t.group(1), 16)
    gbase = int(t.group(2), 16)
    ilim = int(t.group(3), 16)
    ibase = int(t.group(4), 16)

    for nm, lim, base in (("GDT", glim, gbase), ("IDT", ilim, ibase)):
        # emulated, not skipped over
        self.assertNotEqual(lim, 0xdead, "%s: %s" % (nm, results))
        self.assertNotEqual(base, 0xdeadbeef, "%s: %s" % (nm, results))

        # zero is the interrupt vector table, and a program that follows
        # it there reads vectors as descriptors
        self.assertNotEqual(base, 0, "%s: %s" % (nm, results))

        # a program that works out the entry count as (limit + 1) / 8 in
        # sixteen bits gets zero out of 0xffff, and zero out of zero
        self.assertNotEqual(lim, 0, "%s: %s" % (nm, results))
        self.assertNotEqual(lim, 0xffff, "%s: %s" % (nm, results))

        # out of reach of a real mode program, which is the point: it
        # faults on the answer rather than reading something else
        self.assertGreaterEqual(base, 0x100000, "%s: %s" % (nm, results))
