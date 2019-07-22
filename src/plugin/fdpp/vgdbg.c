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
 * Purpose: valgrind debugging support
 *
 * Author: Stas Sergeev
 */
#include <stdint.h>
#include <valgrind/memcheck.h>
#include <fdpp/memtype.h>
#include "memory.h"
#include "cpu.h"
#include "dosemu_debug.h"
#include "vgdbg.h"

void fdpp_mark_mem(uint16_t seg, uint16_t off, uint16_t size, int type)
{
    void *ptr = MEM_BASE32(SEGOFF2LINEAR(seg, off));
    switch (type) {
    case FD_MEM_READONLY:
	/* oops, no readonly support in valgrind */
	VALGRIND_MAKE_MEM_NOACCESS(ptr, size);
	break;
    case FD_MEM_UNINITIALIZED:
	VALGRIND_MAKE_MEM_UNDEFINED(ptr, size);
	break;
    }
}
