#ifndef SYS_VM86_H
#define SYS_VM86_H

#include <syscall.h>

#ifdef __i386__
#ifdef SYS_vm86old
#define vm86(param) syscall(SYS_vm86old, param)
#else
/* vm86old installation check returns -1 when OK, so we use -2 here */
#define vm86(param) -2
#endif
#ifdef SYS_vm86
#define vm86_plus(function,param) syscall(SYS_vm86, function, param)
#define SIG 1
#else
#define vm86_plus(function,param) -1
#endif
typedef struct { int fd; int irq; } SillyG_t;
extern SillyG_t *SillyG;
#endif

#endif
