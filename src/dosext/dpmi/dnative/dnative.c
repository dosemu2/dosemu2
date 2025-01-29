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
#include "port.h"
#include "emudpmi.h"
#include "dnative.h"
/* optimize direct LDT writes */
#define DIRECT_LDT_OPT 1
#if DIRECT_LDT_OPT
#include "msdoshlp.h"
#endif

const struct dnative_ops *dnops;
struct cpio_tmp {
    int base;
    int size;
};
#define CPIO_MAX 50
static struct cpio_tmp cptmp[CPIO_MAX];
static int num_cptmp;
static struct cpio_tmp drtmp[CPIO_MAX];
static int num_drtmp;

static void check_ldt(void)
{
    int ret;
    int i;
    uint8_t buffer[LDT_ENTRIES * LDT_ENTRY_SIZE];
    unsigned int base_addr, limit, *lp;
    int type, np;

    ret = dnops->read_ldt(buffer, sizeof(buffer), (uintptr_t)mem_base);
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
    int ret, i;

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

    for (i = 0; i < num_cptmp; i++) {
        struct cpio_tmp *ct = &cptmp[i];
        dnops->set_cpio(ct->base, ct->size);
    }
    num_cptmp = 0;
    for (i = 0; i < num_drtmp; i++) {
        struct cpio_tmp *ct = &drtmp[i];
        dnops->set_drio(ct->base, ct->size);
    }
    num_drtmp = 0;
    return ret;
}

void native_dpmi_done(void)
{
    if (!dnops)
        return;
    dnops->done();
}

void native_dpmi_set_cpio(int base, int size)
{
    struct cpio_tmp *ct;
    assert(num_cptmp < CPIO_MAX);
    ct = &cptmp[num_cptmp++];
    ct->base = base;
    ct->size = size;
}

void native_dpmi_set_drio(int base, int size)
{
    struct cpio_tmp *ct;
    assert(num_drtmp < CPIO_MAX);
    ct = &drtmp[num_drtmp++];
    ct->base = base;
    ct->size = size;
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
    char buf[MAX_CPIO * sizeof(struct cpio_ent) + sizeof(struct cpio_s)];
    int size;
    int ret;

    static_assert(sizeof(buf) >= sizeof(struct cpio_s) +
            sizeof(struct cpio_ent) + MAX_CPIO, "bad buffer size");
    ret = dnops->control(scp, buf, &size);
    if (size) {
        int i;
        struct cpio_s *cp = (struct cpio_s *)buf;
        for (i = 0; i < cp->num; i++) {
            struct cpio_ent *ce = &cp->ent[i];
            switch(ce->size) {
            case 1: port_outb(ce->base, ce->value); break;
            case 2: port_outw(ce->base, ce->value); break;
            case 4: port_outd(ce->base, ce->value); break;
            }
        }
    }
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

int native_read_ldt(void *ptr, int bytecount, uint64_t base)
{
    return dnops->read_ldt(ptr, bytecount, base);
}

int native_write_ldt(const void *ptr, int bytecount, uint64_t base)
{
    return dnops->write_ldt(ptr, bytecount, base);
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
