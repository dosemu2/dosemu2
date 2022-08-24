
def memory_hma_freespace(self):

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""

    self.mkfile("testit.bat", """\
c:\\hmaspace
rem end
""", newline="\r\n")

    self.mkcom_with_ia16("hmaspace", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
  union REGS r;
  struct SREGS rs;
  int ret = 0;

  r.x.ax = 0x4a01;      // get hma free space
  int86x(0x2f, &r, &r, &rs);
  if (r.x.bx == 0) {
    printf("INFO: DOS not using HMA(BX==0)\n");
    ret += 1;
  }
  if (rs.es == 0xffff && r.x.di == 0xffff) {
    printf("INFO: DOS not using HMA(ES:DI==FFFF:FFFF)\n");
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: No HMA available\n");
    return ret;
  }

  printf("INFO: HMA free space == 0x%04x\n", r.x.bx);
  printf("INFO: HMA area at %04X:%04X\n", rs.es, r.x.di);
  printf("PASS: HMA available\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("PASS:", results)


def memory_hma_alloc(self):

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""

    self.mkfile("testit.bat", """\
c:\\hmaalloc
rem end
""", newline="\r\n")

    self.mkcom_with_ia16("hmaalloc", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
  union REGS r;
  struct SREGS rs;
  int ret = 0;

  r.x.ax = 0x4a02;      // get hma memory block
  r.x.bx = 35;          // alloc 35 bytes, should get rounded up to 48
  int86x(0x2f, &r, &r, &rs);

  printf("INFO: HMA size returned %d\n", r.x.bx);
  printf("INFO: HMA block allocated at %04X:%04X\n", rs.es, r.x.di);

  if (r.x.bx != 48) {
    printf("WARN: HMA returned size unexpected\n");
    ret += 1;
  }

  if (rs.es != 0xffff) {
    printf("WARN: HMA block allocated in wrong segment\n");
    ret += 1;
  }

  if (ret != 0) {
    printf("FAIL: Test failed\n");
    return ret;
  }

  printf("PASS: HMA allocation successful\n");
  return 0;
}

""")

    results = self.runDosemu("testit.bat", config=config)

    self.assertIn("PASS:", results)
