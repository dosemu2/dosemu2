
def cpu_wraparound_ip(self):

    self.mkfile("testit.bat", """\
c:\\cpuwrpip
rem end
""", newline="\r\n")

    # build sources
    self.mkexe_with_nasm("cpuwrpip", r"""

bits 16
cpu 8086

segment CODE1 align=16 class=CODE
..start:
    mov ax, DATA1
    mov ds, ax

    ; Jump into the second code segment to start the test
    jmp CODE2:pretop

segment CODE2 align=16 class=CODE
    ; 1. This code sits at the START of the segment (offset 0000h)
    ; 2. When the CPU wraps from FFFFh, it lands right here.

    mov dx, msg_wrap
    mov ah, 09h
    int 21h

    mov ax, 4C00h
    int 21h

    ; --- PADDING ---
    ; Pad until exactly 2 bytes are left in the 64KB segment
    times 65534 - ($ - $$) db 90h

global pretop
pretop:
    nop ; at offset FFFEh
    nop ; at offset FFFFh
    ; After executing the byte at FFFFh, IP increments to 0000h.
    ; Execution continues there

segment DATA1 align=16 class=DATA
    msg_wrap db 'PASS: Segment wrapped! Back at offset 0000h', 0Dh, 0Ah, '$'

segment STACK stack align=16 class=STACK
    resb 512
""")

    results = self.runDosemu("testit.bat")

    self.assertIn("PASS: Segment wrapped! Back at offset 0000h", results)
