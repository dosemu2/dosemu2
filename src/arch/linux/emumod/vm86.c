/*
 *  linux/kernel/vm86.c
 *
 *  Copyright (C) 1994  Linus Torvalds
 *
 *  changes for the dosemu team  by Lutz Molgedey <lutz@summa.physik.hu-berlin.de>
 *                                  Hans Lermen <lermen@elserv.ffm.fgan.de>
 */
#ifdef _LOADABLE_VM86_
  #include "kversion.h"
#else
  #define KERNEL_VERSION 1003028 /* last verified kernel version */
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

#if defined(_LOADABLE_VM86_) && defined(USE_MHPDBG)
#include "mhpdbg.h"
#endif

/*
 * Known problems:
 *
 * Interrupt handling is not guaranteed:
 * - a real x86 will disable all interrupts for one instruction
 *   after a "mov ss,xx" to make stack handling atomic even without
 *   the 'lss' instruction. We can't guarantee this in v86 mode,
 *   as the next instruction might result in a page fault or similar.
 * - a real x86 will have interrupts disabled for one instruction
 *   past the 'sti' that enables them. We don't bother with all the
 *   details yet..
 *
 * Hopefully these problems do not actually matter for anything.
 */

#ifdef _LOADABLE_VM86_
  #include "emumod.h"
  #ifdef USE_VM86PLUS
    #if KERNEL_VERSION < 1001075
      #error "can't compile with USE_VM86PLUS for kernels < 1.1.75"
    #endif
    #include "vm86plus.h"
  #endif
#endif

/*
 * 8- and 16-bit register defines..
 */
#define AL(regs)	(((unsigned char *)&((regs)->eax))[0])
#define AH(regs)	(((unsigned char *)&((regs)->eax))[1])
#define IP(regs)	(*(unsigned short *)&((regs)->eip))
#define SP(regs)	(*(unsigned short *)&((regs)->esp))

/*
 * virtual flags (16 and 32-bit versions)
 */

#if KERNEL_VERSION >= 1001075
#define VFLAGS	(*(unsigned short *)&(current->tss.v86flags))
#define VEFLAGS	(current->tss.v86flags)
#else
#define VFLAGS	(*(unsigned short *)&(current->v86flags))
#define VEFLAGS	(current->v86flags)
#endif

#define set_flags(X,new,mask) \
((X) = ((X) & ~(mask)) | ((new) & (mask)))

#define SAFE_MASK	(0xDD5)
#define RETURN_MASK	(0xDFF)

asmlinkage struct pt_regs * save_v86_state(struct vm86_regs * regs)
{
	unsigned long tmp;

#if KERNEL_VERSION >= 1001075
	if (!current->tss.vm86_info) {
#else
	if (!current->vm86_info) {
#endif
		printk("no vm86_info: BAD\n");
		do_exit(SIGSEGV);
	}
#if KERNEL_VERSION >= 1001075
	set_flags(regs->eflags, VEFLAGS, VIF_MASK | current->tss.v86mask);
	memcpy_tofs(&current->tss.vm86_info->regs,regs,sizeof(*regs));
	put_fs_long(current->tss.screen_bitmap,&current->tss.vm86_info->screen_bitmap);
#else
	set_flags(regs->eflags, VEFLAGS, VIF_MASK | current->v86mask);
	memcpy_tofs(&current->vm86_info->regs,regs,sizeof(*regs));
	put_fs_long(current->screen_bitmap,&current->vm86_info->screen_bitmap);
#endif
	tmp = current->tss.esp0;
	current->tss.esp0 = current->saved_kernel_stack;
	current->saved_kernel_stack = 0;
	return (struct pt_regs *) tmp;
}

static void mark_screen_rdonly(struct task_struct * tsk)
{
#if KERNEL_VERSION < 1001084
	unsigned long tmp;
	unsigned long *pg_table;

	if ((tmp = tsk->tss.cr3) != 0) {
		tmp = *(unsigned long *) tmp;
		if (tmp & PAGE_PRESENT) {
			tmp &= PAGE_MASK;
			pg_table = (0xA0000 >> PAGE_SHIFT) + (unsigned long *) tmp;
			tmp = 32;
			while (tmp--) {
				if (PAGE_PRESENT & *pg_table)
					*pg_table &= ~PAGE_RW;
				pg_table++;
			}
		}
	}
#else
  #if KERNEL_VERSION < 1002002
	pgd_t *pg_dir;

	pg_dir = PAGE_DIR_OFFSET(tsk, 0);
	if (!pgd_none(*pg_dir)) {
		pte_t *pg_table;
		int i;

		if (pgd_bad(*pg_dir)) {
			printk("vm86: bad page table directory entry %08lx\n", pgd_val(*pg_dir));
			pgd_clear(pg_dir);
			return;
		}
		pg_table = (pte_t *) pgd_page(*pg_dir);
		pg_table += 0xA0000 >> PAGE_SHIFT;
		for (i = 0 ; i < 32 ; i++) {
			if (pte_present(*pg_table))
				*pg_table = pte_wrprotect(*pg_table);
			pg_table++;
 		}
 	}
  #else
	pgd_t *pgd;
	pmd_t *pmd;
	pte_t *pte;
	int i;

#if KERNEL_VERSION < 1003024
	pgd = pgd_offset(tsk, 0xA0000);
#else
	pgd = pgd_offset(tsk->mm, 0xA0000);
#endif
	if (pgd_none(*pgd))
		return;
	if (pgd_bad(*pgd)) {
		printk("vm86: bad pgd entry [%p]:%08lx\n", pgd, pgd_val(*pgd));
		pgd_clear(pgd);
		return;
	}
	pmd = pmd_offset(pgd, 0xA0000);
	if (pmd_none(*pmd))
		return;
	if (pmd_bad(*pmd)) {
		printk("vm86: bad pmd entry [%p]:%08lx\n", pmd, pmd_val(*pmd));
		pmd_clear(pmd);
		return;
	}
	pte = pte_offset(pmd, 0xA0000);
	for (i = 0; i < 32; i++) {
		if (pte_present(*pte))
#if KERNEL_VERSION < 1003022
			*pte = pte_wrprotect(*pte);
#else
			set_pte(pte, pte_wrprotect(*pte));
#endif
		pte++;
 	}
#if KERNEL_VERSION < 1003047
	invalidate();
#else
	invalidate_all();
#endif
  #endif
#endif
}

asmlinkage int sys_vm86(struct vm86_struct * v86)
{
	struct vm86_struct info;
	struct pt_regs * pt_regs = (struct pt_regs *) &v86;
	int error;

	if (current->saved_kernel_stack)
		return -EPERM;
	/* v86 must be readable (now) and writable (for save_v86_state) */
#if defined(_LOADABLE_VM86_) && defined(USE_VM86PLUS)
	error = verify_area(VERIFY_WRITE,v86,sizeof(struct vm86plus_struct));
#else
	error = verify_area(VERIFY_WRITE,v86,sizeof(*v86));
#endif
	if (error)
		return error;
	memcpy_fromfs(&info,v86,sizeof(info));
/*
 * make sure the vm86() system call doesn't try to do anything silly
 */
	info.regs.__null_ds = 0;
	info.regs.__null_es = 0;
	info.regs.__null_fs = 0;
	info.regs.__null_gs = 0;
/*
 * The eflags register is also special: we cannot trust that the user
 * has set it up safely, so this makes sure interrupt etc flags are
 * inherited from protected mode.
 */
 	VEFLAGS = info.regs.eflags;
	info.regs.eflags &= SAFE_MASK;
	info.regs.eflags |= pt_regs->eflags & ~SAFE_MASK;
	info.regs.eflags |= VM_MASK;

	switch (info.cpu_type) {
#if KERNEL_VERSION >= 1001075
		case CPU_286:
			current->tss.v86mask = 0;
			break;
		case CPU_386:
			current->tss.v86mask = NT_MASK | IOPL_MASK;
			break;
		case CPU_486:
			current->tss.v86mask = AC_MASK | NT_MASK | IOPL_MASK;
			break;
		default:
			current->tss.v86mask = ID_MASK | AC_MASK | NT_MASK | IOPL_MASK;
			break;
#else
		case CPU_286:
			current->v86mask = 0;
			break;
		case CPU_386:
			current->v86mask = NT_MASK | IOPL_MASK;
			break;
		case CPU_486:
			current->v86mask = AC_MASK | NT_MASK | IOPL_MASK;
			break;
		default:
			current->v86mask = ID_MASK | AC_MASK | NT_MASK | IOPL_MASK;
			break;
#endif
	}

/*
 * Save old state, set default return value (%eax) to 0
 */
	pt_regs->eax = 0;
	current->saved_kernel_stack = current->tss.esp0;
	current->tss.esp0 = (unsigned long) pt_regs;
#if KERNEL_VERSION >= 1001075
	current->tss.vm86_info = v86;

	current->tss.screen_bitmap = info.screen_bitmap;
#else
	current->vm86_info = v86;

	current->screen_bitmap = info.screen_bitmap;
#endif
	if (info.flags & VM86_SCREEN_BITMAP)
		mark_screen_rdonly(current);
	__asm__ __volatile__("movl %0,%%esp\n\t"
		"jmp ret_from_sys_call"
		: /* no outputs */
		:"r" (&info.regs));
	return 0;
}

static inline void return_to_32bit(struct vm86_regs * regs16, int retval)
{
	struct pt_regs * regs32;

	regs32 = save_v86_state(regs16);
	regs32->eax = retval;
	__asm__ __volatile__("movl %0,%%esp\n\t"
		"jmp ret_from_sys_call"
		: : "r" (regs32));
}

static inline void set_IF(struct vm86_regs * regs)
{
	VEFLAGS |= VIF_MASK;
	if (VEFLAGS & VIP_MASK)
		return_to_32bit(regs, VM86_STI);
}

static inline void clear_IF(struct vm86_regs * regs)
{
	VEFLAGS &= ~VIF_MASK;
}

static inline void clear_TF(struct vm86_regs * regs)
{
	regs->eflags &= ~TF_MASK;
}

static inline void set_vflags_long(unsigned long eflags, struct vm86_regs * regs)
{
#if KERNEL_VERSION >= 1001075
	set_flags(VEFLAGS, eflags, current->tss.v86mask);
#else
	set_flags(VEFLAGS, eflags, current->v86mask);
#endif
	set_flags(regs->eflags, eflags, SAFE_MASK);
#if 1
	if (eflags & IF_MASK)
#else
	if (VEFLAGS & IF_MASK)
#endif
		set_IF(regs);
}

static inline void set_vflags_short(unsigned short flags, struct vm86_regs * regs)
{
#if KERNEL_VERSION >= 1001075
	set_flags(VFLAGS, flags, current->tss.v86mask);
#else
	set_flags(VFLAGS, flags, current->v86mask);
#endif
	set_flags(regs->eflags, flags, SAFE_MASK);
	if (flags & IF_MASK)
		set_IF(regs);
}

static inline unsigned long get_vflags(struct vm86_regs * regs)
{
	unsigned long flags = regs->eflags & RETURN_MASK;

	if (VEFLAGS & VIF_MASK)
		flags |= IF_MASK;
#if KERNEL_VERSION >= 1001075
	return flags | (VEFLAGS & current->tss.v86mask);
#else
	return flags | (VEFLAGS & current->v86mask);
#endif
}

static inline int is_revectored(int nr, struct revectored_struct * bitmap)
{
#if 1 /* KERNEL_VERSION >= 1003002 (BUGFIX) */
	if (verify_area(VERIFY_READ, bitmap, 256/8) < 0)
		return 1;
#endif
	__asm__ __volatile__("btl %2,%%fs:%1\n\tsbbl %0,%0"
		:"=r" (nr)
		:"m" (*bitmap),"r" (nr));
	return nr;
}

/*
 * Boy are these ugly, but we need to do the correct 16-bit arithmetic.
 * Gcc makes a mess of it, so we do it inline and use non-obvious calling
 * conventions..
 */
#define pushb(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %2,%%fs:0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define pushw(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"movb %h2,%%fs:0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,%%fs:0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define pushl(base, ptr, val) \
__asm__ __volatile__( \
	"decw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb %h2,%%fs:0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,%%fs:0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb %h2,%%fs:0(%1,%0)\n\t" \
	"decw %w0\n\t" \
	"movb %b2,%%fs:0(%1,%0)" \
	: "=r" (ptr) \
	: "r" (base), "q" (val), "0" (ptr))

#define popb(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb %%fs:0(%1,%0),%b2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define popw(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb %%fs:0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb %%fs:0(%1,%0),%h2\n\t" \
	"incw %w0" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base), "2" (0)); \
__res; })

#define popl(base, ptr) \
({ unsigned long __res; \
__asm__ __volatile__( \
	"movb %%fs:0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb %%fs:0(%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2\n\t" \
	"movb %%fs:0(%1,%0),%b2\n\t" \
	"incw %w0\n\t" \
	"movb %%fs:0(%1,%0),%h2\n\t" \
	"incw %w0\n\t" \
	"rorl $16,%2" \
	: "=r" (ptr), "=r" (base), "=q" (__res) \
	: "0" (ptr), "1" (base)); \
__res; })

#if defined(_LOADABLE_VM86_) && defined(USE_VM86PLUS) && defined(USE_VM86_STACKVERIFY)
  /* NOTE: You may think that verifying CS:IP also may be needed,
   *       but we rely on the fact that the faulting instruction
   *       has been completely prefetched by the CPU, so it is
   *       save to access it.
   */
  #define STACKVERIFY(x) {\
     unsigned long off1=sp, off2=(sp+(x)) & 0xffff; \
     if (off1 > off2) { off1=off2; off2=sp; } \
     if ((off1 ^ off2) & 0x8000) {  /* wrap case */ \
       if (off1 && off2) { \
         if (verify_area(VERIFY_WRITE,ssp,off1)) return_to_32bit(regs, VM86_STACKVERIFY); \
         off1=off2; \
       } \
       else if (!off1) off1=off2; \
       off2=0x10000; \
     } \
     if (verify_area(VERIFY_WRITE,ssp+off1,off2-off1)) return_to_32bit(regs, VM86_STACKVERIFY); \
   }
#else
  #define STACKVERIFY(x)
#endif

static inline void do_int(struct vm86_regs *regs, int i, unsigned char * ssp, unsigned long sp)
{
#if 1 /* KERNEL_VERSION >= 1003002 (BUGFIX) */                                 
	unsigned short *intr_ptr, seg;
#else
	unsigned short seg = get_fs_word((void *) ((i<<2)+2));
#endif

#if KERNEL_VERSION >= 1001075
  #if 1 /* KERNEL_VERSION >= 1003002 (BUGFIX) */                                 
	if (regs->cs == BIOSSEG)
		goto cannot_handle;
	if (is_revectored(i, &current->tss.vm86_info->int_revectored))
		goto cannot_handle;
	if (i==0x21 && is_revectored(AH(regs),&current->tss.vm86_info->int21_revectored))
		goto cannot_handle;
  #else
	if (seg == BIOSSEG || regs->cs == BIOSSEG ||
	    is_revectored(i, &current->tss.vm86_info->int_revectored))
		return_to_32bit(regs, VM86_INTx + (i << 8));
	if (i==0x21 && is_revectored(AH(regs),&current->tss.vm86_info->int21_revectored))
		return_to_32bit(regs, VM86_INTx + (i << 8));
  #endif
#else
  #if 1 /* KERNEL_VERSION >= 1003002 (BUGFIX) */                                 
	if (regs->cs == BIOSSEG)
		goto cannot_handle;
	if (is_revectored(i, &current->vm86_info->int_revectored))
		goto cannot_handle;
	if (i==0x21 && is_revectored(AH(regs),&current->vm86_info->int21_revectored))
		goto cannot_handle;
  #else
	if (seg == BIOSSEG || regs->cs == BIOSSEG ||
	    is_revectored(i, &current->vm86_info->int_revectored))
		return_to_32bit(regs, VM86_INTx + (i << 8));
	if (i==0x21 && is_revectored(AH(regs),&current->vm86_info->int21_revectored))
		return_to_32bit(regs, VM86_INTx + (i << 8));
  #endif
#endif
#if 1 /* KERNEL_VERSION >= 1003002 (BUGFIX) */                                 
	intr_ptr = (unsigned short *) (i << 2);
	if (verify_area(VERIFY_READ, intr_ptr, 4) < 0)
		goto cannot_handle;
	seg = get_fs_word(intr_ptr+1);
	if (seg == BIOSSEG)
		goto cannot_handle;
#endif
	STACKVERIFY(-6)
	pushw(ssp, sp, get_vflags(regs));
	pushw(ssp, sp, regs->cs);
	pushw(ssp, sp, IP(regs));
	regs->cs = seg;
	SP(regs) -= 6;
#if 1 /* KERNEL_VERSION >= 1003002 (BUGFIX) */                                 
	IP(regs) = get_fs_word(intr_ptr+0);
#else
	IP(regs) = get_fs_word((void *) (i<<2));
#endif
	clear_TF(regs);
	clear_IF(regs);
	return;

#if 1 /* KERNEL_VERSION >= 1003002 (BUGFIX) */                                 
cannot_handle:
	return_to_32bit(regs, VM86_INTx + (i << 8));
#endif
}

void handle_vm86_fault(struct vm86_regs * regs, long error_code)
{
	unsigned char *csp, *ssp;
	unsigned long ip, sp;

#if defined(_LOADABLE_VM86_) && defined(USE_VM86PLUS)
  #if KERNEL_VERSION >= 1001075
    struct vm86plus_struct *vmp=(void *)(current->tss.vm86_info);
  #else
    struct vm86plus_struct *vmp=(void *)(current->vm86_info);
  #endif
  int is_vm86plus=( get_fs_long(&vmp->vm86plus.vm86plus_magic) == VM86PLUS_MAGIC );
  struct vm86plus_info_struct vmpi;
  #define VM86_FAULT_RETURN   if (is_vm86plus) break; else return
  #ifdef USE_MHPDBG
    #define CHECK_IF_IN_TRAP    if (vmpi.mhpdbg_active && vmpi.mhpdbg_TFpendig) pushw(ssp,sp,popw(ssp,sp) | TF_MASK);
  #else
    #define CHECK_IF_IN_TRAP
  #endif
#else
  #define VM86_FAULT_RETURN   return
  #define CHECK_IF_IN_TRAP
#endif

#if defined(_LOADABLE_VM86_) && defined(_VM86_STATISTICS_)
	extern int vm86_fault_count;
	extern int vm86_count_cli,vm86_count_sti;
	vm86_fault_count++;
#endif


#if defined(_LOADABLE_VM86_) && defined(USE_VM86PLUS)
	if (is_vm86plus) {
  #if 0
	  /* We will copy the whole tail later,
	     if more stuff is put into vm86plus */
	  memcpy_fromfs(&vmpi.dosemuver, &vmp->vm86plus.dosemuver, sizeof(struct vm86plus_info_struct)-sizeof(long));
  #else
	  /* For now it's enough to get just the flag bit fields */
	  *(((long *)&vmpi.dosemuver)+1) = get_fs_long(((long *)&vmp->vm86plus.dosemuver)+1);
  #endif
	}
	else *(((long *)&vmpi.dosemuver)+1) = 0;
#endif
	csp = (unsigned char *) (regs->cs << 4);
	ssp = (unsigned char *) (regs->ss << 4);
	sp = SP(regs);
	ip = IP(regs);

	switch (popb(csp, ip)) {

	/* operand size override */
	case 0x66:
		switch (popb(csp, ip)) {

		/* pushfd */
		case 0x9c:
			STACKVERIFY(-4)
			SP(regs) -= 4;
			IP(regs) += 2;
			pushl(ssp, sp, get_vflags(regs));
			VM86_FAULT_RETURN;

		/* popfd */
		case 0x9d:
			STACKVERIFY(+4)
			SP(regs) += 4;
			IP(regs) += 2;
			CHECK_IF_IN_TRAP
			set_vflags_long(popl(ssp, sp), regs);
			VM86_FAULT_RETURN;

/* #if KERNEL_VERSION >= 1002008  
   The below is a bugfix (Dong Liu), we want it even for older kernels */
		/* iretd */
		case 0xcf:
			STACKVERIFY(+12)
			SP(regs) += 12;
			IP(regs) = (unsigned short)popl(ssp, sp);
			regs->cs = (unsigned short)popl(ssp, sp);
			CHECK_IF_IN_TRAP
			set_vflags_long(popl(ssp, sp), regs);
			VM86_FAULT_RETURN;
/* #endif */
#if 1 /* need this to avoid a fallthrough */
		default:
			return_to_32bit(regs, VM86_UNKNOWN);
		}
		break;
#else
		}
#endif

	/* pushf */
	case 0x9c:
		STACKVERIFY(-2)
		SP(regs) -= 2;
		IP(regs)++;
		pushw(ssp, sp, get_vflags(regs));
		VM86_FAULT_RETURN;

	/* popf */
	case 0x9d:
		STACKVERIFY(+2)
		SP(regs) += 2;
		IP(regs)++;
		CHECK_IF_IN_TRAP
		set_vflags_short(popw(ssp, sp), regs);
		VM86_FAULT_RETURN;

#if KERNEL_VERSION < 1001089
#if 0
	/* int 3 */
	case 0xcc:
		IP(regs)++;
		do_int(regs, 3, ssp, sp);
		VM86_FAULT_RETURN;
#endif
#endif
	/* int xx */
	case 0xcd: {
	        int intno=popb(csp, ip);
		IP(regs) += 2;
#if defined(_LOADABLE_VM86_) && defined(USE_MHPDBG)
		if (is_vm86plus) {
		  if (vmpi.mhpdbg_active) {
		    if ( (1 << (intno &7)) & get_fs_byte((char *)&vmp->vm86plus.mhpdbg_intxxtab[intno >> 3]) )
		         return_to_32bit(regs, VM86_INTx + (intno << 8));
		  }
		}
#endif
		do_int(regs, intno, ssp, sp);
		return;
	}
	/* iret */
	case 0xcf:
		STACKVERIFY(+6)
		SP(regs) += 6;
		IP(regs) = popw(ssp, sp);
		regs->cs = popw(ssp, sp);
		CHECK_IF_IN_TRAP
		set_vflags_short(popw(ssp, sp), regs);
		VM86_FAULT_RETURN;

	/* cli */
	case 0xfa:
#ifdef _VM86_STATISTICS_
		vm86_count_cli++;
#endif
		IP(regs)++;
		clear_IF(regs);
		VM86_FAULT_RETURN;

	/* sti */
	/*
	 * Damn. This is incorrect: the 'sti' instruction should actually
	 * enable interrupts after the /next/ instruction. Not good.
	 *
	 * Probably needs some horsing around with the TF flag. Aiee..
	 */
	case 0xfb:
#ifdef _VM86_STATISTICS_
		vm86_count_sti++;
#endif          
		IP(regs)++;
		set_IF(regs);
		VM86_FAULT_RETURN;

	default:
		return_to_32bit(regs, VM86_UNKNOWN);
	}

#if defined(_LOADABLE_VM86_) && defined(USE_VM86PLUS)
	if (is_vm86plus) {
	  if (vmpi.force_return_for_pic  && (VEFLAGS & IF_MASK))
                         return_to_32bit(regs, VM86_PICRETURN);
 	}
#endif
}

void handle_vm86_trap(struct vm86_regs * regs, long error_code, int trapno)
{
#if 0
	if (trapno == 3) {
		if (current->flags & PF_PTRACED)
			current->blocked &= ~(1 << (SIGTRAP-1));
		send_sig(SIGTRAP, current, 1);
		current->tss.trap_no = 1;
		current->tss.error_code = error_code;
		return;
	}
#endif
#if defined(_LOADABLE_VM86_) && defined(_VM86_STATISTICS_)
        {
          extern int vm86_trap_count[8];
          vm86_trap_count[trapno & 7]++;
        }
#endif
#if defined(_LOADABLE_VM86_) && defined(USE_MHPDBG)
	if ( (trapno==3) || (trapno==1) ) return_to_32bit(regs, VM86_TRAP + (trapno << 8));
#endif
	do_int(regs, trapno, (unsigned char *) (regs->ss << 4), SP(regs));
	return;
}
