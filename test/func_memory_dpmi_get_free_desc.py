BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_get_free_desc(self):
    """DPMI 000Bh on a selector nobody allocated fails with 8022h. It used
    to return success and copy an uninitialised buffer of the host's to the
    client, which a client that tells free entries from used ones by this
    call took for a used entry."""

    self.mkfile("testit.bat", BATCHFILE % 'getfree', newline="\r\n")

    self.mkexe_with_djgpp("getfree", r"""
#include <stdio.h>
#include <dpmi.h>

int main(void)
{
  unsigned char d[8];
  int sel, fails = 0;

  sel = __dpmi_allocate_ldt_descriptors(1);
  if (sel == -1) {
    printf("FAIL: no descriptor\n");
    return 1;
  }
  if (__dpmi_get_descriptor(sel, d) == -1) {
    printf("FAIL: allocated %04x, 000Bh refused it\n", sel);
    fails++;
  }
  __dpmi_free_ldt_descriptor(sel);
  if (__dpmi_get_descriptor(sel, d) != -1) {
    printf("FAIL: freed %04x, 000Bh took it\n", sel);
    fails++;
  } else if (__dpmi_error != 0x8022) {
    printf("FAIL: freed %04x, error %04x\n", sel, __dpmi_error);
    fails++;
  }
  if (__dpmi_get_descriptor(0x7ff7, d) != -1) {
    printf("FAIL: 7ff7 was never allocated, 000Bh took it\n");
    fails++;
  }
  if (!fails)
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
