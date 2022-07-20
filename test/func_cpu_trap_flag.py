from cpuinfo import get_cpu_info
from os import access, R_OK, W_OK


def cpu_trap_flag(self, cpu_vm):
    if cpu_vm not in ("kvm", "emulated"):
        raise ValueError('invalid argument')

    if cpu_vm == "kvm" and not access("/dev/kvm", W_OK|R_OK):
        self.skipTest("No KVM available")

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
    self.mkcom_with_gas("cputrapf", r"""
.text
.code16

    .globl  _start16
_start16:

    push    %cs
    pop     %ds

    movw    $0x2501, %ax
    movw    $int1, %dx
    int     $0x21

    movw    $0x14, %ax
    movw    $1, %bx
    int     $0xe6

    pushw   $0x3003
    pushw   $0x3303
    popfw
    popfw
    nop

    movw    $0x14, %ax
    movw    $0, %bx
    int     $0xe6

    movb    $0x9, %ah              # print string
    movw    $result, %dx
    int     $0x21

exit:
    movw    $0x4c00, %ax
    int     $0x21
    ret

int1:
    incb    %cs:cnt
    iret

result:
    .ascii "Result is ("
cnt:
    .byte '0'
    .ascii ")\r\n$"

""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("Result is (1)", results)

    if cpu_vm != 'kvm':
        return

    # check for fixup
    fixupmsg = "KVM: not applying TF fixup"
    knownbad = (
        'AMD FX(tm)-8350 Eight-Core Processor',
    )

    # get log content
    logcontents = 'Missing'
    with open(self.logfiles['log'][0], "r") as f:
        logcontents = f.read()

    cpu = get_cpu_info()
    try:
        name = cpu['brand']
    except KeyError:
        try:
            name = cpu['brand_raw']
        except KeyError:
            name = str(cpu)

    if name in knownbad:
        if fixupmsg in results or fixupmsg in logcontents:
            self.setMessage("known bad cpu '%s'" % name)
        else:
            self.fail("Unexpected success with cpu '%s'" % name)
    else:
        if fixupmsg in results or fixupmsg in logcontents:
            self.fail("Fail with unknown cpu '%s'" % name)
