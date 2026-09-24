import re


def cpu_endbr(self):
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
c:\\endbr
rem end
""", newline="\r\n")

    # gcc with -fcf-protection puts endbr32 at the start of functions, and
    # comcom32 has such functions. Without CET they must execute as NOPs.
    # 0f 1e with a register operand is a hint NOP on its own, and REP in
    # front of a two-byte opcode that does not use it is ignored. An int 06h
    # handler counts #UDs and skips 4 bytes, so a missing decoder entry shows
    # up as a count, not as a dead program.
    self.mkcom_with_nasm("endbr", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 3506h
    int     21h
    mov     [old6], bx
    mov     [old6 + 2], es

    mov     ax, 2506h
    mov     dx, int6
    int     21h

    mov     cx, 1234h
    db      0f3h, 0fh, 1eh, 0fbh        ; endbr32
    inc     cx
    db      0f3h, 0fh, 1eh, 0fah        ; endbr64
    inc     cx
    db      0fh, 1eh, 0c9h              ; hint nop ecx, ecx
    inc     cx
    mov     bl, 0a5h
    mov     ax, 1234h
    rep movzx ax, bl                    ; REP means nothing here
    add     cx, ax
    mov     [after], cx

    push    ds
    lds     dx, [old6]
    mov     ax, 2506h
    int     21h
    pop     ds

    mov     al, [ud_count]
    add     al, '0'
    mov     [outc], al

    mov     ax, [after]
    mov     di, outa
    call    hex16

    mov     ah, 9
    mov     dx, result
    int     21h

    mov     ax, 4c00h
    int     21h

int6:
    push    bp
    mov     bp, sp
    add     word [bp + 2], 4
    inc     byte [cs:ud_count]
    pop     bp
    iret

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

old6:       dd 0
after:      dw 0
ud_count:   db 0

result:
    db "UD("
outc:
    db "X"
    db ") CX("
outa:
    db "XXXX"
    db ")",13,10,'$'
""")

    results = self.runDosemu("testit.bat", config=config)

    r = re.compile(r'UD\(([0-9])\) CX\(([0-9A-F]{4})\)')
    self.assertRegex(results, r)
    t = r.search(results)
    self.assertEqual(t.group(1), '0', "endbr raised #UD\n" + results)
    self.assertEqual(t.group(2), '12DC', results)   # 0x1237 + 0xa5
