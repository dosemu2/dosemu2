import re

BATCHFILE = """\
c:\\%s
rem end
"""

from common_framework import DOSEMU_CONF_DEFAULT

CONF = DOSEMU_CONF_DEFAULT + '$_debug = "+M"\n'

SET_CB = r"""
static void set_cb(unsigned short sel, unsigned long off)
{
  __asm__ volatile(
    "pushl %%es\n\t"
    "movw %w[sel],%%es\n\t"
    "movl %[off],%%edx\n\t"
    "movl $0x0000000c,%%eax\n\t"
    "movl $0x0000ffff,%%ecx\n\t"
    "int $0x33\n\t"
    "popl %%es\n\t"
    : : [sel] "r" (sel), [off] "r" (off)
    : "eax", "ecx", "edx", "memory");
}
"""


def mouse_dpmi_callback_swap(self):
    """int 33h AX=0014h has to hand a protected mode client back the
    handler it had installed before, not the one it is installing now."""

    self.mkfile("testit.bat", BATCHFILE % 'mscb', newline="\r\n")

    self.mkexe_with_djgpp("mscb", r"""
#include <stdio.h>

static void handler1(void) { }
static void handler2(void) { }
""" + SET_CB + r"""
/* too many operands for one asm statement, so pass them in memory */
static unsigned short g_sel, g_oes;
static unsigned long g_off, g_odx;

static void swap_cb(void)
{
  __asm__ volatile(
    "pushl %%es\n\t"
    "movw %[sel],%%es\n\t"
    "movl %[off],%%edx\n\t"
    "movl $0x00000014,%%eax\n\t"
    "movl $0x0000ffff,%%ecx\n\t"
    "int $0x33\n\t"
    "movw %%es,%[oes]\n\t"
    "movl %%edx,%[odx]\n\t"
    "popl %%es\n\t"
    : [oes] "=m" (g_oes), [odx] "=m" (g_odx)
    : [sel] "m" (g_sel), [off] "m" (g_off)
    : "eax", "ecx", "edx", "memory");
}

int main(void)
{
  unsigned short cs;

  __asm__ volatile("movw %%cs,%w0" : "=r" (cs));
  printf("cs=%#x h1=%#lx h2=%#lx\n", cs,
         (unsigned long)handler1, (unsigned long)handler2);

  set_cb(cs, (unsigned long)handler1);
  g_sel = cs;
  g_off = (unsigned long)handler2;
  swap_cb();
  printf("swapped_out=%#x:%#lx\n", g_oes, g_odx);

  if (g_oes == cs && g_odx == (unsigned long)handler1)
    printf("Test OK\n");
  else
    printf("FAIL: expected %#x:%#lx\n", cs, (unsigned long)handler1);
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=CONF, timeout=20)

    self.assertNotIn("FAIL:", results)
    self.assertIn("Test OK", results)


def mouse_dpmi_callback_nested(self):
    """Two DPMI clients must not share one real mode callback. The
    callback is what the mouse driver is given, and the event is
    delivered to the client that allocated it, so a shared one sends
    the child's mouse events to the parent."""

    self.mkfile("testit.bat", BATCHFILE % 'mscbp', newline="\r\n")

    self.mkexe_with_djgpp("mscbc", r"""
#include <stdio.h>

static void chandler(void) { }
""" + SET_CB + r"""
int main(void)
{
  unsigned short cs;

  __asm__ volatile("movw %%cs,%w0" : "=r" (cs));
  set_cb(cs, (unsigned long)chandler);
  printf("child done\n");
  fflush(stdout);
  return 0;
}
""")

    self.mkexe_with_djgpp("mscbp", r"""
#include <stdio.h>
#include <stdlib.h>

static void phandler(void) { }
""" + SET_CB + r"""
int main(void)
{
  unsigned short cs;

  __asm__ volatile("movw %%cs,%w0" : "=r" (cs));
  set_cb(cs, (unsigned long)phandler);
  fflush(stdout);
  system("c:\\mscbc.exe");
  printf("parent done\n");
  fflush(stdout);
  return 0;
}
""")

    results = self.runDosemu("testit.bat", config=CONF, timeout=30)

    self.assertIn("child done", results)
    self.assertIn("parent done", results)

    rmcbs = re.findall(r"set mouse callback \S+ of client (\d+) to (\S+)",
                       self.boot_log())
    self.assertEqual(len(rmcbs), 2,
                     "expected one mouse callback from each of two clients")
    self.assertNotEqual(rmcbs[0][0], rmcbs[1][0],
                        "the two callbacks came from the same client")
    self.assertNotEqual(rmcbs[0][1], rmcbs[1][1],
                        "both clients were given the same real mode callback")
