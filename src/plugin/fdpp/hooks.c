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
 * Purpose: fatfs & coopth hooks
 *
 * Author: Stas Sergeev
 */
#ifdef HAVE_LIBBSD
#include <bsd/string.h>
#endif
#include <fdpp/thunks.h>
#include "emu.h"
#include "cpu.h"
#include "int.h"
#include "lowmem.h"
#include "xms.h"
#include "fatfs.h"
#include "coopth.h"
#include "doshelpers.h"
#include "utilities.h"
#include "mhpdbg.h"
#include "boot.h"
#include "fdppconf.hh"
#include "hooks.h"

#define MAX_CLNUP_TIDS 5
static int clnup_tids[MAX_CLNUP_TIDS];
static int num_clnup_tids;
static int fdpp_tid;
static void *kptr;

static void fdpp_thr(void *arg)
{
    struct vm86_regs regs = REGS;
    int set_tf = isset_TF();
    int err = FdppCall(&regs);
    /* on NORET or ABORT the regs are already up-to-date because we
     * do not save/restore them in the appropriate handlers. In fact
     * we can't save/restore them in handlers, because restoring
     * after abort may prevent reboot, and restoring after noret
     * leaves the stack data below SP.
     * Update regs only for the OK case. */
    if (err == FDPP_RET_OK) {
	set_tf |= isset_TF();
	REGS = regs;
	if (set_tf)
	    set_TF();
    }
}

static void fdpp_cleanup(void)
{
    while (num_clnup_tids) {
        int i = num_clnup_tids - 1;
        coopth_cancel(clnup_tids[i]);
        coopth_unsafe_detach(clnup_tids[i], __FILE__);
    }
    lowmem_free(kptr);
    kptr = NULL;
}

static int fdpp_pre_boot(unsigned char *boot_sec)
{
    int err;
    void *hndl;
    const void *krnl;
    int krnl_len;
    struct fdpp_bss_list *bss;
    const char *fddir;
#ifdef USE_MHPDBG
    char *map;
#endif
    static far_t plt;
    static int initialized;
    uint16_t seg;
    uint16_t heap_seg;
    uint16_t daddr;
    int heap_sz;
    int khigh = 0;
    int hhigh = 0;

    if (!initialized) {
	plt.segment = BIOS_HLT_BLK_SEG;
	fdpp_tid = coopth_create_vm86("fdpp thr", fdpp_thr, fake_retf,
		&plt.offset);
	initialized++;
    }

    fddir = getenv("FDPP_KERNEL_DIR");
#ifdef FDPP_KERNEL_DIR
    if (!fddir)
	fddir = FDPP_KERNEL_DIR;
#endif
    if (!fddir)
	fddir = FdppKernelDir();
    assert(fddir);
    hndl = FdppKernelLoad(fddir, &krnl_len, &bss);
    if (!hndl)
        return -1;
    if (config.dos_up) {
        int to_hma = (xms_helper_init_ext() && config.dos_up == 2);
        heap_sz = to_hma ? 0 : 1024 * 4;
        kptr = lowmem_alloc_aligned(16, krnl_len + heap_sz);
        daddr = DOSEMU_LMHEAP_OFFS_OF(kptr);
        assert(!(daddr & 15));
        heap_seg = 0x90;  // for low heap
        seg = DOSEMU_LMHEAP_SEG + (daddr >> 4);
        khigh++;
        hhigh = to_hma + 1;
    } else {
        heap_sz = 1024 * 7;
        kptr = lowmem_alloc_aligned(16, heap_sz);
        daddr = DOSEMU_LMHEAP_OFFS_OF(kptr);
        assert(!(daddr & 15));
        heap_seg = DOSEMU_LMHEAP_SEG + (daddr >> 4);
        seg = 0x90;
        hhigh++;
    }
    krnl = FdppKernelReloc(hndl, seg);
    if (!krnl)
        return -1;
    /* copy kernel, clear bss and boot it */
    MEMCPY_2DOS(SEGOFF2LINEAR(seg, 0), krnl, krnl_len);
    if (bss) {
	int i;
	for (i = 0; i < bss->num; i++)
	    MEMSET_DOS(SEGOFF2LINEAR(seg, 0) + bss->ent[i].off, 0,
		    bss->ent[i].len);
	free(bss);
    }
    FdppKernelFree(hndl);
    err = fdpp_boot(plt, krnl, krnl_len, seg, khigh, heap_seg, heap_sz, hhigh,
	    boot_sec);
    if (err)
	return err;
    register_cleanup_handler(fdpp_cleanup);

#ifdef USE_MHPDBG
    map = assemble_path(fddir, FdppKernelMapName());
    if (map) {
        mhp_usermap_load_gnuld(map, SREG(cs));
        free(map);
    }
#endif
    return 0;
}

void fdpp_fatfs_hook(struct sys_dsc *sfiles, fatfs_t *fat)
{
    struct sys_dsc *sys_fdpp = &sfiles[FDP_IDX];

    sys_fdpp->is_sys = 1;
    sys_fdpp->flags |= FLG_NOREAD;
    sys_fdpp->pre_boot = fdpp_pre_boot;
}

int do_fdpp_call(uint16_t seg, uint16_t off)
{
    int ret = ASM_CALL_OK;
    int rc;

    assert(num_clnup_tids < MAX_CLNUP_TIDS);
    clnup_tids[num_clnup_tids++] = coopth_get_tid();
    coopth_cancel_disable();
    rc = do_call_back(seg, off);
    /* re-enable cancellability only if it was not canceled already */
    if (rc == 0)
	coopth_cancel_enable();
    else
	ret = ASM_CALL_ABORT;
    num_clnup_tids--;
    return ret;
}
