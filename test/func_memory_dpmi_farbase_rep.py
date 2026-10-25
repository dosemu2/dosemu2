BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_farbase_rep(self):
    """The same descriptor based outside our memory, reached by a string
    op. Once a rep has faulted on a code page the jit stops running it
    natively and calls a helper instead, and that helper used to be a way
    around the check: it runs on a stack frame of its own, so the fault
    found no way back into the client and killed dosemu. A 286|DOS-Extender
    client gets here reading the IDT with rep movsw."""

    self.mkfile("testit.bat", BATCHFILE % 'farbrep', newline="\r\n")

    self.mkexe_with_djgpp("farbrep", r"""
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <dpmi.h>
#include <go32.h>

static jmp_buf jb;

static void handler(int sig)
{
  longjmp(jb, 1);
}

/* one single rep movsb, used twice: the jit patches the site the first
 * time and calls its helper from then on */
static void rep_copy(unsigned short sel, const void *src, void *dst,
                     unsigned n)
{
  __asm__ volatile(
    "pushl %%ds\n\t"
    "movw %w0,%%ds\n\t"
    "cld\n\t"
    "rep movsb\n\t"
    "popl %%ds\n\t"
    : "+r" (sel), "+S" (src), "+D" (dst), "+c" (n) : : "memory");
}

static volatile int sink;

static void victim(void)
{
  sink++;
}

int main(void)
{
  char saved[16];
  char scratch[16];
  int sel;

  /* make sure the jit compiled victim() and write-protected its page,
   * then write the very same bytes back over it with the rep above:
   * that fault is what makes the jit patch the rep into a helper call */
  victim();
  memcpy(saved, (void *)victim, sizeof(saved));
  rep_copy(_my_ds(), saved, (void *)victim, sizeof(saved));
  victim();

  sel = __dpmi_allocate_ldt_descriptors(1);
  if (sel < 0) {
    printf("FAIL: no descriptor\n");
    return 1;
  }
  /* far above anything dosemu has, but still a 32bit linear address */
  if (__dpmi_set_segment_base_address(sel, 0xf0000000) == -1) {
    printf("FAIL: base rejected\n");
    return 1;
  }
  if (__dpmi_set_segment_limit(sel, 0xfff) == -1) {
    printf("FAIL: limit rejected\n");
    return 1;
  }

  signal(SIGSEGV, handler);
  if (setjmp(jb) == 0) {
    rep_copy((unsigned short)sel, (const void *)0x10, scratch,
             sizeof(scratch));
    printf("FAIL: copied %#x through a base outside our memory\n",
           *(unsigned *)scratch);
  } else {
    printf("Test OK\n");
  }
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
