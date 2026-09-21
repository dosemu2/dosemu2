def cpu_fpu_undef(self, cpuemu):
    if self.use_cpu != "emu":
        raise ValueError('undefined fpu opcode test is for the emulated cpu')

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (%i)
""" % cpuemu

    self.mkfile("testit.bat", """\
c:\\cpufpud
rem end
""", newline="\r\n")

    # Of the DE 11011nnn encodings only DE D9, FCOMPP, is defined; the
    # other seven are undefined and the cpu raises an invalid opcode for
    # them.  The emulated cpu used to let three of them through, and then
    # had no implementation to run, so it logged a host error and the
    # instruction quietly did nothing - the program's own int 06h handler
    # never saw it.  The program installs such a handler, which skips the
    # instruction and notes that it was reached, so a run can tell the two
    # outcomes apart without dying.
    self.mkcom_with_nasm("cpufpud", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    xor     ax, ax
    mov     es, ax
    mov     ax, [es:6*4]
    mov     [oldoff], ax
    mov     ax, [es:6*4+2]
    mov     [oldseg], ax
    cli
    mov     word [es:6*4], ud_handler
    mov     [es:6*4+2], cs
    sti

    finit
    fld1
    fld1

%macro TRY 3
    mov     byte [hit], 0
    mov     si, %1
    call    puts
    db      %2, %3
    mov     si, executed
    cmp     byte [hit], 0
    je      %%print
    mov     si, invalid
%%print:
    call    puts
%endmacro

    TRY     n0, 0xde, 0xd8
    TRY     n1, 0xde, 0xd9
    TRY     n2, 0xde, 0xda
    TRY     n3, 0xde, 0xdb
    TRY     n4, 0xde, 0xdc
    TRY     n5, 0xde, 0xdd
    TRY     n6, 0xde, 0xde
    TRY     n7, 0xde, 0xdf

    cli
    mov     ax, [oldoff]
    mov     [es:6*4], ax
    mov     ax, [oldseg]
    mov     [es:6*4+2], ax
    sti

    mov     si, okmsg
    call    puts

    mov     ax, 4c00h
    int     21h

ud_handler:
    push    bp
    mov     bp, sp
    push    ds
    push    ax
    push    cs
    pop     ds
    mov     byte [hit], 1
    add     word [bp+2], 2      ; step over the two byte instruction
    pop     ax
    pop     ds
    pop     bp
    iret

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

n0          db 'de d8 ', 0
n1          db 'de d9 ', 0
n2          db 'de da ', 0
n3          db 'de db ', 0
n4          db 'de dc ', 0
n5          db 'de dd ', 0
n6          db 'de de ', 0
n7          db 'de df ', 0
executed    db 'executed', 13, 10, 0
invalid     db 'invalid', 13, 10, 0
okmsg       db 'Test OK', 13, 10, 0
hit         db 0
oldoff      dw 0
oldseg      dw 0
""")

    results = self.runDosemu("testit.bat", config=config)

    # DE D9 is FCOMPP, the only defined encoding of the eight
    self.assertIn("de d9 executed", results)
    for b in ("d8", "da", "db", "dc", "dd", "de", "df"):
        self.assertIn("de %s invalid" % b, results)
    self.assertIn("Test OK", results)
