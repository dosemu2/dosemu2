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
#if FDPP_API_VER != 11
#error wrong fdpp version
#endif
#include "emu.h"
#include "init.h"
#include "int.h"
#include "utilities.h"
#include "coopth.h"
#include "dos2linux.h"
#include "fatfs.h"
#include "doshelpers.h"

static void copy_stk(uint8_t *sp, uint8_t len)
{
    uint8_t *stk;
    if (!len)
	return;
    LWORD(esp) -= len;
    stk = SEG_ADR((uint8_t *), ss, sp);
    memcpy(stk, sp, len);
}

static void fdpp_call(struct vm86_regs *regs, uint16_t seg,
	uint16_t off, uint8_t *sp, uint8_t len)
{
    struct vm86_regs saved_regs = REGS;
    REGS = *regs;
    copy_stk(sp, len);
    do_call_back(seg, off);
    *regs = REGS;
    REGS = saved_regs;
}

static void fdpp_call_noret(struct vm86_regs *regs, uint16_t seg,
	uint16_t off, uint8_t *sp, uint8_t len)
{
    REGS = *regs;
    coopth_leave();
    fake_iret();
    copy_stk(sp, len);
    jmp_to(0xffff, 0);
    fake_call_to(seg, off);
}

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
        vlog_printf(-1, format, ap);
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

static struct fdpp_api api = {
    .so2lin = fdpp_so2lin,
    .exit = fdpp_exit,
    .abort = fdpp_abort,
    .print = fdpp_print,
    .debug = fdpp_debug,
    .panic = fdpp_panic,
    .cpu_relax = fdpp_relax,
    .asm_call = fdpp_call,
    .asm_call_noret = fdpp_call_noret,
};

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
    register_plugin_call(DOS_HELPER_PLUGIN_ID_FDPP, FdppCall);
    fdkrnl = FdppKernelName();
    assert(fdkrnl);
    fddir = getenv("FDPP_KERNEL_DIR");
    if (!fddir)
	fddir = FdppDataDir();
    assert(fddir);
    fdpath = assemble_path(fddir, fdkrnl, 0);
    err = access(fdpath, R_OK);
    free(fdpath);
    if (err)
	return;
    strcpy(fdpp_krnl, fdkrnl);
    strupper(fdpp_krnl);
    fddir_boot = strdup(fddir);
}
