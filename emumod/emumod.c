/* file: emumod.c
 * emumodule interface and redirection stuff
 *
 * (C) 1994 under GPL, Hans Lermen <lermen@elserv.ffm.fgan.de>
 */
  
#include "kversion.h"

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/string.h>
#include <linux/ptrace.h>

#include <asm/segment.h>
#include <asm/io.h>

#define  _EMUMOD_itself
#include "emumod.h"

/*
 * NOTE:
 *   To install the module, we must include the kernel identification string.
 *   (so, don't panic if you get a GCC warning "_kernel_version not used" )
 */
#include <linux/module.h>
#if KERNEL_VERSION < 1001072
#include "../../linux/tools/version.h"
#else
#include "linux/version.h"
#endif
static char kernel_version[] = UTS_RELEASE;

struct redirect_db {
  void *resident;
  void *transient; 
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
  for (i=0, p=redirect_list; i<num; i++, p++) 
      redirect_resident_routine(p->resident, p->transient, p->savearea);
  restore_flags(flags);
}

static void restore_redirect_all() {
  int num=sizeof(redirect_list) / sizeof(struct redirect_db);
  struct redirect_db *p;
  int flags,i;
  
  save_flags(flags);
  for (i=0, p=&redirect_list[num-1]; i<num; i++, p--) 
      restore_redirect(p->resident, p->savearea);
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
#endif

  /* wait arround to be sure no process is still in the module */
  for (i = 0; i < 100; i++) schedule();
  /* now the memory can be freed */
}
