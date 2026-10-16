BATCHFILE = """\
c:\\%s
rem end
"""


def memory_dpmi_hwint_balance(self):
    """A client that returns twice from one hardware interrupt.

    dosemu pushes the interrupted context on its locked stack and pops it
    back when the client irets to the return address it put in the frame.
    Nothing tied that pop to a delivery: the frame stays where it is, so a
    client that saves SS:SP at the top of its handler and irets off that
    same frame a second time gets put back at the same address again and
    again, forever, while the delivery count runs away into the negative.
    The 286|DOS-Extender games do exactly this, through a timer dispatcher
    that restores SS:SP from a global. Expect one complaint and a stop."""

    self.mkfile("testit.bat", BATCHFILE % 'hwintbal', newline="\r\n")

    self.mkexe_with_djgpp("hwintbal", r"""
#include <stdio.h>
#include <stdint.h>
#include <dpmi.h>
#include <go32.h>

/* written by the handler below, so they cannot be static: the asm
 * refers to them by name */
volatile unsigned short saved_ss;
volatile unsigned int saved_esp;
volatile int ticked;

/* A raw handler, because it has to record SS:ESP before anything is
 * pushed: that is where dosemu's frame for this delivery starts. */
extern void tmr_handler(void);
asm(".text\n"
    ".globl _tmr_handler\n"
    "_tmr_handler:\n"
    "\tmovw %ss, _saved_ss\n"
    "\tmovl %esp, _saved_esp\n"
    "\tmovl $1, _ticked\n"
    "\tiret\n");

int main(void)
{
  __dpmi_paddr old_vec, new_vec;
  unsigned long spins = 0;

  __dpmi_get_protected_mode_interrupt_vector(8, &old_vec);
  new_vec.selector = _go32_my_cs();
  new_vec.offset32 = (uintptr_t)&tmr_handler;
  if (__dpmi_set_protected_mode_interrupt_vector(8, &new_vec) == -1) {
    printf("FAIL: cannot hook the timer\n");
    return 1;
  }

  while (!ticked && spins++ < 200000000UL)
    ;
  __dpmi_set_protected_mode_interrupt_vector(8, &old_vec);
  if (!ticked) {
    printf("FAIL: no timer interrupt arrived\n");
    return 1;
  }

  /* The frame this points at has already been returned from once. */
  __asm__ volatile(
    "cli\n\t"
    "movw %0,%%ss\n\t"
    "movl %1,%%esp\n\t"
    "iret\n"
    :: "m" (saved_ss), "m" (saved_esp));

  printf("FAIL: returned from a hardware interrupt twice\n");
  fflush(stdout);
  return 1;
}
""")

    results = self.runDosemu("testit.bat", timeout=30)

    self.assertNotIn("FAIL:", results)
    self.assertIn("return from hardware interrupt that was not delivered",
                  self.boot_log())
