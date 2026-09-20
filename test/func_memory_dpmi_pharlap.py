BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

PHARLAP_CONF = DOSEMU_CONF_DEFAULT + '$_pharlap = (1)\n'


def memory_dpmi_pharlap(self):
    """Check what a phar-lap style client sees: sldt/sgdt have to answer,
    the GDT they point at has to describe the LDT alias, and a descriptor
    written straight into that alias has to become a working selector."""

    self.mkfile("testit.bat", BATCHFILE % 'plgdt', newline="\r\n")

    self.mkexe_with_djgpp("plgdt", r"""
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct dtr {
  uint16_t limit;
  uint32_t base;
} __attribute__((packed));

int main(void)
{
  struct dtr gdtr;
  uint16_t ldtr_m = 0xffff;
  uint32_t ldtr_r = 0xffffffff;
  uint32_t d0, d1, ldt_base, ldt_limit, acc;
  int gsel, lsel, tsel;
  __dpmi_meminfo mi;
  unsigned long base;
  uint32_t lim;

  /* 1. the three instructions phar lap starts with. Under UMIP these
   * #GP from cpl 3 and have to be emulated by the host. */
  memset(&gdtr, 0xee, sizeof(gdtr));
  __asm__ volatile("sldt %0" : "=m"(ldtr_m));
  __asm__ volatile("sldt %0" : "=r"(ldtr_r));
  __asm__ volatile("sgdt %0" : "=m"(gdtr));
  printf("sldt mem=%#x reg=%#lx sgdt base=%#lx limit=%#x\n",
      ldtr_m, (unsigned long)ldtr_r, (unsigned long)gdtr.base, gdtr.limit);

  if (ldtr_m == 0xffff || gdtr.limit == 0xeeee) {
    printf("FAIL: sldt/sgdt not emulated\n");
    return -1;
  }
  if (ldtr_m != (ldtr_r & 0xffff)) {
    printf("FAIL: sldt disagrees with itself\n");
    return -1;
  }
  if (!ldtr_m || !gdtr.base) {
    printf("FAIL: no LDT reported\n");
    return -1;
  }
  if ((ldtr_m | 7) > gdtr.limit) {
    printf("FAIL: LDT selector outside the GDT\n");
    return -1;
  }
  printf("OKAY: sldt/sgdt answered\n");

  /* 2. read the LDT descriptor out of that GDT */
  gsel = __dpmi_allocate_ldt_descriptors(1);
  if (gsel < 0) {
    printf("FAIL: cannot allocate a descriptor\n");
    return -1;
  }
  if (__dpmi_set_segment_base_address(gsel, gdtr.base) ||
      __dpmi_set_segment_limit(gsel, gdtr.limit)) {
    printf("FAIL: cannot point a selector at the GDT\n");
    return -1;
  }
  d0 = _farpeekl(gsel, ldtr_m & ~7);
  d1 = _farpeekl(gsel, (ldtr_m & ~7) + 4);
  ldt_base = ((d0 >> 16) & 0xffff) | ((d1 & 0xff) << 16) | (d1 & 0xff000000);
  ldt_limit = (d0 & 0xffff) | (((d1 >> 16) & 0xf) << 16);
  acc = (d1 >> 8) & 0xff;
  printf("ldt descriptor base=%#lx limit=%#lx acc=%#lx\n",
      (unsigned long)ldt_base, (unsigned long)ldt_limit, (unsigned long)acc);
  if ((acc & 0x9f) != 0x82) {
    printf("FAIL: not an LDT descriptor\n");
    return -1;
  }
  if (!ldt_base || ldt_limit < 0xfff) {
    printf("FAIL: implausible LDT\n");
    return -1;
  }
  printf("OKAY: GDT describes the LDT\n");

  /* 3. write a descriptor into the LDT by hand, the way phar lap does,
   * and see whether the host turned it into a real one */
  tsel = __dpmi_allocate_ldt_descriptors(1);
  if (tsel < 0) {
    printf("FAIL: cannot allocate the victim descriptor\n");
    return -1;
  }
  mi.size = 0x1000;
  if (__dpmi_allocate_memory(&mi)) {
    printf("FAIL: cannot allocate memory\n");
    return -1;
  }
  lsel = __dpmi_allocate_ldt_descriptors(1);
  if (lsel < 0) {
    printf("FAIL: cannot allocate the LDT window\n");
    return -1;
  }
  if (__dpmi_set_segment_base_address(lsel, ldt_base) ||
      __dpmi_set_segment_limit(lsel, ldt_limit)) {
    printf("FAIL: cannot point a selector at the LDT\n");
    return -1;
  }
  base = mi.address;
  d0 = 0xfff | ((base & 0xffff) << 16);
  d1 = ((base >> 16) & 0xff) | (0xf2 << 8) | (0x40 << 16) |
       ((base >> 24) << 24);
  _farpokel(lsel, tsel & ~7, d0);
  _farpokel(lsel, (tsel & ~7) + 4, d1);

  if (__dpmi_get_segment_base_address(tsel, &base)) {
    printf("FAIL: cannot read back the base\n");
    return -1;
  }
  lim = __dpmi_get_segment_limit(tsel);
  printf("after direct write: base=%#lx (wanted %#lx) limit=%#lx\n",
      base, (unsigned long)mi.address, (unsigned long)lim);
  if (base != mi.address || lim != 0xfff) {
    printf("FAIL: direct LDT write did not take\n");
    return -1;
  }
  printf("OKAY: direct LDT write accepted\n");

  /* 4. and the selector it made has to actually work */
  _farpokel(tsel, 0x40, 0xdeadbeef);
  if (_farpeekl(tsel, 0x40) != 0xdeadbeef) {
    printf("FAIL: selector from the direct write is dead\n");
    return -1;
  }
  printf("OKAY: selector usable\n");
  printf("Test OK\n");
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=PHARLAP_CONF, timeout=45)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
