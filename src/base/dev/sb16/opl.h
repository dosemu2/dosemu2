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

#ifndef __OPL_H__
#define __OPL_H__

// general functions
void opl_init(Bit32u samplerate);
void opl_write(Bitu idx, Bit8u val);
void opl_getsample(Bit16s* sndptr, Bits numsamples);

Bitu opl_reg_read(Bitu port);
void opl_write_index(Bitu port, Bit8u val);

#endif
