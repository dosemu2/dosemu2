#ifndef _EMUMOD_
#define _EMUMOD_

#define ID_STRING "vm86-module"

#ifndef _EMUMOD_itself

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
int handle_vm86_trap(struct vm86_regs * regs, long error_code, int trapno);

#define sys_vm86 _TRANSIENT_sys_vm86
asmlinkage int _TRANSIENT_sys_vm86(unsigned long subfunction, struct vm86_struct * v86_);

#define sys_sigreturn  _TRANSIENT_sys_sigreturn
asmlinkage int _TRANSIENT_sys_sigreturn(unsigned long __unused);

#define sys_modify_ldt  _TRANSIENT_sys_modify_ldt
asmlinkage int _TRANSIENT_sys_modify_ldt(int func, void *ptr, unsigned long bytecount);

#endif /* not _EMUMOD_itself */

#endif /* _EMUMOD_ */
