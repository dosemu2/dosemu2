BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_popf_if(self):
    """A client runs at CPL 3, where POPF leaves IF alone. A client that
    disables interrupts and then pops flags with IF set must still have
    them disabled, or the host walks into the critical section the client
    thinks it is holding."""

    self.mkfile("testit.bat", BATCHFILE % 'popfif', newline="\r\n")

    self.mkexe_with_djgpp("popfif", r"""
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
  int before, after;

  before = vif();
  __asm__ volatile("pushfl\n\t"
                   "cli\n\t"
                   "popfl\n\t"
                   : : : "cc", "memory");
  after = vif();
  /* whatever happened above, leave interrupts on */
  __asm__ volatile("int $0x31" : : "a" (0x0901) : "cc");

  printf("virtual IF before %i, after cli+popf %i\n", before, after);
  if (!before)
    printf("SKIP: interrupts were already disabled\n");
  else if (after)
    printf("FAILURE: popf re-enabled interrupts at CPL 3\n");
  else
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAILURE:", results)
    self.assertNotIn("SKIP:", results)
    self.assertIn("Test OK", results)
