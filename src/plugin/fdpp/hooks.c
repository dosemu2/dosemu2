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
#include "hlt.h"
#include "fatfs.h"
#include "coopth.h"
#include "doshelpers.h"
#include "utilities.h"
#include "mhpdbg.h"
#include "boot.h"
#include "hooks.h"

#define MAX_CLNUP_TIDS 5
static int clnup_tids[MAX_CLNUP_TIDS];
static int num_clnup_tids;
static int fdpp_tid;

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

static void fdpp_plt(Bit16u idx, void *arg)
{
    fake_retf(0);
    coopth_start(fdpp_tid, fdpp_thr, NULL);
}

static void fdpp_cleanup(void)
{
    while (num_clnup_tids) {
        int i = num_clnup_tids - 1;
        coopth_cancel(clnup_tids[i]);
        coopth_unsafe_detach(clnup_tids[i], __FILE__);
    }
}

static int fdpp_pre_boot(void)
{
    int err;
    static far_t plt;
    static int initialized;

    if (!initialized) {
	emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
	hlt_hdlr.name      = "fdpp plt";
	hlt_hdlr.func      = fdpp_plt;
	plt.offset = hlt_register_handler(hlt_hdlr);
	plt.segment = BIOS_HLT_BLK_SEG;
	fdpp_tid = coopth_create("fdpp thr");
	initialized++;
    }

    err = fdpp_boot(plt);
    if (err)
	return err;
    register_cleanup_handler(fdpp_cleanup);

#ifdef USE_MHPDBG
    if (fddir_boot) {
        char *map = assemble_path(fddir_boot, FdppKernelMapName());
        if (map) {
            mhp_usermap_load_gnuld(map, SREG(cs));
            free(map);
        }
    }
#endif
    return 0;
}

void fdpp_fatfs_hook(struct sys_dsc *sfiles, fatfs_t *fat)
{
    static char fdpp_krnl[16];
    char *fdpath;
    const char *fdkrnl;
    int err;
    const char *dir = fatfs_get_host_dir(fat);
    const struct sys_dsc sys_fdpp = { .name = fdpp_krnl, .is_sys = 1,
	    .pre_boot = fdpp_pre_boot };

    if (!fddir_boot || strcmp(dir, fddir_boot) != 0)
	return;
    fdkrnl = FdppKernelName();
    assert(fdkrnl);
    fdpath = assemble_path(fddir_boot, fdkrnl);
    err = access(fdpath, R_OK);
    free(fdpath);
    if (err)
	return;
    strlcpy(fdpp_krnl, fdkrnl, sizeof(fdpp_krnl));
    strupper(fdpp_krnl);
    sfiles[FDP_IDX] = sys_fdpp;
}

int do_fdpp_call(uint16_t seg, uint16_t off)
{
    int ret = ASM_CALL_OK;
    int rc;

    assert(num_clnup_tids < MAX_CLNUP_TIDS);
    clnup_tids[num_clnup_tids++] = coopth_get_tid();
    coopth_cancel_disable();
    rc = do_call_back(seg, off);
    /* re-enable cancelability only if it was not canceled already */
    if (rc == 0)
	coopth_cancel_enable();
    else
	ret = ASM_CALL_ABORT;
    num_clnup_tids--;
    return ret;
}
