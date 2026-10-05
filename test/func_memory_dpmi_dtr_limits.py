BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_dtr_limits(self):
    """sgdt and sidt have to report table sizes a machine could have. A
    limit of 0xffff means 8192 entries, and 16bit code that works out the
    entry count as (limit + 1) / 8 gets zero out of it and decides the
    table is empty, which is how a 286 extender's descriptor code gives
    up."""

    self.mkfile("testit.bat", BATCHFILE % 'dtrlim', newline="\r\n")

    self.mkexe_with_djgpp("dtrlim", r"""
#include <stdio.h>
#include <stdint.h>

struct dtr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

static int bad(const char *name, struct dtr *d)
{
  unsigned count = ((d->limit + 1) & 0xffff) / 8;

  printf("%s limit %#x base %#x, %u entries\n", name, d->limit, d->base,
         count);
  if (d->limit == 0xffff) {
    printf("FAIL: %s claims 8192 entries\n", name);
    return 1;
  }
  if (count == 0) {
    printf("FAIL: %s entry count comes out zero\n", name);
    return 1;
  }
  return 0;
}

int main(void)
{
  struct dtr gdt, idt;
  int err;

  __asm__ volatile("sgdt %0" : "=m" (gdt));
  __asm__ volatile("sidt %0" : "=m" (idt));
  err = bad("gdt", &gdt);
  err |= bad("idt", &idt);
  if (!err)
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
