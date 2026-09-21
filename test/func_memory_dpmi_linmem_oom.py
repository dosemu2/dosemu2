BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_linmem_oom(self):
    """DPMI 1.0 linear memory is taken top down out of the space below
    $_dpmi_base, and page aligned, so the search is for the size plus the
    alignment. Ask for exactly the space that is left and the search fails
    while a free area that size is right there: the out of memory report
    was then raised at a priority that means "not out of memory", and the
    host died on the assertion that forbids it."""

    self.mkfile("testit.bat", BATCHFILE % 'linoom', newline="\r\n")

    self.mkexe_with_djgpp("linoom", r"""
#include <stdio.h>
#include <dpmi.h>

int main(void)
{
  __dpmi_free_mem_info info;
  __dpmi_meminfo m;
  unsigned long avail;

  if (__dpmi_get_free_memory_information(&info) != 0) {
    printf("FAIL: no memory information\n");
    return 1;
  }
  avail = (unsigned long)info.free_linear_address_space_in_pages * 4096UL;
  printf("linear space left %#lx\n", avail);
  if (!avail) {
    printf("FAIL: no linear space at all\n");
    return 1;
  }

  /* all of it: nothing is left over for the alignment, so this cannot be
   * placed, and the free area it was looked for in is exactly this size */
  m.size = avail;
  m.address = 0;
  if (__dpmi_allocate_linear_memory(&m, 0) == 0) {
    printf("FAIL: granted %#lx at %#lx\n", avail,
           (unsigned long)m.address);
    __dpmi_free_memory(m.handle);
    return 1;
  }
  printf("refused, as it has to be\n");

  /* half of it still has to be there, or the pool was simply empty */
  m.size = avail / 2;
  m.address = 0;
  if (__dpmi_allocate_linear_memory(&m, 0) != 0) {
    printf("FAIL: half of it is gone too\n");
    return 1;
  }
  printf("got %#lx at %#lx\n", avail / 2, (unsigned long)m.address);
  __dpmi_free_memory(m.handle);

  printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", timeout=20)

    self.assertNotIn("FAIL:", results)
    # the refusal has to happen, or the test proves nothing
    self.assertIn("refused, as it has to be", results)
    # and the host has to still be there afterwards
    self.assertIn("Test OK", results)
