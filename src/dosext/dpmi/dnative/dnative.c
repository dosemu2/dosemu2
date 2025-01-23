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
#include "cpu.h"
#include "dosemu_debug.h"
#include "plugin_config.h"
#include "utilities.h"
#include "vgaemu.h"
#include "emu.h"
#include "cpu-emu.h"
#include "emudpmi.h"
#include "dnative.h"
/* optimize direct LDT writes */
#define DIRECT_LDT_OPT 1
#if DIRECT_LDT_OPT
#include "msdoshlp.h"
#endif

const struct dnative_ops *dnops;

static void check_ldt(void)
{
    int ret;
    int i;
    uint8_t buffer[LDT_ENTRIES * LDT_ENTRY_SIZE];
    unsigned int base_addr, limit, *lp;
    int type, np;

    ret = dnops->read_ldt(buffer, sizeof(buffer));
    /* may return 0 if no LDT */
    if (ret == sizeof(buffer)) {
        for (i = 0; i < MAX_SELECTORS; i++) {
            lp = (unsigned int *)&buffer[i * LDT_ENTRY_SIZE];
            base_addr = (*lp >> 16) & 0x0000FFFF;
            limit = *lp & 0x0000FFFF;
            lp++;
            base_addr |= (*lp & 0xFF000000) | ((*lp << 16) & 0x00FF0000);
            limit |= (*lp & 0x000F0000);
            type = (*lp >> 10) & 3;
            np = ((*lp >> 15) & 1) ^ 1;
            if (!np) {
                D_printf("LDT entry 0x%x used: b=0x%x l=0x%x t=%i\n",i,base_addr,limit,type);
                segment_set_user(i, 0xfe);
            }
        }
    }
}

int native_dpmi_setup(void)
{
    int ret;

#ifdef SEARPC_SUPPORT
    if (!dnops && config.dpmi_remote)
        load_plugin("dremote");
#endif
#ifdef DNATIVE
    if (!dnops && !config.dpmi_remote)
        load_plugin("dnative");
#endif
    if (!dnops) {
        error("Native DPMI not compiled in\n");
        return -1;
    }
    ret = dnops->setup();
    if (ret) {
        dnops = NULL;
        return ret;
    }
    check_ldt();
    return ret;
}

void native_dpmi_done(void)
{
    if (!dnops)
        return;
    dnops->done();
}

static int handle_pf(cpuctx_t *scp)
{
    int rc;
    dosaddr_t cr2 = _cr2;

#if DIRECT_LDT_OPT
    if (msdos_ldt_pagefault(scp))
        return DPMI_RET_CLIENT;
#endif
#ifdef X86_EMULATOR
#ifdef X86_JIT
    /* DPMI code touches cpuemu prot */
    if (IS_EMU_JIT() && e_invalidate_page_full(cr2))
        return DPMI_RET_CLIENT;
#endif
#endif
    rc = vga_emu_fault(cr2, _err, scp);
    if (rc == True)
        return DPMI_RET_CLIENT;
    return DPMI_RET_FAULT;
}

int native_dpmi_control(cpuctx_t *scp)
{
    char buf[PAGE_SIZE];
    int size;
    int ret = dnops->control(scp, buf, &size);
    if (ret == DPMI_RET_FAULT && _trapno == 0x0e)
        ret = handle_pf(scp);
    return ret;
}

int native_dpmi_exit(cpuctx_t *scp)
{
    int size;
    dnops->exit(scp);
    return dnops->control(scp, NULL, &size);
}

int native_read_ldt(void *ptr, int bytecount)
{
    return dnops->read_ldt(ptr, bytecount);
}

int native_write_ldt(void *ptr, int bytecount)
{
    return dnops->write_ldt(ptr, bytecount);
}

int native_check_verr(unsigned short selector)
{
    return dnops->check_verr(selector);
}

int native_debug_breakpoint(int op, cpuctx_t *scp, int err)
{
    return dnops->debug_breakpoint(op, scp, err);
}

int register_dnative_ops(const struct dnative_ops *ops)
{
    assert(!dnops);
    dnops = ops;
    return 0;
}
