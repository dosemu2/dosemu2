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
#ifndef __SNDPCM_H__
#define __SNDPCM_H__

extern int pcm_init(void);
extern void pcm_done(void);
extern void pcm_reset(void);
extern int pcm_allocate_stream(int channels, char *name);
extern void pcm_set_flag(int strm_idx, int flag);
extern void pcm_set_mode(int strm_idx, int mode);
extern int pcm_flush(int strm_idx);
extern double pcm_samp_period(double rate, int channels);
extern double pcm_frag_period(int size, struct player_params *params);
extern void pcm_write_samples(void *ptr, size_t size,
	double rate, int format, int strm_idx);
extern int pcm_format_size(int format);
extern void pcm_timer(void);
extern double pcm_get_stream_time(int strm_idx);

#define PCM_FLAG_RAW 1

enum { PCM_MODE_NORMAL, PCM_MODE_POST };

#endif
