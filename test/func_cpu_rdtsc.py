import re


def cpu_rdtsc(self, cpuemu):
    if self.use_cpu != "emu":
        raise ValueError('rdtsc test is for the emulated cpu')

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (%i)
""" % cpuemu

    self.mkfile("testit.bat", """\
c:\\cpurdtsc
rem end
""", newline="\r\n")

    # Read the counter twice with a delay in between, and report what came
    # out: bit 0 set if the first reading is non-zero, bit 1 set if the
    # second reading is above the first.  An unimplemented rdtsc leaves the
    # registers alone, so both bits stay clear.
    self.mkcom_with_nasm("cpurdtsc", r"""

bits 16
cpu 586

org 100h

section .text

    push    cs
    pop     ds

    rdtsc
    mov     [t0lo], eax
    mov     [t0hi], edx

    xor     cx, cx
spin:
    loop    spin

    rdtsc
    mov     [t1lo], eax
    mov     [t1hi], edx

    mov     bl, 0

    ; is the first reading non-zero?
    mov     eax, [t0lo]
    or      eax, [t0hi]
    jz      .notincr
    or      bl, 1

    ; is the second reading above the first?
.notincr:
    mov     eax, [t1lo]
    mov     edx, [t1hi]
    sub     eax, [t0lo]
    sbb     edx, [t0hi]
    jc      .done               ; borrowed, so it went backwards
    or      eax, edx
    jz      .done               ; stood still
    or      bl, 2

.done:
    add     bl, '0'
    mov     [result.val], bl

    mov     ah, 9
    mov     dx, result
    int     21h

    mov     ax, 4c00h
    int     21h

section .data

result:
    db "Result is ("
.val:
    db '0'
    db ')',13,10,'$'

t0lo:   dd 0
t0hi:   dd 0
t1lo:   dd 0
t1hi:   dd 0
""")

    results = self.runDosemu("testit.bat", config=config)

    r1 = re.compile(r'Result is \((\d+)\)')
    self.assertRegex(results, r1)
    rval = int(r1.search(results).group(1), 10)

    self.assertEqual(rval, 3, results)
