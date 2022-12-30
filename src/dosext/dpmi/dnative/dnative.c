/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * native DPMI backend
 *
 * Author: Stas Sergeev
 */
#include <pthread.h>
#include <stdlib.h>
#include "libpcl/pcl.h"
#include "sig.h"
#include "cpu.h"
#include "dosemu_debug.h"
#include "utilities.h"
#include "emu.h"
#include "vgaemu.h"
#include "cpu-emu.h"
#include "mapping.h"
#include "emudpmi.h"
#include "dpmisel.h"
#include "dnative.h"

#define EMU_X86_FXSR_MAGIC	0x0000
static coroutine_t dpmi_tid;
static cohandle_t co_handle;
static int dpmi_ret_val;
static sigcontext_t emu_stack_frame;
static struct sigaction emu_tmp_act;
#define DPMI_TMP_SIG SIGUSR1
static int in_dpmi_thr;
static int dpmi_thr_running;

static void copy_context(sigcontext_t *d, sigcontext_t *s)
{
#ifdef __linux__
  /* keep pointer to FPU state the same */
  fpregset_t fptr = d->fpregs;
#endif
  *d = *s;
#ifdef __linux__
  d->fpregs = fptr;
#endif
}

#ifdef __i386__
/* On i386 only, if SSE is available (i.e. since the Pentium III),
   the kernel will use FXSAVE to save FPU state, then put the following
   on the signal stack:
   * FXSAVE format converted to FSAVE format (108 bytes)
   * status and magic field where magic == X86_FXSR_MAGIC (4 bytes)
   * FXSAVE format (512 bytes), which can be used directly by our loadfpstate
   However, when restoring FPU state it will only use the mxcsr and xmm
   fields from the FXSAVE format, and take everything else from the FSAVE
   format, so we must "undo" the kernel logic and put those fields into the
   FSAVE region.
   see also arch/x86/kernel/fpu/regset.c in Linux kernel */
static void convert_from_fxsr(fpregset_t fptr,
			      const struct emu_fpxstate *fxsave)
{
  unsigned int tmp;
  int i;

  fptr->cw = fxsave->cwd | 0xffff0000u;
  fptr->sw = fxsave->swd | 0xffff0000u;
  /* Expand the valid bits */
  tmp = fxsave->ftw;			/* 00000000VVVVVVVV */
  tmp = (tmp | (tmp << 4)) & 0x0f0f;	/* 0000VVVV0000VVVV */
  tmp = (tmp | (tmp << 2)) & 0x3333;	/* 00VV00VV00VV00VV */
  tmp = (tmp | (tmp << 1)) & 0x5555;	/* 0V0V0V0V0V0V0V0V */
  /* Transform each bit from 1 to 00 (valid) or 0 to 11 (empty).
     The kernel will convert it back, so no need to worry about zero
     and special states 01 and 10. */
  tmp = ~(tmp | (tmp << 1));
  fptr->tag = tmp | 0xffff0000u;
  fptr->ipoff = fxsave->fip;
  fptr->cssel = (uint16_t)fxsave->fcs | ((uint32_t)fxsave->fop << 16);
  fptr->dataoff = fxsave->fdp;
  fptr->datasel = fxsave->fds;
  for (i = 0; i < 8; i++)
    memcpy(&fptr->_st[i], &fxsave->st[i], sizeof(fptr->_st[i]));
}
#endif

static void copy_to_dpmi(sigcontext_t *scp, cpuctx_t *s)
{
#ifdef __x86_64__
  /* needs to clear high part of RIP and RSP because AMD CPUs
   * check whole 64bit regs against 32bit CS and SS limits on iret */
  _scp_rip = 0;
  _scp_rsp = 0;
#endif
#define _C(x) _scp_##x = get_##x(s)
  _C(es);
  _C(ds);
  _C(ss);
  _C(cs);
  _C(fs);
  _C(gs);
  _C(eax);
  _C(ebx);
  _C(ecx);
  _C(edx);
  _C(esi);
  _C(edi);
  _C(ebp);
  _C(esp);
  _C(eip);
  _C(eflags);
  _C(trapno);
  _C(err);
  _C(cr2);

  if (scp->fpregs) {
    void *fpregs = scp->fpregs;
#ifdef __x86_64__
    static_assert(sizeof(*scp->fpregs) == sizeof(vm86_fpu_state.fxsave),
		  "size mismatch");
#else
    /* i386: convert fxsave state (if available) to fsave state */
    if (emu_fpstate_get_type(&vm86_fpu_state) == EMU_FPSTATE_FXSAVE) {
      convert_from_fxsr(scp->fpregs, &vm86_fpu_state.fxsave);
      if ((scp->fpregs->status >> 16) != EMU_X86_FXSR_MAGIC)
	return;
      fpregs = &scp->fpregs->status + 1;
    }
#endif
    memcpy(fpregs, &vm86_fpu_state, vm86_fpu_state.size);
  }
}

static void copy_to_emu(cpuctx_t *d, sigcontext_t *scp)
{
#define _D(x) get_##x(d) = _scp_##x
  _D(es);
  _D(ds);
  _D(ss);
  _D(cs);
  _D(fs);
  _D(gs);
  _D(eax);
  _D(ebx);
  _D(ecx);
  _D(edx);
  _D(esi);
  _D(edi);
  _D(ebp);
  _D(esp);
  _D(eip);
  _D(eflags);
  _D(trapno);
  _D(err);
  _D(cr2);
  if (scp->fpregs) {
    void *fpregs = scp->fpregs;
#ifdef __x86_64__
    static_assert(sizeof(*scp->fpregs) == sizeof(vm86_fpu_state.fxsave),
		"size mismatch");
#else
    emu_fpstate_set_type(&vm86_fpu_state, EMU_FPSTATE_FSAVE);
    if ((scp->fpregs->status >> 16) == EMU_X86_FXSR_MAGIC) {
      emu_fpstate_set_type(&vm86_fpu_state, EMU_FPSTATE_FXSAVE);
      fpregs = &scp->fpregs->status + 1;
    }
#endif
    memcpy(&vm86_fpu_state, fpregs, vm86_fpu_state.size);
  }
}

static void dpmi_thr(void *arg);

static int handle_pf(cpuctx_t *scp)
{
    int rc;
    dosaddr_t cr2 = DOSADDR_REL(LINP(_cr2));
#ifdef X86_EMULATOR
#ifdef HOST_ARCH_X86
    /* DPMI code touches cpuemu prot */
    if (IS_EMU() && !CONFIG_CPUSIM && e_invalidate_page_full(cr2))
        return DPMI_RET_CLIENT;
#endif
#endif
    signal_unblock_async_sigs();
    rc = vga_emu_fault(cr2, _err, scp);
    /* going for dpmi_fault() or deinit_handler(),
     * careful with async signals and sas_wa */
    signal_restore_async_sigs();
    if (rc == True)
        return DPMI_RET_CLIENT;
    return DPMI_RET_FAULT;
}

/* ======================================================================== */
/*
 * DANG_BEGIN_FUNCTION native_dpmi_control
 *
 * This function is similar to the vm86() syscall in the kernel and
 * switches to dpmi code.
 *
 * DANG_END_FUNCTION
 */

int native_dpmi_control(cpuctx_t *scp)
{
    unsigned saved_IF = (_eflags & IF);

    _eflags = get_EFLAGS(_eflags);
    if (in_dpmi_thr)
        signal_switch_to_dpmi();
    else
        dpmi_tid =
            co_create(co_handle, dpmi_thr, NULL, NULL, SIGSTACK_SIZE);
    dpmi_thr_running++;
    co_call(dpmi_tid);
    dpmi_thr_running--;
    if (in_dpmi_thr)
        signal_switch_to_dosemu();
    assert(_eflags & IF);
    if (!saved_IF)
        _eflags &= ~IF;
    _eflags &= ~VIF;
    if (dpmi_ret_val == DPMI_RET_FAULT && _trapno == 0x0e)
        dpmi_ret_val = handle_pf(scp);
    /* we may return here with sighandler's signal mask.
     * This is done for speed-up. dpmi_control() restores the mask. */
    return dpmi_ret_val;
}

int native_dpmi_exit(cpuctx_t *scp)
{
    int ret;
    if (!in_dpmi_thr)
        return DPMI_RET_DOSEMU;
    D_printf("DPMI: leaving\n");
    dpmi_ret_val = DPMI_RET_EXIT;
    ret = native_dpmi_control(scp);
    if (in_dpmi_thr)
        error("DPMI thread have not terminated properly\n");
    return ret;
}

void dpmi_return(sigcontext_t *scp, int retcode)
{
    /* only used for CPUVM_NATIVE (from sigsegv.c: dosemu_fault1()) */
    if (!DPMIValidSelector(_scp_cs)) {
        dosemu_error("Return to dosemu requested within dosemu context\n");
        return;
    }
    dpmi_ret_val = retcode;
    if (retcode == DPMI_RET_EXIT) {
        copy_context(scp, &emu_stack_frame);
        return;
    }
    copy_to_emu(dpmi_get_scp(), scp);
    /* signal handlers start with clean FPU state, but we unmask
       overflow/division by zero in main code */
    fesetenv(&dosemu_fenv);
    signal_return_to_dosemu();
    co_resume(co_handle);
    signal_return_to_dpmi();
    if (dpmi_ret_val == DPMI_RET_EXIT)
        copy_context(scp, &emu_stack_frame);
    else
        copy_to_dpmi(scp, dpmi_get_scp());
}

static void dpmi_switch_sa(int sig, siginfo_t * inf, void *uc)
{
    ucontext_t *uct = uc;
    sigcontext_t *scp = &uct->uc_mcontext;
    copy_context(&emu_stack_frame, scp);
    copy_to_dpmi(scp, dpmi_get_scp());
    sigaction(DPMI_TMP_SIG, &emu_tmp_act, NULL);
    deinit_handler(scp, &uct->uc_flags);
}

static void indirect_dpmi_transfer(void)
{
    struct sigaction act;

    act.sa_flags = SA_SIGINFO;
    sigfillset(&act.sa_mask);
    sigdelset(&act.sa_mask, SIGSEGV);
    act.sa_sigaction = dpmi_switch_sa;
    sigaction(DPMI_TMP_SIG, &act, &emu_tmp_act);
    signal_set_altstack(1);
    /* for some absolutely unclear reason neither pthread_self() nor
     * pthread_kill() are the memory barriers. */
    asm volatile ("":::"memory");
    pthread_kill(pthread_self(), DPMI_TMP_SIG);
    /* and we are back */
    signal_set_altstack(0);
    /* we inherited FPU state from DPMI, so put back to DOSEMU state */
    fesetenv(&dosemu_fenv);
}

static void dpmi_thr(void *arg)
{
    in_dpmi_thr++;
    indirect_dpmi_transfer();
    in_dpmi_thr--;
}

int native_dpmi_setup(void)
{
    co_handle = co_thread_init(PCL_C_MC);
    return 0;
}

void native_dpmi_done(void)
{
    if (in_dpmi_thr && !dpmi_thr_running)
        co_delete(dpmi_tid);
    co_thread_cleanup(co_handle);
}

void native_dpmi_enter(void)
{
    /* if we are going directly to a sighandler, mask async signals. */
    if (in_dpmi_thr)
        signal_restore_async_sigs();
}

void native_dpmi_leave(void)
{
    /* for speed-up, DPMI switching corrupts signal mask. Fix it here. */
    signal_unblock_async_sigs();
}
