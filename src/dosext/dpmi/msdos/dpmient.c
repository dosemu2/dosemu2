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
 * Purpose: DPMI entry helper for int27/1687h.
 *
 * Author: Stas Sergeev
 *
 */
#include "dosemu_debug.h"
#include "msdoshlp.h"
#include "dpmi_api.h"
#include "hlpmisc.h"
#include "msdos_priv.h"
#include "dpmient.h"

static unsigned helper;

struct pmaddr16_s {
    uint16_t offset16;
    uint16_t selector;
};

static void do_call_stack(sigcontext_t *scp, far_t dst,
	__dpmi_regs *rmreg, int words, const void *data)
{
    unsigned short *sp = SEL_ADR_CLNT(_ss, _esp, 0);
    const unsigned short *d = data;
    int i;

    for (i = 0; i < words; i++)
	*--sp = d[words - 1 - i];
    _LWORD(esp) -= words * 2;
    RMREG(ss) = 0;
    RMREG(sp) = 0;
    RMREG(cs) = dst.segment;
    RMREG(ip) = dst.offset;
    doshlp_dpmient(scp, 0, rmreg, words);
}

static void dpmient_thr(Bit16u offs, void *sc, void *arg)
{
    sigcontext_t *scp = sc;
    struct pmaddr16_s stk;
    __dpmi_regs rmreg = {};
    int is_32 = msdos_is_32();
    unsigned short *sp = SEL_ADR_CLNT(_ss, _esp, is_32);

    D_printf("dpmient helper entry\n");
    /* push arguments */
    *--sp = _ds;
    _LWORD(esp) -= 2;
    stk.selector = _ss;
    stk.offset16 = _LWORD(esp);
    pm_to_rm_regs(scp, &rmreg, ~0);
    /* copy return addr to RM stack so that it can jump to it */
    do_call_stack(scp, get_dpmient_helper(), &rmreg, 2, &stk);
}

static void dpmient_post(sigcontext_t *scp)
{
    dpmienthlp_post(scp);
    _LWORD(esp) += 6;
}

struct pmaddr_s get_dpmient_handler(void)
{
    return doshlp_get_entry(helper);
}

void dpmienthlp_init(void)
{
    helper = doshlp_setup_simple("msdos dpmient", dpmient_thr);
    doshlp_register_post_handler(DPMIENT_RET, dpmient_post);
}
