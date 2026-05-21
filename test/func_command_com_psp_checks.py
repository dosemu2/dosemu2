def command_com_psp_fcbs(self):

    self.mkcom_with_nasm('psp_dump', r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 3c00h           ; create PSP file
    mov     cx, 0
    mov     dx, fname1
    int     21h
    jc      prfailcreate

    mov     word [fhndl], ax

    mov     ax, 4000h           ; write testdata
    mov     bx, word [fhndl]
    mov     cx, 200h            ; PSP + following 0x100 bytes
    mov     dx, 0               ; cs:0000 is PSP
    int     21h
    jc      prfailwrite
    cmp     ax, 512
    jne     prnumwrite

    mov     ax, 3e00h           ; close file
    mov     bx, word [fhndl]
    int     21h

    mov     al, 0
    jmp     exit

prfailcreate:
    mov     dx, failcreate
    jmp     @1

prfailwrite:
    mov     dx, failwrite
    jmp     @1

prnumwrite:
    mov     dx, numwrite
    jmp     @1

@1:
    mov     ah, 9               ; print error string
    int     21h
    mov     al, 1

exit:
    mov     ah, 4ch
    int     21h

section .data

fname1:
    db  "psp_dump.bin", 0

fhndl:
    dw  0

flen:
    dw  0

failcreate:
    db "Create Operation Failed",13,10,'$'
failwrite:
    db "Write Operation Failed",13,10,'$'
numwrite:
    db "Write Incorrect Length",13,10,'$'
""")

    TESTS = (
        ('plain.fil',
            b'\x00PLAIN   FIL\x00\x00\x00\x00',
            b'\x00           \x00\x00\x00\x00'),
        ('a:single.fil',
            b'\x01SINGLE  FIL\x00\x00\x00\x00',
            b'\x00           \x00\x00\x00\x00'),
        ('a:\\single.fil',
            b'\x01           \x00\x00\x00\x00',
            b'\x00           \x00\x00\x00\x00'), # DRDOS 7.01 fails fcb2 (seems to moving on incorrectly)
        ('c:\\subdir\\single.fil',
            b'\x03           \x00\x00\x00\x00',
            b'\x00           \x00\x00\x00\x00'), # DRDOS 7.01 fails fcb2 (as above)

        ('file1.ex2 file2.ex4',
            b'\x00FILE1   EX2\x00\x00\x00\x00',
            b'\x00FILE2   EX4\x00\x00\x00\x00'),
        ('l: m:',
            b'\x0c           \x00\x00\x00\x00',
            b'\x0d           \x00\x00\x00\x00'), # FRDOS 1.2, 1.3 fail fcb1, Git fails fcb2
        ('z:foo a:bar',
            b'\x1aFOO        \x00\x00\x00\x00',
            b'\x01BAR        \x00\x00\x00\x00'), # FRDOS 1.2, 1.3 fail fcb1, Git fails fcb2
        ('z:foo.ex1 a:bar.ex2',
            b'\x1aFOO     EX1\x00\x00\x00\x00',
            b'\x01BAR     EX2\x00\x00\x00\x00'), # FRDOS 1.2, 1.3 fail fcb1, Git fails fcb2
        ('a:filename.bin c:filename.txt',
            b'\x01FILENAMEBIN\x00\x00\x00\x00',
            b'\x03FILENAMETXT\x00\x00\x00\x00'),

        ('a:\\first.fil c:second.ext',
            b'\x01           \x00\x00\x00\x00',
            b'\x03SECOND  EXT\x00\x00\x00\x00'), # FRDOS 1.2, 1.3, Git fail to move on to arg2 properly
        ('c:\\subdir\\first.fil d:second.fil',
            b'\x03           \x00\x00\x00\x00',
            b'\x04SECOND  FIL\x00\x00\x00\x00'), # FRDOS 1.2, 1.3, Git and DRDOS 7.01 fail fcb2

# The tests below were run on MS-DOS 6.22 to provide the reference values, with which
# MS-DOS 7.0 and 7.1 agreed, however the usefulness of such truncated FCBs is very dubious.
#        ('c:verylongfilename.ext d:second.fil',
#           b'\x03VERYLONGEXT\x00\x00\x00\x00',
#           b'\x04SECOND  FIL\x00\x00\x00\x00'), # FRDOS 1.2, 1.3, Git and FDPP fail fcb1 (missing extension), FRDOS also fail fcb2
#        ('a:filename.bin c:verylongfilename.ext',
#           b'\x01FILENAMEBIN\x00\x00\x00\x00',
#           b'\x03VERYLONGEXT\x00\x00\x00\x00'), # FRDOS 1.2, 1.3, Git and FDPP fail fcb2 (missing extension)
    )

    content = ''
    for index, (tstr, *_) in enumerate(TESTS):
        content += f'psp_dump {tstr}\n'
        content += f'ren psp_dump.bin psp_{index:03d}.bin\n'
    content += 'rem end\n'
    self.mkfile("testit.bat", content, newline="\r\n")

    results = self.runDosemu("testit.bat")

    FCB1 = slice(0x5C, 0x6C)
    FCB2 = slice(0x6C, 0x7C)

    for index, (tstr, exp1, exp2) in enumerate(TESTS):
        f = self.workdir / f'psp_{index:03d}.bin'
        try:
            psp = f.read_bytes()[:0x100]
        except:
            raise self.failureException(f"Read error on '{f.name}'") from None

        self.assertEqual(psp[FCB1], exp1, f'''Test {index} failed to match on FCB1 with "psp_dump '{tstr}'"''')
        self.assertEqual(psp[FCB2], exp2, f'''Test {index} failed to match on FCB2 with "psp_dump '{tstr}'"''')
