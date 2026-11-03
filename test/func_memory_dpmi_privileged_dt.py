BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

NATIVE_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "native"\n'


def memory_dpmi_privileged_dt(self):
    """lmsw reads its operand; a host that emulates the fault it raises at
    CPL 3 must not write to it. dosemu2 used to zero the register for every
    instruction sharing the 0f 00 and 0f 01 opcodes, so a client came back
    from lmsw bx with bx cleared. lldt and ltr are written the same way and
    take the same path."""

    self.mkfile("testit.bat", BATCHFILE % 'privdt', newline="\r\n")

    self.mkexe_with_djgpp("privdt", r"""
#include <stdio.h>
#include <stdint.h>

int main(void)
{
  uint16_t out;

  /* lmsw faults at CPL 3, so the host steps over it. What it must not do
   * is touch bx, which is the instruction's source. */
  __asm__ volatile("movw $0x1234, %%bx\n\t"
                   "lmsw %%bx\n\t"
                   "movw %%bx, %0"
                   : "=r" (out) : : "bx");
  printf("after lmsw bx=%#x\n", out);
  if (out != 0x1234)
    printf("FAIL: lmsw wrote to its source register\n");
  else
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=NATIVE_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
