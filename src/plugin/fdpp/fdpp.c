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
#include <fdpp/thunks.h>
#include "emu.h"
#include "init.h"
#include "utilities.h"
#include "coopth.h"
#include "dos2linux.h"

static void fdpp_call(uint16_t seg, uint16_t off, uint8_t *sp,
	uint8_t len)
{
    uint8_t *stk;

    LWORD(esp) -= len;
    stk = SEG_ADR((uint8_t *), ss, sp);
    memcpy(stk, sp, len);
    do_call_back(seg, off);
}

static struct dl_ops ops = {
    .set_symtab = FdppSetSymTab,
    .ccall = FdppThunkCall,
};

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

static void fdpp_print(const char *format, va_list ap)
{
    vprintf(format, ap);
}

static uint8_t *fdpp_mbase(void)
{
    return lowmem_base;
}

static void fdpp_sti(void)
{
    set_IF();
}

static void fdpp_cli(void)
{
    clear_IF();
}

static uint32_t fdpp_getreg(enum FdppReg reg)
{
    switch (reg) {
    case REG_eflags:
	return REG(eflags);
    case REG_eax:
	return REG(eax);
    case REG_ebx:
	return REG(ebx);
    case REG_ecx:
	return REG(ecx);
    case REG_edx:
	return REG(edx);
    case REG_esi:
	return REG(esi);
    case REG_edi:
	return REG(edi);
    case REG_ebp:
	return REG(ebp);
    case REG_cs:
	return SREG(cs);
    case REG_ds:
	return SREG(ds);
    case REG_es:
	return SREG(es);
    default:
	return 0;
    }
}

static void fdpp_setreg(enum FdppReg reg, uint32_t value)
{
    switch (reg) {
    case REG_eflags:
	REG(eflags) = value;
	break;
    case REG_eax:
	REG(eax) = value;
	break;
    case REG_ebx:
	REG(ebx) = value;
	break;
    case REG_ecx:
	REG(ecx) = value;
	break;
    case REG_edx:
	REG(edx) = value;
	break;
    case REG_esi:
	REG(esi) = value;
	break;
    case REG_edi:
	REG(edi) = value;
	break;
    case REG_ebp:
	REG(ebp) = value;
	break;
    case REG_ds:
	SREG(ds) = value;
	break;
    case REG_es:
	SREG(es) = value;
	break;
    default:
	break;
    }
}

static void fdpp_relax(void)
{
    int ii = isset_IF();

    set_IF();
    coopth_wait();
    if (!ii)
	clear_IF();
}

static void fdpp_int3(void)
{
    error("fdpp int3\n");
}

static struct fdpp_api api = {
    .mem_base = fdpp_mbase,
    .abort_handler = fdpp_abort,
    .print_handler = fdpp_print,
    .cpu_relax = fdpp_relax,
    .asm_call = fdpp_call,
    .getreg = fdpp_getreg,
    .setreg = fdpp_setreg,
    .thunks = {
        .enable = fdpp_sti,
        .disable = fdpp_cli,
        .int3 = fdpp_int3,
    },
};

CONSTRUCTOR(static void init(void))
{
    FdppInit(&api);
    register_dl_ops(&ops);
}
