def command_com_cmdline_length(self, name):

    # Note: The test binary is named 'x' simply because if any longer name
    #       is used then DR-DOS 7.01 and MS-DOS 6.22 truncate the command
    #       line to the lengths 126 and 125 respectively.

    self.mkcom_with_nasm('x', r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

    mov     ax, 3c00h			; create file
    mov     cx, 0
    mov     dx, fname
    int     21h
    jc      prfailcreate

    mov     word [fhndl], ax

    mov     ax, 4000h			; write testdata
    mov     bx, word [fhndl]
    mov     cx, 200h            ; PSP + following 0x100 bytes
    mov     dx, 0               ; cs:0000 is PSP
    int     21h
    jc      prfailwrite
    cmp     ax, 512
    jne     prnumwrite

    mov     ax, 3e00h			; close file
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

fname:
    db  "psp_dump.bin", 0

fhndl:
    dw   0

failcreate:
    db "Create Operation Failed",13,10,'$'
failwrite:
    db "Write Operation Failed",13,10,'$'
numwrite:
    db "Write Incorrect Length",13,10,'$'
""")

    tests = {  # args, expected response
        'multiarg01': ( r'X TEST____ 1234567890 23456789',
                     b'\x1d TEST____ 1234567890 23456789\x0d'),

        'singlearg01': (r'X TEST_____12345678901234567890',
                     b'\x1e TEST_____12345678901234567890\x0d'),
        'singlearg02': (r'X TEST_____12345678901234567890123456789012345678901234567890',
                     b'\x3c TEST_____12345678901234567890123456789012345678901234567890\x0d'),
        'singlearg03': (r'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890',
                     b'\x5a TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890\x0d'),
        'singlearg04': (r'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890',
                     b'\x78 TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\x0d'),
        'singlearg05': (r'X TEST_____1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345',
                     b'\x7d TEST_____1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345\x0d'),
        'singlearg06': (r'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456',
                     b'\x7e TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\x0d'),

        'truncate01': ( r'X TEST_____123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567',
                     b'\x7e TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\x0d'),
        'truncate02': ( r'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890',
                     b'\x7e TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\x0d'),
    }

    self.mkfile("testit.bat", "%s\nrem end\n" % tests[name][0], newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

    def fmt(b):
        cnt = '% 3d,' % b[0]
        tail = str(b[1:])
        return cnt + tail

    buffer = (self.workdir / 'psp_dump.bin').read_bytes()[0x80:]
    expected = tests[name][1]
    pascal_len = buffer[0]
    received = buffer[0:1 + pascal_len + 1]
    self.assertEqual(fmt(expected), fmt(received))

