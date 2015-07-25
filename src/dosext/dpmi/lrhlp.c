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
 * Purpose: bios-assisted longread/longwrite helpers
 *
 * Author: Stas Sergeev <stsp@users.sourceforge.net>
 *
 */

#include <string.h>
#include "emu.h"
#include "cpu.h"
#include "memory.h"
#include "int.h"
#include "hlt.h"
#include "lrhlp.h"

static char *io_buffer;
static int io_buffer_size;

void set_io_buffer(char *ptr, unsigned int size)
{
    io_buffer = ptr;
    io_buffer_size = size;
}

void unset_io_buffer()
{
    io_buffer_size = 0;
}

static void lr_hlt(Bit32u idx, void *arg)
{
    unsigned int offs = REG(edi);
    unsigned int size = REG(ecx);
    unsigned int dos_ptr = SEGOFF2LINEAR(REG(ds), LWORD(edx));
    if (offs + size <= io_buffer_size)
	MEMCPY_2UNIX(io_buffer + offs, dos_ptr, size);
    fake_retf(0);
}

static void lw_hlt(Bit32u idx, void *arg)
{
    unsigned int offs = REG(edi);
    unsigned int size = REG(ecx);
    unsigned int dos_ptr = SEGOFF2LINEAR(REG(ds), LWORD(edx));
    if (offs + size <= io_buffer_size)
	MEMCPY_2DOS(dos_ptr, io_buffer + offs, size);
    fake_retf(0);
}

void lrhlp_setup(void)
{
#define MK_LR_OFS(ofs) ((long)(ofs)-(long)MSDOS_lr_start)
#define MK_LW_OFS(ofs) ((long)(ofs)-(long)MSDOS_lw_start)
    emu_hlt_t hlt_hdlr = HLT_INITIALIZER;
    u_short lr_off, lw_off;

    hlt_hdlr.name = "msdos longread";
    hlt_hdlr.func = lr_hlt;
    lr_off = hlt_register_handler(hlt_hdlr);
    hlt_hdlr.name = "msdos longwrite";
    hlt_hdlr.func = lw_hlt;
    lw_off = hlt_register_handler(hlt_hdlr);

    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
			     MK_LR_OFS(MSDOS_lr_entry_ip)), lr_off);
    WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
			     MK_LR_OFS(MSDOS_lr_entry_cs)),
	       BIOS_HLT_BLK_SEG);
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_ip)),
	       lw_off);
    WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_cs)),
	       BIOS_HLT_BLK_SEG);
}
