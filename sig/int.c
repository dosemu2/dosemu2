#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/ptrace.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/irq.h>

static int ints_in_use = 0;
static struct wait_queue *wait_queue = NULL;

static unsigned int int_flag = 0;

void register_interrupt(int intno) {
  cli();
  intno= -(((struct pt_regs *)intno)->orig_eax+2);
  int_flag |= (1 << ((intno & 0xff)-1));
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
  if (request_irq(MINOR(inode->i_rdev), &register_interrupt, 0, "SIG" )) {
#ifdef WANT_DEBUG
    printk("Test Data Generator open failed. INT0x%02x already in use\n", MINOR(inode->i_rdev));
#endif
    return -1;
  }
  ints_in_use++;
#ifdef WANT_DEBUG
  printk("Test Data Generator opened. In use by %d processess.\n", ints_in_use);
#endif
  return 0;
}

static void int_release(struct inode *inode, struct file *file)
{
  free_irq(MINOR(inode->i_rdev));
  ints_in_use--;
#ifdef WANT_DEBUG
  printk("Interrupt Generator released. Still in use by %d processess.\n", ints_in_use);
#endif
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
	int_release	/* int_release */
};

unsigned long int_init(unsigned long kmem_start)
{
  printk("\nSilly Interrupt Generator installed.(ver 0.10)\n\n");
  if (register_chrdev(19,"intr",&int_fops))
    printk("Interrupt Generator error: Cannot register to major device 19!\n");
  return kmem_start;
}
