/* file: emumod.c
 * emumodule interface and redirection stuff
 *
 * (C) 1994 under GPL, Hans Lermen <lermen@elserv.ffm.fgan.de>
 */
  
#include "kversion.h"
#if 0
#define KERNEL_VERSION 1003038 /* last verified kernel version */
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

#define  _EMUMOD_itself
#include "emumod.h"


#if KERNEL_VERSION >= 1003038
  /* need this to force  kernel_version[]=UTS_RELEASE,
   * starting with 1.3.38 defines are moved into <linux/module.h> */
  #undef __NO_VERSION__ 
  #define MODULE
#endif
#include <linux/module.h>


#if KERNEL_VERSION < 1003038
/*
 * NOTE:
 *   To install the module, we must include the kernel identification string.
 *   (so, don't panic if you get a GCC warning "_kernel_version not used" )
 */
#if KERNEL_VERSION < 1001072
#include "../../linux/tools/version.h"
#else
#include "linux/version.h"
#endif
static char kernel_version[] = UTS_RELEASE;
#endif

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

#if (KERNEL_VERSION  >= 1003004) && defined(REPAIR_ODD_MSDOS_FS)
extern void *msdos_dir_inode_operations[], *_TRANSIENT_msdos_dir_operations;
#endif

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
#if (KERNEL_VERSION  >= 1003004) && (KERNEL_VERSION  < 1003040) && defined(REPAIR_ODD_MSDOS_FS)
  , { msdos_dir_inode_operations,
    &_TRANSIENT_msdos_dir_operations, 1 }
#endif
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

extern int init_emusyscalls( void);
extern void remove_emusyscalls( void);


int init_module( void) {
  kernel_version[0] = kernel_version[0];
  printk(ID_STRING ", init_module called \n");
  redirect_all();
  if (init_emusyscalls()) {
    restore_redirect_all();
    return -1;
  }
#if KERNEL_VERSION >= 1003057
  mod_use_count_ &= ~MOD_AUTOCLEAN;
#endif
  return 0;
}

void cleanup_module( void) {
  int i;
  printk(ID_STRING ": cleanup modul called\n");
  remove_emusyscalls();
  restore_redirect_all();
  
#ifdef _VM86_STATISTICS_
  printk(ID_STRING ": statistics vm86_traps= ");
  for (i=0; i<8 ; i++) printk(" %d",vm86_trap_count[i]);
  printk("\n" ID_STRING ": statistics vm86_faults= %d\n",vm86_fault_count);
  printk( ID_STRING ": statistics vm86_count_cli= %d vm86_count_sti= %d\n",  vm86_count_cli, vm86_count_sti);
  printk( ID_STRING ": statistics signalret_count= %d\n",  signalret_count);
  printk( ID_STRING ": statistics sys_ldt_count= %d\n",  sys_ldt_count);
#endif

  /* wait arround to be sure no process is still in the module */
  for (i = 0; i < 100; i++) schedule();
  /* now the memory can be freed */
}
