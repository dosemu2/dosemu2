BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_nullseg(self):
    """A null selector is legal to load into ds and faults only when
    something is addressed through it. The jit has no segment limit checks
    and lets such an access run into unmapped memory, so make sure that
    still reaches the client as an exception instead of killing the host."""

    self.mkfile("testit.bat", BATCHFILE % 'nullseg', newline="\r\n")

    self.mkexe_with_djgpp("nullseg", r"""
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>

static jmp_buf jb;

static void handler(int sig)
{
  longjmp(jb, 1);
}

int main(void)
{
  unsigned v = 0;

  signal(SIGSEGV, handler);
  if (setjmp(jb) == 0) {
    __asm__ volatile(
      "pushl %%ds\n\t"
      "xorl %%eax,%%eax\n\t"
      "movw %%ax,%%ds\n\t"
      "movl 0xec,%0\n\t"
      "popl %%ds\n\t"
      : "=r" (v) : : "eax");
    printf("FAIL: read %#x through a null selector\n", v);
  } else {
    printf("Test OK\n");
  }
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
