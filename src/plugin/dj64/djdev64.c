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
#include "init.h"
#include "emu.h"
#include "dosemu_debug.h"
#include "utilities.h"
#include "emudpmi.h"
#include "msdoshlp.h"
#include "coopth.h"
#include "hlt.h"
#include "dos2linux.h"

static struct dos_helper_s call_hlp;
static unsigned ctrl_off;

static uint8_t *dj64_addr2ptr(uint32_t addr)
{
    return dosaddr_to_unixaddr(addr);
}

static uint32_t dj64_ptr2addr(const uint8_t *ptr)
{
    if (ptr >= MEM_BASE32(config.dpmi_base) &&
            ptr < MEM_BASE32(config.dpmi_base) + dpmi_mem_size())
        return DOSADDR_REL(ptr);
    dosemu_error("bad ptr2addr %p\n", ptr);
    return -1;
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
    cpuctx_t *scp = coopth_pop_user_data_cur();
    copy_stk(scp, sp, len);
    copy_gp(scp, regs);
//    J_printf("asm call to 0x%x:0x%x\n", pma.selector, pma.offset32);
    do_callf(scp, pma);
    coopth_sched();
    bcopy_gp(regs, scp);
    coopth_push_user_data_cur(scp);
    return ASM_CALL_OK;
}

const struct dj64_api api = {
    .addr2ptr = dj64_addr2ptr,
    .ptr2addr = dj64_ptr2addr,
    .print = dj64_print,
    .asm_call = dj64_asm_call,
};

#if DJ64_API_VER != 1
#error wrong dj64 version
#endif

static int do_open(const char *path)
{
    return djdev64_open(path, &api, DJ64_API_VER);
}

static void do_close(cpuctx_t *scp)
{
    djdev64_close(_eax);
}

static const struct djdev64_ops ops = {
    .open = do_open,
    .close = do_close,
    .call = &call_hlp.entry,
    .ctrl = &ctrl_off,
};

static void call_thr(void *arg)
{
    cpuctx_t *scp = arg;
    unsigned char *sp = SEL_ADR(_ss, _edx);  // sp in edx
    J_printf("DJ64: djdev64_call() %s\n", DPMI_show_state(scp));
    coopth_push_user_data_cur(scp);
    djdev64_call(_eax, _ebx, _ecx, _esi, sp);
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
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    hlt_hdlr.name = "dj64 ctrl";
    hlt_hdlr.func = ctrl_hlt;
    ctrl_off = hlt_register_handler_pm(hlt_hdlr);
    doshlp_setup(&call_hlp, "dj64 call", call_thr, do_retf);
    register_djdev64(&ops);
}
