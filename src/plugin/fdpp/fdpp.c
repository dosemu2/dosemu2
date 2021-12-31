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
#include <fdpp/thunks.h>
#if FDPP_API_VER != 30
#error wrong fdpp version
#endif
#include "emu.h"
#include "init.h"
#include "int.h"
#include "utilities.h"
#include "coopth.h"
#include "dos2linux.h"
#include "fatfs.h"
#include "mhpdbg.h"
#include "hooks.h"
#include "vgdbg.h"
#include "fdppconf.hh"

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
    int ret;
    int set_tf = isset_TF();
    REGS = *regs;
    if (set_tf)
	set_TF();
    copy_stk(sp, len);
    ret = do_fdpp_call(seg, off);
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

static int fdpp_dos_space(const void *ptr)
{
    return ((uintptr_t)ptr >= (uintptr_t)lowmem_base &&
	    (uintptr_t)ptr < (uintptr_t)lowmem_base + LOWMEM_SIZE + HMASIZE);
}

static void fdpp_fmemcpy(fdpp_far_t d, fdpp_far_t s, size_t n)
{
    dosaddr_t dst = SEGOFF2LINEAR(d.seg, d.off);
    dosaddr_t src = SEGOFF2LINEAR(s.seg, s.off);
    memcpy_dos2dos(dst, src, n);
}

static void fdpp_n_fmemcpy(fdpp_far_t d, const void *s, size_t n)
{
    dosaddr_t dst = SEGOFF2LINEAR(d.seg, d.off);
    memcpy_2dos(dst, s, n);
}

static void fdpp_fmemset(fdpp_far_t d, int ch, size_t n)
{
    dosaddr_t dst = SEGOFF2LINEAR(d.seg, d.off);
    memset_dos(dst, ch, n);
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
    .fmemcpy = fdpp_fmemcpy,
    .n_fmemcpy = fdpp_n_fmemcpy,
    .fmemset = fdpp_fmemset,
#ifdef USE_MHPDBG
    .relocate_notify = fdpp_relocate_notify,
#endif
#ifdef HAVE_VALGRIND
    .mark_mem = fdpp_mark_mem,
    .prot_mem = fdpp_prot_mem,
#endif
    .is_dos_space = fdpp_dos_space,
};

CONSTRUCTOR(static void init(void))
{
    int req_ver = 0;
    int err = FdppInit(&api, FDPP_API_VER, &req_ver);
    if (err) {
	if (req_ver != FDPP_API_VER)
	    error("fdpp version mismatch: %i %i\n", FDPP_API_VER, req_ver);
	leavedos(3);
	return;
    }
    fatfs_set_sys_hook(fdpp_fatfs_hook);
    register_debug_class('f', NULL, "fdpp");
    dbug_printf("%s\n", FdppVersionString());
    fdpp_loaded();
}
