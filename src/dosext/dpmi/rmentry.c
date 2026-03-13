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
 * Purpose: realmode entry helper for DPMI.
 * Coopth normally can't trace through mode switches, so this helper
 * provides the bridge between RM and PM coopthreads.
 *
 * Author: @stsp
 */

#include "dos2linux.h"
#include "emudpmi.h"
#include "coopth.h"
#include "cpu.h"
#include "int.h"
#include "emu.h"

static int link_umb(int on)
{
    pre_msdos();
    LWORD(eax) = 0x5803;
    LWORD(ebx) = on;
    call_msdos();
    if (isset_CF()) {
	int eax = LWORD(eax);
	_post_msdos();
	return eax;
    }
    LWORD(eax) = 0x5801;
    LWORD(ebx) = on ? 0x80 : 0;
    call_msdos();
    if (isset_CF()) {
	int eax = LWORD(eax);
	_post_msdos();
	return eax;
    }
    post_msdos();
    return 0;
}

int dpmi_rmentry(unsigned short entry16, int is_32)
{
    unsigned short mseg = 0;
    unsigned short es, di;

    pre_msdos();
    LWORD(eax) = 0x1687;
    do_int_call_back(0x2f);
    if (isset_CF() || LWORD(eax) != 0) {
        error("DPMI unavailable\n");
        _post_msdos();
        return -1;
    }
    if (!(LWORD(ebx) & 1)) {
        error("DPMI-32 unavailable\n");
        _post_msdos();
        return -1;
    }
    if (LWORD(esi)) {
        link_umb(1);
        mseg = com_dosallocmem(LWORD(esi));
        link_umb(0);
        if (!mseg) {
            error("malloc of %i para failed\n", LWORD(esi));
            _post_msdos();
            return -1;
        }
    }
    es = SREG(es);
    di = LWORD(edi);
    post_msdos();

    coopth_leave();
    LWORD(eax) = is_32 | 0x400;  // 0x400 - reinit IVT
    LWORD(ebx) = es;
    LWORD(ecx) = di;
    SREG(es) = mseg;
    LWORD(esi) = is_32 ? dpmi_sel32() : dpmi_sel16();
    LWORD(edi) = entry16;
    jmp_to(BIOSSEG, DPMI_rmentry);
    return 0;
}
