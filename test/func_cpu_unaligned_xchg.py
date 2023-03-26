from cpuinfo import get_cpu_info
from os import access, R_OK, W_OK


def cpu_unaligned_xchg(self, cpu_vm):
    if cpu_vm not in ("kvm", "emulated"):
        raise ValueError('invalid argument')

    if cpu_vm == "kvm" and not access("/dev/kvm", W_OK|R_OK):
        self.skipTest("requires KVM")

    config = """
    $_hdimage = "dXXXXs/c:hdtype1 +1"
    $_floppy_a = ""
    $_cpu_vm = "%s"
    """ % cpu_vm

    self.mkfile("testit.bat", """\
c:\\cpuxchg
rem end
""", newline="\r\n")

    # compile sources
    self.mkcom_with_nasm("cpuxchg", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov bx, 0xbeef
    xchg [unaligned],bx
    nop
    nop
    nop
    xchg [aligned],bx
    nop
    nop
    nop

    mov     ah, 9              ; print string
    mov     dx, result
    int     21h

exit:
    mov     ax, 4c00h
    int     21h
    ret

section .data
align 64
  padding1: times 63 db 0
  unaligned: dw 0xdead
  padding2: db 0
  aligned: dw 0xabcd

result:
    db "Reached end",13,10,'$'
""")

    results = self.runDosemu("testit.bat", config=config)

    knownbad = (
        '12th Gen Intel(R) Core(TM) i5-1240P',
    )

    if cpu_vm == "kvm":
        cpu = get_cpu_info()
        try:
            name = cpu['brand']
        except KeyError:
            try:
                name = cpu['brand_raw']
            except KeyError:
                name = str(cpu)
        if name in knownbad:
            if "Reached end" in results:
                self.fail("Unexpected success with cpu '%s'" % name)
            else:
                self.skipTest("Known bad processor")
        else:
            self.assertIn("Reached end", results, "New bad cpu '%s'" % name)
    else:
        self.assertIn("Reached end", results)
