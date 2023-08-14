BATCHFILE = """\
c:\\%s
rem end
"""

CONFIG = """\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
"""


def dpmi_dpmi10_ldt(self):

# Note: Not sure if I need this
    if 'FDPP' in self.version:
        self.mkfile("userhook.sys", """DOS=HIGH\n""", newline="\r\n")

    self.mkfile("testit.bat", BATCHFILE % 'dpmi1ldt primary', newline="\r\n")

    self.mkexe_with_djgpp("dpmi1ldt", r"""
#include <dos.h>
#include <dpmi.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
  int i, err;
  uint32_t lim;
  int primary = -1;

  if (argc < 2) {
    printf("FAIL: Missing argument (primary|secondary)\n");
    return -2;
  }
  if (strcmp(argv[1], "primary") == 0)
    primary = 1;
  if (strcmp(argv[1], "secondary") == 0)
    primary = 0;
  if (primary < 0) {
    printf("FAIL: Invalid argument (primary|secondary)\n");
    return -2;
  }

  if (primary) {
    unsigned short sel;
    char selstr[16];

    for (i = 0; i < 0x10; i++) {
      sel = (i << 3) | 7;
      err = __dpmi_allocate_specific_ldt_descriptor(sel);
      if (err) {
        printf("FAIL: %s: cannot allocate specific desc %#x\n", argv[1], i);
        return -1;
      }
      lim = __dpmi_get_segment_limit(sel);
      if (lim) {
        printf("FAIL: %s: limit of desc %#x is %lx\n", argv[1], i, lim);
        return -1;
      }
      err = __dpmi_set_segment_limit(sel, 0xfff);
      if (err) {
        printf("FAIL: %s: cannot set limit for %x\n", argv[1], i);
        return -1;
      }
    }
    printf("OKAY: %s: Allocated 16 LDT descriptors\n", argv[1]);

    sel = __dpmi_allocate_ldt_descriptors(1);
    if (sel == -1) {
      printf("FAIL: cannot allocate LDT desc\n");
      return -1;
    }
    err = __dpmi_set_segment_limit(sel, 0xfff);
    if (err) {
      printf("FAIL: cannot set segment limit\n");
      return -1;
    }
    printf("OKAY: %s: Allocated descriptor at %#x\n", argv[1], sel);
    sprintf(selstr, "%i", sel);

    // Now start second copy
    spawnlp(P_WAIT, argv[0], argv[0], "secondary", selstr, NULL);

    for (i = 0; i < 0x10; i++) {
      sel = (i << 3) | 7;
      lim = __dpmi_get_segment_limit(sel);
      if (lim != 0xfff) {
        printf("FAIL: %s: limit of desc %#x is %lx\n", argv[1], i, lim);
        return -1;
      }
      err = __dpmi_free_ldt_descriptor(sel);
      if (err) {
        printf("FAIL: %s: cannot free specific desc %#x\n", argv[1], i);
        return -1;
      }
    }
    printf("OKAY: %s: All descriptors checked and freed\n", argv[1]);
    printf("Test 2 OK\n");

  } else { // secondary

    unsigned short sel = 0;

    if (argc < 3) {
      printf("FAIL: Missing argument (sel number)\n");
      return -2;
    }

    sscanf(argv[2], "%hi", &sel);
    if (!sel) {
      printf("FAIL: %s: bad sel value %s\n", argv[1], argv[2]);
      return -1;
    }
    lim = __dpmi_get_segment_limit(sel);
    if (lim == 0) {
      printf("FAIL: %s: sel %#x not allocated\n", argv[1], sel);
      return -1;
    }
    if (lim != 0xfff) {
      printf("FAIL: %s: limit of %#x is %lx\n", argv[1], sel, lim);
      return -1;
    }
    printf("OKAY: %s: limit of %#x is %#lx\n", argv[1], sel, lim);

    for (i = 0; i < 0x10; i++) {
      sel = (i << 3) | 7;
      err = __dpmi_allocate_specific_ldt_descriptor(sel);
      if (err) {
        printf("FAIL: %s: cannot allocate specific desc %#x\n", argv[1], i);
        return -1;
      }
      lim = __dpmi_get_segment_limit(sel);
      if (lim) {
        printf("FAIL: %s: limit of desc %#x is %lx\n", argv[1], i, lim);
        return -1;
      }
      err = __dpmi_free_ldt_descriptor(sel);
      if (err) {
        printf("FAIL: %s: cannot free specific desc %#x\n", argv[1], i);
        return -1;
      }
    }
    printf("OKAY: %s: Allocated and freed 16 LDT descriptors\n", argv[1]);
    printf("Test 1 OK\n");
  }

  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=CONFIG)

    self.assertIn("Test 1 OK", results)
    self.assertIn("Test 2 OK", results)
    self.assertNotIn("FAILURE:", results)
