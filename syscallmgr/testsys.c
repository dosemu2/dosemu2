#include "kversion.h"
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/malloc.h>

#include <linux/unistd.h>
#if KERNEL_VERSION < 1001067
#include <linux/segment.h>
#endif
#include <linux/sys.h>
#include <linux/mm.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/string.h>
#include <asm/irq.h>

#if KERNEL_VERSION >= 1003038
  /* need this to force  kernel_version[]=UTS_RELEASE,
   * starting with 1.3.38 defines are moved into <linux/module.h> */
  #undef __NO_VERSION__ 
  #define MODULE
#endif
#include <linux/module.h>
#include "syscallmgr.h"

#if 0
  #define WANT_DEBUG
  #define ___static
#else
  #define ___static static
#endif

#if KERNEL_VERSION < 1003038
/*
 * NOTE:
 *   To install the module, we must include the kernel identification string.
 *   (so, don't panic if you get a GCC warning "_kernel_version not used" )
 */
#if KERNEL_VERSION < 1001072
#include "linux/tools/version.h"
#else
#include "linux/version.h"
#endif
static char kernel_version[] = UTS_RELEASE;
#endif

#define ID_STRING "Testsys"

extern int printk( const char* fmt, ...);
  


static asmlinkage int testsys(int mode,void *params)
{
  switch (mode) {
    case 0: {
      char s[256];
      strn0cpy_fromfs(s,(char *)params,sizeof(s) -1);
      printk(ID_STRING ": >%s<\n",s);
      return 0;
    }
    case 1: {
      extern void fast_IRQ12_interrupt();
      volatile int *p=(int *)&fast_IRQ12_interrupt;
      int saved=*p; /* normaly = 0x501e06fc */
#if 0 /* testing writeability of kernel code space */
      *p=(saved & ~255) | 0x90;
      printk(ID_STRING ": code at fast_IRQ12_interrupt=%08x (patched=%08x)\n",saved,*p);
      *p=saved;
#else
      printk(ID_STRING ": code at fast_IRQ12_interrupt=%08x\n",saved);
#endif
      return 0;
    }
  }
  return -1;
}

static int __NR_testsys=-1;

int init_module( void) {
  kernel_version[0] = kernel_version[0];
  __NR_testsys=register_syscall(0, testsys,"testsys");
  if (__NR_testsys>0) {
    printk(ID_STRING ", init_module called, NR_testsys=%d\n",__NR_testsys);
    return 0;
  }
  else {
    printk(ID_STRING ", init_module failed\n");
    return -1;
  }
}

void cleanup_module( void) {
  if (MOD_IN_USE) printk(ID_STRING ": device busy, remove delayed\n");
  else printk(ID_STRING ": cleanup modul called\n");
  unregister_syscall(__NR_testsys);
}
