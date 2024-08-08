import re

TTAR = "TEST_JAPHETH.tar"
TFILES = {
    218: ("dpmihxrt218.exe", "65fda018f4422c39dbf36860aac2c537cfee466b"),
    219: ("dpmihxrt219.exe", "4b0f60346244a5be9e3208fe63063bd26209d234"),
    220: ("dpmihxrt220.exe", "c2a37cf9b8bab4fe911186821989fc49e6c0feb2"), # Pre2
}


def doit(self, version, switch, eofisok=False):
    self.unTarOrSkip(TTAR, [TFILES[version],])
    tfile = self.workdir / TFILES[version][0]
    tfile.rename(self.workdir / "dpmi.exe")

    self.mkfile("testit.bat", """\
c:\\dpmi %s
rem end
""" % switch, newline="\r\n")

    return self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_dpmi = (0x40000)
""", timeout=60, eofisok=eofisok)


def bare(self):
    results = doit(self, 218, "")

#Cpu is in V86-mode
#Int 15h, ax=e801h, extended memory: 8256 kB
#int 15h, ax=e820h, available memory at 000100000, size 8256 kB
#XMS v3.0 host found, largest free block: 8192 kB
#No VCPI host found
#DPMI v0.90 host found, cpu: 05, support of 32-bit clients: 1
#entry initial switch to protected-mode: F000:F500
#size task-specific memory: 230 paragraphs
#now in protected mode, client CS/SS/DS/FS/GS: A7/AF/AF/0/0
#Eflags=206, ES (=PSP): B7 (environment: BF, parent PSP segm: 431)
#GDTR: 17.A308100 IDTR: 7FF.A308200 LDTR: 0 TR: 0
#DPMI version flags: 5
#master/slave PICs base: 08/70
#state save protected-mode: 97:1, real-mode: F000:F514
#size state save buffer: 52 bytes
#raw jump to real-mode: 97:0, protected-mode: F000:F513
#largest free/lockable memory block (kB): 131004/131004
#free unlocked (=virtual) memory (kB): 131004
#total/free address space (kB): 32768/32768
#total/free physical memory (kB): 131072/131004
#Coprocessor status: 4D
#vendor: DOSEMU Version 2.0, version: 0.90
#capabilities: 78
#vendor 'MS-DOS' API entry: 97:130
#'MS-DOS' API, ax=100h (get LDT selector): E7

    self.assertRegex(results, r"DPMI v\d.\d+ host found, cpu: \d+, support of 32-bit clients: 1")
    self.assertRegex(results, r"DPMI version flags: 5")
    self.assertRegex(results, r"vendor: DOSEMU2 Version 2.0, version: \d.\d+")
    self.assertRegex(results, r"capabilities: \d+")


def _c(self):
    results = doit(self, 218, "-c")
    self.assertRegex(results, r"time.*CLI/STI: \d+ ms")
    self.assertRegex(results, r"time.*interrupts via DPMI: \d+ ms")


def _d(self):
    results = doit(self, 219, "-d")
    t = re.search(r"(\d+) descriptors allocated", results)
    self.assertIsNotNone(t, "Unable to parse number of allocated descriptors")
    num = int(t.group(1))
    self.assertGreater(num, 8000, results)


def _e(self):
    # Dosemu2 shuts down with stack exhaustion so we have to allow EOF
    results = doit(self, 219, "-e", eofisok=True)
    # We want to achieve in the future at least the current level of nesting (6)
    self.assertIn('ERROR: DPMI: too many nested realmode invocations, in_dpmi_rm_stack=6', results)


def _i(self):
    results = doit(self, 218, "-i")
    self.assertRegex(results, r"time.*IN 21: \d+ ms")


def _m(self):
    results = doit(self, 218, "-m")
    t = re.search(r"alloc largest mem block \(size=(\d+) kB\) returned handle [0-9a-fA-F]+, base \d+", results)
    self.assertIsNotNone(t, "Unable to parse memory block size")
    size = int(t.group(1))
    self.assertGreater(size, 128000, results)


def _r(self):
    results = doit(self, 218, "-r")
    self.assertRegex(results, r"time.*INT 69h: \d+ ms")
    self.assertRegex(results, r"time.*INT 31h, AX=0300h \(Sim INT 69h\): \d+ ms")
    self.assertRegex(results, r"time.*real-mode callback: \d+ ms")
    self.assertRegex(results, r"time.*raw mode switches PM->RM->PM: \d+ ms")


def _t(self):
    results = doit(self, 218, "-t")

#C:\>c:\dpmi -t
#allocated rm callback FF10:214, rmcs=AF:20E4
#calling rm proc [53A:8B6], rm cx=1
#  inside rm proc, ss:sp=7A8:1FC, cx=1
#  calling rm callback FF10:214
#    inside rm callback, ss:esp=9F:EFF4, ds:esi=F7:1FC
#    es:edi=AF:20E4, rm ss:sp=7A8:1FC, rm cx=1
#    calling rm proc [53A:8B6]
#      inside rm proc, ss:sp=7A8:1F8, cx=2
#      calling rm callback FF10:214
#        inside rm callback, ss:esp=9F:EFE0, ds:esi=F7:1F8
#        es:edi=AF:20E4, rm ss:sp=7A8:1F8, rm cx=2
#        exiting
#      back in rm proc, ss:sp=7A8:1F8; exiting
#    back in rm callback, rm ss:sp=7A8:1FC, rm cx=2; exiting
#  back in rm proc, ss:sp=7A8:1FC; exiting
#back in protected-mode, rm ss:sp=7A8:200, rm cx=2

    self.assertRegex(results, re.compile(r"^allocated rm callback", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^  inside rm proc", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^    inside rm callback", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^      inside rm proc", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^        inside rm callback", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^      back in rm proc", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^    back in rm callback", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^  back in rm proc", re.MULTILINE))
    self.assertRegex(results, re.compile(r"^back in protected-mode", re.MULTILINE))


def _z(self):
    results = doit(self, 220, "-z")

    t = re.search(r"DPMI: launch succeeded, int 21h calls: (\d+)", results)
    self.assertIsNotNone(t, "Unable to parse number of int 21h calls")
    num = int(t.group(1))
    self.assertGreater(num, 800, results)


def memory_dpmi_japheth(self, switch):
    f = {
        '': bare,
        '-c': _c,
        '-d': _d,
        '-e': _e,
        '-i': _i,
        '-m': _m,
        '-r': _r,
        '-t': _t,
        '-z': _z,
    }
    f[switch](self)
