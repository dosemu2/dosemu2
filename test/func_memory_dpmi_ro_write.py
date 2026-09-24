BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT


def memory_dpmi_ro_write(self, cpu):
    """A write through a data selector without the writable bit, or through
    a code selector, is a #GP. Reads, compares and a rep with a zero count
    through the same selector are not. The emulator used to let such writes
    through, so a client that relies on read-only aliases to catch writes
    (a 286 extender's program patching its LDT through one) saw none."""

    if cpu == 'kvm' and not self.have_kvm:
        self.skipTest("requires KVM")

    self.mkfile("testit.bat", BATCHFILE % 'rowrite', newline="\r\n")

    self.mkexe_with_djgpp("rowrite", r"""
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <dpmi.h>
#include <sys/exceptn.h>
#include <sys/segments.h>

static jmp_buf jb;
static volatile int exc;
unsigned data[4] = { 0x12345678, 0x9abcdef0, 0x0f0f0f0f, 0xf0f0f0f0 };
static unsigned src[4];
static int fails;

static void handler(int sig)
{
  exc = __djgpp_exception_state->__signum;
  longjmp(jb, 1);
}

/* es and fs are the read only alias for the test, and back to ds after */
#define T(name, should_fault, insn, ...) do { \
  exc = -1; \
  if (setjmp(jb) == 0) { \
    __asm__ volatile( \
      "pushal\n\t" \
      "pushl %%es\n\t" \
      "pushl %%fs\n\t" \
      "movw %w0,%%es\n\t" \
      "movw %w0,%%fs\n\t" \
      insn "\n\t" \
      "popl %%fs\n\t" \
      "popl %%es\n\t" \
      "popal\n\t" \
      : : "r" (ro), ##__VA_ARGS__ : "memory", "cc"); \
  } else { \
    unsigned short d = _my_ds(); \
    __asm__ volatile("movw %0,%%es\n\tmovw %0,%%fs" : : "r" (d)); \
  } \
  if (should_fault && exc != 13) { \
    printf("FAIL: %s: expected #GP, got %d\n", name, exc); \
    fails++; \
  } else if (!should_fault && exc != -1) { \
    printf("FAIL: %s: expected no fault, got %d\n", name, exc); \
    fails++; \
  } \
} while (0)

int main(void)
{
  int ro;
  unsigned ar;
  unsigned v;

  signal(SIGSEGV, handler);
  ro = __dpmi_create_alias_descriptor(_my_ds());
  ar = __dpmi_get_descriptor_access_rights(_my_ds());
  if (ro == -1 || __dpmi_set_descriptor_access_rights(ro, ar & ~2) == -1) {
    printf("FAIL: cannot make a read only alias\n");
    return 1;
  }

  /* these write */
  T("mov r/m,reg", 1, "movl %%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("mov r/m,imm", 1, "movb $1,%%fs:(%1)", "r" (data));
  T("mov moffs,eax", 1, "movl %%eax,%%fs:_data", "a" (1));
  T("add r/m,reg", 1, "addl %%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("add r/m,imm8", 1, "addl $1,%%fs:(%1)", "r" (data));
  T("sub r/m,imm32", 1, "subl $0x12345,%%fs:(%1)", "r" (data));
  T("inc r/m", 1, "incl %%fs:(%1)", "r" (data));
  T("dec r/m8", 1, "decb %%fs:(%1)", "r" (data));
  T("not r/m", 1, "notl %%fs:(%1)", "r" (data));
  T("neg r/m8", 1, "negb %%fs:(%1)", "r" (data));
  T("xchg r/m,reg", 1, "xchgl %%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("shl r/m,imm", 1, "shll $1,%%fs:(%1)", "r" (data));
  T("rcr r/m,1", 1, "rcrb $1,%%fs:(%1)", "r" (data));
  T("setcc r/m", 1, "sete %%fs:(%1)", "r" (data));
  T("bts r/m,imm", 1, "btsl $0,%%fs:(%1)", "r" (data));
  T("btr r/m,reg", 1, "btrl %%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("shld r/m", 1, "shldl $1,%%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("xadd r/m", 1, "xaddl %%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("cmpxchg r/m", 1, "cmpxchgl %%ecx,%%fs:(%1)", "r" (data), "a" (0x12345678), "c" (1));
  T("pop r/m", 1, "pushl $1\n\tpopl %%fs:(%1)", "r" (data));
  T("fstp r/m", 1, "fld1\n\tfstps %%fs:(%1)", "r" (data));
  T("fnstsw r/m", 1, "fnstsw %%fs:(%1)", "r" (data));
  T("stos", 1, "stosl", "D" (data), "a" (1));
  T("rep stos", 1, "cld\n\trep stosl", "D" (data), "a" (1), "c" (4));
  T("movs", 1, "movsl", "D" (data), "S" (src));
  T("rep movsb", 1, "cld\n\trep movsb", "D" (data), "S" (src), "c" (16));
  T("write through cs", 1, "movl %%eax,%%cs:_data", "a" (1));

  /* these do not */
  T("mov reg,r/m", 0, "movl %%fs:(%1),%%eax", "r" (data));
  T("cmp r/m,reg", 0, "cmpl %%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("cmp r/m,imm8", 0, "cmpl $1,%%fs:(%1)", "r" (data));
  T("test r/m,reg", 0, "testl %%eax,%%fs:(%1)", "r" (data), "a" (1));
  T("bt r/m,imm", 0, "btl $0,%%fs:(%1)", "r" (data));
  T("fld r/m", 0, "flds %%fs:(%1)\n\tfstp %%st(0)", "r" (data));
  T("lods", 0, "lodsl %%fs:(%%esi),%%eax", "S" (data));
  T("rep stos, zero count", 0, "cld\n\trep stosl", "D" (data), "a" (1), "c" (0));
  T("read through cs", 0, "movl %%cs:_data,%%eax");

  v = data[0];
  if (v != 0x12345678 || data[1] != 0x9abcdef0) {
    printf("FAIL: memory changed through the read only alias: %08x %08x\n",
        v, data[1]);
    fails++;
  }
  if (!fails)
    printf("Test OK\n");
  fflush(stdout);
  return 0;
}
""")

    config = DOSEMU_CONF_DEFAULT
    if cpu == 'kvm':
        config += '$_cpu_vm_dpmi = "kvm"\n'
    else:
        config += '$_cpu_vm_dpmi = "emulated"\n'
        config += '$_cpuemu = (%i)\n' % (1 if cpu == 'sim' else 0)
    results = self.runDosemu("testit.bat", config=config, timeout=30)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
