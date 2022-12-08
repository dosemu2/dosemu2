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
 * Purpose: realmode callbacks for msdos PM API
 *
 * Author: Stas Sergeev
 *
 */
#include <sys/types.h>
#include <signal.h>
#include "cpu.h"
#include "memory.h"
#include "dosemu_debug.h"
#include "dos2linux.h"
#include "emudpmi.h"
#include "msdoshlp.h"
#include "msdos_priv.h"
#include "callbacks.h"

static void do_retf(struct RealModeCallStructure *rmreg, int rmask)
{
    unsigned int ssp, sp;

    ssp = SEGOFF2LINEAR(READ_RMREG(ss, rmask), 0);
    sp = READ_RMREG(sp, rmask);

    RMREG(ip) = popw(ssp, sp);
    RMREG(cs) = popw(ssp, sp);
    RMREG(sp) += 4;
}

static void rmcb_ret_handler(cpuctx_t *scp,
		      struct RealModeCallStructure *rmreg, int is_32)
{
    do_retf(rmreg, (1 << ss_INDEX) | (1 << esp_INDEX));
}

static void rmcb_ret_from_ps2(cpuctx_t *scp,
		       struct RealModeCallStructure *rmreg, int is_32)
{
    if (is_32)
	_esp += 16;
    else
	_LWORD(esp) += 8;
    do_retf(rmreg, (1 << ss_INDEX) | (1 << esp_INDEX));
}

static void rm_to_pm_regs(cpuctx_t *scp,
			  const struct RealModeCallStructure *rmreg,
			  unsigned int mask)
{
    /* WARNING - realmode flags can contain the dreadful NT flag
     * if we don't use safety masks. */
    if (mask & (1 << eflags_INDEX))
	_eflags = flags_to_pm(RMREG(flags));
    if (mask & (1 << eax_INDEX))
	_LWORD(eax) = RMLWORD(ax);
    if (mask & (1 << ebx_INDEX))
	_LWORD(ebx) = RMLWORD(bx);
    if (mask & (1 << ecx_INDEX))
	_LWORD(ecx) = RMLWORD(cx);
    if (mask & (1 << edx_INDEX))
	_LWORD(edx) = RMLWORD(dx);
    if (mask & (1 << esi_INDEX))
	_LWORD(esi) = RMLWORD(si);
    if (mask & (1 << edi_INDEX))
	_LWORD(edi) = RMLWORD(di);
    if (mask & (1 << ebp_INDEX))
	_LWORD(ebp) = RMLWORD(bp);
}

static void pm_to_rm_regs(const cpuctx_t *scp,
			  struct RealModeCallStructure *rmreg,
			  unsigned int mask)
{
  if (mask & (1 << eflags_INDEX))
    RMREG(flags) = flags_to_rm(_eflags_);
  if (mask & (1 << eax_INDEX))
    X_RMREG(eax) = _LWORD_(eax_);
  if (mask & (1 << ebx_INDEX))
    X_RMREG(ebx) = _LWORD_(ebx_);
  if (mask & (1 << ecx_INDEX))
    X_RMREG(ecx) = _LWORD_(ecx_);
  if (mask & (1 << edx_INDEX))
    X_RMREG(edx) = _LWORD_(edx_);
  if (mask & (1 << esi_INDEX))
    X_RMREG(esi) = _LWORD_(esi_);
  if (mask & (1 << edi_INDEX))
    X_RMREG(edi) = _LWORD_(edi_);
  if (mask & (1 << ebp_INDEX))
    X_RMREG(ebp) = _LWORD_(ebp_);
}

static void mouse_callback(cpuctx_t *scp,
		    const struct RealModeCallStructure *rmreg,
		    int is_32, void *arg)
{
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    void *(*cb)(int) = arg;
    const struct pmaddr_s *mouseCallBack = cb(RMCB_MS);

    if (!ValidAndUsedSelector(mouseCallBack->selector)) {
	D_printf("MSDOS: ERROR: mouse callback to unused segment\n");
	return;
    }
    D_printf("MSDOS: starting mouse callback\n");

    if (is_32) {
	unsigned int *ssp = sp;
	*--ssp = _cs;
	*--ssp = _eip;
	_esp -= 8;
    } else {
	unsigned short *ssp = sp;
	*--ssp = _cs;
	*--ssp = _LWORD(eip);
	_LWORD(esp) -= 4;
    }

    rm_to_pm_regs(scp, rmreg, ~(1 << ebp_INDEX));
    _ds = ConvertSegmentToDescriptor(RMREG(ds));
    _cs = mouseCallBack->selector;
    _eip = mouseCallBack->offset;
}

static void ps2_mouse_callback(cpuctx_t *scp,
			const struct RealModeCallStructure *rmreg,
			int is_32, void *arg)
{
    unsigned short *rm_ssp;
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    void *(*cb)(int) = arg;
    const struct pmaddr_s *PS2mouseCallBack = cb(RMCB_PS2MS);

    if (!ValidAndUsedSelector(PS2mouseCallBack->selector)) {
	D_printf("MSDOS: ERROR: PS2 mouse callback to unused segment\n");
	return;
    }
    D_printf("MSDOS: starting PS2 mouse callback\n");

    rm_ssp = MK_FP32(RMREG(ss), RMREG(sp) + 4 + 8);
    if (is_32) {
	unsigned int *ssp = sp;
	*--ssp = *--rm_ssp;
	D_printf("data: 0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x\n", *ssp);
	*--ssp = _cs;
	*--ssp = _eip;
	_esp -= 24;
    } else {
	unsigned short *ssp = sp;
	*--ssp = *--rm_ssp;
	D_printf("data: 0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x ", *ssp);
	*--ssp = *--rm_ssp;
	D_printf("0x%x\n", *ssp);
	*--ssp = _cs;
	*--ssp = _LWORD(eip);
	_LWORD(esp) -= 12;
    }

    _cs = PS2mouseCallBack->selector;
    _eip = PS2mouseCallBack->offset;
}

struct pmrm_ret msdos_ext_call(cpuctx_t *scp,
	struct RealModeCallStructure *rmreg,
	unsigned short rm_seg, void *(*arg)(int), int off)
{
    const DPMI_INTDESC *prev = arg(off);
    struct pmrm_ret ret = {};
    int rmask = (1 << cs_INDEX) |
	(1 << eip_INDEX) | (1 << ss_INDEX) | (1 << esp_INDEX);
    ret.inum = msdos_get_int_num(off);
    ret.ret = msdos_pre_extender(scp, rmreg, ret.inum, rm_seg, &rmask,
	    &ret.faddr);
    pm_to_rm_regs(scp, rmreg, ~rmask);
    ret.prev = *prev;
    return ret;
}

struct pext_ret msdos_ext_ret(cpuctx_t *scp,
	const struct RealModeCallStructure *rmreg,
	unsigned short rm_seg, int off)
{
    struct pext_ret ret;
    int rmask = (1 << cs_INDEX) |
	(1 << eip_INDEX) | (1 << ss_INDEX) | (1 << esp_INDEX);
    ret.ret = msdos_post_extender(scp, rmreg, msdos_get_int_num(off),
	    rm_seg, &rmask, &ret.arg);
    rm_to_pm_regs(scp, rmreg, rmask);
    return ret;
}

void msdos_api_call(cpuctx_t *scp, void *arg)
{
    u_short *(*cb)(void) = arg;
    const u_short *ldt_alias = cb();

    D_printf("MSDOS: extension API call: 0x%04x\n", _LWORD(eax));
    if (_LWORD(eax) == 0x0100) {
	u_short sel = *ldt_alias;
	if (sel) {
	    _eax = sel;
	    _eflags &= ~CF;
	} else {
	    _eflags |= CF;
	}
    } else {
	_eflags |= CF;
    }
}

void msdos_api_winos2_call(cpuctx_t *scp, void *arg)
{
    u_short *(*cb)(void) = arg;
    const u_short *ldt_alias_winos2 = cb();

    D_printf("MSDOS: WINOS2 extension API call: 0x%04x\n", _LWORD(eax));
    if (_LWORD(eax) == 0x0100) {
	u_short sel = *ldt_alias_winos2;
	if (sel) {
	    _eax = sel;
	    _eflags &= ~CF;
	} else {
	    _eflags |= CF;
	}
    } else {
	_eflags |= CF;
    }
}

static void (*rmcb_handlers[])(cpuctx_t *scp,
		 const struct RealModeCallStructure *rmreg,
		 int is_32, void *arg) = {
    [RMCB_MS] = mouse_callback,
    [RMCB_PS2MS] = ps2_mouse_callback,
};

static void (*rmcb_ret_handlers[])(cpuctx_t *scp,
		 struct RealModeCallStructure *rmreg, int is_32) = {
    [RMCB_MS] = rmcb_ret_handler,
    [RMCB_PS2MS] = rmcb_ret_from_ps2,
};

void callbacks_init(unsigned short rmcb_sel, void *(*cbk_args)(int),
	far_t *r_cbks)
{
    int i;
    for (i = 0; i < MAX_RMCBS; i++) {
	struct pmaddr_s pma = get_pmcb_handler(rmcb_handlers[i], cbk_args,
		rmcb_ret_handlers[i], i);
	r_cbks[i] = DPMI_allocate_realmode_callback(pma.selector, pma.offset,
		rmcb_sel, 0);
    }
}

void callbacks_done(far_t *cbks)
{
    int i;
    for (i = 0; i < MAX_RMCBS; i++)
	DPMI_free_realmode_callback(cbks[i].segment, cbks[i].offset);
}
