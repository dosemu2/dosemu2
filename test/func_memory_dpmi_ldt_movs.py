BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_ldt_movs(self):
    """A descriptor written into the LDT alias with a string op has to
    reach the host, the same as one written with plain stores."""

    self.mkfile("testit.bat", BATCHFILE % 'ldtmovs', newline="\r\n")

    self.mkexe_with_djgpp("ldtmovs", r"""
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <stdio.h>
#include <stdint.h>

/* build a 32-bit data descriptor, dpl 3, for base/limit */
static void mkdesc(uint32_t *d, unsigned long base, uint32_t limit)
{
  d[0] = (limit & 0xffff) | ((base & 0xffff) << 16);
  d[1] = ((base >> 16) & 0xff) | (0xf2 << 8) | (((limit >> 16) & 0xf) << 16) |
         (0x40 << 16) | ((base >> 24) << 24);
}

int main(void)
{
  char name[] = "MS-DOS";
  __dpmi_paddr api;
  __dpmi_meminfo mi;
  unsigned int ax;
  int cf, alias, tsel;
  uint32_t desc[2];
  unsigned long base;
  uint32_t lim;

  {
    /* int 2fh ax=168ah, the DPMI vendor entry point call */
    unsigned int eax = 0x168a;
    uint32_t off = 0;
    unsigned short sel = 0, olde;

    __asm__ volatile("movw %%es, %w3\n\t"
                     "int $0x2f\n\t"
                     "movw %%es, %w2\n\t"
                     "movw %w3, %%es"
                     : "+a"(eax), "=D"(off), "=r"(sel), "=&r"(olde)
                     : "S"(name)
                     : "memory");
    if ((eax & 0xff) != 0) {
      printf("SKIP: no MS-DOS extension API\n");
      return 0;
    }
    api.offset32 = off;
    api.selector = sel;
  }
  ax = 0x0100;
  __asm__ volatile("lcall *%2\n\t"
                   "sbbl %1, %1"
                   : "+a"(ax), "=r"(cf)
                   : "m"(api)
                   : "cc", "memory");
  if (cf) {
    printf("SKIP: host does not hand out an LDT alias\n");
    return 0;
  }
  alias = ax & 0xffff;
  printf("LDT alias selector %#x\n", alias);

  mi.size = 0x1000;
  if (__dpmi_allocate_memory(&mi)) {
    printf("FAIL: cannot allocate memory\n");
    return -1;
  }

  /* 1. the plain-store path, which already works: if it does not, this
   * host has no LDT monitor at all and there is nothing to test. */
  tsel = __dpmi_allocate_ldt_descriptors(1);
  if (tsel < 0) {
    printf("FAIL: cannot allocate a descriptor\n");
    return -1;
  }
  mkdesc(desc, mi.address, 0xfff);
  _farpokel(alias, tsel & ~7, desc[0]);
  _farpokel(alias, (tsel & ~7) + 4, desc[1]);
  if (__dpmi_get_segment_base_address(tsel, &base)) {
    printf("FAIL: cannot read back the base\n");
    return -1;
  }
  lim = __dpmi_get_segment_limit(tsel);
  if (base != mi.address || lim != 0xfff) {
    printf("SKIP: direct LDT writes are not supported here\n");
    return 0;
  }
  printf("OKAY: store into the alias accepted\n");

  /* 2. the same thing with rep movs, the way a 16-bit extender's memcpy
   * copies a descriptor it built on its stack. */
  tsel = __dpmi_allocate_ldt_descriptors(1);
  if (tsel < 0) {
    printf("FAIL: cannot allocate the second descriptor\n");
    return -1;
  }
  mkdesc(desc, mi.address, 0x7ff);
  {
    unsigned short olde;
    uint32_t src = (uint32_t)desc;
    uint32_t dst = tsel & ~7;
    uint32_t cnt = 2;

    __asm__ volatile("movw %%es, %w0\n\t"
                     "movw %w4, %%es\n\t"
                     "cld\n\t"
                     "rep movsl\n\t"
                     "movw %w0, %%es"
                     : "=&r"(olde), "+S"(src), "+D"(dst), "+c"(cnt)
                     : "r"(alias)
                     : "memory");
  }
  if (__dpmi_get_segment_base_address(tsel, &base)) {
    printf("FAIL: cannot read back the base after the string write\n");
    return -1;
  }
  lim = __dpmi_get_segment_limit(tsel);
  printf("after string write: base=%#lx (wanted %#lx) limit=%#lx\n",
      base, (unsigned long)mi.address, (unsigned long)lim);
  if (base != mi.address || lim != 0x7ff) {
    printf("FAIL: LDT write by a string op did not take\n");
    return -1;
  }
  _farpokel(tsel, 0x40, 0xcafebabe);
  if (_farpeekl(tsel, 0x40) != 0xcafebabe) {
    printf("FAIL: selector from the string write is dead\n");
    return -1;
  }
  printf("OKAY: LDT write by a string op accepted\n");
  printf("Test OK\n");
  return 0;
}
""")

    results = self.runDosemu("testit.bat", timeout=30)

    self.assertNotIn("FAIL:", results)
    self.assertNotIn("SKIP:", results)
    self.assertIn("Test OK", results)
