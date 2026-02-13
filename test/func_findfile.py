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


def sfn_findfirst(self, tests):
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
    iname = self.mkimage_vbr("16", cwd=testdir)

    config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = ""
$_lfn_support = (off)
""" % iname

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
