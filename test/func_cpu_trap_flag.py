import re

from cpuinfo import get_cpu_info
from os import access, R_OK, W_OK


def cpu_trap_flag(self, cpu_vm):
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
c:\\cputrapf
rem end
""", newline="\r\n")

    # compile sources
    self.mkcom_with_nasm("cputrapf", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 2501h
    mov     dx, int1
    int     21h

    mov     ax, 14h
    mov     bx, 1
    int     0xe6

    push    3003h
    push    3303h
    popf
    popf
    nop

    mov     ax, 14h
    mov     bx, 0
    int     0xe6

    mov     ah, 9              ; print string
    mov     dx, result
    int     21h

exit:
    mov     ax, 4c00h
    int     21h
    ret

int1:
    inc     byte [cs:result.cnt]
    iret

section .data

result:
    db "Result is ("
.cnt:
    db '0'
    db ')',13,10,'$'
""")

    results = self.runDosemu("testit.bat", config=config)

    # Extract the result value
    r1 = re.compile(r'Result is \((\d+)\)')
    self.assertRegex(results, r1)
    t = r1.search(results)
    rval = int(t.group(1), 10)

    # Some unknown error in the test
    self.assertNotEqual(rval, 0, results)

    if cpu_vm != 'kvm':
        self.assertEqual(rval, 1, results)
        return

    # check for fixup
    fixupmsg = "KVM: not applying TF fixup"
    knownbad = (
        'AMD FX(tm)-8350 Eight-Core Processor',
        'AMD A10-7870K Radeon R7, 12 Compute Cores 4C+8G',
        'AMD Ryzen 5 5600U with Radeon Graphics',
        'AMD EPYC 7763 64-Core Processor',
    )

    # get log content
    logcontents = self.logfiles['log'][0].read_text()

    cpu = get_cpu_info()
    try:
        name = cpu['brand']
    except KeyError:
        try:
            name = cpu['brand_raw']
        except KeyError:
            name = str(cpu)

    if rval == 1:
        if name in knownbad:
            self.fail("Unexpected success with cpu '%s'" % name)
    elif rval > 1 or fixupmsg in results or fixupmsg in logcontents:
        if name in knownbad:
            self.setMessage("known bad cpu '%s'" % name)
        else:
            self.fail("Fail with unknown cpu '%s'" % name)
