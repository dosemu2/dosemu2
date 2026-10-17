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
    that describes its own LDT, at the index sldt reports. The IDT a
    client gets has to take a write, because a client that puts a gate
    of its own there must not fault on it."""

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

/* the base in the operand of sgdt and sidt: four bytes after the limit */
static unsigned base_of(unsigned char *p)
{
  return p[2] | (p[3] << 8) | (p[4] << 16) | ((unsigned)p[5] << 24);
}

/* the base in a descriptor: three bytes after the limit, one at the end */
static unsigned desc_base_of(unsigned char *p)
{
  return p[2] | (p[3] << 8) | (p[4] << 16) | ((unsigned)p[7] << 24);
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

/* the same the other way round: a client that owns a gate writes it */
static int write_at(unsigned lin, const void *src, unsigned n)
{
  int sel = __dpmi_allocate_ldt_descriptors(1);
  int ok = 0;

  if (sel < 0)
    return 0;
  if (__dpmi_set_segment_base_address(sel, lin) != -1 &&
      __dpmi_set_segment_limit(sel, 0xfff) != -1) {
    unsigned i;
    const unsigned char *d = src;
    for (i = 0; i < n; i++)
      _farpokeb(sel, i, d[i]);
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
  } else if (!write_at(ibase, tbl, 8)) {
    /* a client of its own gates writes here, and the write has to
     * land somewhere rather than fault */
    printf("FAIL: cannot write the idt where sidt says it is\n");
    bad = 1;
  }
  if (!read_at(gbase + (ldt_sel & 0xfff8), tbl, 8)) {
    printf("FAIL: cannot read the gdt where sgdt says it is\n");
    bad = 1;
  } else {
    dbase = desc_base_of(tbl);
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
    /* and the table it describes is the live one: a descriptor set
     * through dpmi right now has to read back from it */
    if (!bad) {
      int sel = __dpmi_allocate_ldt_descriptors(1);
      if (sel < 0 || __dpmi_set_segment_base_address(sel, 0x12340000) == -1) {
        printf("FAIL: cannot set up a descriptor of our own\n");
        bad = 1;
      } else if (!read_at(dbase + (sel & 0xfff8), tbl, 8)) {
        printf("FAIL: cannot read the ldt the gdt entry describes\n");
        bad = 1;
      } else {
        printf("our entry %04x base=%08x\n", sel, desc_base_of(tbl));
        if (desc_base_of(tbl) != 0x12340000) {
          printf("FAIL: the ldt does not carry what dpmi just set\n");
          bad = 1;
        }
      }
      if (sel >= 0)
        __dpmi_free_ldt_descriptor(sel);
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
