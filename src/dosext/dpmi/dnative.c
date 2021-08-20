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
#include "mapping.h"
#include "dpmi.h"
#include "dpmisel.h"
#include "dnative.h"

static coroutine_t dpmi_tid;
static cohandle_t co_handle;
static int dpmi_ret_val;
static sigcontext_t emu_stack_frame;
static struct sigaction emu_tmp_act;
#define DPMI_TMP_SIG SIGUSR1
static int in_dpmi_thr;
static int dpmi_thr_running;

#ifdef __x86_64__
static unsigned int *iret_frame;

static void iret_frame_setup(sigcontext_t * scp)
{
    /* set up a frame to get back to DPMI via iret. The kernel does not save
       %ss, and the SYSCALL instruction in sigreturn() destroys it.

       IRET pops off everything in 64-bit mode even if the privilege
       does not change which is nice, but clobbers the high 48 bits
       of rsp if the DPMI client uses a 16-bit stack which is not so
       nice (see EMUfailure.txt). Setting %rsp to 0x100000000 so that
       bits 16-31 are zero works around this problem, as DPMI code
       can't see bits 32-63 anyway.
     */

    iret_frame[0] = _eip;
    iret_frame[1] = _cs;
    iret_frame[2] = _eflags;
    iret_frame[3] = _esp;
    iret_frame[4] = _ss;
    _rsp = (unsigned long) iret_frame;
}

void dpmi_iret_setup(sigcontext_t * scp)
{
    iret_frame_setup(scp);
    _eflags &= ~TF;
    _rip = (unsigned long) DPMI_iret;
    _cs = getsegment(cs);
}

void dpmi_iret_unwind(sigcontext_t * scp)
{
    if (_rip != (unsigned long) DPMI_iret)
        return;
    _eip = iret_frame[0];
    _cs = iret_frame[1];
    _eflags = iret_frame[2];
    _esp = iret_frame[3];
    _ss = iret_frame[4];
}
#endif


static void dpmi_thr(void *arg);

/* ======================================================================== */
/*
 * DANG_BEGIN_FUNCTION native_dpmi_control
 *
 * This function is similar to the vm86() syscall in the kernel and
 * switches to dpmi code.
 *
 * DANG_END_FUNCTION
 */

int native_dpmi_control(sigcontext_t * scp)
{
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
    /* we may return here with sighandler's signal mask.
     * This is done for speed-up. dpmi_control() restores the mask. */
    return dpmi_ret_val;
}

int native_dpmi_exit(sigcontext_t * scp)
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

void dpmi_return(sigcontext_t * scp, int retcode)
{
    /* only used for CPUVM_NATIVE (from sigsegv.c: dosemu_fault1()) */
    if (!DPMIValidSelector(_cs)) {
        dosemu_error("Return to dosemu requested within dosemu context\n");
        return;
    }
    copy_context(dpmi_get_scp(), scp, 1);
    dpmi_ret_val = retcode;
    signal_return_to_dosemu();
    co_resume(co_handle);
    signal_return_to_dpmi();
    if (dpmi_ret_val == DPMI_RET_EXIT)
        copy_context(scp, &emu_stack_frame, 1);
    else
        copy_context(scp, dpmi_get_scp(), 1);
}

static void dpmi_switch_sa(int sig, siginfo_t * inf, void *uc)
{
    ucontext_t *uct = uc;
    sigcontext_t *scp = &uct->uc_mcontext;
#ifdef __linux__
    emu_stack_frame.fpregs = aligned_alloc(16, sizeof(*__fpstate));
#endif
    copy_context(&emu_stack_frame, scp, 1);
    copy_context(scp, dpmi_get_scp(), 1);
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
#ifdef __linux__
    free(emu_stack_frame.fpregs);
#endif
}

static void dpmi_thr(void *arg)
{
    in_dpmi_thr++;
    indirect_dpmi_transfer();
    in_dpmi_thr--;
}

void native_dpmi_setup(void)
{
    co_handle = co_thread_init(PCL_C_MC);

#ifdef __x86_64__
    {
        unsigned int i, j;
        void *addr;
        /* search for page with bits 16-31 clear within first 47 bits
           of address space */
        for (i = 1; i < 0x8000; i++) {
            for (j = 0; j < 0x10000; j += PAGE_SIZE) {
                addr = (void *) (i * 0x100000000UL + j);
                iret_frame =
                    mmap_mapping_ux(MAPPING_SCRATCH | MAPPING_NOOVERLAP,
                                    addr, PAGE_SIZE,
                                    PROT_READ | PROT_WRITE);
                if (iret_frame != MAP_FAILED)
                    goto out;
            }
        }
      out:
        if (iret_frame != addr) {
            error("Can't find DPMI iret page, leaving\n");
            leavedos(0x24);
        }
    }

    /* reserve last page to prevent DPMI from allocating it.
     * Our code is full of potential 32bit integer overflows. */
    mmap_mapping(MAPPING_DPMI | MAPPING_SCRATCH | MAPPING_NOOVERLAP,
                 (uint32_t) PAGE_MASK, PAGE_SIZE, PROT_NONE);
#endif
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
