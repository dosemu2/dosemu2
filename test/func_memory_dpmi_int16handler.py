BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

EMU_CONF = DOSEMU_CONF_DEFAULT + '$_cpu_vm_dpmi = "emulated"\n'


def memory_dpmi_int16handler(self):
    """A 32bit client may install an interrupt handler in a 16bit code
    segment; a DOS extender running a 16bit program does it for every
    vector the program hooks. Such a handler returns with a 16bit iret and
    needs a 6 byte frame, not the 12 byte one the client's own bitness
    would call for."""

    self.mkfile("testit.bat", BATCHFILE % 'int16h', newline="\r\n")

    self.mkexe_with_djgpp("int16h", r"""
#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <dpmi.h>
#include <go32.h>
#include <crt0.h>

#define VEC 0x64

/* mov ax,0x1234 ; iret - sixteen bit, six bytes of frame */
static unsigned char handler[] = { 0xb8, 0x34, 0x12, 0xcf };

static jmp_buf jb;

static void sigh(int sig)
{
  longjmp(jb, 1);
}

int main(void)
{
  __dpmi_paddr old, new;
  unsigned short ax = 0;
  int sel;

  sel = __dpmi_allocate_ldt_descriptors(1);
  if (sel == -1) {
    printf("FAIL: no descriptor\n");
    return 1;
  }
  if (__dpmi_set_segment_base_address(sel,
        __djgpp_base_address + (unsigned long)handler) == -1 ||
      __dpmi_set_segment_limit(sel, 0xffff) == -1 ||
      /* present, DPL 3, code, readable, 16bit */
      __dpmi_set_descriptor_access_rights(sel, 0x00fa) == -1) {
    printf("FAIL: cannot describe the handler\n");
    return 1;
  }
  if (__dpmi_get_protected_mode_interrupt_vector(VEC, &old) == -1) {
    printf("FAIL: cannot read vector %#x\n", VEC);
    return 1;
  }
  new.selector = sel;
  new.offset32 = 0;
  if (__dpmi_set_protected_mode_interrupt_vector(VEC, &new) == -1) {
    printf("FAIL: cannot hook vector %#x\n", VEC);
    return 1;
  }

  signal(SIGSEGV, sigh);
  signal(SIGILL, sigh);
  if (setjmp(jb) == 0) {
    __asm__ volatile("int $0x64" : "=a" (ax) : "a" (0));
    printf("handler returned ax=%#x\n", ax);
    if (ax != 0x1234)
      printf("FAIL: handler did not run\n");
    else
      printf("Test OK\n");
  } else {
    printf("FAIL: faulted on the way back from a 16bit handler\n");
  }
  __dpmi_set_protected_mode_interrupt_vector(VEC, &old);
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=EMU_CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)
