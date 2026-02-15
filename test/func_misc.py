def create_new_psp(self):
    ename = "getnwpsp"
    cmdline = "COMMAND TAIL TEST"

    self.mkfile("testit.bat", """\
c:\\%s %s
rem end
""" % (ename, cmdline), newline="\r\n")

    # compile sources
    self.mkcom_with_nasm(ename, r"""
bits 16
cpu 386

org 100h

section .text

; designate target segment
    push    cs
    pop     ax
    add     ax, 0200h
    mov     es, ax

; create PSP in memory
    mov     dx, es
    mov     ax, 2600h
    int     21h

; see if the exit field is set
    cmp     word [es:0000], 20cdh
    jne     extfail

; see if the parent PSP is zero
    cmp     word [es:0016h], 0
    je      pntzero

; see if the parent PSP points to a PSP
    push    es
    push    word [es:0016h]
    pop     es
    cmp     word [es:0000h], 20cdh
    pop     es
    jne     pntnpsp

; see if the 'INT 21,RETF' is there
    cmp     word [es:0050h], 21cdh
    jne     int21ms
    cmp     byte [es:0052h], 0cbh
    jne     int21ms

; see if the cmdtail is there
    movzx   cx, byte [es:0080h]
    cmp     cx, %d
    jne     cmdlngth

    inc     cx
    mov     si, cmdline
    mov     di, 81h
    cld
    repe    cmpsb
    jne     cmdtail

success:
    mov     dx, successmsg
    jmp     exit

extfail:
    mov     dx, extfailmsg
    jmp     exit

pntzero:
    mov     dx, pntzeromsg
    jmp     exit

pntnpsp:
    mov     dx, pntnpspmsg
    jmp     exit

int21ms:
    mov     dx, int21msmsg
    jmp     exit

cmdlngth:
    mov     dx, cmdlngthmsg
    jmp     exit

cmdtail:
    mov     dx, cmdtailmsg
    jmp     exit

exit:
    mov     ah, 9
    int     21h

    mov     ah, 4ch
    int     21h

extfailmsg:
    db  "PSP exit field not set to CD20",13,10,'$'

pntzeromsg:
    db  "PSP parent is zero",13,10,'$'

pntnpspmsg:
    db  "PSP parent doesn't point to a PSP",13,10,'$'

int21msmsg:
    db  "PSP is missing INT21, RETF",13,10,'$'

cmdlngthmsg:
    db  "PSP has incorrect command length",13,10,'$'

cmdtailmsg:
    db  "PSP has incorrect command tail",13,10,'$'

successmsg:
    db  "PSP structure okay",13,10,'$'

; 05 20 54 45 53 54 0d
cmdline:
    db " %s",13

""" % (1 + len(cmdline), cmdline))

    results = self.runDosemu("testit.bat")

    self.assertIn("PSP structure okay", results)


def passing_dos_errorlevel_back(self):
    self.mkcom_with_ia16("justerro", r"""
int main(int argc, char *argv[])
{
  return 53;
}
""")

    results = self.runDosemuCmdline(["-E", "justerro.com"])

    self.assertNotIn('Timeout', results)
    self.assertIn('NonZeroReturn:53', results)


def passing_environment_variable(self):
    tstring1 = "0123456789aBcDeF"

    self.mkfile("testit.bat", """\
@echo on
echo %TESTVAR1%
rem end
""", newline="\r\n")

    args = ["TESTVAR1=" + tstring1, "-E", "testit.bat"]
    results = self.runDosemuCmdline(args)

    self.assertNotIn('Timeout', results)
    self.assertNotIn('NonZeroReturn', results)
    self.assertIn("rem end", results, msg="Test incomplete:\n")
    self.assertIn(tstring1, results)


def systype(self):
    self.runDosemu("version.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_debug = "-D+d"
""")

    # read the logfile
    systypeline = "Not found in logfile"
    for line in self.logfiles['log'][0].read_text().splitlines():
        if "system type is" in line:
            systypeline = line
            break

    self.assertIn(self.systype, systypeline)

