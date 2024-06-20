/*
 *  Copyright (C) 2024  stsp
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <dlfcn.h>
#include <stdio.h>
#include <djdev64/djdev64.h>
#include <djdev64/dj64init.h>
#include <djdev64/stub.h>
#include "init.h"
#include "emu.h"
#include "cpu-emu.h"
#include "dosemu_debug.h"
#include "utilities.h"
#include "emudpmi.h"
#include "msdoshlp.h"
#include "coopth.h"
#include "coopth_pm.h"
#include "hlt.h"
#include "dos2linux.h"
#include "dos.h"
#include "dpmiops.h"

#if DJ64_API_VER != 8
#error wrong djdev64 version
#endif

static unsigned ctrl_off;
#define HNDL_MAX 5
static struct dos_helper_s call_hlp[HNDL_MAX];
static struct dos_helper_s stub_hlp;
#define MAX_CLNUP_TIDS 5
static int clnup_tids[HNDL_MAX][MAX_CLNUP_TIDS];
static int num_clnup_tids[HNDL_MAX];
static int exiting;
static int exit_rc;

struct udata {
    cpuctx_t *scp;
    int handle;
};

static void call_thr(void *arg);
static void stub_thr(void *arg);
static void ctrl_hlt(Bit16u offs, void *sc, void *arg);
static void do_retf(cpuctx_t *scp);

static uint8_t *dj64_addr2ptr(uint32_t addr)
{
    return dosaddr_to_unixaddr(addr);
}

static uint8_t *dj64_addr2ptr2(uint32_t addr, uint32_t len)
{
    e_invalidate(addr, len);
    return dosaddr_to_unixaddr(addr);
}

static uint32_t dj64_ptr2addr(const uint8_t *ptr)
{
    if (ptr >= MEM_BASE32(config.dpmi_base) &&
            ptr < MEM_BASE32(config.dpmi_base + dpmi_mem_size()))
        return DOSADDR_REL(ptr);
    dosemu_error("bad ptr2addr %p\n", ptr);
    return -1;
}

static int dj64_dos_ptr(const uint8_t *ptr)
{
    if ((ptr >= MEM_BASE32(config.dpmi_base) &&
            ptr < MEM_BASE32(config.dpmi_base + dpmi_mem_size())) ||
            (ptr >= MEM_BASE32(0) && ptr < MEM_BASE32(LOWMEM_SIZE + HMASIZE)))
        return 1;
    return 0;
}

static void dj64_print(int prio, const char *format, va_list ap)
{
    switch(prio) {
    case DJ64_PRINT_TERMINAL:
        vfprintf(stderr, format, ap);
        break;
    case DJ64_PRINT_LOG:
        if (debug_level('J')) {
            log_printf(-1, "dj64: ");
            vlog_printf(-1, format, ap);
        }
        break;
    case DJ64_PRINT_SCREEN:
        p_direct_vstr(format, ap);
        break;
    }
}

static void copy_stk(cpuctx_t *scp, uint8_t *sp, uint8_t len)
{
    uint8_t *stk;
    if (!len)
        return;
    _esp -= len;
    stk = SEL_ADR(_ss, _esp);
    memcpy(stk, sp, len);
}

static void copy_gp(cpuctx_t *scp, dpmi_regs *src)
{
#define CP_R(r) _##r = src->r
    CP_R(eax);
    CP_R(ebx);
    CP_R(ecx);
    CP_R(edx);
    CP_R(esi);
    CP_R(edi);
    CP_R(ebp);
#undef CP_R
}

static void bcopy_gp(dpmi_regs *dst, cpuctx_t *scp)
{
#define CP_R(r) dst->r = _##r
    CP_R(eax);
    CP_R(ebx);
    CP_R(ecx);
    CP_R(edx);
    CP_R(esi);
    CP_R(edi);
    CP_R(ebp);
#undef CP_R
}

static void do_callf(cpuctx_t *scp, dpmi_paddr pma)
{
    unsigned int *ssp = SEL_ADR(_ss, _esp);
    *--ssp = _cs;
    *--ssp = _eip;
    _esp -= 8;

    _cs = pma.selector;
    _eip = pma.offset32;
}

static int dj64_asm_call(dpmi_regs *regs, dpmi_paddr pma, uint8_t *sp,
        uint8_t len)
{
    int rc;
    int ret = ASM_CALL_OK;
    struct udata *ud = coopth_get_user_data_cur();
    copy_stk(ud->scp, sp, len);
    copy_gp(ud->scp, regs);
//    J_printf("asm call to 0x%x:0x%x\n", pma.selector, pma.offset32);
    do_callf(ud->scp, pma);
    coopth_cancel_disable_cur();
    rc = coopth_sched();
    /* re-enable cancellability only if it was not canceled already */
    if (rc == 0) {
        coopth_cancel_enable_cur();
        bcopy_gp(regs, ud->scp);
    } else {
        ret = ASM_CALL_ABORT;
    }
    return ret;
}

static void dj64_asm_noret(dpmi_regs *regs, dpmi_paddr pma, uint8_t *sp,
        uint8_t len)
{
    struct pmaddr_s abt = doshlp_get_abort_helper();
    struct udata *ud = coopth_pop_user_data_cur();
    coopth_leave_pm(ud->scp);
    copy_stk(ud->scp, sp, len);
    copy_gp(ud->scp, regs);
    ud->scp->cs = abt.selector;
    ud->scp->eip = abt.offset;
    do_callf(ud->scp, pma);
}

static uint8_t *dj64_inc_esp(uint32_t len)
{
    struct udata *ud = coopth_get_user_data_cur();
    ud->scp->esp += len;
    return SEL_ADR(ud->scp->ss, ud->scp->esp);
}

static int dj64_get_handle(void)
{
    struct udata *ud = coopth_get_user_data_cur();
    return ud->handle;
}

static void dj64_exit(int rc)
{
    int h, i;

    if (exiting)
        return;
    exiting++;
    exit_rc = rc;

    for (h = 0; h < HNDL_MAX; h++) {
        for (i = 0; i < num_clnup_tids[h]; i++) {
            int tid = clnup_tids[h][i];
            if (coopth_get_tid() != tid)
                coopth_cancel(tid);
        }
    }
}

const struct dj64_api api = {
    .addr2ptr = dj64_addr2ptr,
    .addr2ptr2 = dj64_addr2ptr2,
    .ptr2addr = dj64_ptr2addr,
    .print = dj64_print,
    .asm_call = dj64_asm_call,
    .asm_noret = dj64_asm_noret,
    .inc_esp = dj64_inc_esp,
    .is_dos_ptr = dj64_dos_ptr,
    .get_handle = dj64_get_handle,
    .exit = dj64_exit,
};

static int do_open(const char *path, unsigned flags)
{
    int ret = djdev64_open(path, &api, DJ64_API_VER, flags);
    if (ret == -1)
        return ret;
    assert(ret < HNDL_MAX);
    if (!call_hlp[ret].tid)
        doshlp_setup(&call_hlp[ret], "dj64 call", call_thr, do_retf);
    if (!ctrl_off) {
        emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
        hlt_hdlr.name = "dj64 ctrl";
        hlt_hdlr.func = ctrl_hlt;
        ctrl_off = hlt_register_handler_pm(hlt_hdlr);
    }
    return ret;
}

static void do_close(int handle)
{
    while (num_clnup_tids[handle]) {
        int i = num_clnup_tids[handle] - 1;
        int tid = clnup_tids[handle][i];
        coopth_cancel(tid);
        if (coopth_get_tid() == tid) {
            num_clnup_tids[handle]--;  // skip own thread
            continue;
        }
        coopth_unsafe_detach(tid, __FILE__);
    }
    djdev64_close(handle);
}

static char *_SEL_ADR(uint16_t sel)
{
    return SEL_ADR(sel, 0);
}

static struct dos_ops dosops = {
    _dos_open,
    _dos_read,
    _dos_write,
    _dos_seek,
    _dos_close,
    _dos_link_umb,
};

static void stub_thr(void *arg)
{
    cpuctx_t *scp = arg;
    struct stub_ret_regs regs = {};
    int argc = _ecx;
    unsigned *argp = SEL_ADR(_ds, _edx);
    char **argv = alloca((argc + 1) * sizeof(char *));
    int envc = _ebx;
    unsigned *envpp = SEL_ADR(_ds, _esi);
    char **envp = alloca((envc + 1) * sizeof(char *));
    int i;
    int err;

    for (i = 0; i < argc; i++)
        argv[i] = SEL_ADR(_ds, argp[i]);
    argv[i] = NULL;
    for (i = 0; i < envc; i++)
        envp[i] = SEL_ADR(_ds, envpp[i]);
    envp[i] = NULL;

    err = djstub_main(argc, argv, envp, _eax, &regs, _SEL_ADR, &dosops,
            &dpmiops);
    if (err) {
        error("djstub: load failed\n");
        return;
    }
    coopth_leave_pm(scp);
    _es = 0;
    _gs = 0;
    _fs = regs.fs;
    _ds = regs.ds;
    _cs = regs.cs;
    _eip = regs.eip;
}

static unsigned call_entry(int handle)
{
    return call_hlp[handle].entry;
}

static unsigned ctrl_entry(int handle)
{
    return ctrl_off;
}

static unsigned stub_entry(void)
{
    if (!stub_hlp.tid)
        doshlp_setup(&stub_hlp, "dj64 stub", stub_thr, do_retf);
    return stub_hlp.entry;
}

static const struct djdev64_ops ops = {
    .open = do_open,
    .close = do_close,
    .call = call_entry,
    .ctrl = ctrl_entry,
    .stub = stub_entry,
};

static void call_thr(void *arg)
{
    cpuctx_t *scp = arg;
    unsigned char *sp = SEL_ADR(_ss, _edx);  // sp in edx
    int handle = _eax;
    struct udata ud = { scp, handle };;
    if (handle >= HNDL_MAX) {
        error("DJ64: bad handle %x\n", handle);
        return;
    }
    J_printf("DJ64: djdev64_call(%i) %s\n", handle, DPMI_show_state(scp));
    coopth_push_user_data_cur(&ud);
    assert(num_clnup_tids[handle] < MAX_CLNUP_TIDS);
    clnup_tids[handle][num_clnup_tids[handle]++] = coopth_get_tid();
    djdev64_call(handle, _ebx, _ecx, _esi, sp);
    num_clnup_tids[handle]--;

    if (exiting)
        leavedos(exit_rc);
}

static void do_retf(cpuctx_t *scp)
{
    unsigned int *ssp = SEL_ADR(_ss, _esp);
    _eip = *ssp++;
    _cs = *ssp++;
    _esp += 8;
}

static void ctrl_hlt(Bit16u offs, void *sc, void *arg)
{
    cpuctx_t *scp = sc;
    unsigned char *sp = SEL_ADR(_ss, _edx);  // sp in edx
    do_retf(scp);
    J_printf("DJ64: djdev64_ctrl() %s\n", DPMI_show_state(scp));
    djdev64_ctrl(_eax, _ebx, _ecx, _esi, sp);
}

CONSTRUCTOR(static void djdev64_init(void))
{
    register_djdev64(&ops);
}
