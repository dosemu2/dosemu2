BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + \
    '$_cpu_vm_dpmi = "emulated"\n' \
    '$_ext_mem = (1024)\n'


def memory_dpmi_int15_window(self):
    """A client may ask DPMI 0x0800 for the whole int15 memory in one call,
    starting at 1M. The host keeps that memory in a hardware-ram region of
    its own, and a request that begins below the region's base cannot be
    served at all, because there is one virtual base to answer with. Check
    that the range starting at 1M is mappable, with a range that was never
    in doubt as the control."""

    self.mkfile("testit.bat", BATCHFILE % 'i15win', newline="\r\n")

    self.mkexe_with_djgpp("i15win", r"""
#include <dpmi.h>
#include <stdio.h>

static int try_map(unsigned long addr, unsigned long size, const char *what)
{
  __dpmi_meminfo mi;
  int rc;

  mi.address = addr;
  mi.size = size;
  rc = __dpmi_physical_address_mapping(&mi);
  printf("%s: addr=%#lx size=%#lx rc=%d linear=%#lx\n", what, addr, size,
      rc, (unsigned long)mi.address);
  if (rc == 0)
    __dpmi_free_physical_address_mapping(&mi);
  return rc;
}

int main(void)
{
  int ctl, win;

  ctl = try_map(0x200000, 0x100000, "control");
  win = try_map(0x100000, 0x100000, "int15 window");

  if (ctl != 0)
    printf("FAIL: the control mapping was refused too\n");
  else if (win != 0)
    printf("FAIL: 1M..2M refused\n");
  else
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
