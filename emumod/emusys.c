/* file: emusys.c
 *
 * special dosemu systemcalls
 *
 * (C) 1994 under GPL, Hans Lermen <lermen@elserv.ffm.fgan.de>
 */

#include "kversion.h"
#if 0
#define KERNEL_VERSION 1002001 /* last verified kernel version */
#endif

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#if KERNEL_VERSION >= 1001085
#include <linux/mm.h>
#endif


#include <asm/segment.h>
#if KERNEL_VERSION >= 1001088
#include <asm/pgtable.h>
#endif
#include <asm/io.h>
#include <asm/system.h>
#include <asm/irq.h>
#include <linux/head.h>

#include "syscallmgr.h"
#include "emusys.h"


#define ID_STRING "emusys"

extern int printk( const char* fmt, ...);


/*>>>>>>>>>>>>>>>>>>>>>>>>>>> irq handling >>>>>>>>>>>>>>>>>>>>>>*/

static struct task_struct *tasks[16]={0};
static int irqbits=0;


#if KERNEL_VERSION < 1001089
static void irq_handler(int intno) {
#else
static void irq_handler(int intno, struct pt_regs * unused) {
#endif
  int int_bit;
  cli();
  intno= (-(((struct pt_regs *)intno)->orig_eax+2) & 0xf);
  int_bit = 1 << intno;
  if (!(irqbits & int_bit)) {
    irqbits |= int_bit;
    if (tasks[intno]) send_sig(SIGIO,tasks[intno], 1);
  }
  sti();
  return;
}



static int do_irq_job(int mode, int irqnumber)
{
  if ( (irqnumber<3) || (irqnumber>15) ) return -1;
  switch (mode) {
    case EMUSYS_REQUEST_IRQ: {
      if (tasks[irqnumber]) return -1;
      irqbits &= ~(1 << irqnumber);
      if (request_irq(irqnumber, &irq_handler, 0, ID_STRING )) return -1;
      tasks[irqnumber] = current;
      return irqnumber;
    }
    case EMUSYS_FREE_IRQ: {
      if (!tasks[irqnumber]) return -1;
      free_irq(irqnumber);
      tasks[irqnumber] = 0;
      irqbits &= ~(1 << irqnumber);
      return irqnumber;
    }    
  }
  return -1;
} 


static void remove_all_irqs()
{
  int i;
  for (i=3; i<16; i++) if (tasks[i]) {
   free_irq(i);
   tasks[i]=0;
  }
  irqbits = 0;
}

/*<<<<<<<<<<<<<<<<<<<<<<<<<< irq handling <<<<<<<<<<<<<<<<<<<<<<*/


/*>>>>>>>>>>>>>>>>>>>>>>>>>> fast syscall >>>>>>>>>>>>>>>>>>>>>>*/

/*
 * Note: 
 *   This syscalls are realized by an INT 0xe6 to Linux (not to DOS),
 *   which is a non Linux standard software interrupt.
 *   We do this, to avoid
 *     1.  System overhead
 *     2.  Taskswitches
 *     3.  Interrupts
 *   So, a call to fast_syscall can't use any kernel routines
 *   that require or enable the above. (e.g. NO printk e.t.c. !)
 *
 *   While in do_fast_syscall we must 
 *     1.  NEVER enable interrupts !
 *     2.  return as fast as possible.
 *     3.  use the special kernel macros for accessing user space ( *fs*() )
 *     4.  not alter the fast_syscall_regs, as long as we know what do,
 *         because this are the true register values of the user process.
 *     5.  return a function value in fast_syscall_regs.eax
 *     6.  avoid anything that can lead to a "security hole"
 *         (e.g. check current->uid for root)
 *     7.  NOT run dosemu under GDB, unless you also set
 *         #define HANDLE_DEBUG_REGISTERS 1 (in fastsys.S),
 *         see the comment there.
 */


void asmlinkage do_fast_syscall(struct fast_syscall_regs *regs)
{
  if (current->uid) {
    /* some body other then root is calling us ! */
    regs->eax = -EACCES;
    return;
  }
  switch ((regs->eax) & FASTSYS_MODE_MASK) {
    case FASTSYS_GET_IRQFLAGS: {
      regs->eax = irqbits;
      return;
    }
    case FASTSYS_CLEAR_IRQ_BIT: {
      register int bit = 1 << regs->ebx;
      regs->eax = irqbits & bit;
      irqbits &= ~bit;
      return;
    }
    case FASTSYS_SET_IRQ_BIT: {
      regs->eax = irqbits |= 1 << regs->ebx;
      return;
    }
  }
  regs->eax = -1;
  return;
}



static struct desc_struct saved_fast_int = {0};


static void init_fast_syscall()
{
  int flags;
  extern void fast_syscall(void);

  save_flags(flags);    
  cli();
  if (!saved_fast_int.a) {
    saved_fast_int = idt[FASTSYS_INT];
    set_system_gate(FASTSYS_INT,&fast_syscall);
  }
  restore_flags(flags);  
}

static void remove_fast_syscall()
{
  int flags;

  save_flags(flags);    
  cli();
  if (saved_fast_int.a) {
    idt[FASTSYS_INT] = saved_fast_int;
  }
  restore_flags(flags);
}


/*<<<<<<<<<<<<<<<<<<<<<<<<<< fast syscall <<<<<<<<<<<<<<<<<<<<<<*/




static int support_flags= EMUSYS_HAS_SILLYINT;

static asmlinkage int emusys(int mode,void *params)
{
  switch (mode) {
    case EMUSYS_GETVERSION:  return EMUSYSVERSION;
    case EMUSYS_GETSUPPORT:  return support_flags;
    case EMUSYS_REQUEST_IRQ: 
    case EMUSYS_FREE_IRQ: {
      if (!(support_flags & EMUSYS_HAS_SILLYINT)) return -1;
      return do_irq_job(mode, (int)params);
    }
  }
  return -1;
}


static int __NR_emusys=-1;

int init_emusyscalls( void) {
  __NR_emusys=register_syscall(0, emusys,"emusys");
  if (__NR_emusys>0) {
    printk(ID_STRING ", syscall installed, NR_emusys=%d\n",__NR_emusys);
    init_fast_syscall();
    return 0;
  }
  else {
    printk(ID_STRING ", can't register syscall, init failed\n");
    return -1;
  }
}


void remove_emusyscalls( void) {
  unregister_syscall(__NR_emusys);
  remove_fast_syscall();
  remove_all_irqs();
  printk(ID_STRING ": syscall unregistered\n");
}
