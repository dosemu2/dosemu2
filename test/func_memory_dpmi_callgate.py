BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_callgate(self):
    """A client that writes a call gate into its LDT alias has to get a
    working gate: a far call through it enters the code the gate names."""

    self.mkfile("testit.bat", BATCHFILE % 'callgate', newline="\r\n")

    self.mkexe_with_djgpp("callgate", r"""
#include <dpmi.h>
#include <go32.h>
#include <sys/farptr.h>
#include <stdio.h>
#include <stdint.h>

volatile int gate_hit;

int main(void)
{
  char name[] = "MS-DOS";
  __dpmi_paddr api;
  unsigned int ax;
  int cf, alias, gsel;
  uint32_t desc[2], target;
  unsigned char back[8];
  struct {
    uint32_t off;
    uint16_t sel;
  } __attribute__((packed)) fp;

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

  /* the routine the gate will name, placed inline and jumped over */
  __asm__ volatile("movl $1f, %0\n\t"
                   "jmp 2f\n"
                   "1:\n\t"
                   "movl $1, %1\n\t"
                   "lret\n"
                   "2:"
                   : "=r"(target), "=m"(gate_hit));

  gsel = __dpmi_allocate_ldt_descriptors(1);
  if (gsel < 0) {
    printf("FAIL: cannot allocate a descriptor\n");
    return -1;
  }
  /* a 386 call gate, dpl 3, no stack parameters */
  desc[0] = (target & 0xffff) | (_my_cs() << 16);
  desc[1] = (0xec << 8) | (target & 0xffff0000);
  _farpokel(alias, gsel & ~7, desc[0]);
  _farpokel(alias, (gsel & ~7) + 4, desc[1]);

  if (__dpmi_get_descriptor(gsel, back)) {
    printf("FAIL: cannot read the descriptor back\n");
    return -1;
  }
  printf("gate reads back as %02x %02x %02x %02x %02x %02x %02x %02x\n",
      back[0], back[1], back[2], back[3], back[4], back[5], back[6], back[7]);
  if (back[5] != 0xec) {
    printf("SKIP: this host cannot hold a call gate\n");
    return 0;
  }

  fp.off = 0;			/* a gate ignores the offset */
  fp.sel = gsel;
  __asm__ volatile("lcall *%0" : : "m"(fp) : "memory");

  if (!gate_hit) {
    printf("FAIL: the call did not reach the gate's entry point\n");
    return -1;
  }
  printf("OKAY: call through the gate arrived and returned\n");
  printf("Test OK\n");
  return 0;
}
""")

    results = self.runDosemu("testit.bat", timeout=30)

    self.assertNotIn("FAIL:", results)
    self.assertNotIn("SKIP:", results)
    self.assertIn("Test OK", results)
