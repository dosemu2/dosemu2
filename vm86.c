#define __LIBRARY__
#include <linux/unistd.h>
#include <linux/vm86.h>
_syscall1(int, vm86, struct vm86_struct *, vm);
