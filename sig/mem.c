/*
 *  linux/kernel/chr_drv/mem.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/tty.h>
#include <linux/mouse.h>
#include <linux/tpqic02.h>
#include <linux/malloc.h>
#include <linux/mman.h>

#include <asm/segment.h>
#include <asm/io.h>

#ifdef CONFIG_SOUND
extern long soundcard_init(long mem_start);
#endif
unsigned long int_init(unsigned long);

static int read_ram(struct inode * inode, struct file * file,char * buf, int count)
{
	return -EIO;
}

static int write_ram(struct inode * inode, struct file * file,char * buf, int count)
{
	return -EIO;
}

static int read_mem(struct inode * inode, struct file * file,char * buf, int count)
{
	unsigned long p = file->f_pos;
	int read;

	if (count < 0)
		return -EINVAL;
	if (p >= high_memory)
		return 0;
	if (count > high_memory - p)
		count = high_memory - p;
	read = 0;
	while (p < PAGE_SIZE && count > 0) {
		put_fs_byte(0,buf);
		buf++;
		p++;
		count--;
		read++;
	}
	memcpy_tofs(buf,(void *) p,count);
	read += count;
	file->f_pos += read;
	return read;
}

static int write_mem(struct inode * inode, struct file * file,char * buf, int count)
{
	unsigned long p = file->f_pos;
	int written;

	if (count < 0)
		return -EINVAL;
	if (p >= high_memory)
		return 0;
	if (count > high_memory - p)
		count = high_memory - p;
	written = 0;
	while (p < PAGE_SIZE && count > 0) {
		/* Hmm. Do something? */
		buf++;
		p++;
		count--;
		written++;
	}
	memcpy_fromfs((void *) p,buf,count);
	written += count;
	file->f_pos += written;
	return count;
}

static int mmap_mem(struct inode * inode, struct file * file, struct vm_area_struct * vma)
{
	if (vma->vm_offset & ~PAGE_MASK)
		return -ENXIO;
	if (x86 > 3 && vma->vm_offset >= high_memory)
		vma->vm_page_prot |= PAGE_PCD;
	if (remap_page_range(vma->vm_start, vma->vm_offset, vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
	vma->vm_inode = inode;
	inode->i_count++;
	return 0;
}

static int read_kmem(struct inode *inode, struct file *file, char *buf, int count)
{
	int read1, read2;

	read1 = read_mem(inode, file, buf, count);
	if (read1 < 0)
		return read1;
	read2 = vread(buf + read1, (char *) ((unsigned long) file->f_pos), count - read1);
	if (read2 < 0)
		return read2;
	file->f_pos += read2;
	return read1 + read2;
}

static int read_port(struct inode * inode,struct file * file,char * buf, int count)
{
	unsigned int i = file->f_pos;
	char * tmp = buf;

	while (count-- > 0 && i < 65536) {
		put_fs_byte(inb(i),tmp);
		i++;
		tmp++;
	}
	file->f_pos = i;
	return tmp-buf;
}

static int write_port(struct inode * inode,struct file * file,char * buf, int count)
{
	unsigned int i = file->f_pos;
	char * tmp = buf;

	while (count-- > 0 && i < 65536) {
		outb(get_fs_byte(tmp),i);
		i++;
		tmp++;
	}
	file->f_pos = i;
	return tmp-buf;
}

static int read_null(struct inode * node,struct file * file,char * buf,int count)
{
	return 0;
}

static int write_null(struct inode * inode,struct file * file,char * buf, int count)
{
	return count;
}

static int read_zero(struct inode * node,struct file * file,char * buf,int count)
{
	int left;

	for (left = count; left > 0; left--) {
		put_fs_byte(0,buf);
		buf++;
	}
	return count;
}

static int mmap_zero(struct inode * inode, struct file * file, struct vm_area_struct * vma)
{
	if (vma->vm_page_prot & PAGE_RW)
		return -EINVAL;
	if (zeromap_page_range(vma->vm_start, vma->vm_end - vma->vm_start, vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

static int read_full(struct inode * node,struct file * file,char * buf,int count)
{
	return count;
}

static int write_full(struct inode * inode,struct file * file,char * buf, int count)
{
	return -ENOSPC;
}

/*
 * Special lseek() function for /dev/null and /dev/zero.  Most notably, you can fopen()
 * both devices with "a" now.  This was previously impossible.  SRB.
 */

static int null_lseek(struct inode * inode, struct file * file, off_t offset, int orig)
{
	return file->f_pos=0;
}
/*
 * The memory devices use the full 32 bits of the offset, and so we cannot
 * check against negative addresses: they are ok. The return value is weird,
 * though, in that case (0).
 *
 * also note that seeking relative to the "end of file" isn't supported:
 * it has no meaning, so it returns -EINVAL.
 */
static int memory_lseek(struct inode * inode, struct file * file, off_t offset, int orig)
{
	switch (orig) {
		case 0:
			file->f_pos = offset;
			return file->f_pos;
		case 1:
			file->f_pos += offset;
			return file->f_pos;
		default:
			return -EINVAL;
	}
	if (file->f_pos < 0)
		return 0;
	return file->f_pos;
}

#define write_kmem	write_mem
#define mmap_kmem	mmap_mem
#define zero_lseek	null_lseek
#define write_zero	write_null

static struct file_operations ram_fops = {
	memory_lseek,
	read_ram,
	write_ram,
	NULL,		/* ram_readdir */
	NULL,		/* ram_select */
	NULL,		/* ram_ioctl */
	NULL,		/* ram_mmap */
	NULL,		/* no special open code */
	NULL,		/* no special release code */
	NULL		/* fsync */
};

static struct file_operations mem_fops = {
	memory_lseek,
	read_mem,
	write_mem,
	NULL,		/* mem_readdir */
	NULL,		/* mem_select */
	NULL,		/* mem_ioctl */
	mmap_mem,
	NULL,		/* no special open code */
	NULL,		/* no special release code */
	NULL		/* fsync */
};

static struct file_operations kmem_fops = {
	memory_lseek,
	read_kmem,
	write_kmem,
	NULL,		/* kmem_readdir */
	NULL,		/* kmem_select */
	NULL,		/* kmem_ioctl */
	mmap_kmem,
	NULL,		/* no special open code */
	NULL,		/* no special release code */
	NULL		/* fsync */
};

static struct file_operations null_fops = {
	null_lseek,
	read_null,
	write_null,
	NULL,		/* null_readdir */
	NULL,		/* null_select */
	NULL,		/* null_ioctl */
	NULL,		/* null_mmap */
	NULL,		/* no special open code */
	NULL,		/* no special release code */
	NULL		/* fsync */
};

static struct file_operations port_fops = {
	memory_lseek,
	read_port,
	write_port,
	NULL,		/* port_readdir */
	NULL,		/* port_select */
	NULL,		/* port_ioctl */
	NULL,		/* port_mmap */
	NULL,		/* no special open code */
	NULL,		/* no special release code */
	NULL		/* fsync */
};

static struct file_operations zero_fops = {
	zero_lseek,
	read_zero,
	write_zero,
	NULL,		/* zero_readdir */
	NULL,		/* zero_select */
	NULL,		/* zero_ioctl */
	mmap_zero,
	NULL,		/* no special open code */
	NULL		/* no special release code */
};

static struct file_operations full_fops = {
	memory_lseek,
	read_full,
	write_full,
	NULL,		/* full_readdir */
	NULL,		/* full_select */
	NULL,		/* full_ioctl */	
	NULL,		/* full_mmap */
	NULL,		/* no special open code */
	NULL		/* no special release code */
};

static int memory_open(struct inode * inode, struct file * filp)
{
	switch (MINOR(inode->i_rdev)) {
		case 0:
			filp->f_op = &ram_fops;
			break;
		case 1:
			filp->f_op = &mem_fops;
			break;
		case 2:
			filp->f_op = &kmem_fops;
			break;
		case 3:
			filp->f_op = &null_fops;
			break;
		case 4:
			filp->f_op = &port_fops;
			break;
		case 5:
			filp->f_op = &zero_fops;
			break;
		case 7:
			filp->f_op = &full_fops;
			break;
		default:
			return -ENODEV;
	}
	if (filp->f_op && filp->f_op->open)
		return filp->f_op->open(inode,filp);
	return 0;
}

static struct file_operations memory_fops = {
	NULL,		/* lseek */
	NULL,		/* read */
	NULL,		/* write */
	NULL,		/* readdir */
	NULL,		/* select */
	NULL,		/* ioctl */
	NULL,		/* mmap */
	memory_open,	/* just a selector for the real open */
	NULL,		/* release */
	NULL		/* fsync */
};

#ifdef CONFIG_FTAPE
char* ftape_big_buffer;
#endif

long chr_dev_init(long mem_start, long mem_end)
{
	if (register_chrdev(MEM_MAJOR,"mem",&memory_fops))
		printk("unable to get major %d for memory devs\n", MEM_MAJOR);
	mem_start = tty_init(mem_start);
#ifdef CONFIG_PRINTER
	mem_start = lp_init(mem_start);
#endif
#if defined (CONFIG_BUSMOUSE) || defined (CONFIG_82C710_MOUSE) || \
    defined (CONFIG_PSMOUSE) || defined (CONFIG_MS_BUSMOUSE) || \
    defined (CONFIG_ATIXL_BUSMOUSE)
	mem_start = mouse_init(mem_start);
#endif
#ifdef CONFIG_SOUND
	mem_start = soundcard_init(mem_start);
#endif
#if CONFIG_QIC02_TAPE
	mem_start = qic02_tape_init(mem_start);
#endif
/*
 *      Rude way to allocate kernel memory buffer for tape device
 */
#ifdef CONFIG_FTAPE
        /* allocate NR_FTAPE_BUFFERS 32Kb buffers at aligned address */
        ftape_big_buffer= (char*) ((mem_start + 0x7fff) & ~0x7fff);
        printk( "ftape: allocated %d buffers aligned at: %p\n",
               NR_FTAPE_BUFFERS, ftape_big_buffer);
        mem_start = (long) ftape_big_buffer + NR_FTAPE_BUFFERS * 0x8000;
#endif 
	mem_start = int_init(mem_start);
	return mem_start;
}
