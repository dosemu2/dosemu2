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
 * Purpose: long i/o helpers
 *
 * Author: Stas Sergeev
 */
#ifdef DOSEMU
#include "sig.h"
#include "utilities.h"
#endif
#include "dos2linux.h"
#include "emudpmi.h"
#include "dpmi_api.h"
#include "msdoshlp.h"
#include "hlpmisc.h"
#include "lio.h"

#define D_16_32(x) (is_32 ? (x) : (x) & 0xffff)

enum { DOSHLP_LR, DOSHLP_LW, LIOHLP_MAX };

static struct dos_helper_s helpers[LIOHLP_MAX];

struct liohlp_priv {
    unsigned short rm_seg;
    void (*post)(cpuctx_t *);
    int is_32;
};
static struct liohlp_priv lio_priv[LIOHLP_MAX];

static void lrhlp_thr(void *arg);
static void lwhlp_thr(void *arg);
struct hlp_hndl {
    void (*thr)(void *arg);
    const char *name;
};
static const struct hlp_hndl hlp_thr[LIOHLP_MAX] = {
    { lrhlp_thr, "msdos lr thr" },
    { lwhlp_thr, "msdos lw thr" },
};

static unsigned short lio_rmseg(cpuctx_t *scp, int off, void *arg)
{
    struct liohlp_priv *h = arg;
    return h->rm_seg;
}

static void liohlp_setup(int hlp, int is_32,
	unsigned short rm_seg, void (*post)(cpuctx_t *))
{
    struct liohlp_priv *h = &lio_priv[hlp];

    h->rm_seg = rm_seg;
    h->post = post;
    h->is_32 = is_32;
}

static void do_callf(cpuctx_t *scp, int is_32, struct pmaddr_s pma)
{
    void *sp = SEL_ADR_CLNT(_ss, _esp, is_32);
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
    _cs = pma.selector;
    _eip = pma.offset;
}

void msdos_lr_helper(cpuctx_t *scp, int is_32,
	unsigned short rm_seg, void (*post)(cpuctx_t *))
{
    liohlp_setup(DOSHLP_LR, is_32, rm_seg, post);
    do_callf(scp, is_32, doshlp_get_entry(helpers[DOSHLP_LR].entry));
}

void msdos_lw_helper(cpuctx_t *scp, int is_32,
	unsigned short rm_seg, void (*post)(cpuctx_t *))
{
    liohlp_setup(DOSHLP_LW, is_32, rm_seg, post);
    do_callf(scp, is_32, doshlp_get_entry(helpers[DOSHLP_LW].entry));
}

static void do_int_call(cpuctx_t *scp, int is_32, int num,
	__dpmi_regs *rmreg)
{
    RMREG(ss) = 0;
    RMREG(sp) = 0;
    _dpmi_simulate_real_mode_interrupt(scp, is_32, num, rmreg);
}

static void lrhlp_thr(void *arg)
{
    cpuctx_t *scp = arg;
    cpuctx_t sa = *scp;
    struct liohlp_priv *hlp = &lio_priv[DOSHLP_LR];
    __dpmi_regs _rmreg = {};
    __dpmi_regs *rmreg = &_rmreg;
    unsigned short rm_seg = hlp->rm_seg;
    int is_32 = hlp->is_32;
    dosaddr_t buf = GetSegmentBase(_ds) + D_16_32(_edx);
    dosaddr_t dos_buf = SEGOFF2LINEAR(rm_seg, 0);
    int len = D_16_32(_ecx);
    int done = 0;

    if (rm_seg == (unsigned short)-1) {
	error("RM seg not set\n");
	doshlp_quit_dpmi(scp);
	return;
    }
    pm_to_rm_regs(scp, &_rmreg, ~((1 << esi_INDEX) | (1 << edi_INDEX) |
	    (1 << ebp_INDEX) | (1 << edx_INDEX)));
    RMREG(ds) = rm_seg;

    D_printf("MSDOS: going to read %i bytes from fd %i\n", len, _LWORD(ebx));
    if (!len) {
        /* checks handle validity or EOF perhaps */
        do_int_call(scp, is_32, 0x21, &_rmreg);
    }
    while (len) {
        int to_read = _min(len, 0xffff);
        int rd;
        X_RMREG(ecx) = to_read;
        X_RMREG(eax) = 0x3f00;
        do_int_call(scp, is_32, 0x21, &_rmreg);
        if (RMREG(flags) & CF) {
            D_printf("MSDOS: read error %x\n", X_RMREG(eax));
            break;
        }
        if (!X_RMREG(eax)) {
            D_printf("MSDOS: read eof\n");
            break;
        }
        rd = _min(X_RMREG(eax), to_read);
        memcpy_dos2dos(buf + done, dos_buf, rd);
        done += rd;
        len -= rd;
        if (rd < to_read) {
            D_printf("MSDOS: shortened read, done %i remain %i\n", done, len);
            break;
        }
    }

    *scp = sa;
    if (RMREG(flags) & CF) {
        _eflags |= CF;
        _eax = X_RMREG(eax);
    } else {
        _eflags &= ~CF;
        _eax = done;
    }
    if (lio_priv[DOSHLP_LR].post)
        lio_priv[DOSHLP_LR].post(scp);
}

static void lwhlp_thr(void *arg)
{
    cpuctx_t *scp = arg;
    cpuctx_t sa = *scp;
    struct liohlp_priv *hlp = &lio_priv[DOSHLP_LW];
    __dpmi_regs _rmreg = {};
    __dpmi_regs *rmreg = &_rmreg;
    unsigned short rm_seg = hlp->rm_seg;
    int is_32 = hlp->is_32;
    dosaddr_t buf = GetSegmentBase(_ds) + D_16_32(_edx);
    dosaddr_t dos_buf = SEGOFF2LINEAR(rm_seg, 0);
    int len = D_16_32(_ecx);
    int done = 0;

    if (rm_seg == (unsigned short)-1) {
	error("RM seg not set\n");
	doshlp_quit_dpmi(scp);
	return;
    }
    pm_to_rm_regs(scp, &_rmreg, ~((1 << esi_INDEX) | (1 << edi_INDEX) |
	    (1 << ebp_INDEX) | (1 << edx_INDEX)));
    RMREG(ds) = rm_seg;

    D_printf("MSDOS: going to write %i bytes to fd %i\n", len, _LWORD(ebx));
    if (!len) {
        /* truncate */
        do_int_call(scp, is_32, 0x21, &_rmreg);
    }
    while (len) {
        int to_write = _min(len, 0xffff);
        int wr;
        memcpy_dos2dos(dos_buf, buf + done, to_write);
        X_RMREG(ecx) = to_write;
        X_RMREG(eax) = 0x4000;
        do_int_call(scp, is_32, 0x21, &_rmreg);
        if (RMREG(flags) & CF) {
            D_printf("MSDOS: write error %x\n", X_RMREG(eax));
            break;
        }
        if (!X_RMREG(eax)) {
            D_printf("MSDOS: write error, disk full?\n");
            break;
        }
        wr = _min(X_RMREG(eax), to_write);
        done += wr;
        len -= wr;
        if (wr < to_write) {
            error("MSDOS: shortened write, done %i remain %i\n", done, len);
            break;
        }
    }

    *scp = sa;
    if (RMREG(flags) & CF) {
        _eflags |= CF;
        _eax = X_RMREG(eax);
    } else {
        _eflags &= ~CF;
        _eax = done;
    }
    if (lio_priv[DOSHLP_LW].post)
        lio_priv[DOSHLP_LW].post(scp);
}

void lio_init(void)
{
    int i;
    for (i = 0; i < LIOHLP_MAX; i++) {
	const struct hlp_hndl *ht = &hlp_thr[i];
	doshlp_setup_retf(&helpers[i],
	    ht->name, ht->thr, lio_rmseg, &lio_priv[i]);
    }
}
