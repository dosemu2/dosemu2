/*
 * linux/kernel/ldt.c
 *
 * Copyright (C) 1992 Krishna Balasubramanian and Linus Torvalds
 *
 *  This is a "patch-module" against Linux-1.2.1 and this is
 *  Copyright (C) 1995 Lutz Molgedey <lutz@summa.physik.hu-berlin.de>
 *
 *  Module implementation and 'user-root accessable kernel space' (func 0x10) is
 *  Copyright (C) 1995 Hans Lermen <lermen@elserv.ffm.fgan.de> 
 *  
 *  The original source can be found on
 *  ftp.cs.helsinki.fi:/pub/Software/Linux/Kernel/v1.2
 *  in the file linux/arch/i386/kernel/ldt.c
 *
 *    ... I hope we did set up all (C)'s correctly 
 *        in order to not violate the GPL (did we ?)
 */

#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)

/* NOTE
 * in emumodule LDT stuff there is an new function for modify_ldt (0x10)
 * which creates a discriptor pointing into the kernel space.
 *
 * ====================================
 * THIS CAN ONLY BE DONE UNDER UID ROOT, else -EACCES is returned.
 * ====================================
 *
 * The contents of struct modify_ldt_ldt_s must be set as follows:
 *
 * s.entry_number    = selector >> 3;
 * s.base_addr       = 0, then an descriptor to the LDT itself is created
 * or
 * s.base_addr       > 0 and < high_memory, then a descriptor into kernelspace
 *                       is created. e.g. 0x100000 will point to 'startup_32'
 * s.limit =         as usual, but will be overwritten in case of base_addr=0
 * s.seg_32bit=      as usual
 * s.contents        must be <=1, but will be set to 0 in case of base_addr=0
 * s.read_exec_only  as usual, but NOTE: Be careful not to produce a securety hole.
 * s.limit_in_pages  as usual,  but will be set to 0 in case of base_addr=0
 * s.seg_not_present is allways set to 0
 *
 */

#endif


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


#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)

#include <asm/page.h>
#include <asm/pgtable.h>

static void unprotect_vmarea(unsigned long address, unsigned long size)
{
  unsigned long addr=VMALLOC_VMADDR(address);
  pgd_t * dir;
  unsigned long end = addr + size;
  if (!addr) return;
  
  /* NOTE: we support only Intel x86 family, so we have 2-Level page table
   *       and we skip the pmd_t related stuff
   */
#if KERNEL_VERSION < 1003024
  dir = pgd_offset(current, addr);
#else
  dir = pgd_offset(current->mm, addr);
#endif
  while (addr < end) {
#if 0
    printk("LDT: addr=%08x, pgd=%08x\n", addr, pgd_val(*dir));
#endif
    {
      unsigned long addr_= addr & ~PGDIR_MASK;
      unsigned long end_ = addr_ + (end - addr);
      pte_t * pte = pte_offset( (pmd_t *)dir, addr_);
      if (end_ > PGDIR_SIZE) end_ = PGDIR_SIZE;
      while (addr_ < end_) {
        if (pte_val(*pte) & _PAGE_PRESENT)
        pte_val(*pte) |= _PAGE_USER;
#if 0
        printk("LDT: addr_=%08x, >pte<=%08x, pte=%08x\n", addr_, pte ,pte_val(*pte));
#endif
        addr_ += PAGE_SIZE;
        pte++;
      }
    }
    addr = (addr + PGDIR_SIZE) & PGDIR_MASK;
    dir++;
  }
  invalidate();
}
#endif


static int write_ldt(void * ptr, unsigned long bytecount)
{
	struct modify_ldt_ldt_s ldt_info;
	unsigned long *lp;
	unsigned long base, limit;
	int error, i, userspace =bytecount;

#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
	if (!userspace) {
	  /* Request to create a kernel space descriptor for user 'root' 
	   * We have to verify that it's real 'root'
	   */
	  if (!suser()) return  -EACCES;
	} 
        else
#endif
	if (bytecount != sizeof(ldt_info))
		return -EINVAL;
	error = verify_area(VERIFY_READ, ptr, sizeof(ldt_info));
	if (error)
		return error;

	memcpy_fromfs(&ldt_info, ptr, sizeof(ldt_info));

#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
	if (!userspace) {
	  if (ldt_info.contents > 1) return -EINVAL;
	  if (ldt_info.base_addr) {
	    /* normal kernel space */
	    if ( ldt_info.base_addr > ~TASK_SIZE ) return -EINVAL;
	    ldt_info.base_addr +=  TASK_SIZE;
	  }
	}
	else if ((ldt_info.contents == 3 && ldt_info.seg_not_present == 0) || ldt_info.entry_number >= LDT_ENTRIES)
#else
	if (ldt_info.contents == 3 || ldt_info.entry_number >= LDT_ENTRIES)
#endif
		return -EINVAL;

	limit = ldt_info.limit;
	base = ldt_info.base_addr;
#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
	if (ldt_info.limit_in_pages) {
		limit *= PAGE_SIZE;
		limit += PAGE_SIZE-1; 
	}
	limit += base;
	if (!userspace) {
	  if (limit < base ) return -EINVAL;
	}
	else if (ptr && (limit < base || limit >= 0xC0000000) && ldt_info.seg_not_present == 0)
		return -EINVAL;
#else
	if (ldt_info.limit_in_pages)
		limit *= PAGE_SIZE;

	limit += base;
	if (limit < base || limit >= 0xC0000000)
		return -EINVAL;
#endif

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
	if (!userspace) {
	  if (!ldt_info.base_addr) {
	    ldt_info.base_addr = VMALLOC_VMADDR(current->ldt);
	    ldt_info.limit =  (LDT_ENTRIES*LDT_ENTRY_SIZE)-1;
	    ldt_info.limit_in_pages =0;
	    ldt_info.contents = MODIFY_LDT_CONTENTS_DATA;
	  }
	  else userspace=-1; /* avoid below unprotect() */
	  ldt_info.seg_not_present = 0;
	}
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
	if (!userspace) unprotect_vmarea((unsigned long)current->ldt, LDT_ENTRIES*LDT_ENTRY_SIZE);
#endif
	return 0;
}

asmlinkage int sys_modify_ldt(int func, void *ptr, unsigned long bytecount)
{
#if defined(_LOADABLE_VM86_) && defined(_VM86_STATISTICS_)
        extern int sys_ldt_count;
        sys_ldt_count++;
#endif
	if (func == 0)
		return read_ldt(ptr, bytecount);
#if defined(_LOADABLE_VM86_) && defined(WANT_WINDOWS)
	if (bytecount != sizeof(struct modify_ldt_ldt_s)) return -EINVAL;
	if (func == 1)
		return write_ldt(ptr, bytecount);
	if (func == 0x10)
		return write_ldt(ptr, 0);
#else
	if (func == 1)
		return write_ldt(ptr, bytecount);
#endif
	return -ENOSYS;
}
