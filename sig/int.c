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
 * DANG_BEGIN_CHANGELOG
 * $Date: 1994/09/26 23:12:22 $
 * $Source: /home/src/dosemu0.60/sig/RCS/int.c,v $
 * $Revision: 1.2 $
 * $State: Exp $
 *
 * $Log: int.c,v $
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

#ifdef MAKE_LOADABLE_MODULE
#include <linux/module.h>

#if 0
#define WANT_SIGIO
#endif

/*
 * NOTE:
 *   To install the module, we must include the kernel identification string.
 *   (so, don't panic if you get a GCC warning "_kernel_version not used" )
 */
#include "linux/tools/version.h"
static char kernel_version[] = UTS_RELEASE;

#endif /* MAKE_LOADABLE_MODULE */

extern int printk( const char* fmt, ...);

#ifdef WANT_SIGIO
static int int_fasync(struct inode *inode, struct file *file, int on);
static struct tty_struct * int_tty[16];
#endif

static int ints_in_use = 0;
static struct wait_queue *wait_queue = NULL;

static unsigned int int_flag = 0;

#ifdef MAKE_LOADABLE_MODULE
  #define ID_STRING "sillyint"
#else
  #define ID_STRING "intr"
#endif /* MAKE_LOADABLE_MODULE */


static void register_interrupt(int intno) {
  cli();
  intno= -(((struct pt_regs *)intno)->orig_eax+2);
  int_flag |= (1 << ((intno & 0xff)-1));
#ifdef WANT_SIGIO
  printk("Owner=%d\n", int_tty[intno]->fasync->fa_file->f_owner);
  if (int_tty[intno]->fasync)
    send_sig(SIGIO,(int)int_tty[intno]->fasync->fa_file->f_owner, 1);
#endif
  wake_up_interruptible(&wait_queue);
#ifdef WANT_DEBUG
  printk("Caught INT 0x%02x\n",intno);
  printk("One of %d processes could be affected. Int_flag=0x%04x\n",ints_in_use, int_flag);
#endif
  sti();
  return;
}

static int int_select(struct inode *inode, struct file *file,
                       int sel_type, select_table *wait)
{

#ifdef WANT_DEBUG
  printk("Driver entering select.\n");
#endif
  if (sel_type==SEL_IN) /* ready for read? */
  {
#ifdef WANT_DEBUG
    printk("Int_flag = 0x%04x.\n", int_flag);
    printk("Interrupt= 0x%04x.\n", (1 << (MINOR(inode->i_rdev)-1)));
#endif
    if (!((1 << (MINOR(inode->i_rdev)-1)) & int_flag)) /* Interrupt ready? */
    {
      select_wait(&wait_queue, wait);
#ifdef WANT_DEBUG
      printk("Interrupt not ready\n"); 
#endif
      return 0;  /* Not ready yet */
    }
    return 1;  /* Ready */
  }
  return 1;  /* Always ready for writes and exceptions */
}

static int int_write(struct inode * inode, struct file * file,
  char * buffer, int count)
{
  cli();
  int_flag = (int_flag | (1 << (MINOR(inode->i_rdev)-1) ));
#ifdef WANT_DEBUG
  printk("Manually setting interrupt %02x. Int_flag=0x%04x\n", MINOR(inode->i_rdev), int_flag);
#endif
  sti();
  wake_up_interruptible(&wait_queue);
  return MINOR(inode->i_rdev);
}

static int int_read(struct inode * inode, struct file * file, 
                     char * buffer, int count)
{

#ifdef WANT_DEBUG
  printk("Interrupt Generator, Looking for interrupt 0x%02x via flag %04x\n", MINOR(inode->i_rdev), int_flag);
#endif
  while ( !( (1 << (MINOR(inode->i_rdev)-1) ) & int_flag) ){
#ifdef WANT_DEBUG
    printk("Needs match in %04x\n", (1 << (MINOR(inode->i_rdev)-1)));
#endif
    interruptible_sleep_on(&wait_queue);
    if (current->signal & ~current->blocked) /* signalled? */
    {
#ifdef WANT_DEBUG
      printk("Process signalled.\n"); */
#endif
      return -ERESTARTNOINTR; /* Will restart call automagically */
    }
#ifdef WANT_DEBUG
    printk("Process has gotten interrupt!\n");
#endif
  }
  int_flag &= ~(1 << (MINOR(inode->i_rdev)-1));
#ifdef WANT_DEBUG
  printk("Int_flag reset to %04x\n", int_flag);
#endif
  return MINOR(inode->i_rdev);
}

static int int_open(struct inode *inode, struct file *file)
{
#ifdef WANT_SIGIO
  int rdev;
  rdev = MINOR(inode->i_rdev);
#endif

  if (request_irq(MINOR(inode->i_rdev), &register_interrupt, 0, ID_STRING )) {
#ifdef WANT_DEBUG
    printk("Interrupt Generator open failed. INT0x%02x already in use\n", MINOR(inode->i_rdev));
#endif
    return -1;
  }
  ints_in_use++;
#ifdef WANT_DEBUG
  printk("Interrupt Generator opened. In use by %d processess.\n", ints_in_use);
#endif
#ifdef WANT_SIGIO
  int_tty[rdev] = (struct tty_struct *)file->private_data;
  int_tty[rdev]->fasync = NULL;
#endif
  MOD_INC_USE_COUNT;
  return 0;
}

static void int_release(struct inode *inode, struct file *file)
{
  free_irq(MINOR(inode->i_rdev));
  ints_in_use--;
#ifdef WANT_DEBUG
  printk("Interrupt Generator released. Still in use by %d processess.\n", ints_in_use);
#endif
  MOD_DEC_USE_COUNT;
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
#ifdef WANT_SIGIO
	int_fasync	/* Asyncronous I/O */
#else
	NULL
#endif
};



#ifndef  MAKE_LOADABLE_MODULE

#define SIG_MAJOR 19

unsigned long int_init(unsigned long kmem_start)
{
  printk("\nSilly Interrupt Generator installed.(ver 0.11)\n\n");
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
  printk("Silly Interrupt Generator (ver 0.11):  init_module called\n");
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
  printk( "Silly Interrupt Generator:  cleanup_module called\n");
  if (MOD_IN_USE) printk(ID_STRING ": device busy, remove delayed\n");
  if (unregister_chrdev(SIG_MAJOR, ID_STRING) != 0) {
    printk("cleanup_module failed\n");
  } else {
    printk("cleanup_module succeeded\n");
  }    
}

#endif /* MAKE_LOADABLE_MODULE */

#ifdef WANT_SIGIO
static int int_fasync(struct inode * inode, struct file * filp, int on)
{
	struct fasync_struct *fa, *prev;
	int rdev;

	rdev = MINOR(inode->i_rdev);
  	printk("FASYNC called i_rdev=%x\n",rdev);
 
	int_tty[rdev] = (struct tty_struct *)filp->private_data;

	for (fa = int_tty[rdev]->fasync, prev = 0; fa; prev= fa, fa = fa->fa_next) {
		if (fa->fa_file == filp)
			break;
	}

	if (on) {
		if (fa)
			return 0;
		fa = (struct fasync_struct *)kmalloc(sizeof(struct fasync_struct), GFP_KERNEL);
		if (!fa)
			return -ENOMEM;
		fa->magic = FASYNC_MAGIC;
		fa->fa_file = filp;
		fa->fa_next = int_tty[rdev]->fasync;
		int_tty[rdev]->fasync = fa;
		if (!int_tty[rdev]->read_wait)
			int_tty[rdev]->minimum_to_wake = 1;
	} else {
		if (!fa)
			return 0;
		if (prev)
			prev->fa_next = fa->fa_next;
		else
			int_tty[rdev]->fasync = fa->fa_next;
		kfree_s(fa, sizeof(struct fasync_struct));
		if (!int_tty[rdev]->fasync && !int_tty[rdev]->read_wait)
			int_tty[rdev]->minimum_to_wake = N_TTY_BUF_SIZE;
	}
	return 0;	
}
#endif
