
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


def memory_hma_a20(self):

    config = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""

    self.mkfile("testit.bat", """\
c:\\hmaa20
rem end
""", newline="\r\n")

    self.mkcom_with_ia16("hmaa20", r"""

#include <i86.h>
#include <stdio.h>
#include <string.h>

#define TSTRING "HELLO THERE"

int main(int argc, char *argv[])
{
  union REGS r = {};
  struct SREGS rs;
  char testbuf[sizeof(TSTRING)];
  char savebuf[sizeof(TSTRING)];
  char __far *hma_location;
  char __far *cnv_location;
  int ret = 0;
  int a20;
  int res1, res2, val1, val2;

  r.x.ax = 0x4a02;      // get hma memory block
  r.x.bx = 35;          // alloc 35 bytes, should get rounded up to 48
  int86x(0x2f, &r, &r, &rs);

  printf("INFO: HMA size returned %d\n", r.x.bx);
  printf("INFO: HMA block allocated at %04X:%04X\n", rs.es, r.x.di);

  if (rs.es != 0xffff) {
    printf("WARN: HMA block allocated in wrong segment\n");
    ret += 1;
  }

  // Note: wrapped address will always be in first segment
  hma_location = MK_FP(rs.es, r.x.di);
  cnv_location = MK_FP(0x0000, ((rs.es << 4) + r.x.di) & 0xffff);

  _disable();

  // Get A20 status
  r.x.ax = 0x2402;
  r.x.cflag = 1;
  int86x(0x15, &r, &r, &rs);
  if (!r.x.cflag) {
    a20 = r.h.al;
  } else {
    a20 = 1; // default to on
  }
  res1 = r.x.cflag;
  val1 = r.h.al;
  val2 = r.h.ah;

  // Save memory
  _fmemcpy(savebuf, cnv_location, sizeof(TSTRING));

  // Write to HMA memory
  _fmemcpy(hma_location, TSTRING, sizeof(TSTRING));

  // Read from the wrapped address
  _fmemcpy(testbuf, cnv_location, sizeof(TSTRING));

  // Restore corrupted memory as soon as possible
  _fmemcpy(cnv_location, savebuf, sizeof(TSTRING));

  _enable();

  if (!res1) {
    printf("INFO: A20 is initially %s\n", val1 == 0 ? "disabled" : "enabled");
  } else {
    printf("WARN: A20 status could not be determined (0x%02x)\n", val2);
    ret += 1;
  }

  // Compare to make sure we are not writing in the wrong place
  if (memcmp(testbuf, TSTRING, sizeof(TSTRING)) == 0) {
    printf("WARN: Wrapping is evident\n");
    ret += 1;
  } else {
    printf("INFO: Wrapping is not evident\n");
  }

  _disable();

  // Disable the A20 to enable the wrapping of memory
  r.x.ax = 0x2400;
  r.x.cflag = 1;
  int86x(0x15, &r, &r, &rs);
  res1 = r.x.cflag;
  if (res1)
    val1 = r.h.ah;

  // Save memory
  _fmemcpy(savebuf, cnv_location, sizeof(TSTRING));

  // Write to HMA memory
  _fmemcpy(hma_location, TSTRING, sizeof(TSTRING));

  // Read from the wrapped address
  _fmemcpy(testbuf, cnv_location, sizeof(TSTRING));

  // Restore corrupted memory as soon as possible
  _fmemcpy(cnv_location, savebuf, sizeof(TSTRING));

  // Maybe reenable the A20 to return to normal
  if (a20) {
    r.x.ax = 0x2401;
    r.x.cflag = 1;
    int86x(0x15, &r, &r, &rs);
    res2 = r.x.cflag;
    val2 = r.h.ah;
  }

  _enable();

  if (!res1) {
    printf("INFO: A20 successfully disabled\n");
  } else {
    printf("WARN: A20 could not be disabled (0x%02x)\n", val1);
    ret += 1;
  }

  if (a20)
    if (!res2) {
      printf("INFO: A20 successfully reenabled\n");
    } else {
      printf("WARN: A20 could not be reenabled\n (0x%02x)\n", val2);
      ret += 1;
    }

  // Compare
  if (memcmp(testbuf, TSTRING, sizeof(TSTRING)) == 0) {
    printf("INFO: Wrapping is evident and expected\n");
  } else {
    printf("WARN: Wrapping is not evident\n");
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
