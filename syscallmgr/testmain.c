#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <strings.h>
#include <linux/unistd.h>
#include <linux/module.h>

#include "syscallmgr.h"


int __NR_testsys=-1;
static inline _syscall2(int,testsys, int, mode, void *,params)


int main()
{
  __NR_testsys=resolve_syscall("testsys",0);
  testsys(0,"hello");
  testsys(1,0);
  return 0;
}
