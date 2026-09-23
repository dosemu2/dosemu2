BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_ldt_string_write(self):
    """A client that owns the LDT alias may write a descriptor into it with
    a string instruction as well as with a plain store. The jit runs rep
    movs and rep stos itself and does not take them through the checks that
    ordinary stores pass, so those bytes used to land in the alias page and
    nowhere else: the host LDT went on holding the old entry and the
    selector the program had just made pointed at something else. Ultima
    VIII builds a call gate this way and faults on the first far call
    through it. Here the same is asked of a limit: write the descriptor
    with rep movs, then ask the host what the selector's limit is."""

    self.mkfile("testit.bat", BATCHFILE % 'ldtstrw', newline="\r\n")

    self.mkexe_with_djgpp("ldtstrw", r"""
#include <stdio.h>
#include <string.h>
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>

/* int 2Fh AX=1688h in protected mode: the LDT alias selector in BX */
static unsigned short get_ldt_alias(void)
{
  unsigned short ax = 0x1688, bx = 0;

  asm volatile("int $0x2f" : "+a"(ax), "+b"(bx));
  if (ax != 0)
    return 0;
  return bx;
}

static void ldt_movs(unsigned short alias, unsigned offs,
                     const void *src, int dwords)
{
  asm volatile("pushl %%es\n\t"
               "movw %w0, %%es\n\t"
               "cld\n\t"
               "rep movsl\n\t"
               "popl %%es"
               : "+r"(alias), "+S"(src), "+D"(offs), "+c"(dwords)
               :: "memory");
}

static void ldt_stos(unsigned short alias, unsigned offs,
                     unsigned long val, int dwords)
{
  asm volatile("pushl %%es\n\t"
               "movw %w0, %%es\n\t"
               "cld\n\t"
               "rep stosl\n\t"
               "popl %%es"
               : "+r"(alias), "+D"(offs), "+c"(dwords), "+a"(val)
               :: "memory");
}

int main(void)
{
  unsigned short alias, sel;
  unsigned char d[8];
  unsigned long lim;
  unsigned offs;

  alias = get_ldt_alias();
  if (!alias) {
    printf("FAIL: no LDT alias\n");
    return 1;
  }
  printf("alias selector %#x\n", alias);

  sel = __dpmi_allocate_ldt_descriptors(1);
  if (sel == 0xffff) {
    printf("FAIL: no descriptor\n");
    return 1;
  }
  if (__dpmi_set_segment_base_address(sel, 0) != 0 ||
      __dpmi_set_segment_limit(sel, 0x0fff) != 0) {
    printf("FAIL: cannot set up %#x\n", sel);
    return 1;
  }
  if (__dpmi_get_segment_limit(sel) != 0x0fff) {
    printf("FAIL: limit did not take, %#lx\n",
           (unsigned long)__dpmi_get_segment_limit(sel));
    return 1;
  }

  offs = sel & 0xfff8;
  if (__dpmi_get_descriptor(sel, d) != 0) {
    printf("FAIL: cannot read descriptor %#x\n", sel);
    return 1;
  }
  /* the alias has to show the same bytes, or this test measures nothing */
  if (_farpeekl(alias, offs) != *(unsigned long *)d ||
      _farpeekl(alias, offs + 4) != *(unsigned long *)(d + 4)) {
    printf("FAIL: alias does not show descriptor %#x\n", sel);
    return 1;
  }

  /* a new limit, written the way a string instruction writes it */
  d[0] = 0xff;
  d[1] = 0x2f;
  ldt_movs(alias, offs, d, 2);

  lim = __dpmi_get_segment_limit(sel);
  printf("limit after rep movs %#lx\n", lim);
  if (lim != 0x2fff) {
    printf("FAIL: host kept %#lx, the alias has the new bytes\n", lim);
    return 1;
  }
  printf("rep movs reached the host LDT\n");

  /* clearing an entry the same way: no S bit, so the host drops it */
  ldt_stos(alias, offs, 0, 2);
  lim = __dpmi_get_segment_limit(sel);
  printf("limit after rep stos %#lx\n", lim);
  if (lim != 0) {
    printf("FAIL: host kept %#lx after the entry was cleared\n", lim);
    return 1;
  }
  printf("rep stos reached the host LDT\n");

  printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    # the jit is where the hole is, so ask for it by name
    results = self.runDosemu("testit.bat", timeout=20, config="""\
$_hdimage = "dXXXXs/c:hdtype1 +1"
$_floppy_a = ""
$_cpu_vm_dpmi = "emulated"
$_cpuemu = (0)
""")

    self.assertNotIn("FAIL:", results)
    self.assertIn("rep movs reached the host LDT", results)
    self.assertIn("rep stos reached the host LDT", results)
    self.assertIn("Test OK", results)
