/* (C) 1994 James Maclean
 *
 * DANG_BEGIN_MODULE
 * 
 * Silly Interrupt Generator device driver for Linux 1.1.47 or higher.
 * Needs the new modules utilities.
 *
 * The driver uses MAJOR 19, the minors can be from 3 .. 15 and
 * represent the IRQ-level, which is intercepted for use with DOSEMU.
 * This driver must be compiled on the system it is running on, see the
 * doc in the modules packages.
 * To load it (must be the superuser) type
 *   insmod sillyint.o
 * or
 *   insmod sillyint.o SIG_MAJOR=mm
 * if you want to use MAJOR mm intstead of the default 19.
 *
 * To make the devices go into your /dev directory and create a subdirectory
 * called int. Change into this directory and add a node for the IRQ you wish
 * to use. Bare in mind that in the future if you use another interrupt, you'll
 * have to come to /dev/int and add it too.
 * To MaKe this NODe, type:
 *   mknod <x> c 19 <x>
 * where <x> is the IRQ number you wish to use. I will add at this time
 * that SIG will not open an interrupt up that is already in use. This will
 * mean that some applications that access their interrupts from boot up
 * like ethernet cards, must not be configured into the kernel or must be
 * given port bases, that are not probed by the kernel. You may use the
 * kernel command line feature, either from LILO or LOADLIN to change those
 * ports at boot time of Linux.
 * DANG_END_MODULE
 *
 * General rules (how it works):
 *
 *   1. Only one process may open the device at a given time (of course).
 *      Opening an already opened device results in an error.
 *
 *   2. The process which opens should *not* read() from the device,
 *      but use select(15, &readfds, NULL, NULL, ...) to wait for the 
 *      interrupt. Doing so we spare one system call per IRQ.
 *      However, reads do not cause an error, but return the IRQ number
 *      if an IRQ is pending else 0. (does none of handshake, see 3.)
 *      For better performance it can set up a signal handler for SIGIO
 *      but then must also set the FASYNC bit and the owners pid. Like
 *         flags = fcntl(fd, F_GETFL);
 *         fcntl(fd, F_SETOWN,  getpid());
 *         fcntl(fd, F_SETFL, flags | FASYNC);
 *
 *   3. Handshake is done by setting a bit in int_flag within
 *      the interrupt handler and clearing it within the int_select() function.
 *      The interrupt handler returns dummy, if the bit in
 *      int_flag is already set (should not happen under current Linux, but we
 *      want to be shure for the future).
 *
 *
 * DANG_BEGIN_CHANGELOG
 * $Date: 1994/10/03 00:27:30 $
 * $Source: /home/src/dosemu0.60/sig/RCS/int.c,v $
 * $Revision: 1.3 $
 * $State: Exp $
 *
 * $Log: int.c,v $
 * Revision 1.4  1994/10/02  18:45:25  root
 * further enhancement of SIGIO, changed handshaking.  (Hans Lermen)
 *
 * Revision 1.3  1994/10/03  00:27:30  root
 * Checkin prior to pre53_25.tgz
 *
 * Revision 1.2  1994/09/26  23:12:22  root
 * Prep for pre53_22.
 *
 * Revision 0.11  1994/09/19  01:01:23  root
 * Driver converted to loadable module (Hans Lermen)
 *
 * Revision 0.10  1994/09/5  00:49:34  root
 * Original, kernel based driver for pre53_19 (Jmaclean).
 *
 * DANG_END_CHANGELOG
 */

#define SIG_VERSION "1.4"

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/malloc.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/irq.h>

#if 0
  #define WANT_DEBUG
  #define ___static
#else
  #define ___static static
#endif

#ifdef MAKE_LOADABLE_MODULE
#include <linux/module.h>

/*
 * NOTE:
 *   To install the module, we must include the kernel identification string.
 *   (so, don't panic if you get a GCC warning "_kernel_version not used" )
 */
#include "../tools/version.h"
static char kernel_version[] = UTS_RELEASE;

#endif /* MAKE_LOADABLE_MODULE */

extern int printk( const char* fmt, ...);

___static int int_fasync(struct inode *inode, struct file *file, int on);
___static struct task_struct * task_of_pid(int pid);
static struct task_struct *tasks[16]={0};

static struct wait_queue *wait_queue = NULL;

static unsigned int int_flag = 0;

#ifdef MAKE_LOADABLE_MODULE
  #define ID_STRING "sillyint"
#else
  #define ID_STRING "intr"
#endif /* MAKE_LOADABLE_MODULE */

___static void register_interrupt(int intno) {
  int int_bit;
  cli();
  intno= (-(((struct pt_regs *)intno)->orig_eax+2) & 0xf);
  int_bit = 1 << intno;
  if (!(int_flag & int_bit)) {
    int_flag |= int_bit;
    if (tasks[intno]) send_sig(SIGIO,tasks[intno], 1);

    /* if we are fast enough, e.g if int_select never enters select_wait()
       we will have *no* wait queue, so we can skip wake_up
       and gain some micro seconds 
    */
    if (wait_queue) wake_up_interruptible(&wait_queue);

#ifdef WANT_DEBUG
    printk("Caught INT 0x%02x Int_flag=0x%04x\n",intno,int_flag);
#endif
  }
  sti();
  return;
}

___static int int_select(struct inode *inode, struct file *file,
                       int sel_type, select_table *wait)
{
  int int_bit = 1 << MINOR(inode->i_rdev);
  
#ifdef WANT_DEBUG
  printk("Driver entering select.\n");
#endif
  if (sel_type==SEL_IN) /* ready for read? */
  {
#ifdef WANT_DEBUG
    printk("Int_flag = 0x%04x.\n", int_flag);
    printk("Interrupt= 0x%04x.\n", int_bit);
#endif
    if (!(int_bit & int_flag)) /* Interrupt ready? */
    {
      select_wait(&wait_queue, wait);
#ifdef WANT_DEBUG
      printk("Interrupt not ready\n"); 
#endif
      return 0;  /* Not ready yet */
    }
    int_flag &= ~int_bit;
    return 1;  /* Ready */
  }
  return 1;  /* Always ready for writes and exceptions */
}

___static int int_write(struct inode * inode, struct file * file,
  char * buffer, int count)
{
  int int_bit;
  cli();
  int_bit = 1 << MINOR(inode->i_rdev);
  if (!(int_flag & int_bit)) {
    int_flag |= int_bit;
#ifdef WANT_DEBUG
    printk("Manually setting interrupt %02x. Int_flag=0x%04x\n", MINOR(inod->i_rdev), int_flag);
#endif
    sti();
    wake_up_interruptible(&wait_queue);
  }
  return MINOR(inode->i_rdev);
}


___static int int_read(struct inode * inode, struct file * file, 
                     char * buffer, int count)
{
  int int_bit = 1 << MINOR(inode->i_rdev);
  if (int_flag | int_bit) return MINOR(inode->i_rdev);
  else return 0;
}

___static int int_open(struct inode *inode, struct file *file)
{
  if (request_irq(MINOR(inode->i_rdev), &register_interrupt, 0, ID_STRING )) {
#ifdef WANT_DEBUG
    printk("Interrupt Generator open failed. INT0x%02x already in use\n", MINOR(inode->i_rdev));
#endif
    return -1;
  }
#ifdef WANT_DEBUG
  printk("Interrupt Generator opened.\n");
#endif
#ifdef MAKE_LOADABLE_MODULE
  MOD_INC_USE_COUNT;
#endif
  return 0;
}

___static void int_release(struct inode *inode, struct file *file)
{
  int rdev;
  rdev = MINOR(inode->i_rdev);
  free_irq(rdev);
  tasks[rdev]=NULL;
#ifdef WANT_DEBUG
  printk("Interrupt Generator released.\n");
#endif
#ifdef MAKE_LOADABLE_MODULE
  MOD_DEC_USE_COUNT;
#endif
}

___static struct task_struct * task_of_pid(int pid)
{
  struct task_struct *p = current; /* pointing to active task */
  if (pid < 0) return 0;
  while (p->pid != pid) {
    p = p->next_task;
    if (p==current) return 0;
  }
  return p;
}

___static int int_fasync(struct inode * inode, struct file * filp, int on)
{
  int rdev = MINOR(inode->i_rdev);
  /* Under current Linux filp->f_owner *is* the current task.
   * But we want to be sure for the future, 
   * so we search for the task in the task_list.
   */
  if (on) tasks[rdev]=task_of_pid(filp->f_owner);
  else tasks[rdev]=0;
  return 0;
}


static struct file_operations int_fops = {
	NULL,		/* int_seek */
	int_read,	/* int_read */
	int_write,	/* int_write */
	NULL, 		/* int_readdir */
	int_select,	/* int_select */
	NULL, 		/* int_ioctl */
	NULL,		/* int_mmap */
	int_open,	/* int_open */
	int_release,	/* int_release */
	NULL,            /* fsync */
  	int_fasync	/* Asyncronous I/O */
};



#ifndef  MAKE_LOADABLE_MODULE

#define SIG_MAJOR 19

unsigned long int_init(unsigned long kmem_start)
{
  printk("\nSilly Interrupt Generator installed.(ver " SIG_VERSION ")\n\n");
  if (register_chrdev(SIG_MAJOR,ID_STRING,&int_fops))
    printk("Interrupt Generator error: Cannot register to major device %d !\n",SIG_MAJOR);
  return kmem_start;
}

#else /* MAKE_LOADABLE_MODULE */

/*
 * And now the modules code and kernel interface.
 */

int SIG_MAJOR=19;

  
int init_module( void) {
  kernel_version[0] = kernel_version[0];
  printk("Silly Interrupt Generator (ver " SIG_VERSION "):  init_module called\n");
  if (register_chrdev(SIG_MAJOR,ID_STRING,&int_fops)) {
    printk("Interrupt Generator error: Cannot register to major device %d !\n",SIG_MAJOR);
    return -EIO;
  }
  else {
    printk("Silly Interrupt Generator installed, using MAJOR %d\n",SIG_MAJOR);
    return 0;
  }
}

void cleanup_module( void) {
  printk( "Silly Interrupt Generator (ver " SIG_VERSION "):  cleanup_module called\n");
  if (MOD_IN_USE) printk(ID_STRING ": device busy, remove delayed\n");
  if (unregister_chrdev(SIG_MAJOR, ID_STRING) != 0) {
    printk("cleanup_module failed\n");
  } else {
    printk("cleanup_module succeeded\n");
  }    
}

#endif /* MAKE_LOADABLE_MODULE */
