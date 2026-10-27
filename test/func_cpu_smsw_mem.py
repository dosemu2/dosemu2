

def cpu_smsw_mem(self):
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

    self.mkbat_testit("smswmem")

    # 'smsw word [disp16]' is what the DOS/4GW startup code uses, so it is
    # tested together with the register and the [bx] forms.
    self.mkcom_with_nasm("smswmem", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    smsw    ax
    mov     di, outr
    call    hex16

    smsw    word [msw1]
    mov     ax, [msw1]
    mov     di, out1
    call    hex16

    mov     bx, msw2
    smsw    word [bx]
    mov     ax, [msw2]
    mov     di, out2
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

msw1:   dw 0xdead
msw2:   dw 0xdead

result:
    db "REG("
outr:
    db "XXXX"
    db ") MEM("
out1:
    db "XXXX"
    db ") BX("
out2:
    db "XXXX"
    db ")",13,10,'$'
""")

    results = self.runDosemu("testit.bat", config=config)

    with self.assertRegexSearch(r"REG\(([0-9A-F]{4})\) MEM\(([0-9A-F]{4})\) BX\(([0-9A-F]{4})\)", results) as m:
        reg = int(m.group(1), 16)
        mem = int(m.group(2), 16)
        bxm = int(m.group(3), 16)

    # PE is set whenever DOS runs under dosemu2
    self.assertEqual(reg & 1, 1, results)

    # the memory forms must be emulated too, not skipped over
    self.assertNotEqual(mem, 0xdead, results)
    self.assertNotEqual(bxm, 0xdead, results)
    self.assertEqual(mem & 1, 1, results)
    self.assertEqual(bxm & 1, 1, results)
    self.assertEqual(mem, reg, results)
    self.assertEqual(bxm, reg, results)
