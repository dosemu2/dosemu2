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
 * Purpose: RSP startup
 *
 * Author: Stas Sergeev
 *
 */
#include <assert.h>
#include "dosemu_debug.h"
#include "msdoshlp.h"
#include "msdos_priv.h"
#include "dpmi_api.h"
#include "rsp.h"

static struct dos_helper_s hlp16, hlp32;

static void do_retf(sigcontext_t *scp)
{
    int is_32 = _LWORD(eax);	// from do_common_start()
    void *sp = SEL_ADR(_ss, _esp);

    if (is_32) {
	unsigned int *ssp = sp;
	_eip = *ssp++;
	_cs = *ssp++;
	_esp += 8;
    } else {
	unsigned short *ssp = sp;
	_LWORD(eip) = *ssp++;
	_cs = *ssp++;
	_LWORD(esp) += 4;
    }
}

static unsigned short get_psp(sigcontext_t *scp, int is_32)
{
    __dpmi_regs r = {};

    r.h.ah = 0x62;
    _dpmi_simulate_real_mode_interrupt(scp, is_32, 0x21, &r);
    if (r.x.flags & CF) {
	error("get_psp failed\n");
	return 0;
    }
    return r.x.bx;
}

static void do_common_start(sigcontext_t *scp, int is_32)
{
    unsigned short psp;

    switch (_LWORD(eax)) {
    case 0:
	psp = get_psp(scp, is_32);
	msdos_init(_LWORD(ebx), is_32, _LWORD(edx), psp, _LWORD(ecx));
	break;
    case 1:
	msdos_done(_LWORD(ecx));
	break;
    case 2:
	msdos_set_client(_LWORD(ebx));
	break;
    default:
	error("unsupported rsp %i\n", _LWORD(eax));
	break;
    }

    _LWORD(eax) = is_32;
}

static void do_start16(void *arg)
{
    do_common_start(arg, 0);
}

static void do_start32(void *arg)
{
    do_common_start(arg, 1);
}

void rsp_setup(void)
{
    doshlp_setup(&hlp16, "msdos rsp16 thr", do_start16, do_retf);
    doshlp_setup(&hlp32, "msdos rsp32 thr", do_start32, do_retf);
}

void rsp_init(void)
{
    struct pmaddr_s rsp16, rsp32;
    struct RSPcall_s rsp = {};
    int err;

    rsp16 = doshlp_get_entry16(hlp16.entry);
    rsp32 = doshlp_get_entry32(hlp32.entry);
    err = GetDescriptor(rsp16.selector, (unsigned *)rsp.code16);
    assert(!err);
    rsp.ip = rsp16.offset;
    err = GetDescriptor(rsp32.selector, (unsigned *)rsp.code32);
    assert(!err);
    rsp.eip = rsp32.offset;
    /* FIXME: maybe fill data descs too? */
    rsp.flags = RSP_F_SW | RSP_F_LOWMEM;
    rsp.para = msdos_get_lowmem_size();
    err = dpmi_install_rsp(&rsp);
    assert(!err);
}
