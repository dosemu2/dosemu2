/*
 * User Syscall manager for loadable modules
 *
 * (C) 1994,1995 under GPL:  Hans Lermen <lermen@elserv.ffm.fgan.de>
 *  
 * Should be loaded via insmod as first of all loadable modules and must stay loaded
 * as long as there is any application and/or loadable module using it.
 *
 * Syscalls must be registered by other loadable modules before use as follows:
 *
 *   register_syscall(nr_syscall, routine, name);
 *      nr_syscall  The number of the syscall (greater 140)
 *                  or ZERO, if allocating a number dynamically
 *      routine     Pointer to the new user syscall routine, must be conform
 *                  to the kernel interface, see below resolve_syscall routine
 *                  as an example.
 *      name        The identification string for this syscall, e.g the name of
 *                  the routine.
 *   It returns the desired or the allocated syscall number on success or
 *   or < 0 on error. As the kernel syscall interface (int 0x80) refuses numbers
 *   out of range, this is a save way, even if usinging an unallocated number.
 *
 * Syscall must be unregistered by the module's cleanup routine via:
 *
 *   unregister_syscall(nr_syscall)
 *   where nr_syscall is the number delivered by register_syscall.
 *
 * At userlevel a program first must include syscallmgr.h 
 * and get the allocated nr_syscall via:
 *
 *   resolve_syscall(char *name, struct syscall_sym_entry *buf);
 *      name         identification string for the syscall
 *      buf          =NULL for normal purpose,
 *                   else address of a buffer to get the list of all
 *                   allocated user syscalls.
 *   It returns the syscall number or <0 on error (see above).
 *   To get the required size of "buf" you may do:
 *       size = resolve_syscall(0,0);
 *   to get the number of entries you may do:
 *       count=resolve_syscall(0,buf)
 *
 * Example (user level):
 *
 *   #include <linux/unistd.h>
 *   #include "syscallmgr.h"
 *   
 *   int __NR_my_private_syscall=-1;
 *   static inline _syscall1(int,my_private_syscall, char *,hellotext);
 *
 *   main() {
 *     struct syscall_sym_entry *p;
 *     int size,count,i;
 *     __NR_my_private_syscall = resolve_syscall("my_private_syscall",0);
 *     my_private_syscall("Hello Linux kernel");
 *     my_private_syscall("I'm not invited, but I'm here");
 *     size=resolve_syscall(0,0);
 *     p=malloc(size);
 *     count=resolve_syscall(0,p);
 *     for (i=0; i <count; i++) printf("nr=%d %s\n",p[i].nr, p[i].name);
 *   }
 *
 *
 * NOTE:
 * 1. Naming convention with <unistd.h> is    __NR_routine_name, 
 *    but naming convention in <syscall.h> is SYS_routine_name !
 *    You can use all standard macros/function along with your syscall number.
 * 2. Syscallmgr can NOT be used to replace existing kernel resident syscalls.
 *    (though you may do it manually).
 */

#include "kversion.h"

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tty.h>
#include <linux/signal.h>
#include <linux/errno.h>
#include <linux/ptrace.h>
#include <linux/malloc.h>

#include <linux/unistd.h>

#if KERNEL_VERSION < 1001067
#include <linux/segment.h>
#endif

#include <linux/sys.h>
#include <linux/mm.h>

#include <asm/io.h>
#include <asm/segment.h>
#include <asm/system.h>
#include <asm/string.h>
#include <asm/irq.h>

#if KERNEL_VERSION >= 1003038
  /* need this to force  kernel_version[]=UTS_RELEASE,
   * starting with 1.3.38 defines are moved into <linux/module.h> */
  #undef __NO_VERSION__ 
  #define MODULE
#endif
#include <linux/module.h>
#include "syscallmgr.h"

#if 0
  #define WANT_DEBUG
  #define ___static
#else
  #define ___static static
#endif

#if KERNEL_VERSION < 1003038
/*
 * NOTE:
 *   To install the module, we must include the kernel identification string.
 *   (so, don't panic if you get a GCC warning "_kernel_version not used" )
 */
#if KERNEL_VERSION < 1001072
#include "linux/tools/version.h"
#else
#include "linux/version.h"
#endif
static char kernel_version[] = UTS_RELEASE;
#endif

#define ID_STRING "Syscall_Modul"

extern int printk( const char* fmt, ...);
extern void *sys_call_table[];
  

int first_free_NR_syscall=NR_syscalls-1;

struct syscall_sym {
  struct syscall_sym *next;
  struct syscall_sym_entry e;
};  

static struct syscall_sym *syscall_list=0;

typedef int walk_func(struct syscall_sym *, void *);

static int search_name(struct syscall_sym *p, char *name)
{
  return (!strcmp(name,p->e.name));
}

#if 0
static int search_nr(struct syscall_sym *p, int nr)
{
  return (p->e.nr == nr);
}
#endif

static int count_items(struct syscall_sym *p, int *count)
{
  (*count)++;
  return 0;
}

static int put_entries(struct syscall_sym *p, struct syscall_sym_entry **pp)
{
  memcpy_tofs(*pp, &p->e, sizeof(struct syscall_sym_entry));
  (*pp)++;
  return 0;
}

static struct syscall_sym *walk_syms(void *param, walk_func *func)
{
   struct syscall_sym *p=syscall_list;
   while (p) {
     if ( (*func)(p,param) ) return p;
     p=p->next;
   }
   return 0;
}

static struct syscall_sym *findsym(char *name)
{
   return walk_syms(name,(walk_func *)search_name);
}

static int find_free_syscall_start()
{
  int i,flags;

  save_flags(flags);
  cli();
  for (i=NR_syscalls-1; !sys_call_table[i]; i--);
  restore_flags(flags);
  return i+1; 

}

static int do_register(int nr_syscall, void *routine, char *name)
{
  struct syscall_sym *p=kmalloc(sizeof(struct syscall_sym),GFP_KERNEL);
  if ( name && name[0] && !findsym(name) ) {
    int flags;
    save_flags(flags);
    cli();
    if (!sys_call_table[nr_syscall]) {
      strncpy(p->e.name,name,MAX_NR_NAME_SIZE-1);
      p->e.name[MAX_NR_NAME_SIZE-1]=0;
      p->e.nr=nr_syscall;
      p->next=syscall_list;
      syscall_list=p;
      sys_call_table[nr_syscall]=routine;
      restore_flags(flags);
      if (nr_syscall != __NR_resolve_syscall) MOD_INC_USE_COUNT;
      return nr_syscall;
    }
    restore_flags(flags);
  }
  kfree_s(p,sizeof(struct syscall_sym));
  return -1;
}

/* Register a given syscall number (nr_syscall !=0)
 * or allocated a free number in sys_call_table (nr_syscall =0).
 * Returns the syscall number or -1 on error
 */

int register_syscall(int nr_syscall, void *routine, char *name)
{
  int i;

  if (!suser()) return -EPERM; /* normally superflous, 
                                because insmod requires suid, 
                                but.., be aware for intruder hackers ! */
  if (nr_syscall) return do_register(nr_syscall,routine,name);
  for (i=first_free_NR_syscall; i<NR_syscalls; i++) {
    if (!sys_call_table[i]) return do_register(i,routine,name);
  }
  return -1; 
}

static myself=0; /* inhibit unregistration of _NR_resolve_syscall */

void unregister_syscall(int nr_syscall)
{
   struct syscall_sym *p=syscall_list, *p_=0;
   int flags;
   if (!myself && (nr_syscall == __NR_resolve_syscall)) return;
   save_flags(flags);
   cli();
   while (p) {
     if (p->e.nr ==  nr_syscall) {
       sys_call_table[nr_syscall]=0;
       if (p_) p_->next = p->next;
       else syscall_list=p->next;
       restore_flags(flags);
       kfree_s(p,sizeof(struct syscall_sym));
       if (nr_syscall != __NR_resolve_syscall) MOD_DEC_USE_COUNT;
       return;
     }
     p_=p;
     p=p->next;
   }
   restore_flags(flags);
}


static asmlinkage int resolve_syscall(char *syscallname, struct syscall_sym_entry *buf)
{
   char name[MAX_NR_NAME_SIZE];
   struct syscall_sym *p;
   int i;
   
   if (buf || !syscallname) {
     int count=0, size;
     walk_syms(&count,(walk_func *)count_items);
     size = count * sizeof(struct syscall_sym_entry );
     if (!buf) return size;
     if ((i=verify_area(VERIFY_WRITE, buf, size)) != 0) return i;
     walk_syms(&buf,(walk_func *)put_entries);
     return count;
   }
   else {
     if (!syscallname) return -1;
     strn0cpy_fromfs(name,syscallname,sizeof(name) -1);
     name[sizeof(name) -1]=0;
     if (!(p=findsym(name))) return -1;
     return p->e.nr;
   }
}


int init_module( void) {
  int NR_resolve_syscall=-1;
  kernel_version[0] = kernel_version[0];
  first_free_NR_syscall=find_free_syscall_start();
  NR_resolve_syscall=
                register_syscall(__NR_resolve_syscall,
                   resolve_syscall,"resolve_syscall");
  if (NR_resolve_syscall>0) {
    printk(ID_STRING ", init_module called, NR_resolve_syscall=%d\n",
        NR_resolve_syscall);
    return 0;
  }
  else {
    printk(ID_STRING ", init_module failed\n");
    return -1;
  }
}

void cleanup_module( void) {
  if (MOD_IN_USE) printk(ID_STRING ": device busy, remove delayed\n");
  else printk(ID_STRING ": cleanup modul called\n");
  myself=1;
  unregister_syscall(__NR_resolve_syscall);
}
