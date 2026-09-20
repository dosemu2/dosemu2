def video_font_load(self):
    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""

    self.mkfile("testit.bat", """\
c:\\fontload
rem end
""", newline="\r\n")

    # The tests run with dumb video, where vgaemu used to be left
    # uninitialised, so a BIOS font call dereferenced a NULL plane.
    self.mkcom_with_nasm("fontload", r"""

bits 16
cpu 386

org 100h

section .text

    mov     ax, 1101h       ; load the 8x14 ROM font into block 0
    xor     bl, bl
    int     10h

    mov     ax, 1102h       ; and the 8x8 one into block 1
    mov     bl, 1
    int     10h

    mov     ah, 9
    mov     dx, done
    int     21h

    mov     ax, 4c00h
    int     21h

done    db  "FONTS LOADED", 13, 10, '$'
""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("FONTS LOADED", results)
