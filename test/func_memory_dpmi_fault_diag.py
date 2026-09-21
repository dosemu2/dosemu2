BATCHFILE = """\
c:\\faultdia
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_fault_diag(self):
    """A client that faults and then exits with an error code used to leave
    nothing in the log at the default debug level, so the run looked like
    dosemu quitting on its own. Check that the fault is now reported."""

    self.mkfile("testit.bat", BATCHFILE % (), newline="\r\n")

    # Reads through a null selector, which is #GP with a zero error code,
    # and leaves the fault to the runtime, which prints its own dump and
    # exits with a non-zero code.
    self.mkexe_with_djgpp("faultdia", r"""
#include <stdio.h>

int main(void)
{
  unsigned v;

  printf("About to fault\n");
  fflush(stdout);
  __asm__ volatile(
    "pushl %%ds\n\t"
    "xorl %%eax,%%eax\n\t"
    "movw %%ax,%%ds\n\t"
    "movl 0xec,%0\n\t"
    "popl %%ds\n\t"
    : "=r" (v) : : "eax");
  printf("NOTFAULTED: read %#x\n", v);
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertIn("About to fault", results)
    self.assertNotIn("NOTFAULTED:", results)

    log = self.boot_log()
    self.assertIn("DPMI: client exited with code", log)
    self.assertIn("last CPU exception was 0x0d", log)
