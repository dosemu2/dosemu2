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
 * Purpose: XMS realmode call interface
 *
 * Author: Stas Sergeev
 *
 */
#include "dosemu_debug.h"
#include "msdoshlp.h"
#include "dpmi_api.h"
#include "hlpmisc.h"
#include "msdos_priv.h"
#include "xms_msdos.h"

static struct dos_helper_s helper;

static void msdos_pre_xms(const cpuctx_t *scp,
	__dpmi_regs *rmreg, unsigned short rm_seg, int *r_mask)
{
    int rm_mask = *r_mask;
    x_printf("in msdos_pre_xms for function %02X\n", _HI_(ax));
    switch (_HI_(ax)) {
    case 0x0b:
	RMPRESERVE1(esi);
	SET_RMREG(ds, rm_seg);
	SET_RMLWORD(si, 0);
	MEMCPY_2DOS(SEGOFF2LINEAR(rm_seg, 0),
			SEL_ADR_CLNT(_ds_, _esi_, msdos_is_32()), 0x10);
	break;
    case 0x89:
	X_RMREG(edx) = _edx_;
	break;
    case 0x8F:
	X_RMREG(ebx) = _ebx_;
	break;
    }
    *r_mask = rm_mask;
}

static void msdos_post_xms(cpuctx_t *scp,
	const __dpmi_regs *rmreg, int *r_mask)
{
    int rm_mask = *r_mask;
    x_printf("in msdos_post_xms for function %02X\n", _HI_(ax));
    switch (_HI_(ax)) {
    case 0x0b:
	RMPRESERVE1(esi);
	break;
    case 0x88:
	_eax = X_RMREG(eax);
	_ecx = X_RMREG(ecx);
	_edx = X_RMREG(edx);
	break;
    case 0x8E:
	_edx = X_RMREG(edx);
	break;
    }
    *r_mask = rm_mask;
}

static void xms_call(const cpuctx_t *scp,
	__dpmi_regs *rmreg, unsigned short rm_seg)
{
    int rmask = (1 << cs_INDEX) |
	(1 << eip_INDEX) | (1 << ss_INDEX) | (1 << esp_INDEX);
    msdos_pre_xms(scp, rmreg, rm_seg, &rmask);
    pm_to_rm_regs(scp, rmreg, ~rmask);
}

static void xms_ret(cpuctx_t *scp, const __dpmi_regs *rmreg)
{
    int rmask = 0;
    msdos_post_xms(scp, rmreg, &rmask);
    rm_to_pm_regs(scp, rmreg, ~rmask);
    D_printf("MSDOS: XMS call return\n");
}

struct __attribute__ ((__packed__)) EMM {
   unsigned int Length;
   unsigned short SourceHandle;
   unsigned int SourceOffset;
   unsigned short DestHandle;
   unsigned int DestOffset;
};

static void do_xms_call(cpuctx_t *scp)
{
    cpuctx_t sa = *scp;
    __dpmi_regs rmreg = {};
    int is_32 = msdos_is_32();
    unsigned short rm_seg = helper.rm_seg(scp, 0, helper.rm_arg);
    xms_call(scp, &rmreg, rm_seg);
    do_call_to(scp, is_32, get_xms_call(), &rmreg);
    *scp = sa;
    xms_ret(scp, &rmreg);
}

static dosaddr_t xms_map(cpuctx_t *scp, unsigned handle, unsigned len)
{
    cpuctx_t sa = *scp;
    dosaddr_t ret = -1;
    unsigned pa, sz;

    _LWORD(eax) = 0x0e00;  // get emb info
    _LWORD(edx) = handle;
    do_xms_call(scp);
    if (_LWORD(eax) != 1)
        goto out;
    sz = _LWORD(edx) * 1024;
    if (len > sz)
        goto out;
    _LWORD(eax) = 0x0c00;  // lock
    _LWORD(edx) = handle;
    do_xms_call(scp);
    if (_LWORD(eax) != 1)
        goto out;
    pa = (_LWORD(edx) << 16) | _LWORD(ebx);
    ret = DPMIMapHWRam(pa & _PAGE_MASK, PAGE_ALIGN(sz));
    if (ret == (dosaddr_t)-1 || (ret & (PAGE_SIZE - 1)))
        goto out;
    ret += pa & (PAGE_SIZE - 1);
out:
    *scp = sa;
    return ret;
}

static void xms_unmap(cpuctx_t *scp, unsigned handle, dosaddr_t va)
{
    int err;
    cpuctx_t sa = *scp;
    err = DPMIUnmapHWRam(va & _PAGE_MASK);
    if (err)
        error("error unmapping hwram\n");
    _LWORD(eax) = 0x0d00;  // unlock
    _LWORD(edx) = handle;
    do_xms_call(scp);
    if (_LWORD(eax) != 1)
        error("error unlocking emb\n");
    *scp = sa;
}

static void xmshlp_thr(void *arg)
{
    cpuctx_t *scp = arg;
    int is_32 = msdos_is_32();

    if (_HI_(ax) == 0x0b) {
        struct EMM *e = SEL_ADR_CLNT(_ds_, _esi_, is_32);
        if (e->SourceHandle == 0 || e->DestHandle == 0) {
            dosaddr_t src = -1, dst = -1;
            unsigned char *s = NULL, *d = NULL;
            if (e->SourceHandle != 0) {
                src = xms_map(scp, e->SourceHandle,
                              e->SourceOffset + e->Length);
                if (src != (dosaddr_t)-1)
                    s = MEM_BASE32(src + e->SourceOffset);
            } else {
                s = SEL_ADR_CLNT(_ds_, e->SourceOffset, is_32);
            }
            if (!s) {
                _LWORD(eax) = 0;
                _LWORD(ebx) = 0xa7;
                return;
            }
            if (e->DestHandle != 0) {
                dst = xms_map(scp, e->DestHandle, e->DestOffset + e->Length);
                if (dst != (dosaddr_t)-1)
                    d = MEM_BASE32(dst + e->DestOffset);
            } else {
                d = SEL_ADR_CLNT(_ds_, e->DestOffset, is_32);
            }
            if (!d) {
                _LWORD(eax) = 0;
                _LWORD(ebx) = 0xa7;
                return;
            }
            memcpy(d, s, e->Length);
            if (e->SourceHandle != 0)
                xms_unmap(scp, e->SourceHandle, src);
            if (e->DestHandle != 0)
                xms_unmap(scp, e->DestHandle, dst);
            _LWORD(eax) = 1;
            return;
        }
    }

    do_xms_call(scp);
}

struct pmaddr_s get_xms_handler(void)
{
    return doshlp_get_entry(helper.entry);
}

void xmshlp_init(void)
{
    doshlp_setup_retf(&helper, "msdos xms thr", xmshlp_thr, scratch_seg, NULL);
}
