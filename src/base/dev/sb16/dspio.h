/*
 *  Copyright (C) 2006 Stas Sergeev <stsp@users.sourceforge.net>
 *
 * The below copyright strings have to be distributed unchanged together
 * with this file. This prefix can not be modified or separated.
 */

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
#ifndef __DSPIO_H__
#define __DSPIO_H__

struct dspio_state;

extern struct dspio_state *dspio_init(void);
extern void dspio_reset(struct dspio_state *dspio);
extern void dspio_done(struct dspio_state *dspio);
extern void dspio_start_dma(struct dspio_state *dspio);
extern void dspio_stop_dma(struct dspio_state *dspio);
extern void dspio_stop_midi(struct dspio_state *dspio);
extern void dspio_toggle_speaker(struct dspio_state *dspio, int on);
extern int dspio_get_speaker_state(struct dspio_state *dspio);
extern Bit32u dspio_get_midi_in_time(struct dspio_state *dspio);
extern void dspio_write_dac(struct dspio_state *dspio, Bit8u samp);
extern void dspio_timer(struct dspio_state *dspio);
extern void dspio_run_synth(void);
extern void dspio_clear_fifos(struct dspio_state *dspio);
extern int dspio_input_enable(struct dspio_state *dspio, enum MixChan mc);
extern int dspio_input_disable(struct dspio_state *dspio, enum MixChan mc);
extern double dspio_calc_vol(int val, int step, int init_db);

#endif
