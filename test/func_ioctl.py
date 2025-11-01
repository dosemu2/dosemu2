
from subprocess import check_call, DEVNULL


def drv_removable(self):
    ename = "drvremov"

    self.mkfile("testit.bat", """\
REM - FAT16
C:\\{0} d:

REM - MFS network
C:\\{0} C:

REM - FAT12 floppy
C:\\{0} A:

REM - phantom floppy
C:\\{0} B:

REM - non existent device
C:\\{0} X:

rem end
""".format(ename), newline="\r\n")

    testdir = self.mkworkdir('d')
    iname = self.mkimage_vbr("16", cwd=testdir)

    testfpy = self.imagedir / "a.img"
    testfpy.write_bytes(b'\x00' * 1474560)
    check_call(["mkfs", "-t", "fat", str(testfpy)], stdout=DEVNULL, stderr=DEVNULL)

    config="""\
$_hdimage = "dXXXXs/c:hdtype1 %s +1"
$_floppy_a = "a.img"
$_lfn_support = (off)
""" % iname

    # compile sources
    self.mkcom_with_ia16(ename, r"""

#include <ctype.h>
#include <dos.h>
#include <i86.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

int main(int argc, char *argv[])
{
  union REGS r = {};
  unsigned drive;
  char dletter;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <drive>:\n", argv[0]);
    return 1;
  }

  dletter = toupper(argv[1][0]);
  if (dletter < 'A' || dletter > 'Z' || argv[1][1] != ':' || argv[1][2]) {
    fprintf(stderr, "Usage: %s <drive>:\n", argv[0]);
    return 1;
  }

  r.x.ax = 0x4408;
  r.h.bl = dletter - 'A' + 1;
  // need to set CF so we can detect if the function is implemented
  r.x.cflag = 1;
  int86(0x21, &r, &r);
  if (r.x.cflag) {
    printf("Drive %c: ERROR: 0x%04x\n", dletter, r.x.ax);
  } else {
    printf("Drive %c: %s\n", dletter, (r.x.ax == 0) ? "removable": "fixed");
  }

  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=config)
    self.assertIn(r"Drive C: ERROR: 0x0001", results) # remote drive
    self.assertIn(r"Drive D: fixed", results)
    self.assertIn(r"Drive A: removable", results)
    self.assertIn(r"Drive B: removable", results)
    self.assertIn(r"Drive X: ERROR: 0x000f", results) # non-existent
