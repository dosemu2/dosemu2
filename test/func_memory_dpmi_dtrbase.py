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
    and died on it. So the base has to name a table that is there, and
    the GDT's has to hold the one entry the client came for: the one
    that describes its own LDT, at the index sldt reports."""

    self.mkfile("testit.bat", BATCHFILE % 'dtrbase', newline="\r\n")

    self.mkexe_with_djgpp("dtrbase", r"""
#include <stdio.h>
#include <string.h>
#include <dpmi.h>
#include <sys/farptr.h>

static unsigned char tbl[16];

static unsigned lim_of(unsigned char *p)
{
  return p[0] | (p[1] << 8);
}

static unsigned base_of(unsigned char *p)
{
  return p[2] | (p[3] << 8) | (p[4] << 16) | ((unsigned)p[5] << 24);
}

/* read through a descriptor of our own: the table is not in the flat
 * space djgpp hands us, it is wherever the host put it */
static int read_at(unsigned lin, void *dst, unsigned n)
{
  int sel = __dpmi_allocate_ldt_descriptors(1);
  int ok = 0;

  if (sel < 0)
    return 0;
  if (__dpmi_set_segment_base_address(sel, lin) != -1 &&
      __dpmi_set_segment_limit(sel, 0xfff) != -1) {
    unsigned i;
    unsigned char *d = dst;
    for (i = 0; i < n; i++)
      d[i] = _farpeekb(sel, i);
    ok = 1;
  }
  __dpmi_free_ldt_descriptor(sel);
  return ok;
}

int main(void)
{
  unsigned char g[8], i[8];
  unsigned short ldt_sel;
  unsigned gbase, glim, ibase, ilim, dbase, dlim;
  int bad = 0;

  __asm__ volatile("sgdt %0" : "=m" (g));
  __asm__ volatile("sidt %0" : "=m" (i));
  __asm__ volatile("sldt %0" : "=m" (ldt_sel));
  gbase = base_of(g); glim = lim_of(g);
  ibase = base_of(i); ilim = lim_of(i);
  printf("gdt limit=%04x base=%08x\n", glim, gbase);
  printf("idt limit=%04x base=%08x\n", ilim, ibase);
  printf("ldt selector=%04x\n", ldt_sel);

  /* a limit of 0xffff is 8192 entries, which no machine has, and
   * sixteen bit code working out (limit + 1) / 8 gets zero from it */
  if (glim == 0xffff || glim == 0 || ilim == 0xffff || ilim == 0) {
    printf("FAIL: limit is not a size a table can have\n");
    bad = 1;
  }
  if (!read_at(ibase, tbl, 8)) {
    printf("FAIL: cannot read the idt where sidt says it is\n");
    bad = 1;
  }
  if (!read_at(gbase + (ldt_sel & 0xfff8), tbl, 8)) {
    printf("FAIL: cannot read the gdt where sgdt says it is\n");
    bad = 1;
  } else {
    dbase = base_of(tbl);
    dlim = lim_of(tbl) | ((tbl[6] & 0x0f) << 16);
    printf("ldt entry base=%08x limit=%05x type=%02x\n", dbase, dlim, tbl[5]);
    if ((tbl[5] & 0x1f) != 0x02) {
      printf("FAIL: the entry sldt points at is no ldt\n");
      bad = 1;
    }
    if (!dbase || !dlim) {
      printf("FAIL: the ldt entry describes nothing\n");
      bad = 1;
    }
  }
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
