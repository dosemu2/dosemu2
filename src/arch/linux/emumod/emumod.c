/* file: emumod.c
 * emumodule interface and redirection stuff
 *
 * (C) 1994 under GPL, Hans Lermen <lermen@elserv.ffm.fgan.de>
 */
  

#include "kversion.h"
#if KERNEL_VERSION < 2000000
  #error "sorry, but we cleaned up history and do nolonger support kernels < 2.0.0"
#endif

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/mm.h>

#include <asm/segment.h>
#include <asm/pgtable.h>
#include <asm/io.h>

#define  _EMUMOD_itself
#include "emumod.h"


/* need this to force  kernel_version[]=UTS_RELEASE,
 * starting with 1.3.38 defines are moved into <linux/module.h> */
#undef __NO_VERSION__ 
#define MODULE
#include <linux/module.h>

struct redirect_db {
  void *resident;
  void *transient; 
  char is_pointer;
  char savearea[5];
};

extern do_divide_error(), _TRANSIENT_do_divide_error();
extern do_int3(), _TRANSIENT_do_int3();
extern do_overflow(), _TRANSIENT_do_overflow();
extern do_bounds(), _TRANSIENT_do_bounds();
extern do_device_not_available(), _TRANSIENT_do_device_not_available();
extern do_debug(), _TRANSIENT_do_debug();
extern save_v86_state(), _TRANSIENT_save_v86_state();
extern sys_vm86(), _TRANSIENT_sys_vm86();
extern sys_sigreturn(), _TRANSIENT_sys_sigreturn();
extern sys_modify_ldt(), _TRANSIENT_sys_modify_ldt();
extern /*handle_vm86_fault(),*/  _TRANSIENT_handle_vm86_fault();

static struct redirect_db redirect_list[] = {
  { save_v86_state,
    _TRANSIENT_save_v86_state },
  { handle_vm86_fault, 
    _TRANSIENT_handle_vm86_fault },
  { do_divide_error, 
    _TRANSIENT_do_divide_error },
  { do_int3, 
    _TRANSIENT_do_int3 },
  { do_overflow, 
    _TRANSIENT_do_overflow },
  { do_bounds, 
    _TRANSIENT_do_bounds },
  { do_device_not_available, 
    _TRANSIENT_do_device_not_available },
  { do_debug, 
    _TRANSIENT_do_debug },
  { sys_vm86,
    _TRANSIENT_sys_vm86 }
  , { sys_sigreturn,
    _TRANSIENT_sys_sigreturn }
  , { sys_modify_ldt,
    _TRANSIENT_sys_modify_ldt }
};



struct jmpop {
  unsigned char opcode;
  int displacement __attribute__ ((packed));
};


static void redirect_resident_routine(void *resident_addr, void *transient_addr, char *savearea)
{
  enum { JMP_OPCODE=0x0e9 };
  struct jmpop  *raddr = resident_addr;
  int flags, displ;
  
  displ=((int)transient_addr) - ((int)resident_addr+sizeof(struct jmpop));
  save_flags(flags);
  cli();
  memcpy(savearea,resident_addr,sizeof(struct jmpop));
  raddr->opcode = JMP_OPCODE;
  raddr->displacement = displ;  
  restore_flags(flags);
}

static void restore_redirect(void *resident_addr,  char *savearea)
{
  int flags;
  
  save_flags(flags);
  cli();
  memcpy(resident_addr,savearea,5);
  restore_flags(flags);
}


static void redirect_all() {
  int num=sizeof(redirect_list) / sizeof(struct redirect_db);
  struct redirect_db *p;
  int flags,i;
  
  save_flags(flags);
  for (i=0, p=redirect_list; i<num; i++, p++) {
    if (p->is_pointer) {
      *((long *)(p->savearea)) = *((long *)(p->resident));
      *((long *)(p->resident)) = (long)p->transient;
    }
    else redirect_resident_routine(p->resident, p->transient, p->savearea);
  }
  restore_flags(flags);
}

static void restore_redirect_all() {
  int num=sizeof(redirect_list) / sizeof(struct redirect_db);
  struct redirect_db *p;
  int flags,i;
  
  save_flags(flags);
  for (i=0, p=&redirect_list[num-1]; i<num; i++, p--) {
    if (p->is_pointer) {
      *((long *)(p->resident)) = *((long *)(p->savearea));
    }
    else restore_redirect(p->resident, p->savearea);
  }
  restore_flags(flags);
}

int init_module( void) {
  kernel_version[0] = kernel_version[0];
  printk(ID_STRING ", init_module called \n");
  redirect_all();
  mod_use_count_ &= ~MOD_AUTOCLEAN;
  return 0;
}

void cleanup_module( void) {
  int i;
  extern void force_free_all_vm86irq(void);
  printk(ID_STRING ": cleanup modul called\n");
  force_free_all_vm86irq();
  restore_redirect_all();
  
  /* wait arround to be sure no process is still in the module */
  for (i = 0; i < 100; i++) schedule();
  /* now the memory can be freed */
}
