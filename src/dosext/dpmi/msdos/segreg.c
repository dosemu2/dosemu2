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
#include "instr_dec.h"
#include "msdos_ldt.h"
#include "msdos_priv.h"
#include "segreg.h"

enum MfRet { MFR_HANDLED, MFR_NOT_HANDLED, MFR_ERROR };

static enum MfRet msdos_sel_fault(struct sigcontext *scp)
{
    struct sigcontext new_sct;
    int reg;
    unsigned int segment;
    unsigned short desc;

    D_printf("MSDOS: msdos_fault, err=%#lx\n", _err);
    if ((_err & 0xffff) == 0)	/*  not a selector error */
	return MFR_NOT_HANDLED;

    /* now it is a invalid selector error, try to fix it if it is */
    /* caused by an instruction such as mov Sreg,r/m16            */
#define ALL_GDTS 0
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

int msdos_fault(struct sigcontext *scp)
{
    enum MfRet ret;

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

    return 0;
}
