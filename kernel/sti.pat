diff -ru linux.1.1.44/config.in linux/config.in
--- linux.1.1.44/config.in	Fri Aug 12 18:55:00 1994
+++ linux/config.in	Sun Aug 14 12:50:49 1994
@@ -12,6 +12,7 @@
 bool 'Limit memory to low 16MB' CONFIG_MAX_16M n
 bool 'System V IPC' CONFIG_SYSVIPC y
 bool 'Use -m486 flag for 486-specific optimizations' CONFIG_M486 y
+bool 'Correct but slow STI handling (virtual 8086 mode)' CONFIG_CORRECT_STI n
 
 if [ "$CONFIG_NET" = "y" ]; then
 comment 'Networking options'
diff -ru linux.1.1.44/include/linux/vm86.h linux/include/linux/vm86.h
--- linux.1.1.44/include/linux/vm86.h	Mon May  2 08:31:55 1994
+++ linux/include/linux/vm86.h	Sat Aug 13 22:35:16 1994
@@ -21,6 +21,7 @@
 #define VIF_MASK	0x00080000	/* virtual interrupt flag */
 #define VIP_MASK	0x00100000	/* virtual interrupt pending */
 #define ID_MASK		0x00200000
+#define STIP_MASK	0x80000000	/* STI pending */
 
 #define BIOSSEG		0x0f000
 
@@ -101,8 +102,7 @@
 
 #ifdef __KERNEL__
 
-void handle_vm86_fault(struct vm86_regs *, long);
-void handle_vm86_debug(struct vm86_regs *, long);
+void handle_vm86_fault(struct vm86_regs *, long, int);
 
 #endif
 
diff -ru linux.1.1.44/kernel/traps.c linux/kernel/traps.c
--- linux.1.1.44/kernel/traps.c	Thu Aug 11 16:06:32 1994
+++ linux/kernel/traps.c	Fri Aug 12 19:00:39 1994
@@ -39,6 +39,21 @@
 	die_if_kernel(str,regs,error_code); \
 }
 
+#define DO_VM86_ERROR(trapnr, signr, str, name, tsk) \
+asmlinkage void do_##name(struct pt_regs * regs, long error_code) \
+{ \
+	if (regs->eflags & VM_MASK) { \
+		handle_vm86_fault((struct vm86_regs *) regs, error_code, trapnr); \
+		return; \
+	} \
+	tsk->tss.error_code = error_code; \
+	tsk->tss.trap_no = trapnr; \
+	if (signr == SIGTRAP && current->flags & PF_PTRACED) \
+		current->blocked &= ~(1 << (SIGTRAP-1)); \
+	send_sig(signr, tsk, 1); \
+	die_if_kernel(str,regs,error_code); \
+}
+
 #define get_seg_byte(seg,addr) ({ \
 register unsigned char __res; \
 __asm__("push %%fs;mov %%ax,%%fs;movb %%fs:%2,%%al;pop %%fs" \
@@ -114,12 +129,12 @@
 	do_exit(SIGSEGV);
 }
 
-DO_ERROR( 0, SIGFPE,  "divide error", divide_error, current)
-DO_ERROR( 3, SIGTRAP, "int3", int3, current)
-DO_ERROR( 4, SIGSEGV, "overflow", overflow, current)
-DO_ERROR( 5, SIGSEGV, "bounds", bounds, current)
+DO_VM86_ERROR( 0, SIGFPE,  "divide error", divide_error, current)
+DO_VM86_ERROR( 3, SIGTRAP, "int3", int3, current)
+DO_VM86_ERROR( 4, SIGSEGV, "overflow", overflow, current)
+DO_VM86_ERROR( 5, SIGSEGV, "bounds", bounds, current)
 DO_ERROR( 6, SIGILL,  "invalid operand", invalid_op, current)
-DO_ERROR( 7, SIGSEGV, "device not available", device_not_available, current)
+DO_VM86_ERROR( 7, SIGSEGV, "device not available", device_not_available, current)
 DO_ERROR( 8, SIGSEGV, "double fault", double_fault, current)
 DO_ERROR( 9, SIGFPE,  "coprocessor segment overrun", coprocessor_segment_overrun, last_task_used_math)
 DO_ERROR(10, SIGSEGV, "invalid TSS", invalid_TSS, current)
@@ -133,7 +148,7 @@
 	int signr = SIGSEGV;
 
 	if (regs->eflags & VM_MASK) {
-		handle_vm86_fault((struct vm86_regs *) regs, error_code);
+		handle_vm86_fault((struct vm86_regs *) regs, error_code, 13);
 		return;
 	}
 	die_if_kernel("general protection",regs,error_code);
@@ -158,7 +173,7 @@
 asmlinkage void do_debug(struct pt_regs * regs, long error_code)
 {
 	if (regs->eflags & VM_MASK) {
-		handle_vm86_debug((struct vm86_regs *) regs, error_code);
+		handle_vm86_fault((struct vm86_regs *) regs, error_code, 1);
 		return;
 	}
 	if (current->flags & PF_PTRACED)
diff -ru linux.1.1.44/kernel/vm86.c linux/kernel/vm86.c
--- linux.1.1.44/kernel/vm86.c	Mon May 23 00:45:07 1994
+++ linux/kernel/vm86.c	Sun Aug 14 12:22:49 1994
@@ -3,6 +3,7 @@
  *
  *  Copyright (C) 1994  Linus Torvalds
  */
+#include <linux/config.h>
 #include <linux/errno.h>
 #include <linux/sched.h>
 #include <linux/kernel.h>
@@ -21,11 +22,8 @@
  *   after a "mov ss,xx" to make stack handling atomic even without
  *   the 'lss' instruction. We can't guarantee this in v86 mode,
  *   as the next instruction might result in a page fault or similar.
- * - a real x86 will have interrupts disabled for one instruction
- *   past the 'sti' that enables them. We don't bother with all the
- *   details yet..
  *
- * Hopefully these problems do not actually matter for anything.
+ * Hopefully this problem do not actually matter for anything.
  */
 
 /*
@@ -56,7 +54,14 @@
 		printk("no vm86_info: BAD\n");
 		do_exit(SIGSEGV);
 	}
+#ifdef CONFIG_CORRECT_STI
+	if (VEFLAGS & STIP_MASK)
+		set_flags(regs->eflags, VEFLAGS, STIP_MASK|VIP_MASK|TF_MASK|VIF_MASK | current->v86mask);
+	else
+		set_flags(regs->eflags, VEFLAGS, STIP_MASK|VIP_MASK|VIF_MASK | current->v86mask);
+#else
 	set_flags(regs->eflags, VEFLAGS, VIF_MASK | current->v86mask);
+#endif
 	memcpy_tofs(&current->vm86_info->regs,regs,sizeof(*regs));
 	put_fs_long(current->screen_bitmap,&current->vm86_info->screen_bitmap);
 	tmp = current->tss.esp0;
@@ -114,6 +119,10 @@
 	info.regs.eflags &= SAFE_MASK;
 	info.regs.eflags |= pt_regs->eflags & ~SAFE_MASK;
 	info.regs.eflags |= VM_MASK;
+#ifdef CONFIG_CORRECT_STI
+	if ((VEFLAGS & (STIP_MASK|VIP_MASK)) == (STIP_MASK|VIP_MASK))
+		info.regs.eflags |= TF_MASK; /* single step to set IF next instruction */
+#endif
 
 	switch (info.cpu_type) {
 		case CPU_286:
@@ -162,8 +171,10 @@
 static inline void set_IF(struct vm86_regs * regs)
 {
 	VEFLAGS |= VIF_MASK;
+#ifndef CONFIG_CORRECT_STI
 	if (VEFLAGS & VIP_MASK)
 		return_to_32bit(regs, VM86_STI);
+#endif
 }
 
 static inline void clear_IF(struct vm86_regs * regs)
@@ -302,20 +313,7 @@
 	return;
 }
 
-void handle_vm86_debug(struct vm86_regs * regs, long error_code)
-{
-#if 0
-	do_int(regs, 1, (unsigned char *) (regs->ss << 4), SP(regs));
-#else
-	if (current->flags & PF_PTRACED)
-		current->blocked &= ~(1 << (SIGTRAP-1));
-	send_sig(SIGTRAP, current, 1);
-	current->tss.trap_no = 1;
-	current->tss.error_code = error_code;
-#endif
-}
-
-void handle_vm86_fault(struct vm86_regs * regs, long error_code)
+static inline void vm86_gp_fault(struct vm86_regs * regs, long error_code)
 {
 	unsigned char *csp, *ssp;
 	unsigned long ip, sp;
@@ -360,12 +358,6 @@
 		set_vflags_short(popw(ssp, sp), regs);
 		return;
 
-	/* int 3 */
-	case 0xcc:
-		IP(regs)++;
-		do_int(regs, 3, ssp, sp);
-		return;
-
 	/* int xx */
 	case 0xcd:
 		IP(regs) += 2;
@@ -387,18 +379,61 @@
 		return;
 
 	/* sti */
-	/*
-	 * Damn. This is incorrect: the 'sti' instruction should actually
-	 * enable interrupts after the /next/ instruction. Not good.
-	 *
-	 * Probably needs some horsing around with the TF flag. Aiee..
-	 */
 	case 0xfb:
 		IP(regs)++;
-		set_IF(regs);
+		if (!(VEFLAGS & VIF_MASK)) {
+#ifdef CONFIG_CORRECT_STI
+			if (VEFLAGS & VIP_MASK) {
+				VEFLAGS |= STIP_MASK;
+
+				set_flags(VEFLAGS, regs->eflags, TF_MASK);
+
+				/* single step to set IF next instruction */
+				regs->eflags |= TF_MASK;
+
+			}
+#else
+			set_IF(regs);
+#endif
+		}
 		return;
 
 	default:
 		return_to_32bit(regs, VM86_UNKNOWN);
+	}
+}
+
+void handle_vm86_fault(struct vm86_regs * regs, long error_code, int trapno)
+{
+#ifdef CONFIG_CORRECT_STI
+	if (VEFLAGS & STIP_MASK) {
+		VEFLAGS &= ~STIP_MASK;
+
+		VEFLAGS |= VIF_MASK;
+		if (!(VEFLAGS & TF_MASK))
+			clear_TF(regs);
+	}
+#endif
+	switch (trapno) {
+#ifdef CONFIG_CORRECT_STI
+	case 1:
+		if (regs->eflags & TF_MASK)
+			/* Trap Flag was set - handle it as single step */
+			do_int(regs, trapno, (unsigned char *) (regs->ss << 4), SP(regs));
+		else
+			return_to_32bit(regs, VM86_STI);
+		return;
+#endif
+	case 13:
+		vm86_gp_fault(regs, error_code);
+#ifdef CONFIG_CORRECT_STI
+		if ((VEFLAGS & (VIP_MASK|VIF_MASK)) == (VIP_MASK|VIF_MASK))
+			return_to_32bit(regs, VM86_STI);
+#endif
+		return;
+
+	default:
+		do_int(regs, trapno, (unsigned char *) (regs->ss << 4), SP(regs));
+		return;
 	}
 }
