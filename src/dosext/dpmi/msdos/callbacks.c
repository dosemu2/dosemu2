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
#include "dpmi.h"
#include "msdoshlp.h"
#include "callbacks.h"

static dosaddr_t io_buffer;
static int io_buffer_size;
static int io_error;
static uint16_t io_error_code;

void set_io_buffer(dosaddr_t ptr, unsigned int size)
{
    io_buffer = ptr;
    io_buffer_size = size;
    io_error = 0;
}

void unset_io_buffer(void)
{
    io_buffer_size = 0;
}

int is_io_error(uint16_t * r_code)
{
    if (io_error && r_code)
	*r_code = io_error_code;
    return io_error;
}

static void do_call(int cs, int ip, struct RealModeCallStructure *rmreg,
		    int rmask)
{
    unsigned int ssp, sp;

    ssp = SEGOFF2LINEAR(READ_RMREG(ss, rmask), 0);
    sp = READ_RMREG(sp, rmask);

    g_printf("fake_call() CS:IP %04x:%04x\n", cs, ip);
    pushw(ssp, sp, cs);
    pushw(ssp, sp, ip);
    RMREG(sp) -= 4;
}

static void do_call_to(int cs, int ip, struct RealModeCallStructure *rmreg,
		       int rmask)
{
    do_call(READ_RMREG(cs, rmask), READ_RMREG(ip, rmask), rmreg, rmask);
    RMREG(cs) = cs;
    RMREG(ip) = ip;
}

static void do_retf(struct RealModeCallStructure *rmreg, int rmask)
{
    unsigned int ssp, sp;

    ssp = SEGOFF2LINEAR(READ_RMREG(ss, rmask), 0);
    sp = READ_RMREG(sp, rmask);

    RMREG(ip) = popw(ssp, sp);
    RMREG(cs) = popw(ssp, sp);
    RMREG(sp) += 4;
}

static void rmcb_ret_handler(struct sigcontext *scp,
		      struct RealModeCallStructure *rmreg, int is_32)
{
    do_retf(rmreg, (1 << ss_INDEX) | (1 << esp_INDEX));
}

static void rmcb_ret_from_ps2(struct sigcontext *scp,
		       struct RealModeCallStructure *rmreg, int is_32)
{
    if (is_32)
	_esp += 16;
    else
	_LWORD(esp) += 8;
    do_retf(rmreg, (1 << ss_INDEX) | (1 << esp_INDEX));
}

void rm_to_pm_regs(struct sigcontext *scp,
			  const struct RealModeCallStructure *rmreg,
			  unsigned int mask)
{
    /* WARNING - realmode flags can contain the dreadful NT flag
     * if we don't use safety masks. */
    if (mask & (1 << eflags_INDEX))
	_eflags = 0x0202 | (0x0dd5 & RMREG(flags));
    if (mask & (1 << eax_INDEX))
	_eax = RMLWORD(ax);
    if (mask & (1 << ebx_INDEX))
	_ebx = RMLWORD(bx);
    if (mask & (1 << ecx_INDEX))
	_ecx = RMLWORD(cx);
    if (mask & (1 << edx_INDEX))
	_edx = RMLWORD(dx);
    if (mask & (1 << esi_INDEX))
	_esi = RMLWORD(si);
    if (mask & (1 << edi_INDEX))
	_edi = RMLWORD(di);
    if (mask & (1 << ebp_INDEX))
	_ebp = RMLWORD(bp);
}

static void pm_to_rm_regs(const struct sigcontext *scp,
			  struct RealModeCallStructure *rmreg,
			  unsigned int mask)
{
  if (mask & (1 << eflags_INDEX))
    RMREG(flags) = _eflags;
  if (mask & (1 << eax_INDEX))
    RMLWORD(ax) = _eax;
  if (mask & (1 << ebx_INDEX))
    RMLWORD(bx) = _ebx;
  if (mask & (1 << ecx_INDEX))
    RMLWORD(cx) = _ecx;
  if (mask & (1 << edx_INDEX))
    RMLWORD(dx) = _edx;
  if (mask & (1 << esi_INDEX))
    RMLWORD(si) = _esi;
  if (mask & (1 << edi_INDEX))
    RMLWORD(di) = _edi;
  if (mask & (1 << ebp_INDEX))
    RMLWORD(bp) = _ebp;
}

static void mouse_callback(struct sigcontext *scp,
		    const struct RealModeCallStructure *rmreg,
		    int is_32, void *arg)
{
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    struct pmaddr_s *mouseCallBack = arg;

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

static void ps2_mouse_callback(struct sigcontext *scp,
			const struct RealModeCallStructure *rmreg,
			int is_32, void *arg)
{
    unsigned short *rm_ssp;
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
    struct pmaddr_s *PS2mouseCallBack = arg;

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

void xms_call(const struct sigcontext *scp,
	struct RealModeCallStructure *rmreg, void *arg)
{
    far_t *XMS_call = arg;
    int rmask = (1 << cs_INDEX) |
	(1 << eip_INDEX) | (1 << ss_INDEX) | (1 << esp_INDEX);
    D_printf("MSDOS: XMS call to 0x%x:0x%x\n",
	     XMS_call->segment, XMS_call->offset);
    pm_to_rm_regs(scp, rmreg, ~rmask);
    do_call_to(XMS_call->segment, XMS_call->offset, rmreg, rmask);
}

void xms_ret(struct sigcontext *scp, const struct RealModeCallStructure *rmreg)
{
    rm_to_pm_regs(scp, rmreg, ~0);
    D_printf("MSDOS: XMS call return\n");
}

static void rmcb_handler(struct sigcontext *scp,
		  const struct RealModeCallStructure *rmreg, int is_32,
		  void *arg)
{
    switch (RM_LO(ax)) {
    case 0:			/* read */
	{
	    unsigned int offs = E_RMREG(edi);
	    unsigned int size = E_RMREG(ecx);
	    unsigned int dos_ptr = SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx));
	    D_printf("MSDOS: read %x %x\n", offs, size);
	    /* need to use copy function that takes VGA mem into account */
	    if (offs + size <= io_buffer_size)
		memcpy_dos2dos(io_buffer + offs, dos_ptr, size);
	    else
		error("MSDOS: bad read (%x %x %x)\n", offs, size,
		      io_buffer_size);
	    break;
	}
    case 1:			/* write */
	{
	    unsigned int offs = E_RMREG(edi);
	    unsigned int size = E_RMREG(ecx);
	    unsigned int dos_ptr = SEGOFF2LINEAR(RMREG(ds), RMLWORD(dx));
	    D_printf("MSDOS: write %x %x\n", offs, size);
	    /* need to use copy function that takes VGA mem into account */
	    if (offs + size <= io_buffer_size)
		memcpy_dos2dos(dos_ptr, io_buffer + offs, size);
	    else
		error("MSDOS: bad write (%x %x %x)\n", offs, size,
		      io_buffer_size);
	    break;
	}
    case 2:			/* error */
	io_error = 1;
	io_error_code = RMLWORD(cx);
	D_printf("MSDOS: set I/O error %x\n", io_error_code);
	break;
    default:
	error("MSDOS: unknown rmcb 0x%x\n", RM_LO(ax));
	break;
    }
}

void msdos_api_call(struct sigcontext *scp, void *arg)
{
    u_short *ldt_alias = arg;

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

void msdos_api_winos2_call(struct sigcontext *scp, void *arg)
{
    u_short *ldt_alias_winos2 = arg;

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

static void (*rmcb_handlers[])(struct sigcontext *scp,
		 const struct RealModeCallStructure *rmreg,
		 int is_32, void *arg) = {
    [RMCB_IO] = rmcb_handler,
    [RMCB_MS] = mouse_callback,
    [RMCB_PS2MS] = ps2_mouse_callback,
};

static void (*rmcb_ret_handlers[])(struct sigcontext *scp,
		 struct RealModeCallStructure *rmreg, int is_32) = {
    [RMCB_IO] = rmcb_ret_handler,
    [RMCB_MS] = rmcb_ret_handler,
    [RMCB_PS2MS] = rmcb_ret_from_ps2,
};

void callbacks_init(unsigned short rmcb_sel, void *(*cbk_args)(int),
	far_t *r_cbks)
{
    int i;
    for (i = 0; i < MAX_RMCBS; i++) {
	struct pmaddr_s pma = get_pmcb_handler(rmcb_handlers[i], cbk_args(i),
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
