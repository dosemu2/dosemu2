
def command_com_cmdline_length(self, tipo):

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
    mov     ah, 9               ; print string
    int     21h

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
        'test01': (r'TEST01____12345678901234567890',
                   b'\x1f TEST01____12345678901234567890\x0d'),
        'test02': (r'TEST02____12345678901234567890123456789012345678901234567890',
                   b'\x3d TEST02____12345678901234567890123456789012345678901234567890\x0d'),
        'test03': (r'TEST03____12345678901234567890123456789012345678901234567890123456789012345678901234567890',
                   b'\x5b TEST03____12345678901234567890123456789012345678901234567890123456789012345678901234567890\x0d'),
        'test04': (r'TEST04____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890',
                   b'\x79 TEST04____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\x0d'),
        'test05': (r'TEST05____1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345',
                   b'\x7e TEST05____1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345\x0d'),
        'test06': (r'TEST06____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456',
                   b'\x7e TEST06____1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345\x0d'),
        'test07': (r'TEST07____123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567',
                   b'\x7e TEST07____1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345\x0d'),
        'test08': (r'TEST08____12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890',
                   b'\x7e TEST08____1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345\x0d'),
        'test09': (r'TEST09____ 1234567890 123456',
                   b'\x1d TEST09____ 1234567890 123456\x0d'),
    }

    battxt = ''
    for key in tests:
        battxt += 'x ' + tests[key][0] + '\n'
        battxt += 'ren psp_dump.bin %s.bin\n' % key
    battxt += 'rem end\n'

    self.mkfile("testit.bat", battxt, newline="\r\n")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
""")

    def do_test(name):
        def fmt(b):
            cnt = '% 3d,' % b[0]
            tail = str(b[1:])
            return cnt + tail

        fn = name + '.bin'
        buffer = (self.workdir / fn).read_bytes()[0x80:]
        expected = tests[name][1]
        received = buffer[0:len(expected)]
        self.assertEqual(fmt(expected), fmt(received))

    if tipo == 'singlearg':
        do_test('test01')
        do_test('test02')
        do_test('test03')
        do_test('test04')
        do_test('test05')

    elif tipo == 'truncate':
        do_test('test06')
        do_test('test07')
        do_test('test08')

    elif tipo == 'multiargs':
        do_test('test09')

