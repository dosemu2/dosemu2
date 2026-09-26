def vga_unmapped_window(self, cpuemu):
    if self.use_cpu != "emu":
        raise ValueError('vga unmapped window test is for the emulated cpu')

    # $_video = "vga" is what puts the a0000 graphics window under vgaemu;
    # without it the window is not a mapped region at all and the path this
    # test is about is never taken.
    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_video = "vga"
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (%i)
""" % cpuemu

    self.mkfile("testit.bat", """\
c:\\vgaunmap
rem end
""", newline="\r\n")

    # Touching a000:0000 after leaving a graphics mode is ordinary: the
    # window is still a mapped region, but vga.mem.map[] now describes the
    # text buffer at b8000, so vga_get_mem_base_offset() finds no mapping.
    # Every caller handles that by writing to plain DOS memory instead, so
    # the byte must read back and nothing must be reported to the user.
    self.mkcom_with_nasm("vgaunmap", r"""

bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 0013h           ; graphics, buffer at a000
    int     10h
    mov     ax, 0003h           ; text, buffer at b800
    int     10h

    mov     ax, 0a000h
    mov     es, ax
    xor     di, di
    mov     byte [es:di], 55h
    mov     word [es:di + 140h], 0aa55h
    mov     dword [es:di + 280h], 12345678h
    mov     al, [es:di]
    mov     [back], al

    mov     si, msg
    call    puts
    mov     al, [back]
    call    puthex
    mov     si, crlf
    call    puts

    mov     si, okmsg
    call    puts

    mov     ax, 4c00h
    int     21h

puthex:
    push    ax
    shr     al, 4
    call    .nyb
    pop     ax
    and     al, 0fh
.nyb:
    add     al, '0'
    cmp     al, '9'
    jbe     .put
    add     al, 7
.put:
    mov     dl, al
    mov     ah, 2
    int     21h
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

back        db 0
msg         db 'readback=', 0
crlf        db 13, 10, 0
okmsg       db 'Test OK', 13, 10, 0
""")

    results = self.runDosemu("testit.bat", config=config)

    # Both backends must agree on what the guest sees: the jit writes
    # straight through the mapping, the interpreter goes the long way
    # through vga_write(), and the byte has to come back either way.
    self.assertIn("readback=55", results)
    self.assertIn("Test OK", results)

    log = self.boot_log()
    if "reserving 128Kb at 0xA0000" not in log:
        # The test harness runs dosemu with -td, which forces Video_none,
        # and then the graphics window is not a mapped region at all, so
        # vga_write_access() says no and vga_write() is never called.  The
        # half of this test below needs a video plugin that maps the
        # window; it was written against the X plugin under Xvfb.
        self.skipTest("no VGA aperture with this video plugin")

    # The jit never reaches vga_get_mem_base_offset() for these writes, so
    # this only ever fired on the interpreter, which made it look like a
    # difference between the two backends.  It stays available as a debug
    # message, so only the error() form is checked for here.
    self.assertNotIn("ERROR: VGA address", log)
