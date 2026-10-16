BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT


def memory_dpmi_hwram_seam(self):
    """A client may ask to map a run of physical memory that dosemu has
    registered in more than one piece. The first megabyte of extended
    memory is one: the HMA and the memory above it are registered
    separately, so a request for it crosses the seam at 0x110000. The
    window handed back has to be one window, and it has to be the same
    memory both halves were before."""

    self.mkfile("testit.bat", BATCHFILE % 'hwseam', newline="\r\n")

    self.mkexe_with_djgpp("hwseam", r"""
#include <stdio.h>
#include <dpmi.h>
#include <sys/farptr.h>
#include <go32.h>

#define HMA_PHYS 0x100000u
#define EXT_PHYS 0x110000u	/* where the second registration starts */
#define HALF     0x010000u

static int map_phys(unsigned phys, unsigned size, unsigned *lin)
{
  __dpmi_meminfo mi;

  mi.address = phys;
  mi.size = size;
  if (__dpmi_physical_address_mapping(&mi) == -1)
    return -1;
  *lin = mi.address;
  return 0;
}

static void unmap_phys(unsigned lin)
{
  __dpmi_meminfo mi;

  mi.address = lin;
  mi.size = 0;
  __dpmi_free_physical_address_mapping(&mi);
}

static int sel_for(unsigned lin, unsigned size)
{
  int sel = __dpmi_allocate_ldt_descriptors(1);

  if (sel < 0)
    return -1;
  if (__dpmi_set_segment_base_address(sel, lin) == -1)
    return -1;
  if (__dpmi_set_segment_limit(sel, size - 1) == -1)
    return -1;
  return sel;
}

static int poke(unsigned lin, unsigned size, unsigned off, unsigned val)
{
  int sel = sel_for(lin, size);

  if (sel < 0)
    return -1;
  _farsetsel(sel);
  _farnspokel(off, val);
  __dpmi_free_ldt_descriptor(sel);
  return 0;
}

static int peek(unsigned lin, unsigned size, unsigned off, unsigned *val)
{
  int sel = sel_for(lin, size);

  if (sel < 0)
    return -1;
  _farsetsel(sel);
  *val = _farnspeekl(off);
  __dpmi_free_ldt_descriptor(sel);
  return 0;
}

int main(void)
{
  unsigned lo, hi, both, v;
  int bad = 0;

  /* each half on its own fits inside one registration and always
   * worked. Leave a mark in each. */
  if (map_phys(HMA_PHYS, HALF, &lo) == -1) {
    printf("FAIL: cannot map the low half alone\n");
    return 1;
  }
  if (map_phys(EXT_PHYS, HALF, &hi) == -1) {
    printf("FAIL: cannot map the high half alone\n");
    return 1;
  }
  if (poke(lo, HALF, 0, 0xa5a5a5a5) == -1 ||
      poke(hi, HALF, 0, 0x5a5a5a5a) == -1) {
    printf("FAIL: cannot write through the halves\n");
    return 1;
  }
  unmap_phys(lo);
  unmap_phys(hi);

  /* and now the same memory in one piece */
  if (map_phys(HMA_PHYS, 2 * HALF, &both) == -1) {
    printf("FAIL: mapping across the seam refused\n");
    return 1;
  }
  if (peek(both, 2 * HALF, 0, &v) == -1) {
    printf("FAIL: cannot read the window\n");
    return 1;
  }
  if (v != 0xa5a5a5a5) {
    printf("FAIL: low half reads %08x, not the mark\n", v);
    bad = 1;
  }
  if (peek(both, 2 * HALF, HALF, &v) == -1) {
    printf("FAIL: cannot read past the seam\n");
    return 1;
  }
  if (v != 0x5a5a5a5a) {
    printf("FAIL: high half reads %08x, not the mark\n", v);
    bad = 1;
  }
  unmap_phys(both);

  if (!bad)
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=DOSEMU_CONF_DEFAULT, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
