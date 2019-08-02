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
 * Purpose: fdpp kernel support
 *
 * Author: Stas Sergeev
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fdpp/thunks.h>
#if FDPP_API_VER != 23
#error wrong fdpp version
#endif
#include "emu.h"
#include "init.h"
#include "int.h"
#include "hlt.h"
#include "utilities.h"
#include "coopth.h"
#include "dos2linux.h"
#include "fatfs.h"
#include "doshelpers.h"
#include "mhpdbg.h"
#include "boot.h"
#include "vgdbg.h"
#include "fdppconf.hh"

static char fdpp_krnl[16];
#define MAX_CLNUP_TIDS 5
static int clnup_tids[MAX_CLNUP_TIDS];
static int num_clnup_tids;
static int fdpp_tid;

static void copy_stk(uint8_t *sp, uint8_t len)
{
    uint8_t *stk;
    if (!len)
	return;
    LWORD(esp) -= len;
    stk = SEG_ADR((uint8_t *), ss, sp);
    memcpy(stk, sp, len);
}

static int fdpp_call(struct vm86_regs *regs, uint16_t seg,
        uint16_t off, uint8_t *sp, uint8_t len)
{
    jmp_buf canc;
    int ret = ASM_CALL_OK;
    REGS = *regs;
    copy_stk(sp, len);
    assert(num_clnup_tids < MAX_CLNUP_TIDS);
    clnup_tids[num_clnup_tids++] = coopth_get_tid();
    if (setjmp(canc)) {
	ret = ASM_CALL_ABORT;
    } else {
	coopth_set_cancel_target(&canc);
	do_call_back(seg, off);
	coopth_set_cancel_target(NULL);
    }
    num_clnup_tids--;
    *regs = REGS;
    return ret;
}

static void fdpp_call_noret(struct vm86_regs *regs, uint16_t seg,
	uint16_t off, uint8_t *sp, uint8_t len)
{
    REGS = *regs;
    coopth_leave();
    copy_stk(sp, len);
    jmp_to(0xffff, 0);
    fake_call_to(seg, off);
    *regs = REGS;
}

#ifdef USE_MHPDBG
static void fdpp_relocate_notify(uint16_t oldseg, uint16_t newseg,
                                 uint16_t startoff, uint32_t blklen)
{
    mhp_usermap_move_block(oldseg, newseg, startoff, blklen);
}
#endif

static void fdpp_abort(const char *file, int line)
{
    p_dos_str("\nfdpp crashed.\n");
    dosemu_error("fdpp: abort at %s:%i\n", file, line);
    p_dos_str("Press any key to exit.\n");
    set_IF();
    com_biosgetch();
    clear_IF();
    leavedos(3);
}

static void fdpp_exit(int rc)
{
    leavedos(rc);
}

static void fdpp_panic(const char *msg)
{
    error("fdpp: PANIC: %s\n", msg);
    p_dos_str("PANIC: %s\n", msg);
    p_dos_str("Press any key to exit.\n");
    set_IF();
    com_biosgetch();
    clear_IF();
    leavedos(3);
}

static void fdpp_print(int prio, const char *format, va_list ap)
{
    switch(prio) {
    case FDPP_PRINT_TERMINAL:
        vprintf(format, ap);
        break;
    case FDPP_PRINT_LOG:
        if (debug_level('f')) {
            log_printf(-1, "fdpp: ");
            vlog_printf(-1, format, ap);
        }
        break;
    case FDPP_PRINT_SCREEN:
        p_dos_vstr(format, ap);
        break;
    }
}

static uint8_t *fdpp_so2lin(uint16_t seg, uint16_t off)
{
    return LINEAR2UNIX(SEGOFF2LINEAR(seg, off));
}

static int fdpp_ping(void)
{
    if (signal_pending())
	coopth_yield();
#if 0
    if (GETusTIME(0) - watchdog > 1000000) {
	watchdog = GETusTIME(0);	// just once
	error("fdpp hang, rebooting\n");
	coopth_leave();
	dos_ctrl_alt_del();
	return -1;
    }
#endif
    return 0;
}

static void fdpp_relax(void)
{
    int ii = isset_IF();

    set_IF();
    coopth_wait();
    if (!ii)
	clear_IF();
}

static void fdpp_debug(const char *msg)
{
    dosemu_error("%s\n", msg);
}

static void fdpp_cleanup(void)
{
    while (num_clnup_tids) {
        int i = num_clnup_tids - 1;
        coopth_cancel(clnup_tids[i]);
        coopth_unsafe_detach(clnup_tids[i], __FILE__);
    }
}

static struct fdpp_api api = {
    .so2lin = fdpp_so2lin,
    .exit = fdpp_exit,
    .abort = fdpp_abort,
    .print = fdpp_print,
    .debug = fdpp_debug,
    .panic = fdpp_panic,
    .cpu_relax = fdpp_relax,
    .ping = fdpp_ping,
    .asm_call = fdpp_call,
    .asm_call_noret = fdpp_call_noret,
#ifdef USE_MHPDBG
    .relocate_notify = fdpp_relocate_notify,
#endif
#ifdef HAVE_VALGRIND
    .mark_mem = fdpp_mark_mem,
    .prot_mem = fdpp_prot_mem,
#endif
};

static void fdpp_thr(void *arg)
{
    struct vm86_regs regs = REGS;
    FdppCall(&regs);
    REGS = regs;
}

static void fdpp_plt(Bit16u idx, void *arg)
{
    fake_retf(0);
    coopth_start(fdpp_tid, fdpp_thr, NULL);
}

static int fdpp_pre_boot(void)
{
    int err;
    far_t plt;
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    hlt_hdlr.name      = "fdpp plt";
    hlt_hdlr.func      = fdpp_plt;
    plt.offset = hlt_register_handler(hlt_hdlr);
    plt.segment = BIOS_HLT_BLK_SEG;
    fdpp_tid = coopth_create("fdpp thr");

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

static void fdpp_fatfs_hook(struct sys_dsc *sfiles, fatfs_t *fat)
{
    const char *dir = fatfs_get_host_dir(fat);
    const struct sys_dsc sys_fdpp = { .name = fdpp_krnl, .is_sys = 1,
	    .pre_boot = fdpp_pre_boot };

    if (strcmp(dir, fddir_boot) != 0)
	return;
    sfiles[FDP_IDX] = sys_fdpp;
}

CONSTRUCTOR(static void init(void))
{
    int req_ver = 0;
    char *fdpath;
    const char *fdkrnl;
    const char *fddir = NULL;
    int err = FdppInit(&api, FDPP_API_VER, &req_ver);
    if (err) {
	if (req_ver != FDPP_API_VER)
	    error("fdpp version mismatch: %i %i\n", FDPP_API_VER, req_ver);
	leavedos(3);
    }
    fddir = getenv("FDPP_KERNEL_DIR");
    if (!fddir)
	fddir = FdppDataDir();
    assert(fddir);

    fdkrnl = FdppKernelName();
    assert(fdkrnl);
    fdpath = assemble_path(fddir, fdkrnl);
    err = access(fdpath, R_OK);
    free(fdpath);
    if (err)
	return;
    strcpy(fdpp_krnl, fdkrnl);
    strupper(fdpp_krnl);
    fddir_boot = strdup(fddir);
    fatfs_set_sys_hook(fdpp_fatfs_hook);
    register_debug_class('f', NULL, "fdpp");
    dbug_printf("%s\n", FdppVersionString());
}
