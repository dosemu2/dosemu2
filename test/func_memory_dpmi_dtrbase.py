BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

# the two DPMI backends whose answer is ours to pick. Under native DPMI
# the instruction never reaches us: the kernel traps it under UMIP and
# answers itself, and on a host without UMIP the processor answers.
DPMI_VMS = ("emulated", "kvm")


def memory_dpmi_dtrbase(self):
    """sgdt and sidt are all a client ever learns about the two tables,
    and a client that believes what it is told will go and read them.
    Saying the GDT is at linear 0 points it at the interrupt vector
    table, where a 286 extender read a descriptor out of vectors 0 and 1
    and died on it. Whatever base we report has to be one the client
    cannot follow into memory it can reach, which is what the kernel's
    UMIP emulation and the KVM path both do."""

    self.mkfile("testit.bat", BATCHFILE % 'dtrbase', newline="\r\n")

    self.mkexe_with_djgpp("dtrbase", r"""
#include <stdio.h>

/* what we answer with has to be out of the client's world altogether,
 * so require it in the top two gigabytes. Anything lower is somewhere
 * a client could plausibly have memory: dosemu hands DPMI memory out
 * from $_dpmi_base, which the test configuration puts at 128M, and a
 * client may map more above that. */
#define UNREACHABLE_START 0x80000000u

static int check(const char *nm, unsigned char *p)
{
  unsigned limit = p[0] | (p[1] << 8);
  unsigned base = p[2] | (p[3] << 8) | (p[4] << 16) | ((unsigned)p[5] << 24);
  int bad = 0;

  printf("%s limit=%04x base=%08x\n", nm, limit, base);
  if (base < UNREACHABLE_START) {
    printf("FAIL: %s base is memory the client can reach\n", nm);
    bad = 1;
  }
  /* a limit of 0xffff is 8192 entries, which no machine has, and
   * sixteen bit code working out (limit + 1) / 8 gets zero from it */
  if (limit == 0xffff || limit == 0) {
    printf("FAIL: %s limit %#x is not a size a table can have\n", nm, limit);
    bad = 1;
  }
  return bad;
}

int main(void)
{
  unsigned char g[8], i[8];
  int bad = 0;

  __asm__ volatile("sgdt %0" : "=m" (g));
  __asm__ volatile("sidt %0" : "=m" (i));
  bad |= check("gdt", g);
  bad |= check("idt", i);
  if (!bad)
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    for vm in DPMI_VMS:
        if vm == "kvm" and not self.have_kvm:
            continue
        conf = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "%s"\n' % vm
        results = self.runDosemu("testit.bat", config=conf, timeout=20)

        self.assertNotIn("FAIL:", results, vm)
        self.assertIn("Test OK", results, vm)
