import re

BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_popf_iopl(self):
    """A client of a 286|DOS-Extender tells a 386 from a 286 by writing the
    IOPL field and reading it back, which it may do because that extender
    runs it in ring 0. The field has to take the value, and the interrupt
    flag has to stay out of the client's reach all the same."""

    self.mkfile("testit.bat", BATCHFILE % 'popfiopl', newline="\r\n")

    self.mkexe_with_djgpp("popfiopl", r"""
#include <stdio.h>

/* DPMI 0x0902: get virtual interrupt state, AL = 1 when enabled */
static int vif(void)
{
  unsigned ax = 0x0902;

  __asm__ volatile("int $0x31" : "+a" (ax) : : "cc");
  return ax & 0xff;
}

int main(void)
{
  unsigned flags;
  int before, after;

  before = vif();
  /* the 386 test: set both IOPL bits, read the flags back */
  __asm__ volatile("pushfl\n\t"
                   "popl %%eax\n\t"
                   "orl $0x3000, %%eax\n\t"
                   "andl $~0x200, %%eax\n\t"	/* and clear IF while at it */
                   "pushl %%eax\n\t"
                   "popfl\n\t"
                   "pushfl\n\t"
                   "popl %0"
                   : "=r" (flags) : : "eax", "cc", "memory");
  after = vif();
  /* whatever happened above, leave interrupts on */
  __asm__ volatile("int $0x31" : : "a" (0x0901) : "cc");

  printf("flags %08x, virtual IF before %i, after %i\n", flags, before, after);
  if (!before)
    printf("SKIP: interrupts were already disabled\n");
  else if ((flags & 0x3000) != 0x3000)
    printf("FAILURE: the IOPL field did not take the value\n");
  else if (!after)
    printf("FAILURE: popf disabled interrupts at CPL 3\n");
  else
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAILURE:", results)
    skip = re.search(r"SKIP: (.*)", results)
    if skip:
        self.skipTest(skip.group(1).strip())
    self.assertIn("Test OK", results)
