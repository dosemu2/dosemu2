def cpu_lmsw(self, cpuemu):
    if self.use_cpu != "emu":
        raise ValueError('lmsw test is for the emulated cpu')

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (%i)
""" % cpuemu

    self.mkfile("testit.bat", """\
c:\\cpulmsw
rem end
""", newline="\r\n")

    # lmsw is privileged, so a client running at CPL 3 - which is what both
    # vm86 and DPMI give it - faults with #GP and the host steps over the
    # instruction.  What must not happen is an invalid opcode, which is what
    # the emulated cpu used to raise, killing the program.  Both operand
    # forms are tried, as they take different paths through the decoder.
    self.mkcom_with_nasm("cpulmsw", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, [msw]
    lmsw    ax                  ; 0f 01 f0
    mov     si, after_reg
    call    puts

    lmsw    word [msw]          ; 0f 01 36 xx xx
    mov     si, after_mem
    call    puts

    mov     si, okmsg
    call    puts

    mov     ax, 4c00h
    int     21h

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

msw         dw 0x0013
after_reg   db 'lmsw reg done', 13, 10, 0
after_mem   db 'lmsw mem done', 13, 10, 0
okmsg       db 'Test OK', 13, 10, 0
""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("lmsw reg done", results)
    self.assertIn("lmsw mem done", results)
    self.assertIn("Test OK", results)
