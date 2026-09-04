from common_framework import (setup_vfat_mounted_image,
                              teardown_vfat_mounted_image, VFAT_MNTPNT)
from pathlib import Path


def mfs_findfile(self, fstype, nametype, tests):

    if nametype == "LFN":
        disablelfn = ""
    elif nametype == "SFN":
        disablelfn = "set LFN=n"
    else:
        raise ValueError("Incorrect argument")

    if fstype == "UFS":
        testdir = self.workdir.parent / 'd'
        testdir.mkdir(exist_ok=True)

        batchfile = """\
%s
d:
c:\\mfsfind
rem end
""" % disablelfn

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 dXXXXs/d:hdtype1 +1"
$_floppy_a = ""
"""

    elif fstype == "VFAT":
        testdir = Path(VFAT_MNTPNT)
        setup_vfat_mounted_image(self)

        batchfile = """\
lredir X: /mnt/dosemu
lredir
%s
x:
c:\\mfsfind
rem end
""" % disablelfn

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "/mnt/dosemu"
"""

    else:
        raise ValueError("Incorrect argument")

    # common

# Make test files and directory names
    for i in tests:
        p = testdir / i[1];
        if i[0] == "FILE":
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("Some data")
        elif i[0] == "DIR":
            p.mkdir(parents=True, exist_ok=True)
# Extract names to find
    names = [i[2] for i in tests]

# common

    self.mkfile("testit.bat", batchfile, newline="\r\n")

    self.mkexe_with_djgpp("mfsfind", r"""
#include <dir.h>
#include <stdio.h>

int main(void) {
  struct ffblk f;

  int done = findfirst("*.*", &f, FA_HIDDEN | FA_SYSTEM | FA_DIREC);
  while (!done) {
    printf("%10u %2u:%02u:%02u %2u/%02u/%4u %s\n", f.ff_fsize,
           (f.ff_ftime >> 11) & 0x1f, (f.ff_ftime >> 5) & 0x3f,
           (f.ff_ftime & 0x1f) * 2, (f.ff_fdate >> 5) & 0x0f,
           (f.ff_fdate & 0x1f), ((f.ff_fdate >> 9) & 0x7f) + 1980, f.ff_name);
    done = findnext(&f);
  }
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=config)

    if fstype == "VFAT":
        teardown_vfat_mounted_image(self)
        self.assertRegex(results, r"X: = .*LINUX\\FS/mnt/dosemu")

    for name in names:
        self.assertIn(name, results)


def mfs_findfile_ufs_lfn(self):
    tests = (
        # Type, Create, Find
        ("DIR", "Program Files", "Program Files"),
        ("FILE", "verylongfilename.txt", "verylongfilename.txt"),
        ("FILE", "verylongfilename2.txt", "verylongfilename2.txt"),
        ("FILE", "space embedded filename.txt", "space embedded filename.txt"),
        ("FILE", "MixedCaseFilename.ext", "MixedCaseFilename.ext"),
    )
    mfs_findfile(self, "UFS", "LFN", tests)


def mfs_findfile_ufs_sfn(self):
    tests = (
        # Type, Create, Find
        ("DIR", "Program Files", "PROGR~-I"),
        ("FILE", "verylongfilename.txt", "VERYL~3G.TXT"),
        ("FILE", "verylongfilename2.txt", "VERYL~2N.TXT"),
        ("FILE", "space embedded filename.txt", "SPACE~L#.TXT"),
        ("FILE", "MixedCaseFilename.ext", "MIXED~G4.EXT"),
    )
    mfs_findfile(self, "UFS", "SFN", tests)


def mfs_findfile_vfat_linux_mounted_lfn(self):
    tests = (
        # Type, Create, Find
        ("DIR", "Program Files", "Program Files"),
        ("FILE", "verylongfilename.txt", "verylongfilename.txt"),
        ("FILE", "verylongfilename2.txt", "verylongfilename2.txt"),
        ("FILE", "space embedded filename.txt", "space embedded filename.txt"),
        ("FILE", "MixedCaseFilename.ext", "MixedCaseFilename.ext"),
    )
    mfs_findfile(self, "VFAT", "LFN", tests)


def mfs_findfile_vfat_linux_mounted_sfn(self):
    tests = (
        # Type, Create, Find
        ("DIR", "Program Files", "PROGRA~1"),
        ("FILE", "verylongfilename.txt", "VERYLO~1.TXT"),
        ("FILE", "verylongfilename2.txt", "VERYLO~2.TXT"),
        ("FILE", "space embedded filename.txt", "SPACEE~1.TXT"),
        ("FILE", "MixedCaseFilename.ext", "MIXEDC~1.EXT"),
    )
    mfs_findfile(self, "VFAT", "SFN", tests)


def sfn_findfirst(self):
    # Notes:
    #       1/ these all tested on qemu with ms-dos 6.22 and no redirector loaded, so
    #       results are certain to be from the kernel itself not int 2f/1123.
    #       2/ '$' is special as it's replaced with the current drive at run time.

    tests = (
        # sent                 expected

    # devices
        (r"nul",               r"NUL attrib 0x0040"),
        (r"nul.ext",           r"NUL attrib 0x0040"),
        (r"$:nul",             r"NUL attrib 0x0040"),
        (r"$:test\\nul",       r"NUL attrib 0x0040"),
        (r"\\dev\\nul",        r"NUL attrib 0x0040"),
        (r"\\dev\\nul.ext",    r"NUL attrib 0x0040"),
        (r"\\nul",             r"NUL attrib 0x0040"),
        (r"\\nul.ext",         r"NUL attrib 0x0040"),
        (r"\\test\\nul",       r"NUL attrib 0x0040"),
        (r"\\nonexist\\nul",   r"ERROR: 0x0003 - Path not found"),
        (r"$:\\nul",           r"NUL attrib 0x0040"),
        (r"$:\\test\\nul",     r"NUL attrib 0x0040"),
        (r"$:\\nonexist\\nul", r"ERROR: 0x0003 - Path not found"),
        (r"?:\\nul",           r"ERROR: 0x0003 - Path not found"),
        (r"X:\\nul",           r"ERROR: 0x0003 - Path not found"),
        (r"X:\\test\\nul",     r"ERROR: 0x0003 - Path not found"),
        (r"X:\\nonexist\\nul", r"ERROR: 0x0003 - Path not found"),

    # files
        (r"bob",               r"ERROR: 0x0012 - No more files"),
        (r"hello.txt",         r"HELLO.TXT attrib 0x0020"),

    # directories
        (r"test",              r"TEST attrib 0x0010"),
        (r"test\\sub",         r"SUB attrib 0x0010"),
        (r"test\\nosub",       r"ERROR: 0x0012 - No more files"),
    )

    ename = "sfnfindf"

    self.mkfile("testit.bat", """\
REM - FAT16
D:
mkdir test
mkdir test\\sub
echo hello > hello.txt
C:\\{0}

REM - MFS
C:
mkdir test
mkdir test\\sub
echo hello > hello.txt
C:\\{0}

rem end
""".format(ename), newline="\r\n")

    testdir = self.mkworkdir('d')
    (testdir / "there.txt").write_text('there')
    img = self.mkimage_vbr("16", cwd=testdir)

    config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
$_lfn_support = (off)
""" % img.name

    def mkctests(xtests):
        results = "test_t test[] = {\n"
        for t in xtests:
            results += '    {"%s", "%s"},\n' % (t[0], t[1])
        results += '  };\n'
        results += '  int tlen = %d;' % len(xtests)
        return results

    # compile sources
    self.mkcom_with_ia16(ename, r"""

#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  const char *input;
  const char *expected;
} test_t;

%s

static inline void __far *getdta(void)
{
  unsigned short seg, off;

  asm volatile("int $0x21" : "=e"(seg), "=b"(off) : "Rah"((char)0x2f));
  return MK_FP(seg, off);
}

static inline void setdta(void __far *dta)
{
  asm volatile("int $0x21" : : "Rah"((char)0x1a), "Rds"(FP_SEG(dta)), "d"(FP_OFF(dta)));
}

static union {
  struct find_t ffs;
  char padding[512];
} ffu;

int main(void)
{
  int ret = 0;
  union REGS r = {};
  struct SREGS rs;
  unsigned drive;
  int i;
  char *p;
  char src[1024];
  char dst[1024];
  void __far *dta;
  struct find_t *ff = &ffu.ffs;

  // Get current drive
  _dos_getdrive(&drive);

  // set dta
  dta = getdta();
  setdta(ff);

  segread(&rs);

  for (i = 0; i < tlen; i++) {

    strncpy(src, test[i].input, sizeof src);
    if ((p = strchr(src, '$')))
      *p = 'A' + drive - 1;

    r.x.ax = 0x4e00;
    r.x.cx = _A_NORMAL | _A_RDONLY | _A_HIDDEN | _A_SYSTEM | _A_SUBDIR;
    rs.ds = FP_SEG(src);
    r.x.dx = FP_OFF(src);

    // need to set CF so we can detect if the function is implemented
    r.x.cflag = 1;
    int86x(0x21, &r, &r, &rs);

    if (r.x.cflag) {
      if (r.x.ax == 0x2) {
        snprintf(dst, sizeof dst, "ERROR: 0x0002 - File not found");
      } else if (r.x.ax == 0x3) {
        snprintf(dst, sizeof dst, "ERROR: 0x0003 - Path not found");
      } else if (r.x.ax == 0x12) {
        snprintf(dst, sizeof dst, "ERROR: 0x0012 - No more files");
      } else {
        snprintf(dst, sizeof dst, "ERROR: 0x%%04x - unknown error code", r.x.ax);
      }
    } else
      snprintf(dst, sizeof dst, "%%s attrib 0x%%04x", ff->name, ff->attrib);

    if (strcmp(dst, test[i].expected) != 0) {
      printf("FAIL: (sent '%%s', expected '%%s', got '%%s')\n",
                    src, test[i].expected, dst);
      ret++;
    } else
      printf("OKAY: (sent '%%s', got '%%s')\n", src, dst);
  }

  // restore dta
  setdta(dta);

  if (ret == 0)
    printf("PASS: all\n");
  else
    printf("FAIL: one or more\n");
  return ret;
}
""" % mkctests(tests))

    results = self.runDosemu("testit.bat", config=config)

    self.assertNotIn("FAIL", results)


def ds2_findfirst_volume(self, fstype, exists=False):

    if fstype in ['FAT', 'MFS']:
        if fstype == 'FAT':
            drive = "D"
            fsname = "FAT16"
            label = "MSCD0001" if exists else "SOMEVOLNAME"
        else:
            drive = "C"
            fsname = "MFS"
            label = None   # MFS doesn't import the Volume name
    else:
        raise ValueError("Incorrect fstype argument '{fstype}'")

    ename = "ds2findv"

    self.mkfile("testit.bat", f"""\
REM {self.version}

REM - {fsname}
{drive}:
mkdir test
mkdir test\\sub
echo hello > hello.txt
C:\\{ename} {drive}:

rem end
""", newline="\r\n")

    testdir = self.mkworkdir('d')
    image = self.mkimage_vbr("16", label=label, cwd=testdir)

    config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
$_lfn_support = (off)
""" % image.name

    # compile sources
    self.mkcom_with_nasm(ename, rf"""
; Public Domain

ITERATIONS equ 3

    cpu 8086
    org 256
start:
    mov ah, 19h
    int 21h
    push ax

    xor dx, dx
    mov si, 81h
.loop:
    lodsb
    cmp al, 13
    je .end
    cmp al, 'A'
    jb .loop
    cmp al, 'z'
    ja .loop
.letter:
    dec ax
    and al, 31
    xchg dx, ax
    jmp .loop

.end:
    add byte [msg.drive.patch], dl
    mov ah, 0Eh
    int 21h

    mov si, ITERATIONS
outerloop:
    mov ax, 4E00h
    mov dx, search
    mov cx, 8
innerloop:
    mov bp, ax
    int 21h
    jc .next
    call dump

    mov ah, 4Fh
    jmp innerloop

.next:
    call dump
    dec si
    jnz outerloop

    mov dx, msg.endoftest
    call disp_dx_msg

    pop dx
    mov ah, 0Eh
    int 21h

    mov ax, 4C00h
    int 21h

dump:
    pushf
    push ax
    mov dx, msg.drive.1
    call disp_dx_msg
    mov ax, ITERATIONS
    sub ax, si
    call disp_al_hex.nybble
    mov dx, msg.drive.2
    call disp_dx_msg
    pop ax
    popf
    jc .fail
.success:
    mov ax, bp
    xchg al, ah
    call disp_al_hex

    mov dx, msg.success.1
    call disp_dx_msg
    mov al, [80h + 15h]
    call disp_al_hex
    mov dx, msg.success.2
    call disp_dx_msg
    mov ax, [80h + 18h]
    call disp_ax_hex
    mov dx, msg.success.3
    call disp_dx_msg
    mov ax, [80h + 16h]
    call disp_ax_hex
    mov dx, msg.success.4
    call disp_dx_msg
    mov ax, [80h + 1Ah + 2]
    call disp_ax_hex
    mov dx, msg.success.5
    call disp_dx_msg
    mov ax, [80h + 1Ah]
    call disp_ax_hex
    mov dx, msg.success.6
    call disp_dx_msg
    mov dx, 80h + 1Eh
    mov di, dx
    mov cx, -1
    xor ax, ax
    repne scasb
    not cx
    dec cx
    mov ah, 40h
    mov bx, 1
    int 21h
    mov dx, msg.success.7
    call disp_dx_msg
    retn

.fail:
    xchg ax, bp
    xchg al, ah
    call disp_al_hex
    xchg ax, bp

    mov dx, msg.fail.1
    call disp_dx_msg
    call disp_ax_hex
    mov dx, msg.fail.2
    call disp_dx_msg
    retn

disp_dx_msg:
    push ax
    mov ah, 09h
    int 21h
    pop ax
    retn

disp_ax_hex:
    xchg al, ah
    call disp_al_hex
    xchg al, ah
disp_al_hex:
    push cx
    mov cl, 4
    rol al, cl
    call .nybble
    rol al, cl
    pop cx
.nybble:
    push ax
    push dx
    and al, 15
    add al, '0'
    cmp al, '9'
    jbe .got
    add al, 7
.got:
    xchg dx, ax
    mov ah, 02h
    int 21h
    pop dx
    pop ax
    retn

msg:
.drive.1:      db "Drive "
.drive.patch:  db "A: (",36
.drive.2:      db "): ",36
.fail.1:       db "h: CY error, AX=",36
.fail.2:       db "h",13,10,36
.success.1:    db "h: NC, attrib=",36
.success.2:    db "h, date=",36
.success.3:    db "h, time=",36
.success.4:    db "h, size=",36
.success.5:    db "_",36
.success.6:    db "h, name=",'"',36
.success.7:    db '"',13,10,36
.endoftest:    db "EndOfTest",13,10,36

search:
    db "{'MSCD0001' if exists else 'CON'}",0
""")

    results = self.runDosemu("testit.bat", config=config)

    if exists:
        # Drive D: (0): 4Eh: NC, attrib=08h, date=5CE1h, time=8D53h, size=0000_0000h, name="MSCD0001"
        self.assertRegex(results, rf'Drive {drive}: \(\d\): 4Eh: NC, attrib=08h,.*name="MSCD0001"')
    else:
        # Drive C: (2): 4Eh: NC, attrib=40h, date=5CDEh, time=956Ah, size=0000_0000h, name="CON"
        # Drive C: (2): 4Fh: CY error, AX=0012h
        self.assertNotRegex(results, rf'Drive {drive}: \(\d\): 4[EF]h: NC, attrib=')

    self.assertIn("EndOfTest", results)
