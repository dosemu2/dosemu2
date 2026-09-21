BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_linmem_oom(self):
    """DPMI 1.0 linear memory is taken out of the space below $_dpmi_base
    and is page aligned. Asking for exactly the space that is left has to
    work: the free area is that size and it begins on a page, so nothing
    about the alignment is in the way. The search asked every area for the
    size plus a page anyway and turned this one down, and the refusal was
    then raised at a priority that means "not out of memory", on which the
    host died in an assertion. Both halves are checked here: the whole of
    the free space is granted, and a request that really is too big is
    refused without taking the host with it."""

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

  /* all of it: the free area is exactly this size and starts on a page,
   * so the alignment costs nothing and there is no reason to refuse */
  m.size = avail;
  m.address = 0;
  if (__dpmi_allocate_linear_memory(&m, 0) != 0) {
    printf("FAIL: refused %#lx with that much free\n", avail);
    return 1;
  }
  printf("got all %#lx at %#lx\n", avail, (unsigned long)m.address);
  __dpmi_free_memory(m.handle);

  /* and it really is gone again */
  m.size = avail;
  m.address = 0;
  if (__dpmi_allocate_linear_memory(&m, 0) != 0) {
    printf("FAIL: not given back\n");
    return 1;
  }
  __dpmi_free_memory(m.handle);

  /* more than there is: this one has to be refused, and the host has to
   * be alive to say so rather than dying in do_smerror() */
  m.size = avail + 0x100000;
  m.address = 0;
  if (__dpmi_allocate_linear_memory(&m, 0) == 0) {
    printf("FAIL: granted %#lx out of %#lx\n", avail + 0x100000, avail);
    __dpmi_free_memory(m.handle);
    return 1;
  }
  printf("refused what is not there, as it has to be\n");

  printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", timeout=20)

    self.assertNotIn("FAIL:", results)
    # the whole of the free space, which is what used to be refused
    self.assertIn("got all", results)
    # the refusal still has to happen where there really is no room
    self.assertIn("refused what is not there", results)
    # and the host has to still be there afterwards
    self.assertIn("Test OK", results)
