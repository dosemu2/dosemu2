#ifndef _LINUX_VM86PLUS_H
#define _LINUX_VM86PLUS_H

#include <linux/vm86.h>

#define VM86PLUS_MAGIC 0x4d564544 /* = "DEVM" */

struct vm86plus_struct {
	struct vm86_regs regs;
	unsigned long flags;
	unsigned long screen_bitmap;
	unsigned long cpu_type;
	struct revectored_struct int_revectored;
	struct revectored_struct int21_revectored;
	long vm86plus_magic;
	struct timeval tv;
};

#endif
