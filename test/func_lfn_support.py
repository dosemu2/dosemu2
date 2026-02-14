
def lfn_support(self, fstype, confsw):
    ename = "lfnsuppt"
    testdir = self.mkworkdir('d')

    if fstype == "MFS":
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""
    else:       # FAT
        config = """\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
""" % self.mkimage("12", cwd=testdir)

    config += """$_lfn_support = (%s)\n""" % confsw

    self.mkfile("testit.bat", """\
d:
c:\\%s
rem end
""" % ename, newline="\r\n")

    # compile sources

    self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

    push    cs
    pop     ds

; Get current drive and store its letter in fspath
    mov     ax, 1900h
    int     21h
    add     al, 'A'
    mov     byte [fspath], al

; Get Volume info
;    Windows95 - LONG FILENAME - GET VOLUME INFORMATION
;
;    Call:
;      AX = 71A0h
;      DS:DX -> ASCIZ root name (e.g. "C:\")
;      ES:DI -> buffer for file system name
;      CX = size of ES:DI buffer
;
;    Return:
;      CF clear if successful
;        AX destroyed (0000h and 0200h seen)
;        BX = file system flags (see ;01783)
;        CX = maximum length of file name [usually 255]
;        DX = maximum length of path [usually 260]
;        ES:DI buffer filled (ASCIZ, e.g. "FAT","NTFS","CDFS")
;
;      CF set on error
;        AX = error code
;          7100h if function not supported

    mov     ax, 71a0h
    mov     dx, fspath ; ds:dx
    mov     di, fstype ; es:di
    mov     cx, fstypelen
    stc
    int     21h

    jc      chkfail

    cmp     byte [fstype], '$'
    je      prnofstype

prsuccess:
    mov     di, fstype
    mov     cx, fstypelen
    mov     al, 0
    cld
    repne   scasb
    mov     byte [di-1], ')'
    mov     byte [di], 13
    mov     byte [di+1], 10
    mov     byte [di+2], '$'
    mov     dx, success
    jmp     exit

prnofstype:
    mov     dx, nofstype
    jmp     exit

prnotsupported:
    mov     dx, notsupported
    jmp     exit

prcarryset:
    mov     dx, carryset
    jmp     exit

chkfail:
    cmp     ax, 7100h
    jne     prcarryset

    jmp     prnotsupported

exit:
    mov     ah, 9
    int     21h

    mov     ah, 4ch
    int 21h

section .data

carryset:
    db  "Carry Set",13,10,'$'
notsupported:
    db  "Not Supported(AX=0x7100)",13,10,'$'
nofstype:
    db  "Carry Not Set But No Filesystem Type",13,10,'$'
success:
    db  "Operation Success("
fstype:
    times 32 db '$'
fstypelen equ $ - fstype
successend:
    times 4 db 0
fspath:
    db  "?:\", 0
""")

    results = self.runDosemu("testit.bat", config=config)

    if fstype == "MFS":
        if confsw == "on":
            self.assertIn("Operation Success(%s)" % fstype, results)
        else:
            self.assertIn("Not Supported(AX=0x7100)", results)
    else:    # FAT
            self.assertIn("Not Supported(AX=0x7100)", results)

