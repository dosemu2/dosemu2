import re


def memory_dpmi_reinit_pool(self):
    """DPMI_REINIT reports the private data pool and moves it on request."""

    self.mkfile("testit.bat", """\
c:\\rpool
rem end
""", newline="\r\n")

    self.mkexe_with_djgpp("rpool", r"""
#include <dpmi.h>
#include <dos.h>
#include <go32.h>
#include <sys/farptr.h>
#include <stdio.h>
#include <string.h>

struct ext {
  unsigned short pool;		/* selector of the private data pool */
  unsigned short bx;		/* capability flags */
  unsigned short si;		/* the pool's size in paragraphs */
  unsigned short sel;		/* reinit entry selector */
  unsigned int off;		/* reinit entry offset */
  int err;			/* CF */
};

#define EXT_COOKIE	0xd05e	/* cx: we know about the extension */
#define EXT_GET_POOL	1	/* bx: report the private data pool */

/* int 2fh ax=1687 in protected mode.  Our host answers it with the reinit
 * entry in es:edi.  As an extension, a client that puts the cookie in cx
 * turns bx into a flag word on input; asking for the pool brings back its
 * selector in ax, its size in paragraphs in si, and the status in CF.
 * Without the cookie nothing is read from bx and cx, and ax and si come
 * back zero. */
static void get_ext(struct ext *e, int want_pool)
{
  unsigned short r_ax = 0, r_bx = 0, r_si = 0, r_es = 0;
  unsigned int r_di = 0, r_fl = 0;
  unsigned short i_cx = want_pool ? EXT_COOKIE : 0;
  unsigned short i_bx = want_pool ? EXT_GET_POOL : 0;

  asm volatile(
    "pushl %%ebx\n"
    "pushl %%esi\n"
    "pushl %%es\n"
    "movw %7, %%bx\n"
    "movw %6, %%cx\n"
    "movl $0x1687, %%eax\n"
    "int $0x2f\n"
    "movw %%ax, %0\n"
    "pushfl\n"
    "popl %%eax\n"
    "movl %%eax, %5\n"
    "movw %%bx, %1\n"
    "movw %%si, %2\n"
    "movw %%es, %3\n"
    "movl %%edi, %4\n"
    "popl %%es\n"
    "popl %%esi\n"
    "popl %%ebx\n"
    : "=m"(r_ax), "=m"(r_bx), "=m"(r_si), "=m"(r_es), "=m"(r_di), "=m"(r_fl)
    : "m"(i_cx), "m"(i_bx)
    : "cc", "memory", "eax", "ecx", "edx", "edi");

  e->err = (r_fl & 1) ? -1 : 0;
  e->pool = r_ax;
  e->bx = r_bx;
  e->si = r_si;
  e->sel = r_es;
  e->off = r_di;
}

/* the reinit entry is a far call; ax carries the flags, and with our
 * 0x800 bit bx carries the selector of the block the client has moved the
 * pool to.  it rewrites es, so save it. */
static int call_reinit(struct ext *e, unsigned short ax, unsigned short bx)
{
  struct { unsigned int off; unsigned short sel; } fp;
  unsigned int fl = 0;

  fp.off = e->off;
  fp.sel = e->sel;
  asm volatile(
    "pushl %%ebx\n"
    "pushl %%es\n"
    "movw %2, %%ax\n"
    "movw %3, %%bx\n"
    "lcall *%1\n"
    "pushfl\n"
    "popl %%eax\n"
    "movl %%eax, %0\n"
    "popl %%es\n"
    "popl %%ebx\n"
    : "=m"(fl)
    : "m"(fp), "m"(ax), "m"(bx)
    : "cc", "memory", "eax", "ecx", "edx", "esi", "edi");
  return (fl & 1) ? -1 : 0;
}

int main(void)
{
  struct ext e0, e1, e2;
  __dpmi_regs r;
  struct find_t ff;
  int sel, seg;
  unsigned i;
  unsigned long old_base, new_base;

  /* first without the cookie: an ordinary client must see no pool */
  get_ext(&e0, 0);
  if (e0.err != 0) {
    printf("FAIL: no protected mode 1687\n");
    return 1;
  }
  printf("INFO: caps bx=%04x\n", e0.bx);
  if (e0.pool || e0.si) {
    printf("FAIL: pool reported without the cookie\n");
    return 1;
  }

  if (!(e0.bx & 0x800)) {
    printf("FAIL: host does not report a relocatable pool\n");
    return 1;
  }

  get_ext(&e1, 1);
  if (e1.err != 0) {
    printf("FAIL: extended 1687 refused\n");
    return 1;
  }
  if (!e1.pool || !e1.si) {
    printf("FAIL: implausible pool\n");
    return 1;
  }
  if (__dpmi_get_segment_base_address(e1.pool, &old_base) != 0) {
    printf("FAIL: cannot read the pool's base\n");
    return 1;
  }
  printf("INFO: pool seg=%04x para=%04x\n", (unsigned short)(old_base >> 4),
         e1.si);

  /* a mark we can watch travel */
  _farpokel(_dos_ds, old_base, 0x5a5aa5a5);

  seg = __dpmi_allocate_dos_memory(e1.si, &sel);
  if (seg == -1) {
    printf("FAIL: cannot allocate a new pool\n");
    return 1;
  }
  new_base = (unsigned long)seg * 16;
  printf("INFO: new pool seg=%04x\n", (unsigned short)seg);

  /* the pool is ours, so we move it ourselves: the host only has to be
   * told where it went */
  for (i = 0; i < (unsigned)e1.si * 16; i += 4)
    _farpokel(_dos_ds, new_base + i, _farpeekl(_dos_ds, old_base + i));

  if (call_reinit(&e1, 0x801, (unsigned short)sel) != 0) {
    printf("FAIL: reinit refused the move\n");
    return 1;
  }

  if (_farpeekl(_dos_ds, new_base) != 0x5a5aa5a5) {
    printf("FAIL: pool contents lost across reinit\n");
    return 1;
  }

  get_ext(&e2, 1);
  if (__dpmi_get_segment_base_address(e2.pool, &new_base) != 0) {
    printf("FAIL: cannot read the pool's new base\n");
    return 1;
  }
  printf("INFO: pool now seg=%04x para=%04x\n", (unsigned short)(new_base >> 4),
         e2.si);
  if (new_base != (unsigned long)seg * 16 || e2.si != e1.si) {
    printf("FAIL: pool not reported at its new place\n");
    return 1;
  }

  /* exercise the moved pool: a real mode call runs on its stacks, and
   * findfirst goes through the translation buffer that lives in it */
  memset(&r, 0, sizeof(r));
  r.h.ah = 0x2a;
  __dpmi_int(0x21, &r);
  printf("INFO: date %04d-%02d-%02d\n", r.x.cx, r.h.dh, r.h.dl);
  if (_dos_findfirst("C:\\TESTIT.BAT", _A_NORMAL, &ff) != 0) {
    printf("FAIL: findfirst after the move\n");
    return 1;
  }
  printf("INFO: found %s\n", ff.name);

  printf("DONE:\n");
  return 0;
}
""")

    results = self.runDosemu("testit.bat")

    m = re.search(r'INFO: pool seg=([\da-f]+) para=([\da-f]+)', results)
    self.assertIsNotNone(m, results)
    n = re.search(r'INFO: new pool seg=([\da-f]+)', results)
    self.assertIsNotNone(n, results)
    self.assertNotEqual(m.group(1), n.group(1), results)
    self.assertRegex(results,
                     r'INFO: pool now seg=%s para=%s' % (n.group(1), m.group(2)))
    self.assertNotIn("FAIL:", results)
    self.assertIn("DONE:", results)
