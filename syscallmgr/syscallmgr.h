/*
 * User Syscall manager for loadable modules
 *
 * (C) 1994 under GPL:  Hans Lermen <lermen@elserv.ffm.fgan.de>
 *  
 */
 
#ifndef _SYSCALLMGR_H_
#define _SYSCALLMGR_H_


#include <linux/sys.h>

#ifdef SYS_open /* why not, any thing  like SYS_xxxxx */
#error "syscallmgr: You have included <system.h>, nameing is incompatible to <asm/unistd.h>"
#endif

#include <asm/unistd.h>

#define __NR_resolve_syscall (NR_syscalls-1)

#define MAX_NR_NAME_SIZE 56

struct syscall_sym_entry {
  int nr;
  char  name[MAX_NR_NAME_SIZE];
};


#ifndef __KERNEL__  /* user space */

/* extern inline int resolve_syscall(char *name, struct syscall_sym_entry *buf); */
extern inline _syscall2(int,resolve_syscall, char *,name, struct syscall_sym_entry *,buf)


#else               /* kernel space */

#if 0
  #include <stddef.h> /* need size_t to be defined ! */
#endif

extern inline char * strcpy_fromfs(char * dest,const char *src)
{
        /* modified version from linux/asm/string.h */
__asm__ __volatile__(
	"cld\n"
	"1:\t fs ; lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b"
	: /* no output */
	:"S" (src),"D" (dest):"si","di","ax","memory");
return dest;
}

extern inline char * strncpy_fromfs(char * dest,const char *src,size_t count)
{
        /* modified version from linux/asm/string.h */
__asm__ __volatile__(
	"cld\n"
	"1:\tdecl %2\n\t"
	"js 2f\n\t"
	"fs ; lodsb\n\t"
	"stosb\n\t"
	"testb %%al,%%al\n\t"
	"jne 1b\n\t"
	"rep\n\t"
	"stosb\n"
	"2:"
	: /* no output */
	:"S" (src),"D" (dest),"c" (count):"si","di","ax","cx","memory");
return dest;
}


extern inline char * strn0cpy_fromfs(char * dest,const char *src,size_t count)
  /* same as strncpy_fromfs, but does NOT fill up with ZEROs 
   * but allways writes derminating ZERO (cuts before end of string)
   */
{
__asm__ __volatile__( 
	"
	 cld
	 jecxz 3f # in case we get count = 0
	 jmp 2f   # to dec count (space for null char)
1:	 
	 fs ; lodsb
	 stosb
	 testb %%al,%%al
	 jz 3f
2:
	 loop 1b
	 stosb
3:
	" 
	: :"S" (src),"D" (dest),"c" (count):"si","di","ax","cx","memory");
return dest;
}

int register_syscall(int nr_syscall, void *routine, char *name);
void unregister_syscall(int nr_syscall);

#endif /* __KERNEL__ */

#endif /* _SYSCALLMGR_H_ */
