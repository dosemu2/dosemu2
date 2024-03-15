import re


def memory_dpmi_leak_check(self, tipo):

    self.mkfile("testit.bat", """\
c:\\dpmileak TEST0 {0}
c:\\dpmileak TEST1 {0}
c:\\dpmileak TEST2 {0}
c:\\dpmileak TEST3 {0}
c:\\dpmileak TEST4 {0}
c:\\dpmileak TEST5 {0}
c:\\dpmileak TEST6 {0}
c:\\dpmileak TEST7 {0}
c:\\dpmileak TEST8 {0}
c:\\dpmileak TEST9 {0}
rem end
""".format(tipo), newline="\r\n")

    self.mkexe_with_djgpp("dpmileak", r"""
#include <dos.h>
#include <dpmi.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

int main(int argc, char *argv[])
{

  __dpmi_meminfo info = {};
  int dofree = 1;

  if (argc < 2) {
    printf("FAIL: Missing argument IDSTRING (normal|nofree)\n");
    return -2;
  }

  if (argc > 2 && strcmp(argv[2], "nofree") == 0)
    dofree = 0;

  info.size = 0x20000;

  if (__dpmi_allocate_memory(&info) == -1) {
    printf("FAIL: (%s) DPMI allocation failed\n", argv[1]);
    return -1;
  }

  printf("INFO: (%s) DPMI allocation info: HNDL=%08lx, SIZE=%08lx, ADDR=%08lx\n",
      argv[1], info.handle, info.size, info.address);

  if (dofree) {
    if (__dpmi_free_memory(info.handle) == -1) {
      printf("FAIL: (%s) DPMI free failed\n", argv[1]);
      return -1;
    }
    printf("INFO: (%s) Successful Free\n", argv[1]);
  } else {
    printf("INFO: (%s) Skipping Free\n", argv[1]);
  }

  printf("DONE: (%s)", argv[1]);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

    # INFO: (TEST0) DPMI allocation info: HNDL=00000012, SIZE=00020000, ADDR=21199000

    m = re.search(r'\(TEST0\).*HNDL=([\da-f]+), SIZE=([\da-f]+), ADDR=([\da-f]+)', results)
    self.assertIsNotNone(m, results)

    FMT = r'(?m)^INFO: \(%s\).*, SIZE=%s, ADDR=%s[\r\n]+'

    self.assertRegex(results, FMT % ('TEST1', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST2', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST3', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST4', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST5', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST6', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST7', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST8', m.group(2), m.group(3)))
    self.assertRegex(results, FMT % ('TEST9', m.group(2), m.group(3)))
    self.assertNotIn("FAIL:", results)
