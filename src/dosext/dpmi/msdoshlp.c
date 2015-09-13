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
 * Purpose: glue between msdos.c and the rest of dosemu
 * This is needed to keep msdos.c portable to djgpp.
 *
 * Author: Stas Sergeev
 *
 * Currently there are only helper stubs here.
 * The helpers itself are in bios.S.
 * TODO: port bios.S asm helpers to C and put here
 */

#include "cpu.h"
#include "utilities.h"
#include "dpmi.h"
#include "dpmisel.h"
#include "msdoshlp.h"


static const struct msdos_ops *msdos;

void doshlp_init(const struct msdos_ops *ops)
{
    msdos = ops;
}

void lrhlp_setup(far_t rmcb)
{
#define MK_LR_OFS(ofs) ((long)(ofs)-(long)MSDOS_lr_start)
    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_ip)), rmcb.offset);
    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_cs)), rmcb.segment);
}

void lwhlp_setup(far_t rmcb)
{
#define MK_LW_OFS(ofs) ((long)(ofs)-(long)MSDOS_lw_start)
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_ip)),
	       rmcb.offset);
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_cs)),
	       rmcb.segment);
}

void exechlp_setup(void)
{
#define MK_EX_OFS(ofs) ((long)(ofs)-(long)MSDOS_exec_start)
    far_t rma;
    struct pmaddr_s pma;
    int len = DPMI_get_save_restore_address(&rma, &pma);
    WRITE_WORD(SEGOFF2LINEAR(BIOSSEG,
	       DOS_EXEC_OFF + MK_EX_OFS(MSDOS_exec_entry_ip)), rma.offset);
    WRITE_WORD(SEGOFF2LINEAR(BIOSSEG,
	       DOS_EXEC_OFF + MK_EX_OFS(MSDOS_exec_entry_cs)), rma.segment);
    WRITE_WORD(SEGOFF2LINEAR(BIOSSEG,
	       DOS_EXEC_OFF + MK_EX_OFS(MSDOS_exec_buf_sz)), len);
}

far_t allocate_realmode_callback(void (*handler)(
	struct RealModeCallStructure *))
{
    return DPMI_allocate_realmode_callback(dpmi_sel(),
	    DPMI_SEL_OFF(MSDOS_rmcb_call), dpmi_data_sel(),
	    DPMI_DATA_OFF(MSDOS_rmcb_data));
}

struct pmaddr_s get_pm_handler(void (*handler)(struct sigcontext *))
{
    struct pmaddr_s ret = {};
    if (handler == msdos->api_call) {
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_API_call);
    } else {
	dosemu_error("unknown pm handler\n");
    }
    return ret;
}

struct pmaddr_s get_pmrm_handler(void (*handler)(
	struct RealModeCallStructure *))
{
    struct pmaddr_s ret = {};
    if (handler == msdos->xms_call) {
	ret.selector = dpmi_sel();
	ret.offset = DPMI_SEL_OFF(MSDOS_XMS_call);
    } else {
	dosemu_error("unknown pmrm handler\n");
    }
    return ret;
}

far_t get_rm_handler(int (*handler)(struct sigcontext *,
	const struct RealModeCallStructure *))
{
    far_t ret = {};
    if (handler == msdos->mouse_callback) {
	ret.segment = DPMI_SEG;
	ret.offset = DPMI_OFF + HLT_OFF(MSDOS_mouse_callback);
    } else if (handler == msdos->ps2_mouse_callback) {
	ret.segment = DPMI_SEG;
	ret.offset = DPMI_OFF + HLT_OFF(MSDOS_PS2_mouse_callback);
    } else {
	dosemu_error("unknown rm handler\n");
    }
    return ret;
}

far_t get_lr_helper(void)
{
    return (far_t){ .segment = DOS_LONG_READ_SEG,
	    .offset = DOS_LONG_READ_OFF };
}

far_t get_lw_helper(void)
{
    return (far_t){ .segment = DOS_LONG_WRITE_SEG,
	    .offset = DOS_LONG_WRITE_OFF };
}

far_t get_exec_helper(void)
{
    return (far_t){ .segment = BIOSSEG,
	    .offset = DOS_EXEC_OFF };
}

void msdos_pm_call(struct sigcontext *scp, int is_32)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_API_call)) {
	msdos->api_call(scp);
    } else if (_eip == 1 + DPMI_SEL_OFF(MSDOS_rmcb_call)) {
	struct RealModeCallStructure *rmreg = SEL_ADR_CLNT(_es, _edi, is_32);
	msdos->rmcb_handler(rmreg);
    } else {
	error("MSDOS: unknown pm call %#x\n", _eip);
    }
}

int msdos_pre_pm(struct sigcontext *scp,
		 struct RealModeCallStructure *rmreg)
{
    if (_eip == 1 + DPMI_SEL_OFF(MSDOS_XMS_call)) {
	msdos->xms_call(rmreg);
    } else {
	error("MSDOS: unknown pm call %#x\n", _eip);
	return 0;
    }
    return 1;
}

int msdos_pre_rm(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg)
{
    int ret = 0;
    unsigned int lina = SEGOFF2LINEAR(rmreg->cs, rmreg->ip) - 1;

    if (lina == DPMI_ADD + HLT_OFF(MSDOS_mouse_callback))
	ret = msdos->mouse_callback(scp, rmreg);
    else if (lina == DPMI_ADD + HLT_OFF(MSDOS_PS2_mouse_callback))
	ret = msdos->ps2_mouse_callback(scp, rmreg);

    return ret;
}
