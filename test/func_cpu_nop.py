def cpu_nop(self, cpuemu):
    if self.use_cpu != "emu":
        raise ValueError('nop test is for the emulated cpu')

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (%i)
""" % cpuemu

    self.mkfile("testit.bat", """\
c:\\cpunop
rem end
""", newline="\r\n")

    # The multi-byte NOP, 0f 1f, is what every compiler emits for alignment
    # padding since the P6.  It has a modrm operand that is never read, so
    # the only thing the decoder has to get right is its length; a wrong
    # length desynchronises the instruction stream and the program dies on
    # a bogus opcode.  Every shape of the operand is tried: the register
    # form, both address sizes, the sib byte with and without a base, and
    # each displacement size.  The two a32 forms with a large displacement
    # also check that no address is generated for an operand that is not
    # read - the real cpu does not fault on them either.
    self.mkcom_with_nasm("cpunop", r"""

bits 16
cpu 686

org 100h

section .text

    push    cs
    pop     ds

    mov     si, m1
    call    puts
    nop     word [bx+si]                    ; 0f 1f 00
    call    ok

    mov     si, m2
    call    puts
    nop     word [bx+si+1]                  ; 0f 1f 40 01
    call    ok

    mov     si, m3
    call    puts
    nop     word [bx+si+1234h]              ; 0f 1f 80 34 12
    call    ok

    mov     si, m4
    call    puts
    nop     word [5678h]                    ; 0f 1f 06 78 56
    call    ok

    mov     si, m5
    call    puts
    db      0x0f, 0x1f, 0xc0                ; register form, mod=3
    call    ok

    mov     si, m6
    call    puts
    o32 nop dword [bx+si]                   ; 66 0f 1f 00
    call    ok

    mov     si, m7
    call    puts
    a32 nop word [eax]                      ; 67 0f 1f 00
    call    ok

    mov     si, m8
    call    puts
    a32 nop word [eax+ecx+2]                ; sib + disp8
    call    ok

    mov     si, m9
    call    puts
    a32 nop word [eax+ecx+11223344h]        ; sib + disp32
    call    ok

    mov     si, m10
    call    puts
    a32 nop word [0fffffff0h]               ; mod=0 rm=5, disp32
    call    ok

    mov     si, m11
    call    puts
    a32 nop word [ecx*4+40000000h]          ; sib without a base, disp32
    call    ok

    mov     si, okmsg
    call    puts

    mov     ax, 4c00h
    int     21h

ok:
    push    si
    mov     si, okline
    call    puts
    pop     si
    ret

puts:
    push    ax
    push    dx
.loop:
    lodsb
    or      al, al
    jz      .end
    mov     dl, al
    mov     ah, 2
    int     21h
    jmp     .loop
.end:
    pop     dx
    pop     ax
    ret

section .data

m1          db 'nop 1 ', 0
m2          db 'nop 2 ', 0
m3          db 'nop 3 ', 0
m4          db 'nop 4 ', 0
m5          db 'nop 5 ', 0
m6          db 'nop 6 ', 0
m7          db 'nop 7 ', 0
m8          db 'nop 8 ', 0
m9          db 'nop 9 ', 0
m10         db 'nop 10 ', 0
m11         db 'nop 11 ', 0
okline      db 'ok', 13, 10, 0
okmsg       db 'Test OK', 13, 10, 0
""")

    results = self.runDosemu("testit.bat", config=config)

    for i in range(1, 12):
        self.assertIn("nop %i ok" % i, results)
    self.assertIn("Test OK", results)
