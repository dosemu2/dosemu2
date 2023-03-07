from pathlib import Path

from common_framework import (VFAT_MNTPNT,
                              setup_vfat_mounted_image, teardown_vfat_mounted_image)


def mfs_truename(self, fstype, tocreate, tests):
    ename = "mfstruen"

    if fstype == "UFS":
        testdir = Path("test-imagedir/dXXXXs/d")
        testdir.mkdir(parents=True, exist_ok=True)

        batchfile = """\
mkdir RootC
d:
cd Sub
c:\\%s
rem end
""" % ename

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
mkdir RootC
x:
cd Sub
c:\\%s
rem end
""" % ename

        config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_lredir_paths = "/mnt/dosemu"
"""

    else:
        self.fail("Incorrect argument")

# Make test files and directory names
    for i in tocreate:
        p = testdir / i[1]
        if i[0] == "FILE":
            p.parents[0].mkdir(parents=True, exist_ok=True)
            p.write_text("Some data")
        elif i[0] == "DIR":
            p.mkdir(parents=True, exist_ok=True)

    self.mkfile("testit.bat", batchfile, newline="\r\n")

    def mkctests(xtests):
        cnv = {
            'LFN0': ('0x7160', '0'),
            'LFN1': ('0x7160', '1'),
            'LFN2': ('0x7160', '2'),
            'SFN': ('0x6000', '0'),
        }
        results = "test_t test[] = {\n"
        for t in xtests:
            results += '    {%s, %s, "%s", "%s"},\n' % (*cnv[t[0]], t[1], t[2])
        results += '  };\n'
        results += '  int tlen = %d;' % len(xtests)
        return results

    # compile sources
    self.mkcom_with_ia16(ename, r"""

#include <i86.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef struct {
  uint16_t intr;
  uint8_t tipo;
  const char *input;
  const char *expected;
} test_t;

%s
char dst[1024];

int main(void)
{
  int ret = 0;
  union REGS r = {};
  struct SREGS rs;
  int i;

  for (i = 0; i < tlen; i++) {
    r.x.ax = test[i].intr;
    if (r.x.ax == 0x7160)
      r.x.cx = test[i].tipo;
    rs.ds = FP_SEG(test[i].input);
    r.x.si = FP_OFF(test[i].input);
    rs.es = FP_SEG(dst);
    r.x.di = FP_OFF(dst);
    // need to set CF so we can detect if the function is implemented
    r.x.cflag = 1;
    int86x(0x21, &r, &r, &rs);
    if (r.x.cflag) {
      if (r.x.ax == 0x7100) {
        snprintf(dst, sizeof(dst), "ERROR: not implemented, or perhaps ignored as not our drive\n");
      } else if (r.x.ax == 0x2) {
        snprintf(dst, sizeof(dst), "ERROR: invalid component");
      } else if (r.x.ax == 0x3) {
        snprintf(dst, sizeof(dst), "ERROR: malformed path or invalid drive letter");
      } else {
        snprintf(dst, sizeof(dst), "ERROR: unknown error code 0x%%04x", r.x.ax);
      }
    }

    if (strcmp(dst, test[i].expected) != 0) {
      printf("FAIL: 0x%%04x/%%d, (sent '%%s', expected '%%s', got '%%s')\n",
          test[i].intr, test[i].tipo, test[i].input, test[i].expected, dst);
      ret += 1;
    } else {
      if (test[i].intr == 0x7160) {
        printf("OKAY: 0x%%04x/%%d, (sent '%%s', got '%%s')\n",
            test[i].intr, test[i].tipo, test[i].input, dst);
      } else {
        printf("OKAY: 0x%%04x    , (sent '%%s', got '%%s')\n",
            test[i].intr, test[i].input, dst);
      }
    }
  }

  if (ret == 0)
    printf("PASS:\n");
  return ret;
}
""" % mkctests(tests))

    results = self.runDosemu("testit.bat", config=config)

    if fstype == "VFAT":
        teardown_vfat_mounted_image(self)
        self.assertRegex(results, r"X: = .*LINUX\\FS/mnt/dosemu")

    self.assertNotIn("FAIL", results)
