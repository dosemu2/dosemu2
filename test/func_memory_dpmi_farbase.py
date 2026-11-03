BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_farbase(self):
    """A client may set a descriptor whose base is nowhere near the memory
    dosemu mapped; only reading through it is an error. The jit checks no
    limits and dereferences such a base directly, so make sure the fault
    comes back to the client as an exception instead of killing the host.
    A Phar Lap 286|DOS-Extender client does this on its way up."""

    self.mkfile("testit.bat", BATCHFILE % 'farbase', newline="\r\n")

    self.mkexe_with_djgpp("farbase", r"""
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <dpmi.h>

static jmp_buf jb;

static void handler(int sig)
{
  longjmp(jb, 1);
}

int main(void)
{
  unsigned v = 0;
  int sel = __dpmi_allocate_ldt_descriptors(1);

  if (sel < 0) {
    printf("FAIL: no descriptor\n");
    return 1;
  }
  /* far above anything dosemu has, but still a 32bit linear address */
  if (__dpmi_set_segment_base_address(sel, 0xf0000000) == -1) {
    printf("FAIL: base rejected\n");
    return 1;
  }
  if (__dpmi_set_segment_limit(sel, 0xfff) == -1) {
    printf("FAIL: limit rejected\n");
    return 1;
  }

  signal(SIGSEGV, handler);
  if (setjmp(jb) == 0) {
    __asm__ volatile(
      "pushl %%es\n\t"
      "movw %1,%%es\n\t"
      "movl %%es:0x10,%0\n\t"
      "popl %%es\n\t"
      : "=r" (v) : "r" ((unsigned short)sel) : "memory");
    printf("FAIL: read %#x through a base outside our memory\n", v);
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
