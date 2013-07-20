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
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 *
 * Management for the static 32K heap in a low memory.
 * Used by various dosemu internal subsystems.
 */

#include <signal.h>
#include "emu.h"
#include "memory.h"
#include "smalloc.h"
#include "utilities.h"
#include "lowmem.h"

static smpool mp;
unsigned char *dosemu_lmheap_base;

int lowmem_heap_init()
{
    dosemu_lmheap_base = MK_FP32(DOSEMU_LMHEAP_SEG, DOSEMU_LMHEAP_OFF);
    sminit(&mp, dosemu_lmheap_base, DOSEMU_LMHEAP_SIZE);
    smregister_error_notifier(dosemu_error);
    return 1;
}

void * lowmem_heap_alloc(int size)
{
	char *ptr = smalloc(&mp, size);
	if (!ptr) {
		error("lowmem_heap: OOM, size=%i\n", size);
		leavedos(86);
	}
	return ptr;
}

void lowmem_heap_free(void *p)
{
	return smfree(&mp, p);
}
