def cpu_undoc_shift(self):
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
c:\\undocsh
rem end
""", newline="\r\n")

    # The /6 encoding of the shift groups is not in the manuals, but every
    # x86 runs it as the documented /4, i.e. as SHL/SAL.  Origin's games use
    # it; Privateer has a D3 /6 in its resource decompressor.  Check every
    # form of it against the /4 the assembler writes for the same operands.
    self.mkcom_with_nasm("undocsh", r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds
    mov     di, marks

; each case: run the undocumented /6 form, keep result and flags, then run
; the documented /4 on the same input and compare both.

; 1: D0 /6 - shl r/m8, 1
    mov     al, 0x94
    db      0xd0, 0xf0
    call    save
    mov     al, 0x94
    shl     al, 1
    call    cmp8

; 2: D1 /6 - shl r/m16, 1
    mov     ax, 0x9412
    db      0xd1, 0xf0
    call    save
    mov     ax, 0x9412
    shl     ax, 1
    call    cmp16

; 3: D2 /6 - shl r/m8, cl
    mov     cl, 3
    mov     al, 0x94
    db      0xd2, 0xf0
    call    save
    mov     cl, 3
    mov     al, 0x94
    shl     al, cl
    call    cmp8

; 4: D3 /6 - shl r/m16, cl
    mov     cl, 5
    mov     ax, 0x9412
    db      0xd3, 0xf0
    call    save
    mov     cl, 5
    mov     ax, 0x9412
    shl     ax, cl
    call    cmp16

; 5: C0 /6 - shl r/m8, imm8
    mov     al, 0x94
    db      0xc0, 0xf0, 3
    call    save
    mov     al, 0x94
    shl     al, 3
    call    cmp8

; 6: C1 /6 - shl r/m16, imm8
    mov     ax, 0x9412
    db      0xc1, 0xf0, 5
    call    save
    mov     ax, 0x9412
    shl     ax, 5
    call    cmp16

; 7: D3 /6 with a memory operand, which is what the game does
    mov     bx, word1
    mov     word [bx], 0x9412
    mov     cl, 5
    db      0xd3, 0x37
    mov     ax, [bx]
    call    save
    mov     ax, 0x9412
    mov     cl, 5
    shl     ax, cl
    call    cmp16

    mov     dx, head
    mov     ah, 9
    int     0x21
    mov     dx, marks
    mov     ah, 9
    int     0x21
    mov     ax, 0x4c00
    int     0x21

; save result and flags of the /6 form
save:
    pushf
    pop     word [saveflg]
    mov     [saveres], ax
    ret

; compare 8-bit result in al plus flags against what was saved
cmp8:
    pushf
    pop     bx
    mov     ah, [saveres]
    cmp     al, ah
    jne     bad
    jmp     flags

; compare 16-bit result in ax plus flags against what was saved
cmp16:
    pushf
    pop     bx
    cmp     ax, [saveres]
    jne     bad
flags:
    mov     ax, [saveflg]
    xor     ax, bx
    and     ax, 0xc5            ; SF ZF PF CF; AF and OF are undefined here
    jnz     bad
    mov     byte [di], 'o'
    inc     di
    ret
bad:
    mov     byte [di], 'X'
    inc     di
    ret

section .data

head:       db "undoc shift /6: $"
marks:      db "       ", 13, 10, "$"
saveres:    dw 0
saveflg:    dw 0
word1:      dw 0
""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("undoc shift /6: ooooooo", results)
