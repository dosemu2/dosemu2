
from common_framework import mkfile, mkcom
from cpuinfo import get_cpu_info


def cpu_trap_flag(self, cpu_vm):

    config = '$_hdimage = "dXXXXs/c:hdtype1 +1"\n$_floppy_a = ""\n'
    if cpu_vm == 'kvm':
        config += '$_cpu_vm = "kvm"\n'
    elif cpu_vm == 'emulated':
        config += '$_cpu_vm = "emulated"\n'
    else:
        raise ValueError('invalid argument')

    mkfile("testit.bat", """\
c:\\cputrapf
rem end
""", newline="\r\n")

    # compile sources
    mkcom("cputrapf", r"""
.text
.code16

    .globl  _start16
_start16:

    push    %cs
    pop     %ds

    movw    $0x2501, %ax
    movw    $int1, %dx
    int     $0x21
    pushw   $0x3003
    pushw   $0x3303
    popfw
    popfw
    nop

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
    fixupmsg = "KVM: applying TF fixup"
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
