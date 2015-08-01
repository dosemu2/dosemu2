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

#include "emu.h"
#include "memory.h"
#include "lrhlp.h"

void lrhlp_setup(far_t rmcb, int is_w)
{
#define MK_LR_OFS(ofs) ((long)(ofs)-(long)MSDOS_lr_start)
#define MK_LW_OFS(ofs) ((long)(ofs)-(long)MSDOS_lw_start)
    if (!is_w) {
	WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_ip)), rmcb.offset);
	WRITE_WORD(SEGOFF2LINEAR(DOS_LONG_READ_SEG, DOS_LONG_READ_OFF +
		     MK_LR_OFS(MSDOS_lr_entry_cs)), rmcb.segment);
    } else {
	WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_ip)),
	       rmcb.offset);
	WRITE_WORD(SEGOFF2LINEAR
	       (DOS_LONG_WRITE_SEG,
		DOS_LONG_WRITE_OFF + MK_LW_OFS(MSDOS_lw_entry_cs)),
	       rmcb.segment);
    }
}
