/*
 * linux/kernel/ldt.c
 *
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds
 *
 *  This is a "patch-module" against Linux-1.2.1 and this is
 *  Copyright (C) 1995 Lutz Molgedey <lutz@summa.physik.hu-berlin.de>
 *
 *  Module implementation is
 *  Copyright (C) 1995 Hans Lermen <lermen@elserv.ffm.fgan.de> 
 *  
 *  The original source can be found on
 *  ftp.cs.helsinki.fi:/pub/Software/Linux/Kernel/v1.2
 *  in the file linux/arch/i386/kernel/ldt.c
 *
 *    ... I hope we did set up all (C)'s correctly 
 *        in order to not violate the GPL (did we ?)
 */


#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <asm/segment.h>
#include <asm/system.h>

#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
#include "ldt.h" /* NOTE we have a patched version in dosemu/include */
#else
#include <linux/ldt.h>
#endif

#ifdef _LOADABLE_VM86_
#include "kversion.h"
#if 0
#define KERNEL_VERSION 1003028 /* last verified kernel version */
#endif

#if KERNEL_VERSION < 1001090
#error "Sorry, but this patch runs only on Linux >= 1.1.90"
#endif

#include "emumod.h"
#endif


static int read_ldt(void * ptr, unsigned long bytecount)
{
	int error;
	void * address = current->ldt;
	unsigned long size;

	if (!ptr)
		return -EINVAL;
	size = LDT_ENTRIES*LDT_ENTRY_SIZE;
	if (!address) {
		address = &default_ldt;
		size = sizeof(default_ldt);
	}
	if (size > bytecount)
		size = bytecount;
	error = verify_area(VERIFY_WRITE, ptr, size);
	if (error)
		return error;
	memcpy_tofs(ptr, address, size);
	return size;
}


static inline int limits_ok(struct modify_ldt_ldt_s *ldt_info)
{
	unsigned long base, limit;
	/* linear address of first and last accessible byte */
	unsigned long first, last;

	base = ldt_info->base_addr;
	limit = ldt_info->limit;
	if (ldt_info->limit_in_pages)
		limit = limit * PAGE_SIZE + PAGE_SIZE - 1;

	first = base;
	last = limit + base;

	/* segment grows down? */
	if (ldt_info->contents == 1) {
		/* data segment grows down */
		first = base+limit+1;
		last = base+65535;
		if (ldt_info->seg_32bit)
			last = base-1;
	}
#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
	if (last < first) return 0;
	if (last >= TASK_SIZE) return suser();
	return 1;
#else
	return (last >= first && last < TASK_SIZE);
#endif
}

static int write_ldt(void * ptr, unsigned long bytecount)
{
	struct modify_ldt_ldt_s ldt_info;
	unsigned long *lp;
	int error, i;

	if (bytecount != sizeof(ldt_info))
		return -EINVAL;
	error = verify_area(VERIFY_READ, ptr, sizeof(ldt_info));
	if (error)
		return error;

	memcpy_fromfs(&ldt_info, ptr, sizeof(ldt_info));

#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
	if ((ldt_info.contents == 3 && ldt_info.seg_not_present == 0) || ldt_info.entry_number >= LDT_ENTRIES)
#else
	if (ldt_info.contents == 3 || ldt_info.entry_number >= LDT_ENTRIES)
#endif
		return -EINVAL;

#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
	if (!limits_ok(&ldt_info) && ldt_info.seg_not_present == 0)
#else
	if (!limits_ok(&ldt_info))
#endif
		return -EINVAL;

	if (!current->ldt) {
		for (i=1 ; i<NR_TASKS ; i++) {
			if (task[i] == current) {
				if (!(current->ldt = (struct desc_struct*) vmalloc(LDT_ENTRIES*LDT_ENTRY_SIZE)))
					return -ENOMEM;
				memset(current->ldt, 0, LDT_ENTRIES*LDT_ENTRY_SIZE);
				set_ldt_desc(gdt+(i<<1)+FIRST_LDT_ENTRY, current->ldt, LDT_ENTRIES);
				load_ldt(i);
			}
		}
	}
	
	lp = (unsigned long *) &current->ldt[ldt_info.entry_number];
   	/* Allow LDTs to be cleared by the user. */
#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
   	if (ldt_info.base_addr == 0 && ldt_info.limit == 0 &&
	    ldt_info.contents == 0 && ldt_info.read_exec_only == 1 &&
	    ldt_info.seg_32bit == 0 && ldt_info.limit_in_pages == 0 &&
	    ldt_info.seg_not_present == 1 && ldt_info.useable == 0) {
#else
   	if (ldt_info.base_addr == 0 && ldt_info.limit == 0) {
#endif
		*lp = 0;
		*(lp+1) = 0;
		return 0;
	}
	*lp = ((ldt_info.base_addr & 0x0000ffff) << 16) |
		  (ldt_info.limit & 0x0ffff);
	*(lp+1) = (ldt_info.base_addr & 0xff000000) |
		  ((ldt_info.base_addr & 0x00ff0000)>>16) |
		  (ldt_info.limit & 0xf0000) |
		  (ldt_info.contents << 10) |
		  ((ldt_info.read_exec_only ^ 1) << 9) |
		  (ldt_info.seg_32bit << 22) |
		  (ldt_info.limit_in_pages << 23) |
		  ((ldt_info.seg_not_present ^1) << 15) |
#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
		  (ldt_info.useable << 20) |
#endif
		  0x7000;
#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
  #if 0
        printk("LDT: addr = %08x, entry= %08x, val =%08x %08x\n",current->ldt, lp,  *lp, *(lp+1));
  #endif
#endif
	return 0;
}

asmlinkage int sys_modify_ldt(int func, void *ptr, unsigned long bytecount)
{
	if (func == 0)
		return read_ldt(ptr, bytecount);
	if (func == 1)
		return write_ldt(ptr, bytecount);
	return -ENOSYS;
}
