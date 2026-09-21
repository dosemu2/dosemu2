def cpu_fcmov(self, cpuemu):
    if self.use_cpu != "emu":
        raise ValueError('fcmov test is for the emulated cpu')

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm = "emulated"
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (%i)
""" % cpuemu

    self.mkfile("testit.bat", """\
c:\\cpufcmov
rem end
""", newline="\r\n")

    # FCMOVcc and FCOMI are the P6 additions to the fpu, and the emulated
    # cpu rejected all of them as undefined opcodes.  The two halves are
    # tested in the way the cpu ties them together: FCMOVcc reads the
    # condition codes in the flags register, FCOMI and its relatives are
    # the only fpu instructions that write them.
    #
    # Each FCMOVcc is run against all eight combinations of CF, PF and ZF
    # with st(0)=1.0 and st(1)=2.0, and the line reports for each whether
    # the move happened.  Each FCOMI reports ZF, PF and CF for the four
    # interesting orderings, and whether the stack was popped.
    self.mkcom_with_nasm("cpufcmov", r"""

bits 16
cpu 686

org 100h

section .text

    push    cs
    pop     ds

; %1, %2 = the two opcode bytes, %3 = flags to set
%macro CMOVTRY 3
    finit
    fld     qword [two]         ; st(1)
    fld     qword [one]         ; st(0)
    push    word (0202h | %3)
    popf
    db      %1, %2              ; fcmovcc st0, st1
    fstp    qword [res]
    mov     al, 'N'
    cmp     word [res+6], 04000h ; the high word of 2.0
    jne     %%notmoved
    mov     al, 'Y'
%%notmoved:
    call    putc
%endmacro

; %1 = name, %2, %3 = the two opcode bytes
%macro CMOVROW 3
    mov     si, %1
    call    puts
    CMOVTRY %2, %3, 000h        ; ....
    CMOVTRY %2, %3, 001h        ; CF
    CMOVTRY %2, %3, 004h        ; PF
    CMOVTRY %2, %3, 005h        ; PF CF
    CMOVTRY %2, %3, 040h        ; ZF
    CMOVTRY %2, %3, 041h        ; ZF CF
    CMOVTRY %2, %3, 044h        ; ZF PF
    CMOVTRY %2, %3, 045h        ; ZF PF CF
    mov     si, crlf
    call    puts
%endmacro

; %1, %2 = the two opcode bytes, %3 = st(0), %4 = st(1)
%macro COMITRY 4
    finit
    fld     qword [%4]          ; st(1)
    fld     qword [%3]          ; st(0)
    push    word 0202h
    popf
    db      %1, %2              ; fcomi st0, st1
    pushf
    pop     bx
    fnstsw  ax
    mov     dx, ax
    shr     dx, 11
    and     dl, 7               ; the stack top, 6 unless popped
    mov     [top], dl
    mov     al, '0'
    test    bl, 040h
    jz      %%nozf
    mov     al, '1'
%%nozf:
    call    putc
    mov     al, '0'
    test    bl, 004h
    jz      %%nopf
    mov     al, '1'
%%nopf:
    call    putc
    mov     al, '0'
    test    bl, 001h
    jz      %%nocf
    mov     al, '1'
%%nocf:
    call    putc
    mov     al, ' '
    call    putc
%endmacro

; %1 = name, %2, %3 = the two opcode bytes
%macro COMIROW 3
    mov     si, %1
    call    puts
    COMITRY %2, %3, one, two    ; less
    COMITRY %2, %3, two, one    ; greater
    COMITRY %2, %3, one, one    ; equal
    COMITRY %2, %3, one, nan    ; unordered
    mov     si, keepmsg
    cmp     byte [top], 6
    je      %%keeps
    mov     si, popmsg
%%keeps:
    call    puts
%endmacro

    CMOVROW nfcmovb,   0dah, 0c1h
    CMOVROW nfcmovnb,  0dbh, 0c1h
    CMOVROW nfcmove,   0dah, 0c9h
    CMOVROW nfcmovne,  0dbh, 0c9h
    CMOVROW nfcmovbe,  0dah, 0d1h
    CMOVROW nfcmovnbe, 0dbh, 0d1h
    CMOVROW nfcmovu,   0dah, 0d9h
    CMOVROW nfcmovnu,  0dbh, 0d9h

    COMIROW nfcomi,    0dbh, 0f1h
    COMIROW nfcomip,   0dfh, 0f1h
    COMIROW nfucomi,   0dbh, 0e9h
    COMIROW nfucomip,  0dfh, 0e9h

    mov     si, okmsg
    call    puts

    mov     ax, 4c00h
    int     21h

putc:
    pusha
    mov     dl, al
    mov     ah, 2
    int     21h
    popa
    ret

puts:
    pusha
.loop:
    lodsb
    or      al, al
    jz      .end
    mov     dl, al
    mov     ah, 2
    int     21h
    jmp     .loop
.end:
    popa
    ret

section .data

nfcmovb     db 'fcmovb   ', 0
nfcmovnb    db 'fcmovnb  ', 0
nfcmove     db 'fcmove   ', 0
nfcmovne    db 'fcmovne  ', 0
nfcmovbe    db 'fcmovbe  ', 0
nfcmovnbe   db 'fcmovnbe ', 0
nfcmovu     db 'fcmovu   ', 0
nfcmovnu    db 'fcmovnu  ', 0
nfcomi      db 'fcomi    ', 0
nfcomip     db 'fcomip   ', 0
nfucomi     db 'fucomi   ', 0
nfucomip    db 'fucomip  ', 0
keepmsg     db 'keep', 13, 10, 0
popmsg      db 'pop', 13, 10, 0
crlf        db 13, 10, 0
okmsg       db 'Test OK', 13, 10, 0

one         dq 1.0
two         dq 2.0
nan         dq 07ff8000000000000h

res         dq 0
top         db 0
""")

    results = self.runDosemu("testit.bat", config=config)

    # bit 0 of the index is CF, bit 1 is PF, bit 2 is ZF
    self.assertIn("fcmovb   NYNYNYNY", results)
    self.assertIn("fcmovnb  YNYNYNYN", results)
    self.assertIn("fcmove   NNNNYYYY", results)
    self.assertIn("fcmovne  YYYYNNNN", results)
    self.assertIn("fcmovbe  NYNYYYYY", results)
    self.assertIn("fcmovnbe YNYNNNNN", results)
    self.assertIn("fcmovu   NNYYNNYY", results)
    self.assertIn("fcmovnu  YYNNYYNN", results)

    # less, greater, equal, unordered, as ZF PF CF
    self.assertIn("fcomi    001 000 100 111 keep", results)
    self.assertIn("fcomip   001 000 100 111 pop", results)
    self.assertIn("fucomi   001 000 100 111 keep", results)
    self.assertIn("fucomip  001 000 100 111 pop", results)

    self.assertIn("Test OK", results)
