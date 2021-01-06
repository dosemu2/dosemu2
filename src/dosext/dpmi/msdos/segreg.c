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
 * Purpose: decode segreg access
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 *
 * Normally DPMI reserved some GDT entries for the API translator,
 * like 0x40, to span some important DOS areas. We can only allocate
 * LDTs, and therefore we replace the GDT selector with LDT selector
 * in the appropriate segreg.
 */

#include "cpu.h"
#include "memory.h"
#include "dpmi.h"
#include "dosemu_debug.h"
#define ALL_GDTS 0
#if ALL_GDTS
#include "instremu.h"
#endif
#include "instr_dec.h"
#include "msdos_ldt.h"
#include "msdos_priv.h"
#include "segreg_priv.h"

static enum MfRet msdos_sel_fault(sigcontext_t *scp)
{
    sigcontext_t new_sct;
    int reg;
    unsigned int segment;
    unsigned short desc;

    D_printf("MSDOS: msdos_fault, err=%#x\n", _err);
    if ((_err & 0xffff) == 0)	/*  not a selector error */
	return MFR_NOT_HANDLED;

    /* now it is a invalid selector error, try to fix it if it is */
    /* caused by an instruction such as mov Sreg,r/m16            */
#if !ALL_GDTS
    segment = (_err & 0xfff8);
    /* only allow using some special GTDs */
    switch (segment) {
    case 0x0040:
    case 0xa000:
    case 0xb000:
    case 0xb800:
    case 0xc000:
    case 0xe000:
    case 0xf000:
    case 0xbf8:
    case 0xf800:
    case 0xff00:
    case 0x38:		// ShellShock installer
	break;
    default:
	return MFR_ERROR;
    }
    copy_context(&new_sct, scp, 0);
    reg = decode_segreg(&new_sct);
    if (reg == -1)
	return MFR_ERROR;
#else
    copy_context(&new_sct, scp, 0);
    reg = decode_modify_segreg_insn(&new_sct, 1, &segment);
    if (reg == -1)
	return MFR_ERROR;

    if (ValidAndUsedSelector(segment)) {
	/*
	 * The selector itself is OK, but the descriptor (type) is not.
	 * We cannot fix this! So just give up immediately and dont
	 * screw up the context.
	 */
	D_printf("MSDOS: msdos_fault: Illegal use of selector %#x\n",
		 segment);
	return MFR_ERROR;
    }
#endif

    D_printf("MSDOS: try mov to a invalid selector 0x%04x\n", segment);

    switch (segment) {
    case 0x38:
	/* dos4gw sets VCPI descriptors 0x28, 0x30, 0x38 */
	/* The 0x38 is the "flat data" segment (0,4G) */
	desc = ConvertSegmentToDescriptor_lim(0, 0xffffffff);
	break;
    default:
	/* any other special cases? */
	desc = ConvertSegmentToDescriptor(segment);
    }
    if (!desc)
	return MFR_ERROR;

    /* OKay, all the sanity checks passed. Now we go and fix the selector */
    copy_context(scp, &new_sct, 0);
    switch (reg) {
    case es_INDEX:
	_es = desc;
	break;
    case cs_INDEX:
	_cs = desc;
	break;
    case ss_INDEX:
	_ss = desc;
	break;
    case ds_INDEX:
	_ds = desc;
	break;
    case fs_INDEX:
	_fs = desc;
	break;
    case gs_INDEX:
	_gs = desc;
	break;
    }

    /* let's hope we fixed the thing, and return */
    return MFR_HANDLED;
}

static int msdos_fault(sigcontext_t *scp)
{
    enum MfRet ret;
    uint16_t sel;

#define MR_CHK(r) do { \
    switch (r) { \
    case MFR_ERROR: \
	return 0; \
    case MFR_HANDLED: \
	return 1; \
    case MFR_NOT_HANDLED: \
	break; \
    } } while (0)
    ret = msdos_sel_fault(scp);
    MR_CHK(ret);

    sel = decode_selector(scp);
    if (!sel)
	return 0;
    ret = msdos_ldt_fault(scp, sel);
    MR_CHK(ret);

    return 0;
}

static void decode_exc(sigcontext_t *scp, const unsigned int *ssp)
{
    ssp += 8+2; // skip legacy frame and cs:eip
    _err = *ssp++;
    _eip = *ssp++;
    _cs = *ssp++;
    _eflags = *ssp++;
    _esp = *ssp++;
    _ss = *ssp++;
    _es = *ssp++;
    _ds = *ssp++;
    _fs = *ssp++;
    _gs = *ssp++;
    _cr2 = *ssp++;
}

static void encode_exc(sigcontext_t *scp, unsigned int *ssp)
{
    ssp += 8+2; // skip legacy frame and cs:eip
    ssp++;  // err
    *ssp++ = _eip;
    *ssp++ = _cs;
    *ssp++ = _eflags;
    *ssp++ = _esp;
    *ssp++ = _ss;
    *ssp++ = _es;
    *ssp++ = _ds;
    *ssp++ = _fs;
    *ssp++ = _gs;
}

static void copy_gp(sigcontext_t *scp, sigcontext_t *src)
{
#define CP_R(r) _##r = get_##r(src)
    CP_R(eax);
    CP_R(ebx);
    CP_R(ecx);
    CP_R(edx);
    CP_R(esi);
    CP_R(edi);
    CP_R(ebp);
}

static void do_fault(sigcontext_t *scp, DPMI_INTDESC *pma,
    int (*cbk)(sigcontext_t *))
{
    unsigned int *ssp;
    sigcontext_t new_sct;

    copy_context(&new_sct, scp, 0);
    ssp = SEL_ADR(_ss,_esp);
    decode_exc(&new_sct, ssp);
    if (!cbk(&new_sct)) {
        int is_32 = (ssp[9] & 0xffff) > 0;
        /* if not handled, we push old addr and return to it */
        if (is_32) {
            D_printf("MSDOS: chain exception to %x:%x\n",
                    pma->selector, pma->offset32);
            *--ssp = pma->selector;
            *--ssp = pma->offset32;
            _esp -= 8;
        } else {
            *--ssp = (pma->selector << 16) | pma->offset32;
            _LWORD(esp) -= 4;
        }
        return;
    }
    encode_exc(&new_sct, ssp);
    copy_gp(scp, &new_sct);
    _esp += 0x20; // skip legacy frame
}

void msdos_fault_handler(sigcontext_t *scp, void *arg)
{
    do_fault(scp, arg, msdos_fault);
}

void msdos_pagefault_handler(sigcontext_t *scp, void *arg)
{
    do_fault(scp, arg, msdos_ldt_pagefault);
}
