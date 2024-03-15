import re


def memory_dpmi_leak_check_dos(self, tipo):

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

  int selector, segment;
  int dofree = 1;

  if (argc < 2) {
    printf("FAIL: Missing argument IDSTRING (normal|nofree)\n");
    return -2;
  }

  if (argc > 2 && strcmp(argv[2], "nofree") == 0)
    dofree = 0;

  segment = __dpmi_allocate_dos_memory(4096, &selector);
  if (segment == -1) {
    printf("FAIL: (%s) DPMI allocation (DOS) failed\n", argv[1]);
    return -1;
  }

  printf("INFO: (%s) DPMI allocation (DOS) info: SEGMENT=%04x, SELECTOR=%04x\n",
      argv[1], segment, selector);

  if (dofree) {
    if (__dpmi_free_dos_memory(selector) == -1) {
      printf("FAIL: (%s) DPMI free (DOS) failed\n", argv[1]);
      return -1;
    }
    printf("INFO: (%s) Successful Free (DOS)\n", argv[1]);
  } else {
    printf("INFO: (%s) Skipping Free (DOS)\n", argv[1]);
  }

  printf("DONE: (%s)", argv[1]);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
""")

    # INFO: (TEST0) DPMI allocation (DOS) info: SEGMENT=04c5, SELECTOR=01bf

    m = re.search(r'\(TEST0\).*info: SEGMENT=([\da-f]+), SELECTOR=([\da-f]+)', results)
    self.assertIsNotNone(m, results)

    FMT = r'(?m)^INFO: \(%s\).*info: SEGMENT=%s, SELECTOR=%s[\r\n]+'

    self.assertRegex(results, FMT % ('TEST1', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST2', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST3', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST4', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST5', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST6', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST7', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST8', m.group(1), m.group(2)))
    self.assertRegex(results, FMT % ('TEST9', m.group(1), m.group(2)))
    self.assertNotIn("FAIL:", results)
