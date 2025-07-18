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


    mov     ax, 3c00h           ; create ENV file
    mov     cx, 0
    mov     dx, fname2
    int     21h
    jc      prfailcreate

    mov     word [fhndl], ax

    mov     ax, 4000h           ; write testdata
    mov     bx, word [fhndl]
    mov     cx, word [2ch]      ; seg of env block is at cs:2c
    dec     cx                  ; move back to include MCB
    push    ds
    mov     ds, cx
    mov     cx, word [03h]      ; size in paragraphs from MCB
    shl     cx, 4               ; convert to bytes
    mov     word cs:[flen], cx  ; save for later
    mov     dx, 16              ; move forward to start of env block
    int     21h
    pop     ds
    jc      prfailwrite
    cmp     ax, word [flen]
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

fname2:
    db  "env_dump.bin", 0

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

    tests = {  # args, expected response
        'multiarg01': (r'X TEST____ 1234567890 234567890',
                    b'\x1e TEST____ 1234567890 234567890\x0d'),

        'singlearg01':(r'X TEST_____12345678901234567890123456789012345678901234567890',
                    b'\x3c TEST_____12345678901234567890123456789012345678901234567890\x0d'),
        'singlearg02':(r'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456',
                    b'\x7e TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\x0d'),

        #  For old behaviour (<= MS-DOS 6.22)
        'old_dos01':  (r'X TEST_____123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567',
                    b'\x7e TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\x0d'),
        'old_dos02':  (r'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890',
                    b'\x7e TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456\x0d'),

        #  For new behaviour that supports extra long command lines via CMDLINE environment variable
        'new_dos01': ( r'X TEST_____123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567',
                       b'X TEST_____123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567'),
        'new_dos02': ( r'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890',
                       b'X TEST_____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890'),
    }

    self.mkfile("testit.bat", "%s\nrem end\n" % tests[name][0], newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

    buffer1 = (self.workdir / 'psp_dump.bin').read_bytes()[0x80:]
    buffer2 = (self.workdir / 'env_dump.bin').read_bytes()
    endofenv = buffer2.find(b'\x00\x00')
    if endofenv != -1:
        buffer2 = buffer2[0:endofenv + 2]

    expected = tests[name][1]

    pascal_len = buffer1[0]

    if pascal_len < 0x7f:
        def fmt(b):
            cnt = '% 3d,' % b[0]
            tail = str(b[1:])
            return cnt + tail

        expected = fmt(expected)
        received = fmt(buffer1[0:1 + pascal_len + 1])

    else:
        start = buffer2.find(b'CMDLINE=')
        self.assertGreaterEqual(start, 0, 'No CMDLINE variable in env block, env follows: ' + str(buffer2))
        start += len('CMDLINE=')
        end = buffer2.find(b'\x00', start)
        received = buffer2[start:end]

    self.assertEqual(expected, received)
