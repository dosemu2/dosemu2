#ifndef _EMUMOD_
#define _EMUMOD_

#if 1
#define _VM86_STATISTICS_
#endif

#define ID_STRING "vm86-module"

#ifdef _EMUMOD_itself

#ifdef _VM86_STATISTICS_
int vm86_fault_count=0;
int vm86_trap_count[8]={0};
int vm86_count_sti=0;
int vm86_count_cli=0;  
int signalret_count=0;
int sys_ldt_count=0;
#endif

#else  /* NOT _EMUMOD_itself */

/* redirection for traps.c */
#define do_divide_error _TRANSIENT_do_divide_error
#define do_int3 _TRANSIENT_do_int3
#define do_overflow _TRANSIENT_do_overflow
#define do_bounds _TRANSIENT_do_bounds
#define do_device_not_available _TRANSIENT_do_device_not_available
#define do_debug _TRANSIENT_do_debug

#define save_v86_state _TRANSIENT_save_v86_state
asmlinkage struct pt_regs * _TRANSIENT_save_v86_state(struct vm86_regs * regs);

#define handle_vm86_fault _TRANSIENT_handle_vm86_fault
void _TRANSIENT_handle_vm86_fault(struct vm86_regs * regs, long error_code);

#define handle_vm86_trap _TRANSIENT_handle_vm86_trap
void handle_vm86_trap(struct vm86_regs * regs, long error_code, int trapno);

#define sys_vm86 _TRANSIENT_sys_vm86
asmlinkage int _TRANSIENT_sys_vm86(struct vm86_struct * v86);

#define sys_sigreturn  _TRANSIENT_sys_sigreturn
asmlinkage int _TRANSIENT_sys_sigreturn(unsigned long __unused);

#define sys_modify_ldt  _TRANSIENT_sys_modify_ldt
asmlinkage int _TRANSIENT_sys_modify_ldt(int func, void *ptr, unsigned long bytecount);

#endif /* _EMUMOD_itself */

#endif /* _EMUMOD_ */
